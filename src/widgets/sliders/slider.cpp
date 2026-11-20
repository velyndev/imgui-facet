// Slider: one value on a track with a fill and a knob, horizontal with the label in front and the
// value at the end, or vertical as a fader; linear or logarithmic, continuous, stepped or whole
// numbers, filled from the start or from the middle, with an optional value bubble and typed
// values (Ctrl+click or Enter).
//
// The input is the value drag core (src/internal/value_drag.h): relative knob grab, a press on
// the track that moves the value under the pointer, Shift fine and Ctrl coarse, the keys, the
// wheel while focused, Escape restoring the value from activation. The look is the family's
// shared track and knob (slider_core.h).

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/value_drag.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

// The fill of a bipolar slider starts at the middle of the track.
constexpr double kSliderCenterFraction = 0.5;
// Stretches of more ticks than this are left unmarked: they would merge into a line.
constexpr int kSliderMaxTicks = 21;

template <class Value>
bool SliderImpl(const char* label, Value* value, double min, double max, bool integer, const SliderOptions& o)
{
    double edited = value != nullptr ? internal::SliderValueIn(*value) : 0.0;
    if (!internal::SliderBody(label, value != nullptr ? &edited : nullptr, min, max, integer, o, {}))
        return false;
    if constexpr (std::is_integral_v<Value>)
        *value = static_cast<Value>(std::llround(edited));
    else
        *value = static_cast<Value>(edited);
    return true;
}

} // namespace

namespace internal
{

bool SliderBody(const char* label, double* value, double min, double max, bool integer, const SliderOptions& o,
                const SliderBodyParams& p)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = p.next != nullptr ? *p.next : internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(o.size);
    internal::ControlTextScope text(m.control);
    if (min > max)
        std::swap(min, max);
    const ImGuiID id = window->GetID(label);
    const bool vertical = o.orientation == Orientation::Vertical;

    internal::ValueMapping mapping;
    mapping.min = min;
    mapping.max = max;
    mapping.scale = o.scale;
    mapping.step = o.step;
    mapping.integer = integer;
    mapping = internal::NormalizeMapping(mapping);
    // An empty range and a missing value are shown, never written.
    const bool fixed = value == nullptr || !(mapping.max > mapping.min);
    const bool readOnly = o.readOnly || fixed || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const double current = value != nullptr ? *value : mapping.min;

    const float valueWidth = vertical || !p.valueColumn
                                 ? 0.0f
                                 : internal::SliderValueWidth(id, mapping.min, mapping.max, current, o.format,
                                                              o.formatter, o.formatterUser, integer);
    const float width = vertical ? m.control.height
                                 : (p.width > 0.0f ? p.width : internal::ResolveItemWidth(o.width, next, kFill));
    const float length = vertical ? Px(o.height > 0.0f ? o.height : T().metrics.faderLength) : 0.0f;
    const internal::SliderFrame frame = internal::LayoutSliderFrame(window->DC.CursorPos, label, width, valueWidth, m,
                                                                    o.orientation, length);

    ImGui::BeginGroup();
    ImGui::ItemSize(frame.widget.GetSize());
    const bool typing = internal::SliderTypingPart(id) == 0;
    bool written = false;
    double edited = current;
    internal::ItemState item;
    if (typing)
    {
        // The typed value replaces the track until Enter, Tab, Escape or a click elsewhere.
        internal::NumberFieldParams params;
        params.format = o.format;
        params.formatter = o.formatter;
        params.formatterUser = o.formatterUser;
        params.min = mapping.min;
        params.max = mapping.max;
        params.integer = integer;
        params.step = 0.0;
        params.size = o.size;
        const float half = std::round(Px(T().metrics.numberFieldWidth) * 0.5f);
        const ImVec2 center(std::round(frame.widget.GetCenter().x), std::round(frame.widget.GetCenter().y));
        const ImRect rect = vertical ? ImRect(center.x - half, center.y - std::round(m.control.height * 0.5f), center.x + half,
                                              center.y - std::round(m.control.height * 0.5f) + m.control.height)
                                     : frame.control;
        const internal::SliderTypeResult typed = internal::SliderTypeField(id, 0, rect, current, params, id);
        if (typed.changed && !readOnly)
        {
            const double snapped = internal::SnapValue(mapping, typed.value);
            if (snapped != current)
            {
                edited = snapped;
                written = true;
            }
        }
    }
    else
    {
        internal::ItemOptions itemOptions;
        itemOptions.press = internal::PressMode::Click;
        itemOptions.readOnly = readOnly;
        itemOptions.mixed = o.mixed;
        itemOptions.baseline = CenteredTextY(frame.widget.Min.y, m.control.height) - frame.widget.Min.y;
        internal::ApplyNextItemData(next);
        item = internal::PartBehavior(id, frame.control, itemOptions);
        if (internal::SliderWantsTyping(item))
        {
            // Ctrl+click or Enter: type a value instead of dragging.
            if (ImGui::GetActiveID() == id)
                ImGui::ClearActiveID();
            internal::SliderBeginTyping(id, 0);
        }
        else
        {
            internal::ValueDragOptions drag;
            drag.mapping = mapping;
            drag.orientation = o.orientation;
            drag.grabRadius = frame.knob * 0.5f;
            drag.readOnly = readOnly;
            const internal::ValueDragResult result = internal::ValueDragBehavior(item, &edited, frame.track, drag);
            written = result.changed && !readOnly;
        }
    }

    if (written)
    {
        if (integer)
            edited = std::round(edited);
        *value = edited;
    }

    const bool disabled = item.disabled || (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::SliderLook look = internal::PickSliderLook(disabled, readOnly);
    const bool mixed = (o.mixed || item.mixed) && !written;
    ImDrawList* drawList = window->DrawList;
    char formatted[64];
    internal::FormatNumber(formatted, sizeof(formatted), edited, o.format, o.formatter, o.formatterUser, integer, false);
    if (!typing && ImGui::IsRectVisible(frame.widget.Min, frame.widget.Max))
    {
        const double fraction = internal::ValueToFraction(mapping, std::isnan(edited) ? mapping.min : edited);
        const double from = o.fillFromCenter ? kSliderCenterFraction : 0.0;
        internal::DrawSliderTrack(drawList, frame.track, o.orientation, m, from, mixed ? from : fraction, look,
                                  o.error != nullptr || p.errorLook);
        if (o.step > 0.0)
        {
            double ticks[kSliderMaxTicks + 1];
            const int tickCount = internal::SliderStepTicks(mapping, ticks, kSliderMaxTicks);
            internal::DrawSliderTicks(drawList, frame.track, o.orientation, m,
                                      std::span<const double>(ticks, static_cast<size_t>(tickCount)), look);
        }
        const ImRect knob = internal::SliderKnobRect(frame.track, o.orientation, frame.knob, fraction);
        if (!mixed)
            internal::DrawSliderKnob(drawList, knob, item, look);
        else
            internal::DrawFocusRing(drawList, knob, knob.GetWidth() * 0.5f, item.focusAmount, 0, internal::RingShape::Round);
        if (!vertical)
            internal::DrawSliderValueText(drawList, frame.value, mixed ? "" : formatted, disabled);
        if (o.valueBubble && !mixed)
            internal::DrawSliderBubble(drawList, knob, formatted, o.orientation, internal::SliderBubbleAmount(id, item));
    }
    if (!vertical && frame.label.text != nullptr)
        internal::DrawFrontLabel(drawList, frame.label, frame.labelPos, m.control.size, disabled);

    // The whole label of a cut front label, and of a vertical slider (which draws none), on hover
    // and keyboard focus.
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    if (vertical)
        internal::CutTextTooltip(item, label, labelEnd, labelEnd != label);
    else
        internal::CutTextTooltip(id, frame.widget, item.hovered, item.focusVisible, label, labelEnd,
                                 frame.label.shown.truncated);

    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
    {
        const float x = vertical ? frame.widget.Min.x : frame.control.Min.x;
        internal::DrawControlMessage(message, ImVec2(x, frame.widget.Max.y), frame.widget.Max.x - x, disabled, id,
                                     frame.widget);
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    if (p.controlRect != nullptr)
        *p.controlRect = frame.control;
    return written;
}

} // namespace internal

bool Slider(const char* label, float* value, float min, float max, const SliderOptions& options)
{
    return SliderImpl(label, value, internal::SliderFloatToDouble(min), internal::SliderFloatToDouble(max), false,
                      options);
}

bool Slider(const char* label, int* value, int min, int max, const SliderOptions& options)
{
    return SliderImpl(label, value, static_cast<double>(min), static_cast<double>(max), true, options);
}

} // namespace facet
