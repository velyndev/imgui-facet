// RangeSlider: two handles selecting [*low, *high] inside [min, max], each its own item, Tab stop
// and focus ring, with the label in front and both values at the end. The handles, the span, the
// typed values and the drawing are the family's two-handle track (range_core.h) on the
// multi-handle block (src/internal/multi_handle.h): handles never cross, keep the minimum gap,
// clamp or push, the span drags both, a track click moves the nearer handle, and mirror and fixed
// span keep the pair symmetric or of one width.

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
#include "internal/item.h"
#include "widgets/sliders/range_core.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

template <class Value>
bool RangeSliderImpl(const char* label, Value* low, Value* high, double min, double max, bool integer,
                     const RangeSliderOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::SliderMetrics m = internal::GetSliderMetrics(o.size);
    internal::ControlTextScope text(m.control);
    if (min > max)
        std::swap(min, max);
    const ImGuiID id = window->GetID(label);
    const bool vertical = o.orientation == Orientation::Vertical;
    const bool missing = low == nullptr || high == nullptr;
    const double lowValue = low != nullptr ? internal::SliderValueIn(*low) : min;
    const double highValue = high != nullptr ? internal::SliderValueIn(*high) : max;

    float valueWidth = 0.0f;
    if (!vertical)
    {
        const float one = internal::SliderValueWidth(id, min, max, lowValue, o.format, o.formatter, o.formatterUser, integer);
        valueWidth = std::max(one, internal::SliderValueWidth(id, min, max, highValue, o.format, o.formatter,
                                                              o.formatterUser, integer));
    }
    const float width = vertical ? m.control.height : internal::ResolveItemWidth(o.width, next, kFill);
    const float length = vertical ? Px(o.height > 0.0f ? o.height : T().metrics.faderLength) : 0.0f;
    const internal::SliderFrame frame = internal::LayoutSliderFrame(
        window->DC.CursorPos, label, width, vertical ? 0.0f : internal::RangeValueColumnWidth(valueWidth), m,
        o.orientation, length);

    internal::RangeTrackParams params;
    params.format = o.format;
    params.formatter = o.formatter;
    params.formatterUser = o.formatterUser;
    params.scale = o.scale;
    params.step = o.step;
    params.minGap = o.minGap;
    params.collision = o.collision;
    params.dragSpan = o.dragSpan;
    params.mirror = o.mirror;
    params.fixedSpan = o.fixedSpan;
    params.valueBubble = o.valueBubble;
    params.allowOutOfRange = o.allowOutOfRange;
    params.doubleClickReset = o.doubleClickReset;
    params.orientation = o.orientation;
    params.size = o.size;
    params.integer = integer;
    params.readOnly = o.readOnly || missing;
    params.mixed = o.mixed;
    params.error = o.error != nullptr;

    ImGui::BeginGroup();
    ImGui::ItemSize(frame.widget.GetSize());
    const internal::RangeTrackResult r = internal::RangeTrackBehavior(id, frame, m, lowValue, highValue, min, max, params);
    Value* targets[2] = {low, high};
    for (int i = 0; i < 2; ++i)
    {
        if (!r.written[i] || targets[i] == nullptr)
            continue;
        if constexpr (std::is_integral_v<Value>)
            *targets[i] = static_cast<Value>(std::llround(r.values[i]));
        else
            *targets[i] = static_cast<Value>(r.values[i]);
    }

    ImDrawList* drawList = window->DrawList;
    if (!vertical && !r.typing)
    {
        char lowText[64];
        char highText[64];
        internal::FormatNumber(lowText, sizeof(lowText), r.values[0], o.format, o.formatter, o.formatterUser, integer, false);
        internal::FormatNumber(highText, sizeof(highText), r.values[1], o.format, o.formatter, o.formatterUser, integer, false);
        if (r.mixedShown)
            lowText[0] = highText[0] = '\0';
        internal::DrawRangeValueText(drawList, frame.value, valueWidth, lowText, highText, r.disabled);
    }
    if (!vertical && frame.label.text != nullptr)
        internal::DrawFrontLabel(drawList, frame.label, frame.labelPos, m.control.size, r.disabled);

    // The whole label when it is cut, and for a vertical slider (which draws none), on hover and
    // on keyboard focus of either handle.
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hovered = r.multi.track.hovered;
    const bool focusVisible = r.handles[0].focusVisible || r.handles[1].focusVisible;
    internal::CutTextTooltip(id, frame.widget, hovered, focusVisible, label, labelEnd,
                             vertical ? labelEnd != label : frame.label.shown.truncated);

    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
    {
        const float x = vertical ? frame.widget.Min.x : frame.control.Min.x;
        internal::DrawControlMessage(message, ImVec2(x, frame.widget.Max.y), frame.widget.Max.x - x, r.disabled, id,
                                     frame.widget);
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return r.changed && !missing;
}

} // namespace

bool RangeSlider(const char* label, float* low, float* high, float min, float max, const RangeSliderOptions& options)
{
    return RangeSliderImpl(label, low, high, internal::SliderFloatToDouble(min), internal::SliderFloatToDouble(max),
                           false, options);
}

bool RangeSlider(const char* label, int* low, int* high, int min, int max, const RangeSliderOptions& options)
{
    return RangeSliderImpl(label, low, high, static_cast<double>(min), static_cast<double>(max), true, options);
}

} // namespace facet
