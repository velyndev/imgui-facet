// Chart core page: generated functions drawn with the chart core (scales, nice ticks, labels,
// grid lines, a line and an area), a crosshair that follows the mouse or the arrow keys, a
// logarithmic axis, a 100000 sample series drawn through decimation, and the series palette
// with the contrast of each color.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/chart_core.h"
#include "internal/row_layout.h"
#include "pages/pages.h"

namespace
{

using namespace facet::internal;

// Chart heights in multiples of the font size, like the other pages size their samples.
constexpr float kChartHeightEm = 13.0f;
// Samples of the generated functions.
constexpr int kWaveSamples = 241;
constexpr int kGrowthSamples = 161;
constexpr int kLongSamples = 100000;

struct ChartCursor
{
    int index = -1;        // the sample under the crosshair, -1 for none
    bool keyboard = false; // set by the arrow keys; the mouse takes over when it moves
};

struct ChartPageState
{
    bool logAxis = true;
    bool showEmpty = false;
    ChartCursor wave;
    ChartCursor growth;
    ChartCursor longCursor;
    int longPoints = 0;
    double longMilliseconds = 0.0;
};

ChartPageState page;

// Read by the selftest.
Crosshair waveCrosshair;
Crosshair growthCrosshair;
bool emptyShown = false;

struct GeneratedData
{
    float waveX[kWaveSamples];
    float sine[kWaveSamples];
    float damped[kWaveSamples];
    float growthX[kGrowthSamples];
    float fast[kGrowthSamples];
    float slow[kGrowthSamples];
    std::vector<float> walk;
    bool walkSorted = true;
    std::vector<ImVec2> points;  // decimation output, grown to the widest plot seen
};

GeneratedData& Data()
{
    static GeneratedData data = [] {
        GeneratedData d{};
        const double twoPi = 2.0 * std::numbers::pi;
        for (int i = 0; i < kWaveSamples; ++i)
        {
            const double x = 2.0 * twoPi * i / (kWaveSamples - 1);
            d.waveX[i] = static_cast<float>(x);
            d.sine[i] = static_cast<float>(std::sin(x));
            d.damped[i] = static_cast<float>(std::exp(-x / 8.0) * std::cos(2.0 * x));
        }
        for (int i = 0; i < kGrowthSamples; ++i)
        {
            const double x = 8.0 * i / (kGrowthSamples - 1);
            d.growthX[i] = static_cast<float>(x);
            d.fast[i] = static_cast<float>(std::pow(10.0, x / 2.0));
            d.slow[i] = static_cast<float>(std::pow(10.0, x / 4.0));
        }
        // A random walk from a fixed seed, so every run draws the same series.
        d.walk.resize(kLongSamples);
        std::uint32_t seed = 2024u;
        float value = 0.0f;
        for (float& sample : d.walk)
        {
            seed = seed * 1664525u + 1013904223u;
            value += static_cast<float>(static_cast<int>(seed >> 9) % 2001 - 1000) / 1000.0f;
            sample = value;
        }
        d.walkSorted = IsSortedByX(SeriesView{.y = d.walk.data(), .count = kLongSamples});
        return d;
    }();
    return data;
}

struct ChartSpec
{
    const char* id = "";
    std::span<const SeriesView> series;
    ScaleKind yKind = ScaleKind::Linear;
    bool areaUnderFirst = false;
    bool sortedByX = true;
};

struct ChartOutcome
{
    Crosshair crosshair;
    int drawnPoints = 0;
};

// Writes "x 3.142, y 0.7071" style values: four significant digits.
void ValueText(char* buffer, std::size_t size, double x, double y)
{
    std::snprintf(buffer, size, "x %.4g, y %.4g", x, y);
}

// Moves the cursor with the arrow keys, Home and End while the plot item is focused. The keys
// are routed to the plot (ImGui::Shortcut with its id), so keyboard navigation does not move
// the focus away at the same time.
void KeyboardCursor(ImGuiID id, ChartCursor& cursor, int count)
{
    if (!ImGui::IsItemFocused() || count <= 0)
        return;
    const ImGuiInputFlags repeat = ImGuiInputFlags_Repeat | ImGuiInputFlags_RouteFocused;
    int index = cursor.index < 0 ? 0 : cursor.index;
    bool moved = false;
    if (ImGui::Shortcut(ImGuiKey_LeftArrow, repeat, id))
    {
        index = std::max(0, index - 1);
        moved = true;
    }
    if (ImGui::Shortcut(ImGuiKey_RightArrow, repeat, id))
    {
        index = std::min(count - 1, index + 1);
        moved = true;
    }
    if (ImGui::Shortcut(ImGuiKey_Home, ImGuiInputFlags_RouteFocused, id))
    {
        index = 0;
        moved = true;
    }
    if (ImGui::Shortcut(ImGuiKey_End, ImGuiInputFlags_RouteFocused, id))
    {
        index = count - 1;
        moved = true;
    }
    if (moved)
    {
        cursor.index = index;
        cursor.keyboard = true;
    }
}

ChartOutcome DrawChart(const ChartSpec& spec, ChartCursor& cursor, const facet::demo::PageCtx& ctx,
                       const char* probe)
{
    const facet::Theme& theme = facet::T();
    const facet::Colors& c = theme.colors;
    ChartOutcome outcome;
    GeneratedData& data = Data();

    // The card and the plot inside it, leaving room for the y labels and the x labels.
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetFontSize() * kChartHeightEm;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImRect card(origin, ImVec2(origin.x + width, origin.y + height));
    const float padding = facet::Px(theme.spacing.md);
    const facet::TextStyleSpec& labelStyle = theme.type[facet::TextStyle::Caption];
    const float labelSize = facet::Px(labelStyle.size);
    ImFont* labelFont = facet::WeightFont(labelStyle.weight);
    if (labelFont == nullptr)
        labelFont = ImGui::GetFont();
    const float gap = facet::Px(theme.spacing.sm);

    // Y range and ticks first: the widest y label decides where the plot starts.
    const Range yData = SeriesRangeY(spec.series, spec.yKind);
    const float plotTop = card.Min.y + padding;
    const float plotBottom = card.Max.y - padding - labelSize - gap;
    const int yTarget = TickTargetCount(plotBottom - plotTop, labelSize, facet::Px(theme.spacing.xl));
    // Linear axes keep the padded data range with ticks inside it, so no space goes to empty
    // rounding; logarithmic axes round out to whole decades.
    Range yRange = PadRange(yData, spec.yKind == ScaleKind::Logarithmic ? 0.0 : 0.05, spec.yKind);
    if (spec.yKind == ScaleKind::Logarithmic)
        yRange = NiceRange(yRange, yTarget, spec.yKind);
    Tick yTicks[kMaxTicks * 4];
    const TickSet ySet = spec.yKind == ScaleKind::Logarithmic ? LogTicks(yRange.min, yRange.max, yTarget, yTicks)
                                                               : LinearTicks(yRange.min, yRange.max, yTarget, yTicks);
    const int yCount = std::min(ySet.count, kMaxTicks * 4);
    float labelWidth = 0.0f;
    char text[48];
    for (int i = 0; i < yCount; ++i)
    {
        if (!yTicks[i].major)
            continue;
        FormatTick(yTicks[i].value, ySet, text, sizeof(text));
        labelWidth = std::max(labelWidth, labelFont->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, text).x);
    }
    const ImRect plot = SnapPlotRect(
        ImRect(ImVec2(card.Min.x + padding + labelWidth + gap, plotTop), ImVec2(card.Max.x - padding, plotBottom)));

    // The plot is one item: hover and keyboard focus read samples. EnableNav makes it a Tab
    // stop; an invisible button is left out of keyboard navigation without it.
    ImGui::SetCursorScreenPos(plot.Min);
    ImGui::InvisibleButton(spec.id, ImVec2(std::max(plot.GetWidth(), 1.0f), std::max(plot.GetHeight(), 1.0f)),
                           ImGuiButtonFlags_EnableNav);
    const ImGuiID plotId = ImGui::GetItemID();
    const bool hovered = ImGui::IsItemHovered();
    ctx.Probe(probe);
    const SeriesView& first = spec.series[0];
    KeyboardCursor(plotId, cursor, first.count);
    ImGui::SetCursorScreenPos(ImVec2(card.Min.x, card.Max.y));
    ImGui::Dummy(ImVec2(width, 0.0f));

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    facet::FillRect(drawList, card.Min, card.Max, facet::ToU32(c.bgSecondary), facet::Px(theme.radii.card));

    const Range xData = SeriesRangeX(first);
    const Scale xScale = MakeScale(ScaleKind::Linear, xData.min, xData.max, plot.Min.x, plot.Max.x);
    const Scale yScale = MakeScale(spec.yKind, yRange.min, yRange.max, plot.Max.y, plot.Min.y);

    // Grid lines and labels.
    const ImU32 gridColor = facet::ToU32(c.separator);
    const ImU32 labelColor = facet::ToU32(c.labelSecondary);
    float yLines[kMaxTicks * 4];
    int yLineCount = 0;
    for (int i = 0; i < yCount; ++i)
    {
        const float y = yScale.ToPixel(yTicks[i].value);
        if (yTicks[i].major)
        {
            yLines[yLineCount++] = y;
            FormatTick(yTicks[i].value, ySet, text, sizeof(text));
            const float w = labelFont->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, text).x;
            drawList->AddText(labelFont, labelSize, ImVec2(plot.Min.x - gap - w, std::round(y - 0.5f * labelSize)),
                              labelColor, text);
        }
        else
        {
            // Minor ticks of a logarithmic axis: a short mark at the edge of the plot.
            facet::HairlineH(drawList, plot.Min.x, plot.Min.x + facet::Px(theme.spacing.xs), y, gridColor);
        }
    }
    const float xLabelWidth = labelFont->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, "00.0").x;
    const int xTarget = TickTargetCount(plot.GetWidth(), xLabelWidth, facet::Px(theme.spacing.xxl));
    Tick xTicks[kMaxTicks * 4];
    const TickSet xSet = LinearTicks(xScale.domainMin, xScale.domainMax, xTarget, xTicks);
    float xLines[kMaxTicks * 4];
    const int xCount = std::min(xSet.count, kMaxTicks * 4);
    for (int i = 0; i < xCount; ++i)
    {
        const float x = xScale.ToPixel(xTicks[i].value);
        xLines[i] = x;
        FormatTick(xTicks[i].value, xSet, text, sizeof(text));
        const float w = labelFont->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, text).x;
        const float left = std::clamp(x - 0.5f * w, card.Min.x + padding, card.Max.x - padding - w);
        drawList->AddText(labelFont, labelSize, ImVec2(std::round(left), plot.Max.y + gap), labelColor, text);
    }
    DrawGridLines(drawList, plot, std::span<const float>(xLines, xCount), std::span<const float>(yLines, yLineCount),
                  gridColor);

    // The series: decimated, the first one with its area when asked for.
    const float lineWidth = 2.0f * facet::HairlineThickness();  // two hairlines: data reads above the grid
    const int capacity = DecimatedCapacity(plot.GetWidth());
    if (static_cast<int>(data.points.size()) < capacity)
        data.points.resize(static_cast<std::size_t>(capacity));
    for (int s = static_cast<int>(spec.series.size()) - 1; s >= 0; --s)
    {
        const ImVec4 color = SeriesColor(c, s);
        const int n = DecimatePolyline(spec.series[static_cast<std::size_t>(s)], xScale, yScale, data.points,
                                       spec.sortedByX);
        outcome.drawnPoints += n;
        const std::span<const ImVec2> points(data.points.data(), static_cast<std::size_t>(n));
        if (s == 0 && spec.areaUnderFirst)
        {
            const float baseline = std::clamp(yScale.ToPixel(0.0), plot.Min.y, plot.Max.y);
            DrawArea(drawList, points, baseline, facet::ToU32(facet::WithAlpha(color, c.accentSubtle.w)), plot);
        }
        DrawPolyline(drawList, points, facet::ToU32(color), lineWidth, plot);
    }

    // The crosshair: the mouse picks the nearest sample when it moves; the arrow keys step.
    const ImGuiIO& io = ImGui::GetIO();
    if (hovered && (!cursor.keyboard || io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f))
    {
        const Crosshair hover = HoverCrosshair(first, xScale, yScale, io.MousePos, plot, spec.sortedByX);
        cursor.index = hover.visible ? hover.index : cursor.index;
        cursor.keyboard = false;
    }
    const bool focused = ImGui::GetFocusID() == plotId;
    const bool show = cursor.index >= 0 && cursor.index < first.count && (hovered || focused || ctx.capture);
    if (show)
    {
        Crosshair& crosshair = outcome.crosshair;
        crosshair.visible = true;
        crosshair.index = cursor.index;
        crosshair.valueX = first.X(cursor.index);
        crosshair.valueY = first.Y(cursor.index);
        crosshair.point = ImVec2(xScale.ToPixel(crosshair.valueX), yScale.ToPixel(crosshair.valueY));
        crosshair.lineX = std::floor(crosshair.point.x);
        facet::HairlineV(drawList, crosshair.lineX, plot.Min.y, plot.Max.y, labelColor);
        const float radius = facet::Px(theme.spacing.xs);
        drawList->AddCircleFilled(crosshair.point, radius + facet::HairlineThickness(), facet::ToU32(c.bgSecondary));
        drawList->AddCircleFilled(crosshair.point, radius, facet::ToU32(SeriesColor(c, 0)));
    }
    ImGui::RenderNavCursor(plot, plotId);
    return outcome;
}

void Readout(const ChartOutcome& outcome, const char* series)
{
    if (!outcome.crosshair.visible)
    {
        facet::demo::Note("Hover the chart, or focus it with Tab and use the arrow keys, Home and End, to read %s.",
                          series);
        return;
    }
    char values[64];
    ValueText(values, sizeof(values), outcome.crosshair.valueX, outcome.crosshair.valueY);
    facet::demo::Note("Sample %d of %s: %s", outcome.crosshair.index, series, values);
}

void DrawEmptyChart(const facet::Theme& theme, const facet::demo::PageCtx& ctx)
{
    emptyShown = true;
    const facet::Colors& c = theme.colors;
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetFontSize() * kChartHeightEm;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    facet::FillRect(ImGui::GetWindowDrawList(), origin, ImVec2(origin.x + width, origin.y + height),
                    facet::ToU32(c.bgSecondary), facet::Px(theme.radii.card));
    const float padding = facet::Px(theme.spacing.xl);
    ImGui::SetCursorScreenPos(ImVec2(origin.x + padding, origin.y + padding));
    ImGui::BeginGroup();
    facet::IconGlyph(facet::Icon("image-off"), facet::Px(theme.spacing.xxl), facet::ToU32(c.labelSecondary));
    facet::Text(facet::TextStyle::Headline, "No samples to show");
    facet::demo::Paragraph("A chart without data keeps its size and says so instead of drawing empty axes.");
    if (facet::Button("Show the generated data"))
        page.showEmpty = false;
    ctx.Probe("chart.showData");
    ImGui::EndGroup();
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + height));
    ImGui::Dummy(ImVec2(width, 0.0f));
}

void DrawPalette(const facet::Theme& theme)
{
    const facet::Colors& c = theme.colors;
    facet::demo::SectionHeader("Series palette");
    facet::demo::Paragraph("Derived from the accent in OKLCH: the first color keeps the accent hue, the others "
                           "step around the hue circle so neighbouring series differ most, and each one is moved "
                           "in lightness until it reaches 3:1 on every surface a chart sits on. Below each swatch: "
                           "its series number and its lowest contrast on those surfaces.");
    const float side = ImGui::GetFrameHeight();
    const float gap = facet::Px(theme.spacing.md);
    const float right = facet::demo::ContentRight();
    const facet::TextStyleSpec& captionStyle = theme.type[facet::TextStyle::Caption];
    ImFont* font = facet::WeightFont(captionStyle.weight);
    if (font == nullptr)
        font = ImGui::GetFont();
    const float captionSize = facet::Px(captionStyle.size);
    const float cellWidth = std::max(side, font->CalcTextSizeA(captionSize, FLT_MAX, 0.0f, "8  10.0:1").x);
    for (int i = 0; i < kSeriesPaletteSize; ++i)
    {
        if (i > 0)
            facet::demo::SameLineIfFits(right, cellWidth, gap);
        const ImVec4 color = SeriesColor(c, i);
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(cellWidth, side + captionSize + facet::Px(theme.spacing.xs)));
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        facet::FillRect(drawList, pos, ImVec2(pos.x + side, pos.y + side), facet::ToU32(color),
                        facet::Px(theme.radii.element));
        char caption[32];
        std::snprintf(caption, sizeof(caption), "%d  %.1f:1", i + 1, MinSurfaceContrast(c, color));
        drawList->AddText(font, captionSize, ImVec2(pos.x, pos.y + side + facet::Px(theme.spacing.xs)),
                          facet::ToU32(c.labelSecondary), caption);
    }
}

} // namespace

FACET_DEMO_PAGE("chart-core", "Chart core", "Blocks", 100, "default", "crosshair", "linear", "empty", "samples")
{
    const facet::Theme& theme = facet::T();
    GeneratedData& data = Data();
    if (ctx.capture)
    {
        page.logAxis = ctx.captureState != 2;
        page.showEmpty = ctx.captureState == 3;
        const bool cross = ctx.captureState == 1;
        page.wave = {cross ? kWaveSamples / 3 : -1, cross};
        page.growth = {cross ? kGrowthSamples * 3 / 4 : -1, cross};
        page.longCursor = {};
    }
    emptyShown = false;

    facet::demo::PageHeader("Chart core",
                            "Scales, nice ticks and labels, hover snapping, decimation and the series colors "
                            "that the chart widgets share. Every series on this page is generated from a formula "
                            "or a fixed random seed; none of it is measured data.");

    facet::demo::SectionHeader("Line and area");
    facet::demo::Note("Generated: y = sin x as an area, y = e^(-x/8) cos 2x as a line.");
    facet::internal::BeginRow("Data");
    facet::Checkbox("Show the empty state", &page.showEmpty);
    ctx.Probe("chart.empty");
    facet::internal::EndRow();
    if (page.showEmpty)
    {
        DrawEmptyChart(theme, ctx);
        waveCrosshair = {};
    }
    else
    {
        const SeriesView wave[] = {SeriesView{.x = data.waveX, .y = data.sine, .count = kWaveSamples},
                                   SeriesView{.x = data.waveX, .y = data.damped, .count = kWaveSamples}};
        const ChartOutcome outcome =
            DrawChart({.id = "##wave", .series = wave, .areaUnderFirst = true}, page.wave, ctx, "chart.wave");
        waveCrosshair = outcome.crosshair;
        Readout(outcome, "the area series");
        if (ctx.captureState == 1)
            ctx.RevealInCapture();
    }
    facet::demo::Note("The data here is generated on the frame thread, so it is never loading and cannot fail; "
                      "the chart widgets take a DataState for data that can.");

    facet::demo::SectionHeader("Logarithmic axis");
    facet::demo::Note("Generated: y = 10^(x/2) and y = 10^(x/4). On a logarithmic axis both are straight lines.");
    facet::internal::BeginRow("Y axis");
    int axis = page.logAxis ? 1 : 0;
    ImGui::RadioButton("Linear", &axis, 0);
    facet::ItemCursor();
    ctx.Probe("chart.linear");
    ImGui::SameLine();
    ImGui::RadioButton("Logarithmic", &axis, 1);
    facet::ItemCursor();
    ctx.Probe("chart.log");
    facet::internal::EndRow();
    page.logAxis = axis == 1;
    {
        const SeriesView growth[] = {SeriesView{.x = data.growthX, .y = data.fast, .count = kGrowthSamples},
                                     SeriesView{.x = data.growthX, .y = data.slow, .count = kGrowthSamples}};
        const ChartOutcome outcome =
            DrawChart({.id = "##growth",
                       .series = growth,
                       .yKind = page.logAxis ? ScaleKind::Logarithmic : ScaleKind::Linear},
                      page.growth, ctx, "chart.growth");
        growthCrosshair = outcome.crosshair;
        Readout(outcome, "10^(x/2)");
        if (ctx.captureState == 2)
            ctx.RevealInCapture();
    }

    facet::demo::SectionHeader("100000 samples");
    facet::demo::Note("Generated: a random walk from a fixed seed. Decimation keeps the first, last, lowest and "
                      "highest sample of every pixel column, so the line covers the same pixels with a few "
                      "points per column.");
    {
        const SeriesView walk[] = {SeriesView{.y = data.walk.data(), .count = kLongSamples}};
        const auto start = std::chrono::steady_clock::now();
        const ChartOutcome outcome = DrawChart({.id = "##walk", .series = walk, .sortedByX = data.walkSorted},
                                               page.longCursor, ctx, "chart.walk");
        page.longMilliseconds =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        page.longPoints = outcome.drawnPoints;
        // Times differ from run to run, so capture runs, which must give the same image every
        // time, leave them out.
        if (ctx.capture)
            facet::demo::Note("%d samples drawn as %d points. Times are not shown in capture runs.", kLongSamples,
                              page.longPoints);
        else
            facet::demo::Note("%d samples drawn as %d points in %.2f ms; frame time %.1f ms.", kLongSamples,
                              page.longPoints, page.longMilliseconds, 1000.0 * ImGui::GetIO().DeltaTime);
    }

    DrawPalette(theme);
    if (ctx.captureState == 4)
        ctx.RevealInCapture(0.9f);
}

FACET_DEMO_SELFTEST("chart-core")
{
    using T = facet::demo::Target;
    test.Do([] {
        page = ChartPageState{};
        page.logAxis = true;
    });
    test.Wait(2);

    // The crosshair follows the mouse to the nearest sample.
    test.MouseMove(T::Item("chart.wave", 0.5f, 0.5f));
    test.Wait(1);
    test.Expect("hovering the middle of the plot snaps to a middle sample",
                [] { return waveCrosshair.visible && std::abs(waveCrosshair.index - kWaveSamples / 2) <= 2; });
    test.MouseMove(T::Item("chart.wave", 0.0f, 0.5f));
    test.Wait(1);
    test.ExpectNear("the left edge snaps to the first sample", [] { return waveCrosshair.index; }, 0.0, 0.0);
    test.MouseMove(T::Item("chart.wave", 0.25f, 0.2f));
    test.Wait(1);
    test.Expect("a quarter of the way snaps to a sample about a quarter in",
                [] { return std::abs(waveCrosshair.index - (kWaveSamples - 1) / 4) <= 2; });

    // Tab reaches the plot from the checkbox before it (clicked twice, so it has the focus and
    // the chart stays shown), and the arrow keys step through the samples there. After mouse
    // input the first Tab only shows the keyboard cursor on the focused checkbox.
    test.Click("chart.empty");
    test.Click("chart.empty");
    test.Expect("the checkbox clicked twice leaves the chart shown", [] { return !page.showEmpty; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("chart.empty");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("chart.wave");
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home goes to the first sample", [] { return waveCrosshair.index; }, 0.0, 0.0);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right twice moves the crosshair two samples on", [] { return waveCrosshair.index; }, 2.0, 0.0);
    test.ExpectFocused("chart.wave");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("Left moves it back", [] { return waveCrosshair.index; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End goes to the last sample", [] { return waveCrosshair.index; }, kWaveSamples - 1.0, 0.0);
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home goes back to the first sample", [] { return waveCrosshair.index; }, 0.0, 0.0);

    // The axis switch.
    test.Click("chart.linear");
    test.Expect("the growth chart switches to a linear axis", [] { return !page.logAxis; });
    test.Click("chart.log");
    test.Expect("and back to a logarithmic one", [] { return page.logAxis; });
    test.MouseMove(T::Item("chart.growth", 0.999f, 0.5f));
    test.Wait(1);
    // The pointer position is whole pixels, so in a narrow chart the last pixel column can sit
    // nearer the sample before the last one: within one sample of 10^4 (0.025 in log10).
    test.Expect("the right edge of the growth chart reads 10^4", [] {
        return growthCrosshair.visible && growthCrosshair.valueY > 0.0 &&
               std::abs(std::log10(growthCrosshair.valueY) - 4.0) <= 0.026;
    });

    // The long series is decimated to a few points per pixel column.
    test.Expect("100000 samples draw as at most four points per column",
                [] { return page.longPoints > 0 && page.longPoints < kLongSamples / 20; });

    // The empty state and its way back.
    test.Click("chart.empty");
    test.Expect("the empty state replaces the chart", [] { return emptyShown; });
    test.Click("chart.showData");
    test.Expect("Show the generated data brings the chart back", [] { return !emptyShown && !page.showEmpty; });
}
