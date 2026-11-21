// Demo page: Charts. Every chart widget with its variants, one section per widget. All series are
// generated on the frame thread from formulas or a fixed seed and labelled as generated; none of
// them is measured data. The one exception is the frame time graph, which shows this app's own
// frame times (in capture runs the fixed time step of the run).

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
    kReadout,
    kStates,
    kDisabled,
};

constexpr int kWaveSamples = 48;
constexpr int kBarSamples = 16;
constexpr int kLineSamples = 121;
constexpr int kLongSamples = 2000;
constexpr int kCategories = 6;
constexpr int kManyCategories = 30;
constexpr int kFrameSamples = 240;
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// Generated series: sines with a deterministic wobble from a fixed seed, not measurements.
struct GeneratedSeries
{
    float wave[kWaveSamples] = {};
    float gaps[kWaveSamples] = {};
    float bars[kBarSamples] = {};
    double time[kLineSamples] = {};  // seconds, 0.5 apart
    float lines[3][kLineSamples] = {};
    float rough[kLineSamples] = {};  // gaps and negative values
    float growth[kLineSamples] = {};
    float large[kLineSamples] = {};
    float long_[kLongSamples] = {};
    float categories[3][kCategories] = {};
    float signedBars[kCategories] = {};
    float many[kManyCategories] = {};
    float frames[kFrameSamples] = {};  // seconds per frame, with spikes

    GeneratedSeries()
    {
        std::uint32_t seed = 7u;
        const auto next = [&seed] {
            seed = seed * 1664525u + 1013904223u;
            return static_cast<float>(static_cast<int>(seed >> 9) % 1000) / 1000.0f;
        };
        for (int i = 0; i < kWaveSamples; ++i)
        {
            wave[i] = 1.0f + std::sin(static_cast<float>(i) * 0.3f) + next() * 0.25f;
            gaps[i] = (i >= 14 && i < 19) || i == 30 ? kNaN : wave[i];
        }
        for (int i = 0; i < kBarSamples; ++i)
            bars[i] = std::sin(static_cast<float>(i) * 0.7f) * 3.0f;
        for (int i = 0; i < kLineSamples; ++i)
        {
            const float x = static_cast<float>(i) * 0.5f;
            time[i] = x;
            lines[0][i] = 4.0f + 2.0f * std::sin(x * 0.2f) + next() * 0.4f;
            lines[1][i] = 3.0f + 1.5f * std::cos(x * 0.15f) + next() * 0.4f;
            lines[2][i] = 2.0f + std::sin(x * 0.35f + 1.0f) + next() * 0.4f;
            rough[i] = (i > 40 && i < 52) || i == 90 ? kNaN : 2.0f * std::sin(x * 0.25f) + next() - 0.5f;
            growth[i] = std::pow(10.0f, x / 15.0f);
            large[i] = 2.5e9f + 1.0e9f * std::sin(x * 0.1f);
        }
        float walk = 0.0f;
        for (int i = 0; i < kLongSamples; ++i)
        {
            walk += next() - 0.5f;
            long_[i] = walk;
        }
        for (int s = 0; s < 3; ++s)
            for (int i = 0; i < kCategories; ++i)
                categories[s][i] = 2.0f + 3.0f * next() + static_cast<float>(s);
        for (int i = 0; i < kCategories; ++i)
            signedBars[i] = 4.0f * std::sin(static_cast<float>(i) * 1.3f);
        for (int i = 0; i < kManyCategories; ++i)
            many[i] = 1.0f + 4.0f * next();
        for (int i = 0; i < kFrameSamples; ++i)
            frames[i] = (1.0f / 60.0f) * (0.82f + 0.14f * next()) + (i % 47 == 0 ? 0.012f : 0.0f);
    }
};

const GeneratedSeries& Data()
{
    static const GeneratedSeries data;
    return data;
}

const char* const kLooks[] = {"Lines", "Areas", "Stacked"};
const char* const kDataStates[] = {"Ready", "Loading", "Error", "Empty"};
const char* const kOrientations[] = {"Vertical", "Horizontal"};
const char* const kCategoryNames[kCategories] = {"Category 1", "Category 2", "Category 3",
                                                 "Category 4", "Category 5", "Category 6"};

struct ChartsUi
{
    facet::LegendItem legend[3] = {{"Series 1"}, {"Series 2"}, {"Series 3"}};
    facet::LegendItem legendVertical[3] = {{"Series 1"}, {"Series 2", 0, true}, {"Series 3"}};
    facet::LegendItem legendReadOnly[2] = {{"Series 1"}, {"Series 2", 0, true}};
    facet::LegendItem legendNarrow[2] = {{"A series with a long placeholder label"}, {"Series 2"}};
    int legendWrites = 0;
    bool sparklineTip = false;  // the popup host answers inside the frame
    int look = 0;
    bool markers = false;
    bool crosshair = true;
    int lineState = 0;
    ImGuiID lineId = 0;
    int orientation = 0;
    bool stacked = false;
    int barState = 0;
    ImGuiID barId = 0;
    bool pie = false;
    ImGuiID donutId = 0;
    float gauge = 62.0f;
    double low = 400.0;
    double high = 900.0;
    double lockedLow = 800.0;
    double lockedHigh = 1400.0;
    int brushWrites = 0;
};

ChartsUi ui;

bool Is(const facet::demo::PageCtx& ctx, CaptureState state)
{
    return ctx.capture && ctx.captureState == state;
}

facet::DataState StateOf(int index)
{
    return index == 1 ? facet::DataState::Loading : (index == 2 ? facet::DataState::Error : facet::DataState::Ready);
}

// Probes for the entries of a legend: "<probe>.<i>" with the entry's id, for clicks and focus.
void ProbeLegend(facet::demo::PageCtx& ctx, const char* probe, const char* id, std::span<const facet::LegendItem> items)
{
    if (ctx.probes == nullptr)
        return;
    const ImGuiID legendId = ImGui::GetID(id);
    for (int i = 0; i < static_cast<int>(items.size()); ++i)
    {
        const ImRect rect = facet::internal::ChartLegendEntryRect(legendId, i);
        char name[64];
        std::snprintf(name, sizeof(name), "%s.%d", probe, i);
        facet::demo::ProbeRect(ctx, name, facet::internal::ChartEntryId(legendId, items[static_cast<std::size_t>(i)].id, i),
                               rect.Min, rect.Max);
    }
}

// The readout of a chart as a line of text, so a reader without the pointer sees what is inspected.
void ReadoutNote(ImGuiID chartId, const char* what)
{
    const int index = facet::internal::ChartReadoutIndex(chartId);
    if (index < 0)
        Note("Hover the chart, or focus it with Tab and use the arrow keys, Home and End, to read %s.", what);
    else
        Note("Inspected: %s %d.", what, index + 1);
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

void LegendSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Legend");
    Paragraph("Swatches with labels. A click, Enter or Space hides an entry; with Alt held it shows that entry "
              "alone, and again shows them all. A hidden entry has a hollow swatch and a secondary label. The "
              "line chart below follows this legend.");
    LabeledRow("Toggle");
    if (facet::Legend("legend", ui.legend))
        ++ui.legendWrites;
    ctx.Probe("charts.legend");
    ProbeLegend(ctx, "charts.legend", "legend", ui.legend);
    char shown[96] = {};
    for (const facet::LegendItem& item : ui.legend)
    {
        if (item.hidden)
            continue;
        const std::size_t used = std::strlen(shown);
        std::snprintf(shown + used, sizeof(shown) - used, "%s%s", used > 0 ? ", " : "", item.label);
    }
    Note("Shown: %s.", shown[0] != '\0' ? shown : "none");
    LabeledRow("Vertical");
    facet::Legend("legend vertical", ui.legendVertical, {.orientation = facet::Orientation::Vertical});
    LabeledRow("Read-only");
    facet::Legend("legend read only", ui.legendReadOnly, {.readOnly = true});
    ProbeLegend(ctx, "charts.legendReadOnly", "legend read only", ui.legendReadOnly);
    LabeledRow("Passive");
    facet::Legend("legend passive", ui.legendVertical, {.toggle = false});
    LabeledRow("Narrow");
    facet::Legend("legend narrow", ui.legendNarrow, {.width = DemoWidth(140.0f)});
}

void LineChartSection(facet::demo::PageCtx& ctx)
{
    const GeneratedSeries& data = Data();
    SectionHeader("Line chart");
    Paragraph("Lines or areas of several series over a shared x axis. Hover the plot to read every series at "
              "the nearest x; with keyboard focus the arrow keys, Home and End move the readout.");
    Note("Generated: sines with a wobble from a fixed seed, x in seconds. Not measured data.");
    LabeledRow("Look");
    facet::SegmentedControl("##look", &ui.look, kLooks);
    ctx.Probe("charts.lineLook");
    LabeledRow("Markers");
    facet::Toggle("##markers", &ui.markers);
    LabeledRow("Crosshair");
    facet::Toggle("##crosshair", &ui.crosshair);
    LabeledRow("Data");
    facet::SegmentedControl("##line data", &ui.lineState, kDataStates);

    facet::ChartSeries series[3];
    for (int s = 0; s < 3; ++s)
        series[s] = {.label = ui.legend[s].label, .values = data.lines[s], .x = data.time, .hidden = ui.legend[s].hidden};
    const bool emptyData = ui.lineState == 3;
    facet::LineChartOptions options{.xAxis = {.format = "%.0f s", .label = "Time (generated)"},
                                    .yAxis = {.label = "Value"},
                                    .area = ui.look >= 1,
                                    .stacked = ui.look == 2,
                                    .markers = ui.markers,
                                    .crosshair = ui.crosshair,
                                    .state = Is(ctx, kStates) ? facet::DataState::Loading : StateOf(ui.lineState)};
    ui.lineId = ImGui::GetID("line");
    if (Is(ctx, kReadout))
        ctx.ForceNextItemState(facet::demo::ForcedItemState::Hovered);
    facet::LineChart("line", emptyData ? std::span<const facet::ChartSeries>{} : std::span<const facet::ChartSeries>(series),
                     options);
    ctx.Probe("charts.line");
    if (Is(ctx, kReadout))
        ctx.RevealInCapture(0.4f);
    if (Is(ctx, kStates))
        ctx.RevealInCapture(0.15f);  // the loading line chart and the bar chart in its error state
    ReadoutNote(ui.lineId, "sample");

    LabeledRow("Gaps, negatives");
    const facet::ChartSeries rough[] = {{.label = "Generated with gaps", .values = data.rough, .x = data.time}};
    facet::LineChart("line rough", rough, {.markers = true, .legend = true, .height = 120.0f});
    LabeledRow("Logarithmic");
    const facet::ChartSeries growth[] = {{.label = "Generated growth", .values = data.growth, .x = data.time}};
    facet::LineChart("line log", growth, {.yAxis = {.scale = facet::ValueScale::Logarithmic}, .height = 120.0f});
    LabeledRow("Large values");
    const facet::ChartSeries large[] = {{.label = "Generated large values", .values = data.large, .x = data.time}};
    facet::LineChart("line large", large, {.area = true, .height = 120.0f});
    LabeledRow("Narrow");
    facet::LineChart("line narrow", series, {.width = DemoWidth(160.0f), .height = 100.0f});
}

void BarChartSection(facet::demo::PageCtx& ctx)
{
    const GeneratedSeries& data = Data();
    SectionHeader("Bar chart");
    Paragraph("Grouped or stacked bars per category. Hover a category to read it; with keyboard focus the arrow "
              "keys move between categories. Stacked segments keep a gap between them.");
    Note("Generated from a fixed seed. Not measured data.");
    LabeledRow("Orientation");
    facet::SegmentedControl("##orientation", &ui.orientation, kOrientations);
    LabeledRow("Stacked");
    facet::Toggle("##stacked", &ui.stacked);
    ctx.Probe("charts.barStacked");
    LabeledRow("Data");
    facet::SegmentedControl("##bar data", &ui.barState, kDataStates);

    const facet::ChartSeries series[] = {
        {.label = "Series 1", .values = data.categories[0]},
        {.label = "Series 2", .values = data.categories[1]},
        {.label = "Series 3", .values = data.categories[2]},
    };
    ui.barId = ImGui::GetID("bars");
    if (Is(ctx, kReadout))
        ctx.ForceNextItemState(facet::demo::ForcedItemState::Hovered);
    facet::BarChart("bars", ui.barState == 3 ? std::span<const facet::ChartSeries>{} : std::span<const facet::ChartSeries>(series),
                    kCategoryNames,
                    {.valueAxis = {.label = "Value"},
                     .orientation = ui.orientation == 1 ? facet::Orientation::Horizontal : facet::Orientation::Vertical,
                     .stacked = ui.stacked,
                     .legend = true,
                     .state = Is(ctx, kStates) ? facet::DataState::Error : StateOf(ui.barState)});
    ctx.Probe("charts.bars");
    ReadoutNote(ui.barId, "category");

    LabeledRow("Negative values");
    const facet::ChartSeries signedSeries[] = {{.label = "Generated", .values = data.signedBars}};
    facet::BarChart("bars signed", signedSeries, kCategoryNames, {.height = 120.0f});
    LabeledRow("Many categories");
    const facet::ChartSeries many[] = {{.label = "Generated", .values = data.many}};
    facet::BarChart("bars many", many, std::span<const char* const>{}, {.height = 120.0f});
}

void DonutSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Donut chart");
    Paragraph("Slices proportional to their values. The ring's center shows the total or the picked slice; slices "
              "below a share merge into one. The arrow keys move between slices.");
    Note("Placeholder shares, not measured data.");
    LabeledRow("Pie");
    facet::Toggle("##pie", &ui.pie);
    static const facet::DonutSlice slices[] = {{"Slice 1", 42.0}, {"Slice 2", 26.0}, {"Slice 3", 18.0},
                                               {"Slice 4", 10.0}, {"Small 1", 2.5},  {"Small 2", 1.5}};
    ui.donutId = ImGui::GetID("donut");
    if (Is(ctx, kReadout))
        ctx.ForceNextItemState(facet::demo::ForcedItemState::Focused);
    const ImVec2 at = ImGui::GetCursorScreenPos();
    facet::DonutChart("donut", Is(ctx, kStates) ? std::span<const facet::DonutSlice>{} : std::span<const facet::DonutSlice>(slices),
                      {.pie = ui.pie, .minSliceFraction = 0.05f, .otherLabel = "Other", .legend = true});
    ctx.Probe("charts.donut");
    const float diameter = std::min(facet::Px(facet::T().metrics.chartHeight), ImGui::GetItemRectSize().x);
    facet::demo::ProbeRect(ctx, "charts.donutRing", 0, at, ImVec2(at.x + diameter, at.y + diameter));
    ReadoutNote(ui.donutId, "slice");
    LabeledRow("Center text");
    static const facet::DonutSlice two[] = {{"Used", 3.0}, {"Free", 1.0}};
    facet::DonutChart("donut small", two, {.centerText = "75 %", .diameter = 96.0f});
}

void GaugeSection()
{
    SectionHeader("Gauge");
    Paragraph("A value between a minimum and a maximum, with zones along the arc. A value past the range is drawn "
              "at the end with a mark, and the readout shows it as it is. Not interactive.");
    LabeledRow("Value");
    facet::Slider("##gauge value", &ui.gauge, -20.0f, 140.0f, {.format = "%.0f", .width = DemoWidth(240.0f)});
    static const facet::GaugeZone zones[] = {{70.0, 90.0, facet::Tone::Warning}, {90.0, 100.0, facet::Tone::Destructive}};
    LabeledRow("Arc, half circle");
    facet::Gauge("Placeholder load##arc", ui.gauge, 0.0, 100.0, {.zones = zones, .diameter = 120.0f});
    ImGui::SameLine();
    facet::Gauge("Placeholder load##half", ui.gauge, 0.0, 100.0,
                 {.shape = facet::GaugeShape::HalfCircle, .zones = zones, .diameter = 120.0f});
    ImGui::SameLine();
    facet::Gauge("Needle##needle", ui.gauge, 0.0, 100.0, {.format = "%.0f %%", .zones = zones, .needle = true, .diameter = 120.0f});
    LabeledRow("Edge cases");
    facet::Gauge("Past the range##over", 130.0, 0.0, 100.0, {.diameter = 96.0f});
    ImGui::SameLine();
    facet::Gauge("Empty range##flat", 5.0, 5.0, 5.0, {.diameter = 96.0f});
}

void MeterSection()
{
    SectionHeader("Level meter");
    Paragraph("One bar per channel that follows its value smoothly, with warning and clipping zones and a peak "
              "mark that holds, then falls back. Not interactive.");
    Note("Generated levels that move with the app's clock. Not measured audio.");
    const float time = static_cast<float>(ImGui::GetTime());
    float channels[4];
    for (int i = 0; i < 4; ++i)
        channels[i] = -30.0f + 26.0f * (0.5f + 0.5f * std::sin(time * (1.3f + 0.4f * static_cast<float>(i)) + static_cast<float>(i)));
    static const facet::GaugeZone zones[] = {{-12.0, -3.0, facet::Tone::Warning}, {-3.0, 0.0, facet::Tone::Destructive}};
    LabeledRow("Stereo");
    facet::LevelMeter("Generated##stereo", std::span<const float>(channels, 2), -60.0, 0.0,
                      {.zones = zones, .width = DemoWidth(320.0f)});
    LabeledRow("Segments");
    facet::LevelMeter("Generated##segments", std::span<const float>(channels, 2), -60.0, 0.0,
                      {.zones = zones, .segments = true, .width = DemoWidth(320.0f)});
    LabeledRow("Vertical");
    facet::LevelMeter("Four channels##vertical", channels, -60.0, 0.0,
                      {.orientation = facet::Orientation::Vertical, .zones = zones, .height = 96.0f});
}

void FrameTimeSection()
{
    const GeneratedSeries& data = Data();
    SectionHeader("Frame time graph");
    Paragraph("A rolling graph of frame times with the frame rate, the minimum, average and maximum, and the "
              "target line; frames over the target are marked. Not interactive.");
    LabeledRow("This app");
    facet::FrameTimeGraph("frame times", {.width = DemoWidth(420.0f), .height = 96.0f});
    LabeledRow("Compact");
    facet::FrameTimeGraph("frame times compact", {.compact = true, .width = DemoWidth(260.0f)});
    LabeledRow("Given times");
    facet::FrameTimeGraph("frame times given", data.frames, {.width = DemoWidth(420.0f), .height = 96.0f});
    Note("Given times: generated around 60 frames per second with a slow frame now and then.");
}

void BrushSection(facet::demo::PageCtx& ctx)
{
    const GeneratedSeries& data = Data();
    SectionHeader("Chart range brush");
    Paragraph("An overview with a window over it that selects the x range of the chart above. Drag an edge or "
              "the window; with keyboard focus on an edge the arrow keys move it (Shift fine, Ctrl coarse). With "
              "an edge focused, or Alt held, the wheel zooms around the pointer.");
    Note("Generated: a random walk from a fixed seed, %d samples. Not measured data.", kLongSamples);
    const facet::ChartSeries walk[] = {{.label = "Generated walk", .values = data.long_}};
    facet::LineChart("brushed", walk,
                     {.xAxis = {.min = ui.low, .max = ui.high}, .crosshair = true, .height = 120.0f});
    if (facet::ChartRangeBrush("Visible range##brush", &ui.low, &ui.high, 0.0, kLongSamples - 1.0, walk,
                               {.minGap = 20.0}))
        ++ui.brushWrites;
    ctx.Probe("charts.brush");
    const ImGuiID brushId = ImGui::GetID("Visible range##brush");
    for (int i = 0; i < 2; ++i)
    {
        const ImRect grip = facet::internal::ChartBrushHandleRect(brushId, i);
        facet::demo::ProbeRect(ctx, i == 0 ? "charts.brush.low" : "charts.brush.high",
                               facet::internal::ChartBrushHandleId(brushId, i), grip.Min, grip.Max);
    }
    Note("Window: %.0f to %.0f.", ui.low, ui.high);
    LabeledRow("Read-only");
    facet::ChartRangeBrush("Read-only range##brush locked", &ui.lockedLow, &ui.lockedHigh, 0.0, kLongSamples - 1.0, walk,
                           {.height = 32.0f, .readOnly = true});
}

} // namespace

FACET_DEMO_PAGE("charts", "Charts", "Widgets", 120, "default", "readout", "states", "disabled")
{
    facet::demo::PageHeader("Charts",
                            "Sparklines, line, bar and donut charts, gauges, level meters, frame time graphs, "
                            "legends and the range brush. All charts share one data form (ChartSeries) and one axis "
                            "options struct (ChartAxis). Every series on this page is generated, not measured.");
    facet::demo::ReadyFamilyNote("charts");

    const bool disabled = Is(ctx, kDisabled);
    if (disabled)
        ImGui::BeginDisabled();
    SparklineSection(ctx);
    LegendSection(ctx);
    LineChartSection(ctx);
    BarChartSection(ctx);
    DonutSection(ctx);
    GaugeSection();
    MeterSection();
    FrameTimeSection();
    BrushSection(ctx);
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

    // Legend: the mouse flips an entry, the keys flip the focused one, Alt shows one alone.
    test.Click("charts.legend.1");
    test.Expect("a click hides series 2", [] { return ui.legend[1].hidden && !ui.legend[0].hidden; });
    test.Click("charts.legend.1");
    test.Expect("a second click shows it again", [] { return !ui.legend[1].hidden; });
    test.KeyPress(ImGuiKey_Tab);  // after a click, the first Tab shows the focus on the entry
    test.ExpectFocused("charts.legend.1");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space hides the focused entry", [] { return ui.legend[1].hidden; });
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("charts.legend.0");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter hides the first entry", [] { return ui.legend[0].hidden; });
    test.KeyDown(ImGuiMod_Alt);
    test.Click("charts.legend.0");
    test.KeyUp(ImGuiMod_Alt);
    test.Expect("Alt and a click show series 1 alone",
                [] { return !ui.legend[0].hidden && ui.legend[1].hidden && ui.legend[2].hidden; });
    test.KeyPress(ImGuiKey_Tab);  // after the click, the first Tab shows the focus on the entry
    test.ExpectFocused("charts.legend.0");
    test.KeyPress(ImGuiKey_Space | ImGuiMod_Alt);
    test.Expect("Alt and Space on the entry shown alone show every entry",
                [] { return !ui.legend[0].hidden && !ui.legend[1].hidden && !ui.legend[2].hidden; });
    test.Expect("each press was reported once", [] { return ui.legendWrites == 6; });
    test.Click("charts.legendReadOnly.1");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("the read-only legend writes nothing",
                [] { return ui.legendReadOnly[1].hidden && !ui.legendReadOnly[0].hidden; });
    test.ExpectFocused("charts.legendReadOnly.1");

    // Line chart: the pointer picks the nearest x; the keys step from there.
    test.Reveal("charts.line");
    test.WaitSeconds(0.4f);  // the smooth scroll settles
    test.MouseMove(T::Item("charts.line", 0.5f, 0.5f));
    test.Wait(2);
    test.Expect("hovering the middle of the line chart reads a middle sample", [] {
        const int index = facet::internal::ChartReadoutIndex(ui.lineId);
        return index > kLineSamples / 4 && index < kLineSamples * 3 / 4;
    });
    test.Click("charts.line");
    test.KeyPress(ImGuiKey_Tab);  // after the click, the first Tab shows the focus on the chart
    test.ExpectFocused("charts.line");
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home reads the first sample", [] { return facet::internal::ChartReadoutIndex(ui.lineId); }, 0.0, 0.0);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right twice reads the third sample", [] { return facet::internal::ChartReadoutIndex(ui.lineId); }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End reads the last sample", [] { return facet::internal::ChartReadoutIndex(ui.lineId); },
                    kLineSamples - 1.0, 0.0);
    test.ExpectFocused("charts.line");
    test.Click("charts.lineLook");
    test.Expect("the look control switches the chart to areas", [] { return ui.look == 1; });

    // Bar chart: the pointer and the keys pick a category.
    test.Reveal("charts.bars");
    test.WaitSeconds(0.4f);
    test.MouseMove(T::Item("charts.bars", 0.6f, 0.5f));
    test.Wait(2);
    test.Expect("hovering the bar chart picks a category", [] { return facet::internal::ChartReadoutIndex(ui.barId) >= 0; });
    test.Click("charts.bars");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("charts.bars");
    test.KeyPress(ImGuiKey_Home);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Home and Right pick the second category", [] { return facet::internal::ChartReadoutIndex(ui.barId); }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End picks the last category", [] { return facet::internal::ChartReadoutIndex(ui.barId); },
                    kCategories - 1.0, 0.0);
    test.Click("charts.barStacked");
    test.Expect("the toggle stacks the bars", [] { return ui.stacked; });

    // Donut: the pointer over the first slice, then the keys.
    test.Reveal("charts.donut");
    test.WaitSeconds(0.4f);
    test.MouseMove(T::Item("charts.donutRing", 0.56f, 0.06f));
    test.Wait(2);
    test.ExpectNear("the pointer at the top of the ring picks the first slice",
                    [] { return facet::internal::ChartReadoutIndex(ui.donutId); }, 0.0, 0.0);
    test.Click("charts.donutRing");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("charts.donut");
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End picks the merged slice", [] { return facet::internal::ChartReadoutIndex(ui.donutId); }, 4.0, 0.0);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("Left moves back a slice", [] { return facet::internal::ChartReadoutIndex(ui.donutId); }, 3.0, 0.0);

    // Range brush: drag the high edge, keep the edges apart, step the low edge by keys.
    test.Reveal("charts.brush");
    test.WaitSeconds(0.4f);
    test.Drag({T::Item("charts.brush.high"), T::Item("charts.brush", 0.5f, 0.5f)}, 0.3f);
    test.Expect("dragging the high edge moves it to the middle",
                [] { return ui.high > kLongSamples * 0.4 && ui.high < kLongSamples * 0.6 && ui.low == 400.0; });
    test.Drag({T::Item("charts.brush.low"), T::Item("charts.brush", 0.95f, 0.5f)}, 0.3f);
    test.Expect("the low edge stops one minimum gap before the high edge",
                [] { return std::abs(ui.high - ui.low - 20.0) < 1e-6; });
    test.MouseMove(T::Item("charts.brush", 0.0f, 0.5f).Offset(6.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a press left of the window moves the nearer edge there", [] { return ui.low; }, 0.0,
                    kLongSamples * 0.05);
    test.Click("charts.brush.low");
    test.KeyPress(ImGuiKey_Tab);  // after the click, the first Tab shows the focus on the edge
    test.ExpectFocused("charts.brush.low");
    test.Do([] { ui.brushWrites = 0; });
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home moves the low edge to the start", [] { return ui.low; }, 0.0, 1e-9);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right steps it a hundredth of the range", [] { return ui.low; }, (kLongSamples - 1.0) / 100.0, 1e-6);
    test.Expect("each key edit was reported", [] { return ui.brushWrites == 2; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("charts.brush.high");
}
