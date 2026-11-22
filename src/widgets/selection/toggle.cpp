// Toggle: an on/off switch with its label in front, a knob that springs between the ends and can
// be dragged, and the read-only, mixed, loading, disabled, error and help states.

#include "facet/widgets/selection.h"

#include <algorithm>
#include <cmath>

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
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

// Animation sub keys under the toggle id (below kItemAnimSubBase).
constexpr int kKnobSub = 1;
constexpr int kOnSub = 2;
// The knob of a loading toggle carries a spinner of this share of the knob's diameter.
constexpr float kSpinnerShare = 0.7f;

// A press that may become a drag of the knob.
struct ToggleDragState
{
    float pressX = 0.0f;   // pixels, where the mouse went down
    bool dragging = false; // the pointer moved further than the drag threshold
    float fraction = 0.0f; // 0 off to 1 on: where the dragged knob is
};

} // namespace

bool Toggle(const char* label, bool* value, const ToggleOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    internal::ControlTextScope text(m);
    const Theme& t = T();
    const Colors& c = t.colors;

    // Measure: the label in front (control.h), then the track. With a set width the label takes the
    // label column, so the switch starts where the controls of a settings column start.
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasLabel = labelEnd != label;
    const ImVec2 track(std::max(1.0f, PxSnapped(t.metrics.toggleWidth)), std::max(1.0f, PxSnapped(t.metrics.toggleHeight)));
    const internal::FrontLabelFit fit = internal::CompactFrontLabelFit(options.width, next);
    const internal::FrontLabel natural = internal::MeasureFrontLabel(label, m.size, FLT_MAX, fit);
    const float width = internal::ResolveItemWidth(options.width, next, std::ceil(natural.width + track.x));
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.size, width, fit);
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    const bool grouped = message.IsSet();

    const ImGuiID id = window->GetID(label);
    const float top = window->DC.CursorPos.y;
    const bool readOnly = options.readOnly || value == nullptr;
    internal::ItemOptions itemOptions;
    itemOptions.baseline = CenteredTextY(top, m.height) - top;
    itemOptions.readOnly = readOnly;
    itemOptions.mixed = options.mixed;
    itemOptions.handCursor = !options.loading && !readOnly && (next.itemFlags & ImGuiItemFlags_ReadOnly) == 0;
    if (grouped)
        ImGui::BeginGroup();
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::ItemBehavior(id, ImVec2(width, m.height), itemOptions);

    // The track follows the label (or starts the item without one), inside the item.
    const float trackX = std::round(std::min(item.rect.Min.x + front.width, item.rect.Max.x - track.x));
    const ImRect trackRect(trackX, std::round(item.rect.GetCenter().y - track.y * 0.5f), trackX + track.x,
                           std::round(item.rect.GetCenter().y - track.y * 0.5f) + track.y);
    const float inset = std::max(1.0f, std::round(Px(t.metrics.toggleKnobInset)));
    const float knob = std::max(1.0f, track.y - inset * 2.0f);
    const float travel = std::max(0.0f, track.x - inset * 2.0f - knob);

    // Input: a click, Space or Enter flips; a drag of the knob sets the side it is let go on.
    const bool interactive = !item.readOnly && !options.loading && !item.disabled && item.forced == internal::ForcedItemState::None;
    const bool current = value != nullptr && *value;
    bool result = current;
    bool edited = false;
    bool dragShown = false;
    float dragFraction = 0.0f;
    if (interactive && (item.active || item.deactivated))
    {
        ImGuiContext& g = *GImGui;
        ToggleDragState& drag = internal::GetState<ToggleDragState>(id);
        const float mouseX = g.IO.MousePos.x;
        if (item.activated)
            drag = {mouseX, false, current ? 1.0f : 0.0f};
        if (item.dragging && std::fabs(mouseX - drag.pressX) > Px(t.drag.startThreshold))
            drag.dragging = true;
        if (drag.dragging)
        {
            const float start = trackRect.Min.x + inset + knob * 0.5f;
            drag.fraction = travel > 0.0f ? std::clamp((mouseX - start) / travel, 0.0f, 1.0f) : (current ? 1.0f : 0.0f);
            dragShown = item.active;
            dragFraction = drag.fraction;
        }
        if (item.deactivated && drag.dragging)
        {
            result = drag.fraction >= 0.5f;
            edited = true;  // a drag is an edit even when it ends on the same side
            drag.dragging = false;
        }
        else if (item.pressed)
        {
            result = !current;
            edited = true;
        }
    }
    else if (interactive && item.pressed)
    {
        result = !current;
        edited = true;
    }
    const bool mixedShown = internal::SelectionMixedShown(id, item.mixed, edited);
    bool written = false;
    if (edited && value != nullptr)
    {
        written = *value != result || item.mixed;
        *value = result;
        if (written)
            internal::MarkWidgetEdited(id);
    }

    // Motion: the knob springs (quick spring), the colors follow over durationFast.
    const bool on = value != nullptr && *value;
    const float knobTarget = mixedShown ? 0.5f : (on ? 1.0f : 0.0f);
    const float sprung = Spring(id, kKnobSub, dragShown ? dragFraction : knobTarget, t.motion.quick);
    const float knobT = dragShown ? dragFraction : sprung;
    const float onT = Tween(id, kOnSub, on && !mixedShown ? 1.0f : 0.0f, t.motion.durationFast);

    bool truncated = false;
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        // Label: everything in front of the track, cut with an ellipsis when it does not fit.
        if (hasLabel)
        {
            const float gap = std::round(Px(t.spacing.sm));
            const internal::FittedText shown =
                internal::FitText(label, labelEnd, std::max(0.0f, trackRect.Min.x - item.rect.Min.x - gap));
            truncated = shown.truncated;
            drawList->AddText(ImVec2(item.rect.Min.x, CenteredTextY(item.rect.Min.y, m.height)),
                              ToU32(internal::ControlLabelColor(item.disabled)), shown.text);
        }

        // Track: the neutral fill with the 3:1 boundary while off, the accent while on.
        internal::ItemState look = item;
        if (options.loading)
        {
            look.hoverAmount = 0.0f;
            look.pressAmount = 0.0f;
        }
        const float radius = track.y * 0.5f;
        ImVec4 trackColor;
        ImVec4 knobColor;
        ImVec4 outline;
        if (item.disabled)
        {
            trackColor = c.fill[3];
            knobColor = c.labelTertiary;
            outline = c.separator;
        }
        else if (item.readOnly)
        {
            // Read-only: labelSecondary instead of the accent, so it does not look like a choice.
            const ImVec4 clear = WithAlpha(c.fill[2], 0.0f);
            trackColor = Mix(clear, c.labelSecondary, onT);
            knobColor = Mix(c.labelSecondary, c.bg, onT);
            outline = Mix(ControlBoundaryColor(c), c.labelSecondary, onT);
        }
        else
        {
            const ImVec4 off = internal::NeutralFill(look);
            const ImVec4 onFill = internal::StateFill(c.accent, c.accentHover, c.accentPressed, look);
            trackColor = onT <= 0.0f ? off : (onT >= 1.0f ? onFill : Mix(off, onFill, onT));
            knobColor = Mix(c.labelSecondary, c.onAccent, onT);
            outline = Mix(ControlBoundaryColor(c), c.focusRing, onT);
        }
        FillRect(drawList, trackRect.Min, trackRect.Max, ToU32(trackColor), radius);
        StrokeRect(drawList, trackRect.Min, trackRect.Max, ToU32(outline), radius, HairlineThickness());

        // Knob: a disc in labelSecondary while off and onAccent while on.
        const ImVec2 center(trackRect.Min.x + inset + knob * 0.5f + knobT * travel, trackRect.GetCenter().y);
        drawList->AddCircleFilled(center, knob * 0.5f, ToU32(knobColor));
        if (options.loading)
            internal::DrawControlSpinner(drawList, center, knob * kSpinnerShare,
                                         ToU32(on && !mixedShown ? c.accent : c.bg));
        internal::DrawFocusRing(drawList, trackRect, radius, item.focusAmount, 0, internal::RingShape::Round);
    }

    // A cut label shows in full on hover and keyboard focus (rule 11).
    internal::CutTextTooltip(item, label, labelEnd, truncated);
    if (grouped)
    {
        const float bottom = internal::DrawControlMessage(message, ImVec2(item.rect.Min.x, item.rect.Max.y),
                                                          item.rect.GetWidth(), item.disabled, id, item.rect);
        window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(item.rect.Max.x, bottom));
        ImGui::EndGroup();
        internal::FinishWidgetGroup(id);
        if (written)
            GImGui->LastItemData.StatusFlags |= ImGuiItemStatusFlags_Edited;
    }
    return written;
}

} // namespace facet
