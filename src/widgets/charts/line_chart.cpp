// LineChart (facet/widgets/charts.h): lines or areas of one or more series over a shared x axis,
// with numeric axes whose labels never overlap, a crosshair readout under the pointer or the
// arrow keys, stacking, markers and a passive legend. The plot is one item and one Tab stop; the
// series and axis parts are the family's (chart_parts.h) on the chart core block.

#include "facet/widgets/charts.h"

#include <algorithm>
#include <cmath>

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

// The readout dot is this much larger than a marker, so it stands out on the line.
constexpr float kLineReadoutDot = 1.5f;

// The sum of the visible series below series `upTo` at sample `index`; gaps count as 0.
double LineStackBelow(std::span<const ChartSeries> series, int upTo, int index)
{
    double sum = 0.0;
    for (int k = 0; k < upTo; ++k)
    {
        const ChartSeries& below = series[static_cast<std::size_t>(k)];
        if (below.hidden)
            continue;
        const SeriesView view = internal::ChartSeriesView(below);
        double x = 0.0, y = 0.0;
        if (index < view.count && view.Sample(index, &x, &y))
            sum += y;
    }
    return sum;
}

// A stacked series: the x of series `upTo` and the sum of the visible series up to it.
struct LineStack
{
    std::span<const ChartSeries> series;
    int upTo = 0;
};

bool ReadLineStack(const void* source, int slot, double* x, double* y)
{
    const LineStack& stack = *static_cast<const LineStack*>(source);
    const SeriesView top = internal::ChartSeriesView(stack.series[static_cast<std::size_t>(stack.upTo)]);
    double tx = 0.0, ty = 0.0;
    if (!top.Sample(slot, &tx, &ty))
        return false;
    *x = tx;
    *y = ty + LineStackBelow(stack.series, stack.upTo, slot);
    return true;
}

// The view a series is drawn from: its own values, or its stack.
SeriesView LineDisplayView(const ChartSeries& series, const LineStack* stack)
{
    SeriesView view = internal::ChartSeriesView(series);
    if (stack == nullptr)
        return view;
    SeriesView stacked;
    stacked.count = view.count;
    stacked.reader = &ReadLineStack;
    stacked.source = stack;
    return stacked;
}

struct LineLegendData
{
    std::span<const ChartSeries> series;
    bool markers = false;
    bool disabled = false;
};

bool LineLegendEntry(const void* data, int index, internal::ChartLabelText* label, internal::ChartLegendLook* look)
{
    const LineLegendData& legend = *static_cast<const LineLegendData*>(data);
    const ChartSeries& series = legend.series[static_cast<std::size_t>(index)];
    *label = internal::ChartVisibleLabel(series.label);
    if (label->Empty())
        return false;
    look->color = internal::ChartDataColor(series.color, index, false);
    look->hidden = series.hidden;
    look->disabled = legend.disabled;
    look->useMarker = legend.markers;
    look->marker = internal::ChartMarkerOf(index);
    return true;
}

// Calls run(begin, end) for every stretch of usable samples of `view` in [from, to).
template <class Run>
void ForLineRuns(const SeriesView& view, int from, int to, Run run)
{
    for (int begin = internal::NextUsableSample(view, from, to); begin < to;)
    {
        const int end = internal::NextGap(view, begin, to);
        run(begin, end);
        begin = internal::NextUsableSample(view, end, to);
    }
}

} // namespace

void LineChart(const char* id, std::span<const ChartSeries> series, const LineChartOptions& o)
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

    // Loading, error, and empty when no series has a sample.
    bool empty = true;
    for (int s = 0; s < seriesCount && empty; ++s)
        empty = internal::ChartSeriesView(series[static_cast<std::size_t>(s)]).count <= 0;
    if (internal::DrawChartDataState(chartId, bounds, o.state, empty, o.emptyText, o.errorText))
    {
        ImGui::ItemSize(bounds.GetSize());
        internal::ApplyNextItemData(next);
        ImGui::ItemAdd(bounds, chartId, nullptr, ImGuiItemFlags_NoNav);
        return;
    }

    // One item for the whole chart: hover, focus (a Tab stop) and the keys of the readout.
    internal::ApplyNextItemData(next);
    internal::ItemOptions itemOptions;
    itemOptions.handCursor = false;
    itemOptions.hitTarget = false;
    const internal::ItemState item = internal::ItemBehavior(chartId, bounds.GetSize(), itemOptions);
    const bool disabled = item.disabled;
    internal::ChartReadoutState& readout = internal::GetState<internal::ChartReadoutState>(chartId);
    if (!item.visible)
        return;

    // Stacks of the visible series (each reads the series below it).
    const auto visible = [&](int s) {
        const ChartSeries& one = series[static_cast<std::size_t>(s)];
        return !one.hidden && internal::ChartSeriesView(one).count > 0;
    };
    int reference = -1;
    for (int s = 0; s < seriesCount && reference < 0; ++s)
        reference = visible(s) ? s : -1;

    const LineLegendData legendData{series, o.markers, disabled};
    const float legendHeight =
        o.legend ? internal::MeasureChartLegend(&LineLegendEntry, &legendData, seriesCount, width) : 0.0f;
    const internal::ChartBands bands =
        internal::LayoutChartBands(bounds, o.yAxis.label != nullptr, !o.xAxis.hideTicks, o.xAxis.label != nullptr, legendHeight);

    // Ranges from the visible series (the stacks when stacked).
    const internal::ScaleKind yKind =
        o.yAxis.scale == ValueScale::Logarithmic ? internal::ScaleKind::Logarithmic : internal::ScaleKind::Linear;
    Range yData;
    Range xData;
    for (int s = 0; s < seriesCount; ++s)
    {
        if (!visible(s))
            continue;
        const LineStack stack{series, s};
        const SeriesView shown = LineDisplayView(series[static_cast<std::size_t>(s)], o.stacked ? &stack : nullptr);
        const Range ys = internal::SeriesRangeY(shown, yKind);
        const Range xs = internal::SeriesRangeX(internal::ChartSeriesView(series[static_cast<std::size_t>(s)]));
        if (ys.valid)
            yData = yData.valid ? Range{std::min(yData.min, ys.min), std::max(yData.max, ys.max), true} : ys;
        if (xs.valid)
            xData = xData.valid ? Range{std::min(xData.min, xs.min), std::max(xData.max, xs.max), true} : xs;
    }

    ImDrawList* drawList = window->DrawList;
    const float marker = Px(t.charts.markerSize);
    const float edge = std::ceil(marker * 0.5f);
    const float labelGap = std::round(Px(t.spacing.sm));
    const float tickGap = Px(t.spacing.lg);
    internal::ChartNumericAxis yAxis;
    internal::ChartNumericAxis xAxis;
    ImRect plot;
    internal::Scale xScale, yScale;
    drawList->PushClipRect(bounds.Min, bounds.Max, true);
    {
        internal::ControlTextScope tickText(internal::kChartTickStyle);
        const float plotHeight = bands.plotBottom - bands.plotTop;
        const internal::ChartRangeRule yRule{.includeZero = o.area || o.stacked, .pad = true, .nice = true};
        const Range yRange =
            internal::ChartResolveRange(o.yAxis, yData, yRule, internal::ChartTickTarget(plotHeight, true, tickGap));
        yAxis = internal::ChartValueAxis(o.yAxis, yRange, plotHeight, true, tickGap);
        const float left = bounds.Min.x + (yAxis.labels ? yAxis.labelWidth + labelGap : edge);
        const float right = std::max(left, bounds.Max.x - edge);
        const Range xRange = internal::ChartResolveRange(o.xAxis, xData, {}, 0);
        // Labels along x keep a wider gap: they are read side by side.
        xAxis = internal::ChartValueAxis(o.xAxis, xRange, right - left, false, Px(t.spacing.xxxl));
        plot = internal::SnapPlotRect(ImRect(left, bands.plotTop, right, bands.plotBottom));
        xScale = internal::ChartAxisScale(xAxis, plot.Min.x, plot.Max.x);
        yScale = internal::ChartAxisScale(yAxis, plot.Max.y, plot.Min.y);

        internal::DrawChartGridY(drawList, plot, yAxis, yScale, true);
        internal::DrawChartGridX(drawList, plot, xAxis, xScale, false);
        internal::DrawChartLabelsY(drawList, o.yAxis, yAxis, yScale, plot.Min.x - labelGap, disabled);
        internal::DrawChartLabelsX(drawList, o.xAxis, xAxis, xScale, bands.xLabelsTop, bounds.Min.x, bounds.Max.x, disabled);
    }
    internal::DrawChartAxisTitle(drawList, o.yAxis.label, bounds.Min.x, bounds.Max.x, bands.yTitleTop, false, disabled);
    internal::DrawChartAxisTitle(drawList, o.xAxis.label, plot.Min.x, plot.Max.x, bands.xTitleTop, true, disabled);

    const bool drawable = plot.GetWidth() >= 1.0f && plot.GetHeight() >= 1.0f;
    const float lineWidth = Px(t.charts.lineWidth);
    // Lines may reach half their width past the plot, so a line along an edge is not cut in half.
    ImRect lineClip(plot.Min.x - lineWidth, plot.Min.y - lineWidth, plot.Max.x + lineWidth, plot.Max.y + lineWidth);
    lineClip.ClipWith(bounds);
    const float baseline =
        std::clamp(yScale.ToPixel(yKind == internal::ScaleKind::Logarithmic ? yAxis.min : 0.0), plot.Min.y, plot.Max.y);
    if (drawable)
    {
        const int capacity = internal::DecimatedCapacity(plot.GetWidth());
        std::span<ImVec2> points = internal::ChartScratchPoints(capacity);
        std::span<ImVec2> base = internal::ChartScratchBase(capacity);
        std::span<int> indices = internal::ChartScratchIndices(capacity);
        // Areas first, then the lines over them, then the markers on top.
        for (int pass = 0; pass < 3; ++pass)
        {
            if ((pass == 0 && !o.area) || (pass == 2 && !o.markers))
                continue;
            for (int s = 0; s < seriesCount; ++s)
            {
                if (!visible(s))
                    continue;
                const ChartSeries& one = series[static_cast<std::size_t>(s)];
                const SeriesView own = internal::ChartSeriesView(one);
                const LineStack stack{series, s};
                const SeriesView shown = LineDisplayView(one, o.stacked ? &stack : nullptr);
                const bool sorted = internal::IsSortedByX(own);
                const ImVec4 color = internal::ChartDataColor(one.color, s, disabled);
                int from = 0, to = 0;
                internal::VisibleSampleRange(own, xScale, sorted, &from, &to);
                if (pass == 2)
                {
                    // Markers only while they are at least a marker apart.
                    if (static_cast<float>(to - from) * marker > plot.GetWidth())
                        continue;
                    for (int i = from; i < to; ++i)
                    {
                        double x = 0.0, y = 0.0;
                        if (shown.Sample(i, &x, &y))
                            internal::DrawChartMarker(drawList, ImVec2(xScale.ToPixel(x), yScale.ToPixel(y)), marker,
                                                      internal::ChartMarkerOf(s), ToU32(color), false);
                    }
                    continue;
                }
                ForLineRuns(shown, from, to, [&](int begin, int end) {
                    const int n = internal::DecimatePolyline(shown, xScale, yScale, points, sorted, begin, end, indices);
                    const std::span<const ImVec2> line(points.data(), static_cast<std::size_t>(n));
                    if (pass == 0 && n > 1)
                    {
                        const ImU32 fill = ToU32(WithAlpha(color, c.accentSubtle.w));
                        if (!o.stacked)
                        {
                            internal::DrawArea(drawList, line, baseline, fill, plot);
                            return;
                        }
                        for (int k = 0; k < n; ++k)
                        {
                            const int index = indices[static_cast<std::size_t>(k)];
                            const double below = LineStackBelow(series, s, index);
                            base[static_cast<std::size_t>(k)] =
                                ImVec2(line[static_cast<std::size_t>(k)].x,
                                       yKind == internal::ScaleKind::Logarithmic && below <= 0.0 ? baseline : yScale.ToPixel(below));
                        }
                        internal::DrawBand(drawList, line, std::span<const ImVec2>(base.data(), static_cast<std::size_t>(n)),
                                           fill, plot);
                        return;
                    }
                    if (pass == 1)
                    {
                        if (n == 1)
                            drawList->AddCircleFilled(line[0], lineWidth, ToU32(color));
                        else
                            internal::DrawPolyline(drawList, line, ToU32(color), lineWidth, lineClip);
                    }
                });
            }
        }
    }

    // The readout: the pointer picks the nearest x (or with crosshair off, the nearest point);
    // Left, Right, Home and End step through the samples of the first visible series.
    if (reference >= 0 && drawable && !disabled)
    {
        const SeriesView refView = internal::ChartSeriesView(series[static_cast<std::size_t>(reference)]);
        const bool refSorted = internal::IsSortedByX(refView);
        if (internal::ChartReadoutKeys(chartId, item.focused, refView.count, false, readout))
            readout.series = -1;
        bool show = false;
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        if (internal::ChartPointerReadout(item.hovered, plot, readout))
        {
            show = true;
            if (o.crosshair)
            {
                readout.index = internal::NearestSampleByX(refView, xScale.FromPixel(mouse.x), refSorted);
                readout.series = -1;
            }
            else
            {
                float best = 0.0f;
                readout.index = -1;
                for (int s = 0; s < seriesCount; ++s)
                {
                    if (!visible(s))
                        continue;
                    const ChartSeries& one = series[static_cast<std::size_t>(s)];
                    const SeriesView own = internal::ChartSeriesView(one);
                    const LineStack stack{series, s};
                    const SeriesView shown = LineDisplayView(one, o.stacked ? &stack : nullptr);
                    const int i = internal::NearestSampleByX(own, xScale.FromPixel(mouse.x), internal::IsSortedByX(own));
                    double x = 0.0, y = 0.0;
                    if (i < 0 || !shown.Sample(i, &x, &y))
                        continue;
                    const float dx = xScale.ToPixel(x) - mouse.x;
                    const float dy = yScale.ToPixel(y) - mouse.y;
                    const float distance = dx * dx + dy * dy;
                    if (readout.index < 0 || distance < best)
                    {
                        best = distance;
                        readout.index = i;
                        readout.series = s;
                    }
                }
            }
        }
        else if (item.hovered && plot.Contains(mouse))
        {
            show = true;  // the keys moved the readout and the pointer rests on the plot
        }
        else if (readout.keyboard && item.focusVisible)
        {
            show = true;
        }
        if (Has(item.forced, internal::ForcedItemState::Hovered) || Has(item.forced, internal::ForcedItemState::Focused))
        {
            // Captures: the readout of the middle sample when none was chosen.
            if (readout.index < 0 || readout.index >= refView.count)
                readout.index = refView.count / 2;
            show = true;
        }
        const bool single = readout.series >= 0 && readout.series < seriesCount && visible(readout.series);
        const SeriesView anchorView =
            single ? internal::ChartSeriesView(series[static_cast<std::size_t>(readout.series)]) : refView;
        double anchorX = 0.0, anchorY = 0.0;
        if (show && readout.index >= 0 && readout.index < anchorView.count && anchorView.Sample(readout.index, &anchorX, &anchorY))
        {
            const float px = xScale.ToPixel(anchorX);
            if (o.crosshair)
                HairlineV(drawList, std::floor(px), plot.Min.y, plot.Max.y, ToU32(c.labelSecondary));
            internal::ChartReadoutLine lines[internal::kChartReadoutLines];
            int count = 0;
            int more = 0;
            for (int s = 0; s < seriesCount; ++s)
            {
                if (!visible(s) || (single && s != readout.series))
                    continue;
                const ChartSeries& one = series[static_cast<std::size_t>(s)];
                const SeriesView own = internal::ChartSeriesView(one);
                const int i = single || s == reference ? readout.index
                                                       : internal::NearestSampleByX(own, anchorX, internal::IsSortedByX(own));
                double x = 0.0, y = 0.0;
                if (i < 0 || !own.Sample(i, &x, &y))
                    continue;
                const double shownY = o.stacked ? y + LineStackBelow(series, s, i) : y;
                const ImVec4 color = internal::ChartDataColor(one.color, s, false);
                const internal::ChartMarker shape = o.markers ? internal::ChartMarkerOf(s) : internal::ChartMarker::Circle;
                internal::DrawChartMarker(drawList, ImVec2(xScale.ToPixel(x), yScale.ToPixel(shownY)), marker * kLineReadoutDot,
                                          shape, ToU32(color), false);
                if (count >= internal::kChartReadoutLines)
                {
                    ++more;
                    continue;
                }
                internal::ChartReadoutLine& line = lines[count++];
                line.label = internal::ChartVisibleLabel(one.label);
                line.color = color;
                line.marker = shape;
                internal::ChartReadoutText(o.yAxis, y, yAxis.set, line.value, sizeof(line.value));
            }
            char title[48];
            internal::ChartReadoutText(o.xAxis, anchorX, xAxis.set, title, sizeof(title));
            internal::DrawChartReadout(drawList, plot, ImVec2(px, plot.Min.y), title,
                                       std::span<const internal::ChartReadoutLine>(lines, static_cast<std::size_t>(count)), more);
        }
    }

    if (o.legend)
        internal::DrawChartLegend(drawList, ImVec2(bounds.Min.x, bands.legendTop), width, &LineLegendEntry, &legendData,
                                  seriesCount);
    drawList->PopClipRect();
    internal::DrawItemFocusRing(item, Px(t.radii.element));
}

} // namespace facet
