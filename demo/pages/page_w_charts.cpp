// Demo page: Charts. Every chart widget with its variants, grouped by widget. All series are
// generated on the frame thread from formulas or a fixed seed and labelled as generated; none of
// them is measured data. Widgets that are still planned draw their placeholder.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/popup_host.h"
#include "pages/pages.h"
#include "widgets/charts/chart_parts.h"

namespace
{

using facet::demo::DemoWidth;
using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kDisabled,
};

constexpr int kWaveSamples = 48;
constexpr int kBarSamples = 16;
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// Generated series: sines with a deterministic wobble from a fixed seed, not measurements.
struct GeneratedSeries
{
    float wave[kWaveSamples] = {};
    float gaps[kWaveSamples] = {};
    float bars[kBarSamples] = {};

    GeneratedSeries()
    {
        std::uint32_t seed = 7u;
        for (int i = 0; i < kWaveSamples; ++i)
        {
            seed = seed * 1664525u + 1013904223u;
            const float wobble = static_cast<float>(static_cast<int>(seed >> 9) % 1000) / 4000.0f;
            wave[i] = 1.0f + std::sin(static_cast<float>(i) * 0.3f) + wobble;
            gaps[i] = (i >= 14 && i < 19) || i == 30 ? kNaN : wave[i];
        }
        for (int i = 0; i < kBarSamples; ++i)
            bars[i] = std::sin(static_cast<float>(i) * 0.7f) * 3.0f;
    }
};

const GeneratedSeries& Data()
{
    static const GeneratedSeries data;
    return data;
}

struct ChartsUi
{
    facet::LegendItem legend[3] = {{"Series 1"}, {"Series 2"}, {"Series 3"}};
    facet::LegendItem legendVertical[3] = {{"Series 1"}, {"Series 2", 0, true}, {"Series 3"}};
    facet::LegendItem legendReadOnly[2] = {{"Series 1"}, {"Series 2", 0, true}};
    facet::LegendItem legendNarrow[2] = {{"A series with a long placeholder label"}, {"Series 2"}};
    int legendWrites = 0;
    bool sparklineTip = false;  // the popup host answers inside the frame
    double low = 1.0;
    double high = 4.0;
};

ChartsUi ui;

bool Is(const facet::demo::PageCtx& ctx, CaptureState state)
{
    return ctx.capture && ctx.captureState == state;
}

void SparklineSection(facet::demo::PageCtx& ctx)
{
    const GeneratedSeries& data = Data();
    SectionHeader("Sparkline");
    Paragraph("A small trend without axes, for a table cell or the side of a number. Hover it to read the "
              "nearest value. It is not a Tab stop: the number it stands next to carries the meaning.");
    Note("Generated: a sine with a wobble from a fixed seed, %d samples. Not measured data.", kWaveSamples);
    LabeledRow("Line");
    facet::Sparkline("Generated wave##line", data.wave, {.width = DemoWidth(200.0f)});
    ui.sparklineTip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
    ctx.Probe("charts.sparkline");
    LabeledRow("Area, min and max");
    facet::Sparkline("Generated wave##area", data.wave, {.area = true, .markMinMax = true, .width = DemoWidth(200.0f)});
    LabeledRow("Bars");
    facet::Sparkline("Generated bars##bars", data.bars, {.bars = true, .markMinMax = true, .width = DemoWidth(200.0f)});
    LabeledRow("Gaps");
    facet::Sparkline("Generated wave with gaps##gaps", data.gaps, {.area = true, .width = DemoWidth(200.0f)});
    LabeledRow("No values");
    facet::Sparkline("No values##empty", std::span<const float>{}, {.width = DemoWidth(200.0f)});
    LabeledRow("Narrow");
    facet::Sparkline("Generated wave##narrow", data.wave, {.width = DemoWidth(32.0f)});
}

// Placeholder sections: these widgets are still planned and draw their placeholder.
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

void PlannedSections()
{
    SectionHeader("Legend");
    LabeledRow("Legend");
    facet::Legend("legend", ui.legend);

    SectionHeader("Charts with axes");
    LabeledRow("Line chart");
    facet::LineChart("line chart", kSeries, {.yAxis = {.label = "Value"}, .crosshair = true});
    LabeledRow("Bar chart");
    facet::BarChart("bar chart", kSeries, kCategories, {.stacked = true});
    LabeledRow("Donut chart");
    facet::DonutChart("donut chart", kSlices, {.legend = true});
    LabeledRow("Chart range brush");
    facet::ChartRangeBrush("Range##brush", &ui.low, &ui.high, 0.0, 5.0, kSeries, {.minGap = 1.0});

    SectionHeader("Gauges and meters");
    LabeledRow("Gauge");
    facet::Gauge("Value##gauge", 0.5, 0.0, 1.0, {.format = "%.2f", .zones = kZones});
    LabeledRow("Level meter");
    facet::LevelMeter("Level##meter", kChannels, 0.0f, 1.0f, {.zones = kZones});
    LabeledRow("Frame time graph");
    facet::FrameTimeGraph("frame times", {.compact = true});
}

} // namespace

FACET_DEMO_PAGE("charts", "Charts", "Widgets", 120, "default", "disabled")
{
    facet::demo::PageHeader("Charts",
                            "Sparklines, line, bar and donut charts, gauges, level meters, frame time graphs, "
                            "legends and the range brush. All charts share one data form (ChartSeries) and one axis "
                            "options struct (ChartAxis). Every series on this page is generated, not measured.");
    facet::demo::PlannedFamilyNote("charts");

    const bool disabled = Is(ctx, kDisabled);
    if (disabled)
        ImGui::BeginDisabled();
    SparklineSection(ctx);
    PlannedSections();
    if (disabled)
        ImGui::EndDisabled();
}

FACET_DEMO_SELFTEST("charts")
{
    using T = facet::demo::Target;
    test.Do([] { ui = ChartsUi{}; });
    test.Wait(2);

    // Sparkline: the hovered value shows in a tooltip.
    test.Hover(T::Item("charts.sparkline", 0.5f, 0.5f), facet::T().overlay.tooltipDelay + 0.3f);
    test.Expect("hovering the sparkline names its nearest value", [] { return ui.sparklineTip; });
}
