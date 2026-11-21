// BarChart (facet/widgets/charts.h): grouped or stacked bars per category, vertical (bars grow
// upward from the value axis baseline) or horizontal (bars grow to the right, categories in rows).
// The chart is one item and one Tab stop; the pointer or the arrow keys pick a category, which is
// washed behind its bars and named in the readout with every visible series. Stacked segments are
// set apart by the bar gap, so neighbouring colors never touch.

#include "facet/widgets/charts.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/chart_core.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "widgets/charts/chart_parts.h"

namespace facet
{
namespace
{

using internal::Range;
using internal::SeriesView;

// The category label column of a horizontal chart takes at most this share of the width.
constexpr float kBarLabelShare = 0.4f;
// A category label needs at least this many font sizes of room, else only every n-th one shows.
constexpr float kBarLabelMinEm = 2.0f;

struct BarLegendData
{
    std::span<const ChartSeries> series;
    bool disabled = false;
};

bool BarLegendEntry(const void* data, int index, internal::ChartLabelText* label, internal::ChartLegendLook* look)
{
    const BarLegendData& legend = *static_cast<const BarLegendData*>(data);
    const ChartSeries& series = legend.series[static_cast<std::size_t>(index)];
    *label = internal::ChartVisibleLabel(series.label);
    if (label->Empty())
        return false;
    look->color = internal::ChartDataColor(series.color, index, false);
    look->hidden = series.hidden;
    look->disabled = legend.disabled;
    return true;
}

// Value i of a series, NaN when it has none there (a gap or past its end).
double BarValue(const SeriesView& view, int index)
{
    double x = 0.0, y = 0.0;
    return index < view.count && view.Sample(index, &x, &y) ? y : std::numeric_limits<double>::quiet_NaN();
}

// Draws one bar or segment from `from` to `to` along the value axis, `t0` to `t1` across it.
// `round` rounds the end at `to`.
void DrawBarRect(ImDrawList* drawList, bool vertical, float from, float to, float t0, float t1, ImU32 color, float radius,
                 bool round)
{
    const float lo = std::min(from, to);
    const float hi = std::max(from, to);
    if (hi - lo < 0.5f || t1 - t0 < 0.5f)
        return;
    const float r = round ? std::min({radius, (t1 - t0) * 0.25f, hi - lo}) : 0.0f;
    ImDrawFlags corners = 0;
    if (vertical)
        corners = to < from ? ImDrawFlags_RoundTop : ImDrawFlags_RoundBottom;
    else
        corners = to > from ? ImDrawFlags_RoundRight : ImDrawFlags_RoundLeft;
    if (vertical)
        FillRect(drawList, ImVec2(t0, lo), ImVec2(t1, hi), color, r, corners);
    else
        FillRect(drawList, ImVec2(lo, t0), ImVec2(hi, t1), color, r, corners);
}

} // namespace

void BarChart(const char* id, std::span<const ChartSeries> series, std::span<const char* const> categories,
              const BarChartOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const float width = internal::ResolveItemWidth(o.width, next, kFill);
    const float height = std::max(1.0f, std::round(o.height > 0.0f ? Px(o.height) : Px(t.metrics.chartHeight)));
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + height));
    const ImGuiID chartId = window->GetID(id);
    const int seriesCount = static_cast<int>(std::min<std::size_t>(series.size(), 1u << 20));
    const bool vertical = o.orientation == Orientation::Vertical;

    // Categories: as many as the longest series, at least the labels given.
    int valueCount = 0;
    for (int s = 0; s < seriesCount; ++s)
        valueCount = std::max(valueCount, internal::ChartSeriesView(series[static_cast<std::size_t>(s)]).count);
    const int count = std::max(valueCount, static_cast<int>(std::min<std::size_t>(categories.size(), 1u << 20)));
    if (internal::DrawChartDataState(chartId, bounds, o.state, valueCount == 0, o.emptyText, o.errorText))
    {
        ImGui::ItemSize(bounds.GetSize());
        internal::ApplyNextItemData(next);
        ImGui::ItemAdd(bounds, chartId, nullptr, ImGuiItemFlags_NoNav);
        return;
    }

    internal::ApplyNextItemData(next);
    internal::ItemOptions itemOptions;
    itemOptions.handCursor = false;
    itemOptions.hitTarget = false;
    const internal::ItemState item = internal::ItemBehavior(chartId, bounds.GetSize(), itemOptions);
    const bool disabled = item.disabled;
    internal::ChartReadoutState& readout = internal::GetState<internal::ChartReadoutState>(chartId);
    if (!item.visible)
        return;
    const auto visible = [&](int s) { return !series[static_cast<std::size_t>(s)].hidden; };
    int shownSeries = 0;
    for (int s = 0; s < seriesCount; ++s)
        shownSeries += visible(s) ? 1 : 0;
    const auto categoryLabel = [&](int i) {
        return i < static_cast<int>(categories.size()) ? internal::ChartVisibleLabel(categories[static_cast<std::size_t>(i)])
                                                       : internal::ChartLabelText{};
    };

    // The value range: every visible value, or the positive and negative sums of a stack.
    Range data;
    const auto extend = [&](double v) {
        if (!std::isfinite(v))
            return;
        data = data.valid ? Range{std::min(data.min, v), std::max(data.max, v), true} : Range{v, v, true};
    };
    const bool logScale = o.valueAxis.scale == ValueScale::Logarithmic;
    for (int i = 0; i < count; ++i)
    {
        double up = 0.0, down = 0.0;
        for (int s = 0; s < seriesCount; ++s)
        {
            if (!visible(s))
                continue;
            const double v = BarValue(internal::ChartSeriesView(series[static_cast<std::size_t>(s)]), i);
            if (!std::isfinite(v) || (logScale && v <= 0.0))
                continue;
            if (!o.stacked)
                extend(v);
            else if (v >= 0.0)
                up += v;
            else
                down += v;
        }
        if (o.stacked)
        {
            extend(up);
            extend(down);
        }
    }

    const BarLegendData legendData{series, disabled};
    const float legendHeight =
        o.legend ? internal::MeasureChartLegend(&BarLegendEntry, &legendData, seriesCount, width) : 0.0f;
    const bool valueTitle = o.valueAxis.label != nullptr;
    const internal::ChartBands bands = vertical
                                           ? internal::LayoutChartBands(bounds, valueTitle, true, false, legendHeight)
                                           : internal::LayoutChartBands(bounds, false, !o.valueAxis.hideTicks, valueTitle,
                                                                        legendHeight);

    ImDrawList* drawList = window->DrawList;
    const float labelGap = std::round(Px(t.spacing.sm));
    const float tickGap = Px(t.spacing.lg);
    const internal::ChartRangeRule rule{.includeZero = true, .pad = true, .nice = true};
    internal::ChartNumericAxis valueAxis;
    ImRect plot;
    internal::Scale valueScale;
    float categoryColumn = 0.0f;  // horizontal: the label column
    drawList->PushClipRect(bounds.Min, bounds.Max, true);
    internal::ControlTextScope tickText(internal::kChartTickStyle);
    const float fontSize = ImGui::GetFontSize();
    if (vertical)
    {
        const float plotHeight = bands.plotBottom - bands.plotTop;
        const Range range = internal::ChartResolveRange(o.valueAxis, data, rule,
                                                        internal::ChartTickTarget(plotHeight, true, tickGap));
        valueAxis = internal::ChartValueAxis(o.valueAxis, range, plotHeight, true, tickGap);
        const float left = bounds.Min.x + (valueAxis.labels ? valueAxis.labelWidth + labelGap : 0.0f);
        plot = internal::SnapPlotRect(ImRect(left, bands.plotTop, bounds.Max.x, bands.plotBottom));
        valueScale = internal::ChartAxisScale(valueAxis, plot.Max.y, plot.Min.y);
        internal::DrawChartGridY(drawList, plot, valueAxis, valueScale, true);
        internal::DrawChartLabelsY(drawList, o.valueAxis, valueAxis, valueScale, plot.Min.x - labelGap, disabled);
    }
    else
    {
        float widest = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            const internal::ChartLabelText label = categoryLabel(i);
            widest = std::max(widest, ImGui::CalcTextSize(label.text, label.end).x);
        }
        categoryColumn = std::min(std::ceil(widest), std::floor(width * kBarLabelShare));
        const float left = bounds.Min.x + (categoryColumn > 0.0f ? categoryColumn + labelGap : 0.0f);
        // The top band is the same as the bottom half-label room, so the plot is centered on its rows.
        const float plotWidth = std::max(1.0f, bounds.Max.x - left);
        const Range range = internal::ChartResolveRange(o.valueAxis, data, rule,
                                                        internal::ChartTickTarget(plotWidth, false, Px(t.spacing.xxxl)));
        valueAxis = internal::ChartValueAxis(o.valueAxis, range, plotWidth, false, Px(t.spacing.xxxl));
        // Half a label of room on the right, so the last value label is not cut.
        const float right = std::max(left, bounds.Max.x - (valueAxis.labels ? std::ceil(valueAxis.labelWidth * 0.5f) : 0.0f));
        plot = internal::SnapPlotRect(ImRect(left, bounds.Min.y, right, bands.plotBottom));
        valueScale = internal::ChartAxisScale(valueAxis, plot.Min.x, plot.Max.x);
        internal::DrawChartGridX(drawList, plot, valueAxis, valueScale, true);
        internal::DrawChartLabelsX(drawList, o.valueAxis, valueAxis, valueScale, bands.xLabelsTop, plot.Min.x,
                                   bounds.Max.x, disabled);
    }
    const float along = vertical ? plot.GetWidth() : plot.GetHeight();
    const float slot = count > 0 ? along / static_cast<float>(count) : along;
    const float alongStart = vertical ? plot.Min.x : plot.Min.y;

    // Category labels: under the bars, or in the column before them; every n-th when crowded.
    {
        const int stride = std::max(1, static_cast<int>(std::ceil((vertical ? fontSize * kBarLabelMinEm : fontSize) / std::max(slot, 0.001f))));
        const ImU32 color = ToU32(disabled ? c.labelTertiary : c.labelSecondary);
        for (int i = 0; i < count; i += stride)
        {
            const internal::ChartLabelText label = categoryLabel(i);
            if (label.Empty())
                continue;
            const float center = alongStart + slot * (static_cast<float>(i) + 0.5f);
            if (vertical)
            {
                const internal::FittedText shown = internal::FitText(label.text, label.end, std::max(0.0f, slot * stride - labelGap));
                const float x = std::clamp(center - shown.width * 0.5f, bounds.Min.x, bounds.Max.x - shown.width);
                drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(std::round(x), bands.xLabelsTop), color, shown.text);
            }
            else if (categoryColumn > 0.0f)
            {
                const internal::FittedText shown = internal::FitText(label.text, label.end, categoryColumn);
                drawList->AddText(ImGui::GetFont(), fontSize,
                                  ImVec2(std::round(plot.Min.x - labelGap - shown.width), std::round(center - fontSize * 0.5f)),
                                  color, shown.text);
            }
        }
    }
    internal::DrawChartAxisTitle(drawList, o.valueAxis.label, vertical ? bounds.Min.x : plot.Min.x,
                                 vertical ? bounds.Max.x : plot.Max.x, vertical ? bands.yTitleTop : bands.xTitleTop,
                                 !vertical, disabled);

    // The pointer or the keys pick a category.
    if (internal::ChartReadoutKeys(chartId, item.focused && !disabled, count, !vertical, readout))
        readout.series = -1;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    bool show = false;
    if (!disabled && count > 0 && internal::ChartPointerReadout(item.hovered, plot, readout))
    {
        const float position = (vertical ? mouse.x : mouse.y) - alongStart;
        readout.index = std::clamp(static_cast<int>(std::floor(position / std::max(slot, 0.001f))), 0, count - 1);
        show = true;
    }
    else if (!disabled && ((item.hovered && plot.Contains(mouse)) || (readout.keyboard && item.focusVisible)))
    {
        show = true;
    }
    if (!disabled && (Has(item.forced, internal::ForcedItemState::Hovered) || Has(item.forced, internal::ForcedItemState::Focused)))
    {
        if (readout.index < 0 || readout.index >= count)
            readout.index = count / 2;
        show = true;
    }
    show = show && readout.index >= 0 && readout.index < count;

    // The bars.
    const float categoryGap = std::min(labelGap, slot * 0.5f);
    const float inner = std::max(1.0f, slot - categoryGap);
    const float barGap = Px(t.charts.barGap);
    const int slots = o.stacked ? 1 : std::max(1, shownSeries);
    const float gapInside = slots > 1 && inner > static_cast<float>(slots) * (barGap + 1.0f) ? barGap : 0.0f;
    const float thickness = std::max(1.0f, (inner - gapInside * static_cast<float>(slots - 1)) / static_cast<float>(slots));
    const float base = logScale ? valueScale.ToPixel(valueAxis.min) : valueScale.ToPixel(0.0);
    const float radius = Px(t.radii.element);
    const float lowEnd = vertical ? plot.Max.y : plot.Min.x;
    const float highEnd = vertical ? plot.Min.y : plot.Max.x;
    const auto clampValue = [&](float p) { return std::clamp(p, std::min(lowEnd, highEnd), std::max(lowEnd, highEnd)); };
    if (show)
    {
        // The picked category: a wash behind its bars.
        const float a = alongStart + slot * static_cast<float>(readout.index);
        if (vertical)
            FillRect(drawList, ImVec2(a, plot.Min.y), ImVec2(a + slot, plot.Max.y), ToU32(c.fill[3]), Px(t.radii.element));
        else
            FillRect(drawList, ImVec2(plot.Min.x, a), ImVec2(plot.Max.x, a + slot), ToU32(c.fill[3]), Px(t.radii.element));
    }
    if (plot.GetWidth() >= 1.0f && plot.GetHeight() >= 1.0f)
    {
        for (int i = 0; i < count; ++i)
        {
            const float start = alongStart + slot * static_cast<float>(i) + categoryGap * 0.5f;
            int place = 0;
            double up = 0.0, down = 0.0;
            // The outermost segment of each direction of a stack gets the rounded end.
            int lastUp = -1, lastDown = -1;
            if (o.stacked)
                for (int s = 0; s < seriesCount; ++s)
                {
                    const double v = visible(s) ? BarValue(internal::ChartSeriesView(series[static_cast<std::size_t>(s)]), i)
                                                : std::numeric_limits<double>::quiet_NaN();
                    if (std::isfinite(v) && v > 0.0)
                        lastUp = s;
                    if (std::isfinite(v) && v < 0.0)
                        lastDown = s;
                }
            for (int s = 0; s < seriesCount; ++s)
            {
                if (!visible(s))
                    continue;
                const ChartSeries& one = series[static_cast<std::size_t>(s)];
                const double v = BarValue(internal::ChartSeriesView(one), i);
                const float t0 = o.stacked ? start : start + static_cast<float>(place) * (thickness + gapInside);
                ++place;
                if (!std::isfinite(v) || (logScale && v <= 0.0))
                    continue;
                const ImU32 color = ToU32(internal::ChartDataColor(one.color, s, disabled));
                if (!o.stacked)
                {
                    DrawBarRect(drawList, vertical, clampValue(base), clampValue(valueScale.ToPixel(v)), t0, t0 + thickness,
                                color, radius, true);
                    continue;
                }
                double& sum = v >= 0.0 ? up : down;
                const float from = valueScale.ToPixel(sum);
                sum += v;
                float to = valueScale.ToPixel(sum);
                // A gap toward the next segment, while the segment is longer than the gap.
                const bool outer = s == (v >= 0.0 ? lastUp : lastDown);
                if (!outer && std::fabs(to - from) > 2.0f * barGap)
                    to += (to > from ? -barGap : barGap);
                DrawBarRect(drawList, vertical, clampValue(from), clampValue(to), t0, t0 + thickness, color, radius, outer);
            }
        }
    }

    if (show)
    {
        internal::ChartReadoutLine lines[internal::kChartReadoutLines];
        int lineCount = 0;
        int more = 0;
        for (int s = 0; s < seriesCount; ++s)
        {
            if (!visible(s))
                continue;
            const ChartSeries& one = series[static_cast<std::size_t>(s)];
            const double v = BarValue(internal::ChartSeriesView(one), readout.index);
            if (!std::isfinite(v))
                continue;
            if (lineCount >= internal::kChartReadoutLines)
            {
                ++more;
                continue;
            }
            internal::ChartReadoutLine& line = lines[lineCount++];
            line.label = internal::ChartVisibleLabel(one.label);
            line.color = internal::ChartDataColor(one.color, s, false);
            line.marker = internal::ChartMarker::Square;
            internal::ChartReadoutText(o.valueAxis, v, valueAxis.set, line.value, sizeof(line.value));
        }
        char title[internal::kFittedTextSize] = {};
        const internal::ChartLabelText label = categoryLabel(readout.index);
        if (!label.Empty())
            ImFormatString(title, sizeof(title), "%.*s", static_cast<int>(label.end - label.text), label.text);
        else
            ImFormatString(title, sizeof(title), "%d", readout.index + 1);
        const float center = alongStart + slot * (static_cast<float>(readout.index) + 0.5f);
        const ImVec2 anchor = vertical ? ImVec2(center + slot * 0.5f, plot.Min.y) : ImVec2(plot.Min.x + plot.GetWidth() * 0.5f, plot.Min.y);
        internal::DrawChartReadout(drawList, plot, anchor, title,
                                   std::span<const internal::ChartReadoutLine>(lines, static_cast<std::size_t>(lineCount)), more);
    }

    if (o.legend)
        internal::DrawChartLegend(drawList, ImVec2(bounds.Min.x, bands.legendTop), width, &BarLegendEntry, &legendData,
                                  seriesCount);
    drawList->PopClipRect();
    internal::DrawItemFocusRing(item, Px(t.radii.element));
}

} // namespace facet
