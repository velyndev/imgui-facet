// Knob: a rotary control. A dial with a track arc along the sweep (open at the bottom by default),
// the value shown as an accent arc, a dot or a wiper, the value text inside the dial and the label
// under it.
//
// Input is the value drag core on a vertical track: dragging up increases, every press grabs
// without moving the value, Shift is finer, the arrows (Up and Right increase, Down and Left
// decrease), Page Up and Down, Home and End step while focused, and the wheel turns it while
// focused or with Alt. With circularDrag the value follows the pointer's angle around the centre
// instead. A double click restores the value the knob had when the first click of it began.

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/value_drag.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

// The default dial is two control heights across: room for the value text inside it.
constexpr float kKnobDiameterHeights = 2.0f;
// The value text sits inside the dial when the dial is at least this many control heights across.
constexpr float kKnobInnerTextHeights = 1.5f;
// The face is inset from the track arc by the arc's thickness plus this share of the radius.
constexpr float kKnobFaceInsetShare = 0.18f;
// The dot of the Dot variant: this share of the face radius, from the edge.
constexpr float kKnobDotShare = 0.16f;
// A held face grows by this share of its radius, less than a slider knob: the dial is larger.
constexpr float kKnobPressGrowth = 0.04f;
// The hover darkens the face outline halfway from the boundary color toward label.
constexpr float kKnobHoverShare = 0.5f;
// With the value text inside, the wiper starts this share of the face radius out, clear of the text.
constexpr float kKnobWiperStartShare = 0.55f;
// The value text inside the face is cut to this share of its radius (most of the diameter).
constexpr float kKnobTextShare = 1.6f;
// The value the knob had when the press of a click began, for the double click.
struct KnobPress
{
    double value = 0.0;
    bool set = false;
};

constexpr float kPi = 3.14159265358979f;

template <class Value>
bool KnobImpl(const char* label, Value* value, double min, double max, bool integer, const KnobOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(o.size);
    if (min > max)
        std::swap(min, max);
    const ImGuiID id = window->GetID(label);
    const Theme& t = T();

    internal::ValueMapping mapping;
    mapping.min = min;
    mapping.max = max;
    mapping.scale = o.scale;
    mapping.step = o.step;
    mapping.integer = integer;
    mapping = internal::NormalizeMapping(mapping);
    const bool fixed = value == nullptr || !(mapping.max > mapping.min);
    const bool readOnly = o.readOnly || fixed || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const double current = value != nullptr ? internal::SliderValueIn(*value) : mapping.min;

    // Size: the dial square, the label line under it.
    float diameter = std::round(o.diameter > 0.0f ? Px(o.diameter) : m.control.height * kKnobDiameterHeights);
    const float available = ImGui::GetContentRegionAvail().x;
    if (available >= 1.0f)
        diameter = std::min(diameter, std::floor(available));
    diameter = std::max(2.0f, diameter);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasLabel = labelEnd != label;
    float labelLine = 0.0f;
    float caption = 0.0f;
    {
        internal::ControlTextScope footnote(TextStyle::Footnote);
        caption = std::ceil(ImGui::GetFontSize());
        labelLine = hasLabel ? PxSnapped(t.spacing.xs) + caption : 0.0f;
    }
    const bool innerText = diameter >= m.control.height * kKnobInnerTextHeights;
    const float valueLine = innerText ? 0.0f : PxSnapped(t.spacing.xs) + caption;
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImRect dial(origin, ImVec2(origin.x + diameter, origin.y + diameter));
    const ImRect widget(origin, ImVec2(origin.x + diameter, dial.Max.y + valueLine + labelLine));

    // The vertical drag track: one fader length for the whole range, centred on the dial.
    const float travel = std::round(Px(t.metrics.faderLength));
    const ImVec2 center(std::round(dial.GetCenter().x), std::round(dial.GetCenter().y));
    const double startFraction = internal::ValueToFraction(mapping, std::isnan(current) ? mapping.min : current);
    const float trackBottom = center.y + static_cast<float>(startFraction) * travel;
    const ImRect track(center.x, trackBottom - travel, center.x, trackBottom);

    ImGui::BeginGroup();
    ImGui::ItemSize(widget.GetSize());
    const bool typing = internal::SliderTypingPart(id) == 0;
    bool written = false;
    double edited = current;
    internal::ItemState item;
    if (typing)
    {
        internal::NumberFieldParams params;
        params.format = o.format;
        params.formatter = o.formatter;
        params.formatterUser = o.formatterUser;
        params.min = mapping.min;
        params.max = mapping.max;
        params.integer = integer;
        params.step = 0.0;
        params.size = o.size;
        const float half = std::round(Px(t.metrics.numberFieldWidth) * 0.5f);
        const ImRect rect(center.x - half, center.y - std::round(m.control.height * 0.5f), center.x + half,
                          center.y - std::round(m.control.height * 0.5f) + m.control.height);
        const internal::SliderTypeResult typed = internal::SliderTypeField(id, 0, rect, current, params, id);
        if (typed.changed && !readOnly)
        {
            const double snapped = internal::SnapValue(mapping, typed.value);
            written = snapped != current;
            edited = snapped;
        }
    }
    else
    {
        internal::ItemOptions itemOptions;
        itemOptions.press = internal::PressMode::Click;
        itemOptions.readOnly = readOnly;
        itemOptions.mixed = o.mixed;
        internal::ApplyNextItemData(next);
        item = internal::PartBehavior(id, dial, itemOptions);
        KnobPress& press = internal::GetState<KnobPress>(id);
        if (internal::SliderWantsTyping(item))
        {
            if (ImGui::GetActiveID() == id)
                ImGui::ClearActiveID();
            internal::SliderBeginTyping(id, 0);
        }
        else if (!readOnly && item.pressed && !item.keyboardPressed && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            // A double click restores the value from the start of its first click.
            if (press.set && press.value != current)
            {
                edited = press.value;
                written = true;
                ImGui::MarkItemEdited(id);
            }
        }
        else
        {
            if (item.pressed && !item.keyboardPressed)
                press = {current, true};
            internal::ValueDragOptions drag;
            drag.mapping = mapping;
            drag.orientation = Orientation::Vertical;
            drag.grabRadius = -1.0f;
            drag.readOnly = readOnly;
            // A dial has no direction on screen: Left and Right turn it like Down and Up.
            drag.crossArrows = true;
            internal::ItemState input = item;
            ImGuiContext& g = *GImGui;
            const bool circular = o.circularDrag && g.ActiveId == id && g.ActiveIdSource == ImGuiInputSource_Mouse &&
                                  item.held && item.forced == internal::ForcedItemState::None;
            if (circular)
                input.held = false;  // the angle below drives the value, not the vertical travel
            const internal::ValueDragResult result = internal::ValueDragBehavior(input, &edited, track, drag);
            written = result.changed && !readOnly;
            if (circular && !readOnly && ImGui::IsMousePosValid(&g.IO.MousePos))
            {
                const float sweep = (o.sweep > 0.0f ? std::min(o.sweep, 360.0f) : t.metrics.dialSweep) * kPi / 180.0f;
                const float start = kPi * 0.5f + (2.0f * kPi - sweep) * 0.5f;
                float angle = std::atan2(g.IO.MousePos.y - center.y, g.IO.MousePos.x - center.x);
                float along = angle - start;
                while (along < 0.0f)
                    along += 2.0f * kPi;
                // Inside the gap at the bottom the value goes to the nearer end.
                double fraction = along <= sweep ? along / sweep : (along - sweep < (2.0f * kPi - sweep) * 0.5f ? 1.0 : 0.0);
                const double next_ = internal::FractionToValue(mapping, fraction);
                if (next_ != edited)
                {
                    edited = next_;
                    written = true;
                    ImGui::MarkItemEdited(id);
                }
                ImGui::SetKeyOwner(ImGuiKey_Escape, id);
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
                {
                    edited = press.set ? press.value : current;
                    written = edited != current;
                    internal::MarkEditCanceled(id);
                    ImGui::ClearActiveID();
                }
            }
        }
    }
    if (written && value != nullptr)
    {
        if (integer)
            edited = std::round(edited);
        if constexpr (std::is_integral_v<Value>)
            *value = static_cast<Value>(std::llround(edited));
        else
            *value = static_cast<Value>(edited);
    }
    else
    {
        written = false;
    }

    // ---- Draw ----
    const bool disabled = item.disabled || (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::SliderLook look = internal::PickSliderLook(disabled, readOnly);
    const bool mixed = (o.mixed || item.mixed) && !written;
    const Colors& c = t.colors;
    ImDrawList* drawList = window->DrawList;
    char formatted[64];
    internal::FormatNumber(formatted, sizeof(formatted), edited, o.format, o.formatter, o.formatterUser, integer, false);
    if (!typing && ImGui::IsRectVisible(widget.Min, widget.Max))
    {
        const float radius = diameter * 0.5f;
        const float thickness = m.track;
        const float arcRadius = radius - thickness * 0.5f;
        const float sweep = (o.sweep > 0.0f ? std::min(o.sweep, 360.0f) : t.metrics.dialSweep) * kPi / 180.0f;
        const float start = kPi * 0.5f + (2.0f * kPi - sweep) * 0.5f;
        const float fraction = static_cast<float>(internal::ValueToFraction(mapping, std::isnan(edited) ? mapping.min : edited));
        const float from = o.fillFromCenter ? 0.5f : 0.0f;
        const int segments = std::max(12, static_cast<int>(radius));
        // Track arc in the 3:1 boundary color (destructive with an error).
        const ImVec4 rest = look == internal::SliderLook::Disabled ? c.fill[3]
                            : (o.error != nullptr ? c.destructive : ControlBoundaryColor(c));
        drawList->PathArcTo(center, arcRadius, start, start + sweep, segments);
        drawList->PathStroke(ToU32(rest), thickness);
        const ImVec4 fill = look == internal::SliderLook::Disabled ? c.labelTertiary
                            : (look == internal::SliderLook::ReadOnly ? c.labelSecondary : c.accent);
        if (!mixed && o.variant == KnobVariant::Arc && fraction != from)
        {
            const float a = start + sweep * std::min(from, fraction);
            const float b = start + sweep * std::max(from, fraction);
            drawList->PathArcTo(center, arcRadius, a, b, segments);
            drawList->PathStroke(ToU32(fill), thickness);
        }
        // The face, inset from the arc, grows a little while held like a slider knob.
        const float face = std::round(radius - thickness - radius * kKnobFaceInsetShare);
        const float faceRadius = std::max(1.0f, std::round(face * (1.0f + (look == internal::SliderLook::Normal ? kKnobPressGrowth * item.pressAmount : 0.0f))));
        const ImVec4 outline = look == internal::SliderLook::Disabled
                                   ? c.labelTertiary
                                   : Mix(ControlBoundaryColor(c), c.label, item.hoverAmount * kKnobHoverShare);
        drawList->AddCircleFilled(center, faceRadius, ToU32(look == internal::SliderLook::Disabled ? c.fill[3] : c.bg));
        drawList->AddCircle(center, faceRadius, ToU32(outline), 0, HairlineThickness() * 1.5f);
        if (!mixed && o.variant != KnobVariant::Arc)
        {
            const float angle = start + sweep * fraction;
            const ImVec2 dir(std::cos(angle), std::sin(angle));
            if (o.variant == KnobVariant::Dot)
            {
                const float dot = std::max(1.5f, std::round(faceRadius * kKnobDotShare));
                const float at = faceRadius - dot * 2.0f;
                drawList->AddCircleFilled(ImVec2(center.x + dir.x * at, center.y + dir.y * at), dot, ToU32(fill));
            }
            else
            {
                const float inner = innerText ? faceRadius * kKnobWiperStartShare : 0.0f;
                drawList->AddLine(ImVec2(center.x + dir.x * inner, center.y + dir.y * inner),
                                  ImVec2(center.x + dir.x * (faceRadius - 1.0f), center.y + dir.y * (faceRadius - 1.0f)),
                                  ToU32(fill), thickness * 0.5f + 1.0f);
            }
        }
        internal::DrawFocusRing(drawList, ImRect(center.x - radius, center.y - radius, center.x + radius, center.y + radius),
                                radius, item.focusAmount, 0, internal::RingShape::Round);
        internal::ControlTextScope footnote(TextStyle::Footnote);
        if (!mixed)
        {
            const internal::FittedText shown = internal::FitText(formatted, nullptr, innerText ? faceRadius * kKnobTextShare : diameter);
            const float y = innerText ? std::round(center.y - ImGui::GetFontSize() * 0.5f) : dial.Max.y + PxSnapped(t.spacing.xs);
            drawList->AddText(ImVec2(std::round(center.x - shown.width * 0.5f), y), ToU32(internal::ControlLabelColor(disabled)),
                              shown.text);
        }
    }
    bool labelCut = false;
    if (hasLabel)
    {
        internal::ControlTextScope footnote(TextStyle::Footnote);
        const internal::FittedText shown = internal::FitText(label, labelEnd, diameter);
        labelCut = shown.truncated;
        const float y = widget.Max.y - caption;
        drawList->AddText(ImVec2(std::round(center.x - shown.width * 0.5f), y), ToU32(internal::ControlSecondaryColor(disabled)),
                          shown.text);
    }
    internal::CutTextTooltip(id, widget, item.hovered, item.focusVisible, label, labelEnd, labelCut);
    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
        internal::DrawControlMessage(message, ImVec2(widget.Min.x, widget.Max.y), std::max(widget.GetWidth(), std::min(std::max(available, 1.0f), Px(t.layout.minRowControl))),
                                     disabled, id, widget);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace

bool Knob(const char* label, float* value, float min, float max, const KnobOptions& options)
{
    return KnobImpl(label, value, internal::SliderFloatToDouble(min), internal::SliderFloatToDouble(max), false, options);
}

bool Knob(const char* label, int* value, int min, int max, const KnobOptions& options)
{
    return KnobImpl(label, value, static_cast<double>(min), static_cast<double>(max), true, options);
}

} // namespace facet
