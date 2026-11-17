// Demo page: Charts. Calls every declared chart widget once with neutral example data, grouped
// by purpose. Until Phase 2 implements a widget, its call draws the placeholder of the
// declared size; Phase 2 replaces the placeholders one by one.

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::SectionHeader;

// Example data: a fixed placeholder pattern, not a measurement.
const float kSeriesA[] = {2.0f, 3.0f, 2.5f, 4.0f, 3.5f, 5.0f};
const float kSeriesB[] = {1.0f, 1.5f, 2.0f, 1.5f, 2.5f, 2.0f};
const facet::ChartSeries kSeries[] = {
    {.label = "Series 1", .values = kSeriesA},
    {.label = "Series 2", .values = kSeriesB},
};
const char* const kCategories[] = {"Category 1", "Category 2", "Category 3", "Category 4", "Category 5",
                                   "Category 6"};
const facet::DonutSlice kSlices[] = {{"Slice 1", 3.0}, {"Slice 2", 2.0}, {"Slice 3", 1.0}};
const facet::GaugeZone kZones[] = {{0.6, 0.8, facet::Tone::Warning}, {0.8, 1.0, facet::Tone::Destructive}};
const float kChannels[] = {0.5f, 0.4f};

struct ChartsUi
{
    facet::LegendItem legend[2] = {{"Series 1"}, {"Series 2"}};
    double low = 1.0;
    double high = 4.0;
};

ChartsUi ui;

} // namespace

FACET_DEMO_PAGE("charts", "Charts", "Widgets", 120)
{
    facet::demo::PageHeader("Charts",
                            "Sparklines, line, bar and donut charts, gauges, level meters, frame time graphs, legends "
                            "and the range brush. All charts share one data form (ChartSeries) and one axis options "
                            "struct (ChartAxis). Every widget on this page is a placeholder of its declared size until "
                            "it is implemented in Phase 2; the example values are a fixed pattern, not data.");

    SectionHeader("Small charts");
    LabeledRow("Sparkline");
    facet::Sparkline("Series 1##sparkline", kSeriesA, {.area = true});
    LabeledRow("Legend");
    facet::Legend("legend", ui.legend);

    SectionHeader("Charts with axes");
    LabeledRow("LineChart");
    facet::LineChart("line chart", kSeries, {.yAxis = {.label = "Value"}, .crosshair = true});
    LabeledRow("BarChart");
    facet::BarChart("bar chart", kSeries, kCategories, {.stacked = true});
    LabeledRow("DonutChart");
    facet::DonutChart("donut chart", kSlices, {.legend = true});
    LabeledRow("ChartRangeBrush");
    facet::ChartRangeBrush("Range##brush", &ui.low, &ui.high, 0.0, 5.0, kSeries, {.minGap = 1.0});

    SectionHeader("Gauges and meters");
    LabeledRow("Gauge");
    facet::Gauge("Value##gauge", 0.5, 0.0, 1.0, {.format = "%.2f", .zones = kZones});
    LabeledRow("LevelMeter");
    facet::LevelMeter("Level##meter", kChannels, 0.0f, 1.0f, {.zones = kZones});
    LabeledRow("FrameTimeGraph");
    facet::FrameTimeGraph("frame times", {.compact = true});
}
