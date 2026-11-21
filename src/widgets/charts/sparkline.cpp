// Sparkline (facet/widgets/charts.h): a small trend line, area or bar row without axes, for a
// table cell or the side of a number. One item that is not a Tab stop; while hovered, the
// nearest value is marked and named in a tooltip ("label: value"). It follows the order of the
// model widget (src/widgets/buttons/button.cpp): entry, measure, one item, draw from tokens.

#include "facet/widgets/charts.h"

#include <algorithm>
#include <climits>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/chart_core.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "widgets/charts/chart_parts.h"

namespace facet
{
namespace
{

using internal::SeriesView;

// Bars keep the bar gap only while a slot is this many gaps wide; narrower bars touch.
constexpr float kSparkGapSlots = 3.0f;

// The value range: the options when they differ, else the data (bars include 0, because a bar is
// read from its baseline).
internal::Range SparklineRange(const SeriesView& view, const SparklineOptions& o)
{
    if (std::isfinite(o.min) && std::isfinite(o.max) && o.min != o.max)
        return {std::min(o.min, o.max), std::max(o.min, o.max), true};
    internal::Range range = internal::SeriesRangeY(view);
    if (range.valid && o.bars)
    {
        range.min = std::min(range.min, 0.0);
        range.max = std::max(range.max, 0.0);
    }
    return range;
}

// The bars of a sparkline: one slot per value along the plot.
struct SparkBars
{
    ImRect plot;
    int count = 0;
    float slot = 0.0f;
    float gap = 0.0f;

    float Left(int i) const { return plot.Min.x + slot * static_cast<float>(i); }
    float Right(int i) const { return Left(i) + std::max(slot - gap, 1.0f); }
    float Center(int i) const { return (Left(i) + Right(i)) * 0.5f; }
};

void DrawSparkBar(ImDrawList* drawList, float x0, float x1, float baseline, float top, ImU32 color, float radius)
{
    const float lo = std::min(baseline, top);
    const float hi = std::max(baseline, top);
    if (hi - lo < 0.5f)
        return;
    const ImDrawFlags corners = top < baseline ? ImDrawFlags_RoundTop : ImDrawFlags_RoundBottom;
    // A small bar keeps square corners; the rounding never takes more than a quarter of its width.
    FillRect(drawList, ImVec2(x0, lo), ImVec2(x1, hi), color, std::min({radius, (x1 - x0) * 0.25f, hi - lo}), corners);
}

// The first lowest and highest usable sample, -1 without any.
void SparkExtremes(const SeriesView& view, int* lowest, int* highest)
{
    *lowest = *highest = -1;
    double low = 0.0, high = 0.0;
    for (int i = 0; i < view.count; ++i)
    {
        double sx = 0.0, sy = 0.0;
        if (!view.Sample(i, &sx, &sy))
            continue;
        if (*lowest < 0 || sy < low)
        {
            *lowest = i;
            low = sy;
        }
        if (*highest < 0 || sy > high)
        {
            *highest = i;
            high = sy;
        }
    }
}

void SparklineImpl(const char* label, const SeriesView& view, const SparklineOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;

    const float width = internal::ResolveItemWidth(o.width, next, kFill);
    const float height = std::max(1.0f, std::round(o.height > 0.0f ? Px(o.height) : ControlHeight(ControlSize::Inherit)));
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + height));

    // One item that is not a Tab stop; the id keys the tooltip.
    const ImGuiID id = window->GetID(label);
    ImGui::ItemSize(bounds.GetSize());
    internal::ApplyNextItemData(next);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav))
        return;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool hovered = !disabled && ImGui::IsItemHovered();
    ImDrawList* drawList = window->DrawList;

    // The plot keeps room for half a marker on every side, so dots at the ends are not cut.
    const float marker = Px(t.charts.markerSize);
    const float lineWidth = Px(t.charts.lineWidth);
    const float inset = std::min(std::ceil(marker * 0.5f), std::floor(std::min(width, height) * 0.25f));
    const ImRect plot = internal::SnapPlotRect(
        ImRect(bounds.Min.x + inset, bounds.Min.y + inset, bounds.Max.x - inset, bounds.Max.y - inset));
    const ImVec4 color = disabled ? c.labelTertiary : (o.color != 0 ? FromU32(o.color) : internal::SeriesColor(c, 0));
    const ImU32 color32 = ToU32(color);

    const internal::Range range = SparklineRange(view, o);
    if (view.count <= 0 || !range.valid)
    {
        // Nothing to draw: a hairline marks the empty slot.
        HairlineH(drawList, bounds.Min.x, bounds.Max.x, std::floor(bounds.GetCenter().y), ToU32(c.separator));
        return;
    }
    const internal::Scale yScale = internal::MakeScale(internal::ScaleKind::Linear, range.min, range.max, plot.Max.y,
                                                       plot.Min.y);
    const auto yOf = [&](double v) { return std::clamp(yScale.ToPixel(v), plot.Min.y, plot.Max.y); };
    const float baseline = yOf(0.0);
    drawList->PushClipRect(bounds.Min, bounds.Max, true);

    int hoveredIndex = -1;
    ImVec2 hoveredPoint;
    double hoveredValue = 0.0;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool pointer = hovered && bounds.Contains(mouse);
    int lowest = -1, highest = -1;
    if (o.markMinMax)
        SparkExtremes(view, &lowest, &highest);

    if (o.bars)
    {
        SparkBars bars;
        bars.plot = plot;
        bars.count = view.count;
        bars.slot = plot.GetWidth() / static_cast<float>(view.count);
        bars.gap = bars.slot > kSparkGapSlots * Px(t.charts.barGap) ? Px(t.charts.barGap) : 0.0f;
        const float radius = Px(t.radii.element);
        if (bars.slot >= 1.0f)
        {
            for (int i = 0; i < view.count; ++i)
            {
                const double v = view.YAt(i);
                if (std::isfinite(v))
                    DrawSparkBar(drawList, bars.Left(i), bars.Right(i), baseline, yOf(v), color32, radius);
            }
        }
        else
        {
            // More values than pixel columns: each column shows the extremes of its values.
            int column = -1;
            float lo = baseline, hi = baseline;
            const auto flush = [&] {
                if (column >= 0)
                {
                    const float x = plot.Min.x + static_cast<float>(column);
                    FillRect(drawList, ImVec2(x, std::min(lo, baseline)), ImVec2(x + 1.0f, std::max(hi, baseline)), color32);
                }
            };
            for (int i = 0; i < view.count; ++i)
            {
                const double v = view.YAt(i);
                if (!std::isfinite(v))
                    continue;
                const int col = static_cast<int>(std::floor(bars.slot * static_cast<float>(i)));
                const float y = yOf(v);
                if (col != column)
                {
                    flush();
                    column = col;
                    lo = hi = y;
                }
                lo = std::min(lo, y);
                hi = std::max(hi, y);
            }
            flush();
        }
        for (const int i : {lowest, highest})
            if (i >= 0)
                internal::DrawChartMarker(drawList, ImVec2(bars.Center(i), yOf(view.YAt(i))), marker,
                                          internal::ChartMarker::Circle, color32, false);
        if (pointer)
        {
            const int i = std::clamp(static_cast<int>((mouse.x - plot.Min.x) / std::max(bars.slot, 1e-6f)), 0, view.count - 1);
            const double v = view.YAt(i);
            if (std::isfinite(v))
            {
                hoveredIndex = i;
                hoveredValue = v;
                hoveredPoint = ImVec2(bars.Center(i), yOf(v));
            }
        }
    }
    else
    {
        const internal::Range xs = internal::SeriesRangeX(view);
        const internal::Scale xScale = internal::MakeScale(internal::ScaleKind::Linear, xs.min, xs.max, plot.Min.x,
                                                           plot.Max.x);
        const bool sorted = internal::IsSortedByX(view);
        std::span<ImVec2> points = internal::ChartScratchPoints(internal::DecimatedCapacity(plot.GetWidth()));
        int begin = 0, end = 0;
        internal::VisibleSampleRange(view, xScale, sorted, &begin, &end);
        const ImU32 areaColor = ToU32(WithAlpha(color, c.accentSubtle.w));
        // One run of the line per stretch of samples between gaps; a run of one sample is a dot.
        for (int run = internal::NextUsableSample(view, begin, end); run < end;)
        {
            const int runEnd = internal::NextGap(view, run, end);
            const int n = internal::DecimatePolyline(view, xScale, yScale, points, sorted, run, runEnd, {});
            const std::span<const ImVec2> line(points.data(), static_cast<std::size_t>(n));
            if (n == 1)
            {
                drawList->AddCircleFilled(line[0], lineWidth, color32);
            }
            else if (n > 1)
            {
                if (o.area)
                    internal::DrawArea(drawList, line, baseline, areaColor, plot);
                internal::DrawPolyline(drawList, line, color32, lineWidth, bounds);
            }
            run = internal::NextUsableSample(view, runEnd, end);
        }
        for (const int i : {lowest, highest})
        {
            double sx = 0.0, sy = 0.0;
            if (i >= 0 && view.Sample(i, &sx, &sy))
                internal::DrawChartMarker(drawList, ImVec2(xScale.ToPixel(sx), yScale.ToPixel(sy)), marker,
                                          internal::ChartMarker::Circle, color32, false);
        }
        if (pointer)
        {
            const int i = internal::NearestSampleByX(view, xScale.FromPixel(mouse.x), sorted);
            double sx = 0.0, sy = 0.0;
            if (i >= 0 && view.Sample(i, &sx, &sy))
            {
                hoveredIndex = i;
                hoveredValue = sy;
                hoveredPoint = ImVec2(xScale.ToPixel(sx), yScale.ToPixel(sy));
            }
        }
    }

    // The hovered value: a dot on it, and the tooltip.
    if (hoveredIndex >= 0)
        internal::DrawChartMarker(drawList, hoveredPoint, marker, internal::ChartMarker::Circle, color32, false);
    drawList->PopClipRect();

    if (internal::BeginPopupTooltip(id, bounds, hoveredIndex >= 0, false))
    {
        char value[48];
        internal::ChartReadoutText(ChartAxis{}, hoveredValue, internal::TickSet{}, value, sizeof(value));
        const internal::ChartLabelText name = internal::ChartVisibleLabel(label);
        if (name.Empty())
            Text(TextStyle::Footnote, "%s", value);
        else
            Text(TextStyle::Footnote, "%.*s: %s", static_cast<int>(name.end - name.text), name.text, value);
        internal::EndPopupTooltip();
    }
}

} // namespace

void Sparkline(const char* label, std::span<const float> values, const SparklineOptions& options)
{
    SeriesView view;
    view.y = values.data();
    view.count = static_cast<int>(std::min<std::size_t>(values.size(), static_cast<std::size_t>(INT_MAX)));
    SparklineImpl(label, view, options);
}

void Sparkline(const char* label, int count, ChartGetter getter, void* user, const SparklineOptions& options)
{
    const internal::ChartGetterSource source{getter, user};
    SparklineImpl(label, internal::ChartGetterView(source, count), options);
}

} // namespace facet
