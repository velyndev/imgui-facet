// RangeSliderHistogram: the two-handle track of RangeSlider (range_core.h) under a bar chart of the
// caller's bins. The bins divide [min, max] evenly and their bars line up with the values on the
// track; the parts of bars outside [*low, *high] are dimmed, and hovering a bar shows its interval
// and weight in a tooltip.

#include "facet/widgets/sliders.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "widgets/sliders/range_core.h"
#include "widgets/sliders/slider_core.h"

namespace facet
{
namespace
{

template <class Value>
bool HistogramImpl(const char* label, Value* low, Value* high, double min, double max, bool integer,
                   std::span<const float> bins, const RangeSliderHistogramOptions& o)
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
    const Theme& t = T();
    const ImGuiID id = window->GetID(label);
    const bool missing = low == nullptr || high == nullptr;
    const double lowValue = low != nullptr ? internal::SliderValueIn(*low) : min;
    const double highValue = high != nullptr ? internal::SliderValueIn(*high) : max;

    const float one = internal::SliderValueWidth(id, min, max, lowValue, o.format, o.formatter, o.formatterUser, integer);
    const float valueWidth =
        std::max(one, internal::SliderValueWidth(id, min, max, highValue, o.format, o.formatter, o.formatterUser, integer));
    const float width = internal::ResolveItemWidth(o.width, next, kFill);
    const float barsHeight = std::round(Px(o.histogramHeight > 0.0f ? o.histogramHeight : t.metrics.histogramHeight));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const internal::SliderFrame frame =
        internal::LayoutSliderFrame(ImVec2(origin.x, origin.y + barsHeight), label, width,
                                    internal::RangeValueColumnWidth(valueWidth), m, Orientation::Horizontal, 0.0f,
                                    internal::kSliderColumnRange);
    const ImRect widget(origin, frame.widget.Max);
    // The bars stand on the track's row, over the knob travel.
    const ImRect bars(frame.track.Min.x, origin.y, frame.track.Max.x, origin.y + barsHeight);

    internal::RangeTrackParams params;
    params.format = o.format;
    params.formatter = o.formatter;
    params.formatterUser = o.formatterUser;
    params.scale = o.scale;
    params.step = o.step;
    params.minGap = o.minGap;
    params.collision = o.collision;
    params.dragSpan = o.dragSpan;
    params.allowOutOfRange = o.allowOutOfRange;
    params.doubleClickReset = o.doubleClickReset;
    params.size = o.size;
    params.integer = integer;
    params.readOnly = o.readOnly || missing;
    params.mixed = o.mixed;
    params.error = o.error != nullptr;

    ImGui::BeginGroup();
    ImGui::ItemSize(widget.GetSize());
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

    // ---- Bars ----
    ImDrawList* drawList = window->DrawList;
    const Colors& c = t.colors;
    const int count = static_cast<int>(bins.size());
    float largest = 0.0f;
    for (const float bin : bins)
    {
        if (std::isfinite(bin))
            largest = std::max(largest, bin);
    }
    const auto BinEdge = [&](int k) {
        const double value = r.mapping.min + (r.mapping.max - r.mapping.min) * k / std::max(1, count);
        return internal::SliderTrackPos(frame.track, Orientation::Horizontal, internal::ValueToFraction(r.mapping, value));
    };
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool barsHovered = ImGui::IsWindowHovered() && bars.Contains(mouse) && !r.typing;
    int hoveredBin = -1;
    if (count > 0 && largest > 0.0f && ImGui::IsRectVisible(bars.Min, bars.Max) && !r.typing)
    {
        // The selected stretch of the bars, between the two handles on the track.
        const float selMin = internal::SliderTrackPos(frame.track, Orientation::Horizontal, internal::ValueToFraction(r.mapping, r.values[0]));
        const float selMax = internal::SliderTrackPos(frame.track, Orientation::Horizontal, internal::ValueToFraction(r.mapping, r.values[1]));
        const bool off = r.disabled;
        const ImU32 dim = ToU32(off ? c.fill[3] : c.fill[0]);
        const ImU32 lit = ToU32(off ? c.labelTertiary : (r.readOnly ? c.labelSecondary : c.accent));
        const float gap = std::max(1.0f, HairlineThickness());
        const float radius = Px(t.radii.element) * 0.5f;
        for (int k = 0; k < count; ++k)
        {
            const float weight = std::isfinite(bins[static_cast<size_t>(k)]) ? std::max(0.0f, bins[static_cast<size_t>(k)]) : 0.0f;
            // Bin k covers an equal share of the range, placed where those values lie on the track
            // (narrower toward max on a logarithmic one).
            const float x0 = BinEdge(k);
            const float x1 = BinEdge(k + 1);
            if (barsHovered && mouse.x >= x0 && mouse.x < x1)
                hoveredBin = k;
            const float h = std::round(barsHeight * weight / largest);
            if (h <= 0.0f || x1 - x0 <= gap)
                continue;
            const ImVec2 a(x0, bars.Max.y - h);
            const ImVec2 b(x1 - gap, bars.Max.y);
            FillRect(drawList, a, b, r.mixedShown ? lit : dim, radius, ImDrawFlags_RoundTop);
            if (!r.mixedShown && selMax > selMin)
            {
                drawList->PushClipRect(ImVec2(selMin, bars.Min.y), ImVec2(selMax, bars.Max.y), true);
                FillRect(drawList, a, b, lit, radius, ImDrawFlags_RoundTop);
                drawList->PopClipRect();
            }
        }
    }
    // The interval and weight of the bar under the pointer, in interval notation.
    if (hoveredBin >= 0)
    {
        const ImGuiID barId = ImHashData(&hoveredBin, sizeof(hoveredBin), id);
        const float x0 = BinEdge(hoveredBin);
        const float x1 = BinEdge(hoveredBin + 1);
        if (internal::BeginPopupTooltip(barId, ImRect(x0, bars.Min.y, x1, bars.Max.y), true, false))
        {
            char a[64];
            char b[64];
            const double span = (r.mapping.max - r.mapping.min) / count;
            internal::FormatNumber(a, sizeof(a), r.mapping.min + span * hoveredBin, o.format, o.formatter, o.formatterUser, false, false);
            internal::FormatNumber(b, sizeof(b), r.mapping.min + span * (hoveredBin + 1), o.format, o.formatter, o.formatterUser, false, false);
            facet::Text(TextStyle::Footnote, "[%s, %s%c", a, b, hoveredBin + 1 == count ? ']' : ')');
            facet::Text(TextStyle::Footnote, "%g", static_cast<double>(bins[static_cast<size_t>(hoveredBin)]));
            internal::EndPopupTooltip();
        }
    }

    if (!r.typing)
    {
        char lowText[64];
        char highText[64];
        internal::FormatNumber(lowText, sizeof(lowText), r.values[0], o.format, o.formatter, o.formatterUser, integer, false);
        internal::FormatNumber(highText, sizeof(highText), r.values[1], o.format, o.formatter, o.formatterUser, integer, false);
        if (r.mixedShown)
            lowText[0] = highText[0] = '\0';
        internal::DrawRangeValueText(drawList, frame.value, valueWidth, lowText, highText, r.disabled);
    }
    if (frame.label.text != nullptr)
        internal::DrawFrontLabel(drawList, frame.label, frame.labelPos, m.control.size, r.disabled);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    internal::CutTextTooltip(id, frame.widget, r.multi.track.hovered, r.handles[0].focusVisible || r.handles[1].focusVisible,
                             label, labelEnd, frame.label.shown.truncated);
    const internal::ControlMessage message = internal::PickControlMessage(o.error, o.helpText);
    if (message.IsSet())
        internal::DrawControlMessage(message, ImVec2(frame.control.Min.x, widget.Max.y), widget.Max.x - frame.control.Min.x,
                                     r.disabled, id, widget);
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    return r.changed && !missing;
}

} // namespace

bool RangeSliderHistogram(const char* label, float* low, float* high, float min, float max, std::span<const float> bins,
                          const RangeSliderHistogramOptions& options)
{
    return HistogramImpl(label, low, high, internal::SliderFloatToDouble(min), internal::SliderFloatToDouble(max), false,
                         bins, options);
}

bool RangeSliderHistogram(const char* label, int* low, int* high, int min, int max, std::span<const float> bins,
                          const RangeSliderHistogramOptions& options)
{
    return HistogramImpl(label, low, high, static_cast<double>(min), static_cast<double>(max), true, bins, options);
}

} // namespace facet
