// SliderStepped: a slider over a list of ascending stop values, with the stops evenly spaced on the
// track, a tick per stop and optional stop labels under the ticks. By default it always rests on a
// stop; with `magnetic` it moves freely and snaps only near a stop.
//
// The value drag core runs in stop index space (0 to stops - 1): whole indices without magnetic,
// so every drag, key and typed value lands on a stop, and continuous indices with snap stops at
// the whole ones with magnetic. The written value is the stop, or the value between two stops at
// the same share of the way, so the track reads like the list of choices it is.

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
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

// The most stops a magnetic slider snaps to and that get a tick; longer lists still work, without
// snapping and ticks.
constexpr int kSteppedMaxStops = 64;
// A value within this share of the way between two stops counts as on the stop.
constexpr double kSteppedOnStop = 1e-6;

// The stop index of `value` on the evenly spaced track: whole at a stop, between two stops at the
// share of the way between their values. Outside the stops it is clamped; NaN is 0.
double SteppedIndexOf(std::span<const double> stops, double value)
{
    const int n = static_cast<int>(stops.size());
    if (n < 2 || std::isnan(value) || value <= stops[0])
        return 0.0;
    if (value >= stops[static_cast<size_t>(n - 1)])
        return static_cast<double>(n - 1);
    for (int i = 0; i + 1 < n; ++i)
    {
        const double a = stops[static_cast<size_t>(i)];
        const double b = stops[static_cast<size_t>(i + 1)];
        if (value >= a && value <= b)
            return b > a ? i + (value - a) / (b - a) : static_cast<double>(i);
    }
    return 0.0;
}

// The value at a stop index: the stop itself at a whole index (exactly), else between two stops.
double SteppedValueAt(std::span<const double> stops, double index)
{
    const int n = static_cast<int>(stops.size());
    if (n == 0)
        return 0.0;
    index = std::clamp(index, 0.0, static_cast<double>(n - 1));
    const double whole = std::round(index);
    if (std::fabs(index - whole) <= kSteppedOnStop)
        return stops[static_cast<size_t>(whole)];
    const int i = static_cast<int>(std::floor(index));
    const double t = index - i;
    return stops[static_cast<size_t>(i)] + (stops[static_cast<size_t>(i + 1)] - stops[static_cast<size_t>(i)]) * t;
}

// Draws the stop labels under the ticks of a horizontal track in Caption, centered on their tick
// and kept inside the control; a label that would touch the one drawn before it is left out, and
// the last label wins over the one before it.
void DrawStopLabels(ImDrawList* drawList, const internal::SliderFrame& frame, float top,
                    std::span<const char* const> labels, int count, bool disabled)
{
    internal::ControlTextScope text(TextStyle::Caption);
    const float gap = PxSnapped(T().spacing.xs);
    const ImU32 color = ToU32(internal::ControlSecondaryColor(disabled));
    float lastRight = -1e30f;
    const auto place = [&](int i) {
        const char* label = labels[static_cast<size_t>(i)];
        const float width = label != nullptr ? ImGui::CalcTextSize(label).x : 0.0f;
        const double fraction = count > 1 ? static_cast<double>(i) / (count - 1) : 0.0;
        const float at = internal::SliderTrackPos(frame.track, Orientation::Horizontal, fraction);
        float x = std::round(at - width * 0.5f);
        x = std::clamp(x, frame.control.Min.x, std::max(frame.control.Min.x, frame.control.Max.x - width));
        return ImVec2(x, x + width);
    };
    // The last label's place, so a crowded label before it gives way.
    const ImVec2 lastSpan = place(count - 1);
    for (int i = 0; i < count; ++i)
    {
        const char* label = labels[static_cast<size_t>(i)];
        if (label == nullptr || label[0] == '\0')
            continue;
        const ImVec2 span = place(i);
        const bool last = i == count - 1;
        if (span.x < lastRight + gap || (!last && span.y + gap > lastSpan.x && count > 1))
            continue;
        drawList->AddText(ImVec2(span.x, top), color, label);
        lastRight = span.y;
    }
}

} // namespace

bool SliderStepped(const char* label, float* value, std::span<const float> stops, const SliderSteppedOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(o.size);
    internal::ControlTextScope text(m.control);
    const ImGuiID id = window->GetID(label);
    const bool vertical = o.orientation == Orientation::Vertical;

    // The stops as doubles, in the order given; at most kSteppedMaxStops take part.
    double stopValues[kSteppedMaxStops];
    const int count = std::min(static_cast<int>(stops.size()), kSteppedMaxStops);
    for (int i = 0; i < count; ++i)
        stopValues[i] = internal::SliderFloatToDouble(stops[static_cast<size_t>(i)]);
    const std::span<const double> stopSpan(stopValues, static_cast<size_t>(count));
    const bool fixed = value == nullptr || count < 2;
    const bool readOnly = o.readOnly || fixed || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const double current = value != nullptr ? internal::SliderValueIn(*value) : (count > 0 ? stopValues[0] : 0.0);
    const double first = count > 0 ? stopValues[0] : 0.0;
    const double last = count > 0 ? stopValues[count - 1] : 0.0;

    // Index space: whole indices, or continuous with snap stops at the whole ones.
    double indexStops[kSteppedMaxStops];
    for (int i = 0; i < count; ++i)
        indexStops[i] = static_cast<double>(i);
    internal::ValueMapping mapping;
    mapping.min = 0.0;
    mapping.max = std::max(1.0, static_cast<double>(count - 1));
    mapping.integer = !o.magnetic;
    if (o.magnetic)
        mapping.stops = std::span<const double>(indexStops, static_cast<size_t>(count));

    const float valueWidth = vertical ? 0.0f
                                      : internal::SliderValueWidth(id, first, last, current, o.format, o.formatter,
                                                                   o.formatterUser, false);
    const float width = vertical ? m.control.height : internal::ResolveItemWidth(o.width, next, kFill);
    const float length = vertical ? Px(o.height > 0.0f ? o.height : T().metrics.faderLength) : 0.0f;
    const internal::SliderFrame frame = internal::LayoutSliderFrame(window->DC.CursorPos, label, width, valueWidth, m,
                                                                    o.orientation, length);
    // Stop labels take one Caption line under the ticks.
    const bool labels = !vertical && !o.stopLabels.empty();
    float labelsHeight = 0.0f;
    if (labels)
    {
        internal::ControlTextScope caption(TextStyle::Caption);
        labelsHeight = std::ceil(ImGui::GetFontSize());
    }
    ImRect widget = frame.widget;
    widget.Max.y += labelsHeight;

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
        params.min = std::min(first, last);
        params.max = std::max(first, last);
        params.step = 0.0;
        params.size = o.size;
        const float half = std::round(Px(T().metrics.numberFieldWidth) * 0.5f);
        const ImVec2 c(std::round(frame.widget.GetCenter().x), std::round(frame.widget.GetCenter().y));
        const ImRect rect = vertical ? ImRect(c.x - half, c.y - std::round(m.control.height * 0.5f), c.x + half,
                                              c.y - std::round(m.control.height * 0.5f) + m.control.height)
                                     : frame.control;
        const internal::SliderTypeResult typed = internal::SliderTypeField(id, 0, rect, current, params, id);
        if (typed.changed && !readOnly)
        {
            // Without magnetic a typed value goes to the nearest stop.
            double index = SteppedIndexOf(stopSpan, typed.value);
            if (!o.magnetic)
                index = std::round(index);
            const double next_ = o.magnetic ? std::clamp(typed.value, std::min(first, last), std::max(first, last))
                                            : SteppedValueAt(stopSpan, index);
            if (next_ != current)
            {
                edited = next_;
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
            const double before = SteppedIndexOf(stopSpan, current);
            double index = before;
            const internal::ValueDragResult result = internal::ValueDragBehavior(item, &index, frame.track, drag);
            if (result.changed && !readOnly)
            {
                // Magnetic keys still go from stop to stop: on to the next one in the direction of travel.
                if (o.magnetic && result.keyboard && !result.canceled)
                {
                    const double whole = index > before ? std::floor(before + kSteppedOnStop) + 1.0
                                                        : std::ceil(before - kSteppedOnStop) - 1.0;
                    if (index != mapping.min && index != mapping.max)
                        index = std::clamp(whole, mapping.min, mapping.max);
                }
                const double next_ = SteppedValueAt(stopSpan, index);
                if (next_ != current)
                {
                    edited = next_;
                    written = true;
                }
            }
        }
    }

    if (written)
    {
        *value = static_cast<float>(edited);
        edited = internal::SliderValueIn(*value);
    }

    const bool disabled = item.disabled || (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::SliderLook look = internal::PickSliderLook(disabled, readOnly);
    const bool mixed = (o.mixed || item.mixed) && !written;
    ImDrawList* drawList = window->DrawList;
    char formatted[64];
    internal::FormatNumber(formatted, sizeof(formatted), edited, o.format, o.formatter, o.formatterUser, false, false);
    if (!typing && ImGui::IsRectVisible(widget.Min, widget.Max))
    {
        const double fraction = count >= 2 ? SteppedIndexOf(stopSpan, edited) / (count - 1) : 0.0;
        internal::DrawSliderTrack(drawList, frame.track, o.orientation, m, 0.0, mixed ? 0.0 : fraction, look,
                                  o.error != nullptr);
        double ticks[kSteppedMaxStops];
        for (int i = 0; i < count; ++i)
            ticks[i] = count > 1 ? static_cast<double>(i) / (count - 1) : 0.0;
        internal::DrawSliderTicks(drawList, frame.track, o.orientation, m,
                                  std::span<const double>(ticks, static_cast<size_t>(count)), look);
        if (labels && count >= 2)
        {
            const int shown = std::min(count, static_cast<int>(o.stopLabels.size()));
            DrawStopLabels(drawList, frame, frame.widget.Max.y, o.stopLabels.first(static_cast<size_t>(shown)),
                           count, disabled);
        }
        const ImRect knob = internal::SliderKnobRect(frame.track, o.orientation, frame.knob, fraction);
        if (!mixed)
            internal::DrawSliderKnob(drawList, knob, item, look);
        else
            internal::DrawFocusRing(drawList, knob, knob.GetWidth() * 0.5f, item.focusAmount, 0, internal::RingShape::Round);
        if (!vertical)
            internal::DrawSliderValueText(drawList, frame.value, mixed ? "" : formatted, disabled);
    }
    if (!vertical && frame.label.text != nullptr)
        internal::DrawFrontLabel(drawList, frame.label, frame.labelPos, m.control.size, disabled);

    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    if (vertical)
        internal::CutTextTooltip(item, label, labelEnd, labelEnd != label);
    else
        internal::CutTextTooltip(id, frame.widget, item.hovered, item.focusVisible, label, labelEnd,
                                 frame.label.shown.truncated);

    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
    {
        const float x = vertical ? widget.Min.x : frame.control.Min.x;
        internal::DrawControlMessage(message, ImVec2(x, widget.Max.y), widget.Max.x - x, disabled, id, widget);
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return written;
}

} // namespace facet
