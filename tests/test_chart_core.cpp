// Chart core (src/internal/chart_core.h): scales, ticks and labels, ranges, hover snapping,
// decimation, drawing and the series palette.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/core/color.h>
#include <facet/core/theme.h>

#include "internal/chart_core.h"
#include "support/session.h"

using namespace facet::internal;

namespace
{

std::vector<double> Values(double min, double max, int target, bool log = false)
{
    Tick ticks[kMaxTicks * 4];
    const TickSet set = log ? LogTicks(min, max, target, ticks) : LinearTicks(min, max, target, ticks);
    std::vector<double> values;
    for (int i = 0; i < std::min(set.count, kMaxTicks * 4); ++i)
    {
        if (ticks[i].major)
            values.push_back(ticks[i].value);
    }
    return values;
}

std::string Label(double value, const TickSet& set)
{
    char buffer[32];
    FormatTick(value, set, buffer, sizeof(buffer));
    return buffer;
}

TickSet SetOf(double min, double max, int target, bool log = false)
{
    Tick ticks[kMaxTicks * 4];
    return log ? LogTicks(min, max, target, ticks) : LinearTicks(min, max, target, ticks);
}

bool Near(double a, double b)
{
    return std::fabs(a - b) <= 1e-9 * std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
}

bool SameValues(const std::vector<double>& a, const std::vector<double>& b)
{
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (!Near(a[i], b[i]))
            return false;
    }
    return true;
}

// A deterministic random walk.
std::vector<float> RandomWalk(int count, std::uint32_t seed)
{
    std::vector<float> values(static_cast<std::size_t>(count));
    float value = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        value += static_cast<float>(static_cast<int>(seed >> 9) % 2001 - 1000) / 1000.0f;
        values[static_cast<std::size_t>(i)] = value;
    }
    return values;
}

float OklabDistance(const ImVec4& a, const ImVec4& b)
{
    const facet::Oklab la = facet::ToOklab(a);
    const facet::Oklab lb = facet::ToOklab(b);
    return std::sqrt((la.L - lb.L) * (la.L - lb.L) + (la.a - lb.a) * (la.a - lb.a) + (la.b - lb.b) * (la.b - lb.b));
}

} // namespace

TEST_CASE("Nice steps are 1, 2 or 5 times a power of ten")
{
    CHECK(NiceStep(10.0, 6) == doctest::Approx(2.0));
    CHECK(NiceStep(10.0, 11) == doctest::Approx(1.0));
    CHECK(NiceStep(1.0, 5) == doctest::Approx(0.5));
    CHECK(NiceStep(7.0, 2) == doctest::Approx(10.0));
    CHECK(NiceStep(0.03, 4) == doctest::Approx(0.01));
    CHECK(NiceStep(4.5e6, 6) == doctest::Approx(1e6));
    CHECK(NiceStep(0.0, 5) == 0.0);
    CHECK(NiceStep(-1.0, 5) == 0.0);
    CHECK(NiceStep(NAN, 5) == 0.0);

    CHECK(TickTargetCount(300.0f, 40.0f, 16.0f) == 5);
    CHECK(TickTargetCount(10.0f, 40.0f, 16.0f) == 2);
    CHECK(TickTargetCount(100000.0f, 10.0f, 2.0f) == kMaxTicks);
    CHECK(TickTargetCount(0.0f, 40.0f, 16.0f) == 2);
}

TEST_CASE("Linear ticks for known ranges")
{
    CHECK(SameValues(Values(0.0, 10.0, 6), {0, 2, 4, 6, 8, 10}));
    CHECK(SameValues(Values(0.0, 100.0, 11), {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100}));
    CHECK(SameValues(Values(0.0, 1.0, 5), {0, 0.5, 1}));
    CHECK(SameValues(Values(-3.7, 12.2, 8), {0, 5, 10}));
    CHECK(SameValues(Values(0.1, 0.35, 6), {0.1, 0.15, 0.2, 0.25, 0.3, 0.35}));
    CHECK(SameValues(Values(-1.0, 1.0, 3), {-1, 0, 1}));
    CHECK(SameValues(Values(10.0, 0.0, 6), {0, 2, 4, 6, 8, 10}));  // swapped bounds
    // Never more ticks than asked for.
    for (int target = 2; target <= 12; ++target)
    {
        for (const double span : {0.37, 1.0, 9.99, 123.0, 4.2e5})
            CHECK(static_cast<int>(Values(0.0, span, target).size()) <= target);
    }
    // An empty range has one tick.
    CHECK(SameValues(Values(3.0, 3.0, 5), {3}));
}

TEST_CASE("Tick labels print only the digits the step needs")
{
    CHECK(StepDecimals(1.0) == 0);
    CHECK(StepDecimals(20.0) == 0);
    CHECK(StepDecimals(0.5) == 1);
    CHECK(StepDecimals(0.2) == 1);
    CHECK(StepDecimals(0.1) == 1);
    CHECK(StepDecimals(0.05) == 2);
    CHECK(StepDecimals(0.001) == 3);

    const TickSet whole = SetOf(0.0, 10.0, 6);
    CHECK(Label(4.0, whole) == "4");
    const TickSet halves = SetOf(0.0, 1.0, 5);
    CHECK(Label(0.5, halves) == "0.5");
    CHECK(Label(1.0, halves) == "1.0");
    const TickSet twentieths = SetOf(0.1, 0.35, 6);
    CHECK(Label(0.15, twentieths) == "0.15");
    CHECK(Label(0.1 + 0.2, twentieths) == "0.30");  // floating point noise is not printed
    // -0 never shows a sign.
    CHECK(Label(-0.0, whole) == "0");
    CHECK(Label(-1e-12, halves) == "0.0");
    CHECK(Label(-0.04, halves) == "0.0");  // rounds to zero: no "-0.0"
    // Large and tiny values use exponents with only the needed mantissa digits.
    const TickSet millions = SetOf(0.0, 5e6, 6);
    CHECK(Label(2e6, millions) == "2e6");
    CHECK(Label(0.0, millions) == "0");
    const TickSet halfMillions = SetOf(0.0, 3e6, 7);
    CHECK(Label(2.5e6, halfMillions) == "2.5e6");
    const TickSet tiny = SetOf(0.0, 5e-8, 6);
    CHECK(Label(3e-8, tiny) == "3e-8");

    // The returned length is the full length, also when cut.
    char cut[3];
    CHECK(FormatTick(12345.0, whole, cut, sizeof(cut)) == 5);
    CHECK(std::string(cut) == "12");
}

TEST_CASE("Logarithmic ticks: decades, minor ticks and labels")
{
    CHECK(SameValues(Values(1.0, 1000.0, 10, true), {1, 10, 100, 1000}));
    Tick ticks[kMaxTicks * 4];
    const TickSet set = LogTicks(1.0, 1000.0, 10, ticks);
    CHECK(set.logarithmic);
    CHECK(set.count == 4 + 3 * 8);  // 2 to 9 in each of the three full decades
    CHECK(ticks[1].value == doctest::Approx(2.0));
    CHECK_FALSE(ticks[1].major);
    CHECK(Label(1000.0, set) == "1000");
    const TickSet fractions = SetOf(0.01, 10.0, 10, true);
    CHECK(Label(0.01, fractions) == "0.01");
    CHECK(Label(0.1, fractions) == "0.1");
    CHECK(Label(1e7, SetOf(1.0, 1e8, 10, true)) == "1e7");

    // Many decades: every n-th one, no minor ticks.
    Tick wide[kMaxTicks * 4];
    const TickSet many = LogTicks(1e-3, 1e9, 4, wide);
    CHECK(many.count == 4);
    CHECK(SameValues(Values(1e-3, 1e9, 4, true), {1e-3, 1e1, 1e5, 1e9}));
    // Less than a decade: plain linear steps.
    CHECK_FALSE(SetOf(2.0, 8.0, 4, true).logarithmic);
    CHECK(SameValues(Values(2.0, 8.0, 4, true), {2, 4, 6, 8}));
    // Values at or below 0 are raised to the floor instead of failing.
    CHECK(SetOf(0.0, 100.0, 20, true).count > 0);
}

TEST_CASE("Scales map values to pixels and back")
{
    const Scale linear = MakeScale(ScaleKind::Linear, -3.0, 7.0, 100.0f, 500.0f);
    CHECK(linear.ToPixel(-3.0) == doctest::Approx(100.0f));
    CHECK(linear.ToPixel(7.0) == doctest::Approx(500.0f));
    CHECK(linear.ToPixel(2.0) == doctest::Approx(300.0f));
    const Scale flipped = MakeScale(ScaleKind::Linear, 0.0, 1.0, 400.0f, 0.0f);  // a y axis
    CHECK(flipped.ToPixel(0.25) == doctest::Approx(300.0f));
    const Scale log = MakeScale(ScaleKind::Logarithmic, 1.0, 1e4, 0.0f, 400.0f);
    CHECK(log.ToPixel(10.0) == doctest::Approx(100.0f));
    CHECK(log.FromPixel(300.0f) == doctest::Approx(1000.0));
    for (const Scale& scale : {linear, flipped, log})
    {
        for (int i = 0; i <= 50; ++i)
        {
            const double value = scale.domainMin + (scale.domainMax - scale.domainMin) * i / 50.0;
            CHECK(scale.FromPixel(scale.ToPixel(value)) == doctest::Approx(value).epsilon(1e-5));
        }
    }
    // Degenerate input gives a usable scale.
    const Scale single = MakeScale(ScaleKind::Linear, 5.0, 5.0, 0.0f, 10.0f);
    CHECK(single.domainMin == doctest::Approx(4.5));
    CHECK(single.domainMax == doctest::Approx(5.5));
    const Scale swapped = MakeScale(ScaleKind::Linear, 9.0, 1.0, 0.0f, 10.0f);
    CHECK(swapped.domainMin == 1.0);
    const Scale bad = MakeScale(ScaleKind::Logarithmic, NAN, 3.0, 0.0f, 10.0f);
    CHECK(bad.domainMin == 1.0);
    CHECK(bad.domainMax == 10.0);
    const Scale nonPositive = MakeScale(ScaleKind::Logarithmic, -5.0, 100.0, 0.0f, 10.0f);
    CHECK(nonPositive.domainMin == kLogFloor);
    CHECK(std::isfinite(nonPositive.ToPixel(0.0)));
}

TEST_CASE("Data ranges skip missing samples and pad and round outward")
{
    const float ys[] = {3.0f, NAN, -2.0f, INFINITY, 7.5f};
    const SeriesView series{.y = ys, .count = 5};
    const Range range = SeriesRangeY(series);
    REQUIRE(range.valid);
    CHECK(range.min == -2.0);
    CHECK(range.max == 7.5);
    const Range xs = SeriesRangeX(series);
    CHECK(xs.min == 0.0);
    CHECK(xs.max == 4.0);
    CHECK_FALSE(SeriesRangeY(SeriesView{}).valid);

    const Range padded = PadRange({0.0, 10.0, true}, 0.1, ScaleKind::Linear);
    CHECK(padded.min == doctest::Approx(-1.0));
    CHECK(padded.max == doctest::Approx(11.0));
    const Range single = PadRange({5.0, 5.0, true}, 0.1, ScaleKind::Linear);
    CHECK(single.min == doctest::Approx(4.4));
    CHECK(single.max == doctest::Approx(5.6));
    const Range logPadded = PadRange({1.0, 100.0, true}, 0.5, ScaleKind::Logarithmic);
    CHECK(logPadded.min == doctest::Approx(0.1));
    CHECK(logPadded.max == doctest::Approx(1000.0));
    CHECK_FALSE(PadRange({}, 0.1, ScaleKind::Linear).valid);

    const Range nice = NiceRange({-3.7, 12.2, true}, 8);
    CHECK(nice.min == doctest::Approx(-5.0));
    CHECK(nice.max == doctest::Approx(15.0));
    const Range exact = NiceRange({0.0, 10.0, true}, 6);
    CHECK(exact.min == 0.0);
    CHECK(exact.max == doctest::Approx(10.0));
    const Range decades = NiceRange({3.0, 700.0, true}, 5, ScaleKind::Logarithmic);
    CHECK(decades.min == doctest::Approx(1.0));
    CHECK(decades.max == doctest::Approx(1000.0));

    // Interleaved data through the stride.
    struct Sample
    {
        float x;
        float y;
    };
    const Sample samples[] = {{1.0f, 10.0f}, {2.0f, -4.0f}, {5.0f, 6.0f}};
    const SeriesView interleaved{.x = &samples[0].x, .y = &samples[0].y, .count = 3,
                                 .stride = static_cast<int>(sizeof(Sample))};
    CHECK(interleaved.X(2) == 5.0f);
    CHECK(SeriesRangeY(interleaved).min == -4.0);
}

TEST_CASE("Hover snaps to the nearest sample by x")
{
    const float xs[] = {0.0f, 1.0f, 2.0f, 4.0f, 8.0f};
    float ys[] = {0.0f, 1.0f, 4.0f, 16.0f, 64.0f};
    const SeriesView series{.x = xs, .y = ys, .count = 5};
    REQUIRE(IsSortedByX(series));
    CHECK(NearestSampleByX(series, 2.9, true) == 2);
    CHECK(NearestSampleByX(series, 3.0, true) == 2);  // a tie goes to the lower index
    CHECK(NearestSampleByX(series, 3.1, true) == 3);
    CHECK(NearestSampleByX(series, -5.0, true) == 0);
    CHECK(NearestSampleByX(series, 100.0, true) == 4);
    CHECK(NearestSampleByX(series, NAN, true) == -1);
    CHECK(NearestSampleByX(SeriesView{}, 1.0, true) == -1);
    ys[3] = NAN;  // a missing sample is passed over
    CHECK(NearestSampleByX(series, 4.1, true) == 2);
    CHECK(NearestSampleByX(series, 4.1, false) == 2);
    ys[3] = 16.0f;

    const float unsortedX[] = {5.0f, 1.0f, 3.0f};
    const float unsortedY[] = {1.0f, 2.0f, 3.0f};
    const SeriesView unsorted{.x = unsortedX, .y = unsortedY, .count = 3};
    CHECK_FALSE(IsSortedByX(unsorted));
    CHECK(NearestSampleByX(unsorted, 2.1, false) == 2);
    CHECK(NearestSampleByX(unsorted, 4.5, false) == 0);

    // Binary search and scan agree on a long sorted series.
    std::vector<float> longX(5000);
    std::vector<float> longY(5000, 1.0f);
    float x = 0.0f;
    std::uint32_t seed = 7;
    for (float& value : longX)
    {
        seed = seed * 1664525u + 1013904223u;
        x += static_cast<float>(seed >> 24) / 64.0f;  // gaps from 0 to 4, some equal
        value = x;
    }
    const SeriesView longSeries{.x = longX.data(), .y = longY.data(), .count = 5000};
    REQUIRE(IsSortedByX(longSeries));
    for (int q = 0; q < 1000; ++q)
    {
        const double at = x * q / 999.0 + 0.37;
        CHECK(NearestSampleByX(longSeries, at, true) == NearestSampleByX(longSeries, at, false));
    }
}

TEST_CASE("The crosshair follows the mouse inside the plot")
{
    const float ys[] = {1.0f, 3.0f, 2.0f, 5.0f};
    const SeriesView series{.y = ys, .count = 4};
    const ImRect plot(ImVec2(100.0f, 50.0f), ImVec2(400.0f, 250.0f));
    const Scale xs = MakeScale(ScaleKind::Linear, 0.0, 3.0, plot.Min.x, plot.Max.x);
    const Scale yScale = MakeScale(ScaleKind::Linear, 0.0, 5.0, plot.Max.y, plot.Min.y);
    const Crosshair hit = HoverCrosshair(series, xs, yScale, ImVec2(290.0f, 120.0f), plot, true);
    REQUIRE(hit.visible);
    CHECK(hit.index == 2);
    CHECK(hit.valueY == 2.0);
    CHECK(hit.point.x == doctest::Approx(300.0f));
    CHECK(hit.point.y == doctest::Approx(170.0f));
    CHECK(hit.lineX == 300.0f);
    CHECK_FALSE(HoverCrosshair(series, xs, yScale, ImVec2(90.0f, 120.0f), plot, true).visible);
    CHECK_FALSE(HoverCrosshair(SeriesView{}, xs, yScale, ImVec2(290.0f, 120.0f), plot, true).visible);
}

TEST_CASE("Decimation keeps every pixel column's extremes with at most four points per column")
{
    const int count = 100000;
    const std::vector<float> ys = RandomWalk(count, 12345u);
    const SeriesView series{.y = ys.data(), .count = count};
    const float width = 600.0f;
    const Scale xs = MakeScale(ScaleKind::Linear, 0.0, count - 1.0, 10.3f, 10.3f + width);
    const Range yRange = SeriesRangeY(series);
    const Scale yScale = MakeScale(ScaleKind::Linear, yRange.min, yRange.max, 400.0f, 0.0f);
    std::vector<ImVec2> points(static_cast<std::size_t>(DecimatedCapacity(width)));
    const int written = DecimatePolyline(series, xs, yScale, points, true);
    REQUIRE(written > 0);
    CHECK(written <= DecimatedCapacity(width));
    CHECK(written < count / 30);

    // Per column, the decimated points reach exactly the same lowest and highest pixel.
    std::map<int, std::pair<float, float>> full;
    std::map<int, std::pair<float, float>> reduced;
    const auto add = [](std::map<int, std::pair<float, float>>& columns, ImVec2 p) {
        const int column = static_cast<int>(std::floor(p.x));
        auto it = columns.find(column);
        if (it == columns.end())
            columns[column] = {p.y, p.y};
        else
            it->second = {std::min(it->second.first, p.y), std::max(it->second.second, p.y)};
    };
    for (int i = 0; i < count; ++i)
        add(full, ImVec2(xs.ToPixel(i), yScale.ToPixel(ys[static_cast<std::size_t>(i)])));
    for (int i = 0; i < written; ++i)
        add(reduced, points[static_cast<std::size_t>(i)]);
    CHECK(full.size() == reduced.size());
    for (const auto& [column, extremes] : full)
    {
        INFO("column ", column);
        REQUIRE(reduced.count(column) == 1);
        CHECK(reduced[column].first == extremes.first);
        CHECK(reduced[column].second == extremes.second);
    }
    // The ends are the series' ends, and the points keep the sample order.
    CHECK(points[0].y == yScale.ToPixel(ys.front()));
    CHECK(points[static_cast<std::size_t>(written - 1)].y == yScale.ToPixel(ys.back()));
    for (int i = 1; i < written; ++i)
        CHECK(points[static_cast<std::size_t>(i)].x >= points[static_cast<std::size_t>(i - 1)].x);
}

TEST_CASE("Decimation visits only the visible samples and copies unsorted series")
{
    const int count = 100000;
    const std::vector<float> ys = RandomWalk(count, 99u);
    const SeriesView series{.y = ys.data(), .count = count};
    // A zoomed view of samples 5000 to 5100 over 200 pixels: every sample has its own column.
    const Scale xs = MakeScale(ScaleKind::Linear, 5000.0, 5100.0, 0.0f, 200.0f);
    const Scale yScale = MakeScale(ScaleKind::Linear, -500.0, 500.0, 100.0f, 0.0f);
    std::vector<ImVec2> points(static_cast<std::size_t>(DecimatedCapacity(200.0f)));
    const int written = DecimatePolyline(series, xs, yScale, points, true);
    CHECK(written == 103);  // 101 visible samples and one on each side
    CHECK(points[0].x == doctest::Approx(-2.0f));

    // NaN samples are left out.
    std::vector<float> holes(ys.begin(), ys.begin() + 10);
    holes[4] = NAN;
    const SeriesView withHoles{.y = holes.data(), .count = 10};
    const Scale wide = MakeScale(ScaleKind::Linear, 0.0, 9.0, 0.0f, 900.0f);
    CHECK(DecimatePolyline(withHoles, wide, yScale, points, true) == 9);

    // An unsorted series is copied in order, as far as the storage holds.
    const float ux[] = {3.0f, 1.0f, 2.0f};
    const float uy[] = {1.0f, 2.0f, 3.0f};
    const SeriesView unsorted{.x = ux, .y = uy, .count = 3};
    ImVec2 two[2];
    CHECK(DecimatePolyline(unsorted, wide, yScale, two, false) == 2);
    CHECK(two[0].x == doctest::Approx(300.0f));
}

TEST_CASE("Long polylines, areas and grid lines draw into a draw list")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const std::vector<float> ys = RandomWalk(20000, 3u);
    std::vector<ImVec2> points(ys.size());
    for (std::size_t i = 0; i < ys.size(); ++i)
        points[i] = ImVec2(static_cast<float>(i) * 0.03f, 200.0f + ys[i]);
    int vertices = 0;
    int areaVertices = 0;
    bool crossingVertex = false;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImRect clip(ImVec2(0.0f, 0.0f), ImVec2(600.0f, 400.0f));
        const int before = drawList->VtxBuffer.Size;
        DrawPolyline(drawList, points, IM_COL32_WHITE, 2.0f, clip);
        vertices = drawList->VtxBuffer.Size - before;
        const ImVec2 crossing[] = {{0.0f, 10.0f}, {10.0f, -10.0f}, {20.0f, -5.0f}};
        const int beforeArea = drawList->VtxBuffer.Size;
        DrawArea(drawList, crossing, 0.0f, IM_COL32_WHITE, clip);
        areaVertices = drawList->VtxBuffer.Size - beforeArea;
        // The fill meets the baseline where the line crosses it, at x = 5.
        for (int v = beforeArea; v < drawList->VtxBuffer.Size; ++v)
        {
            const ImVec2 p = drawList->VtxBuffer[v].pos;
            crossingVertex = crossingVertex || (p.x == 5.0f && p.y == 0.0f);
        }
        const float gx[] = {100.5f, 700.0f};
        const float gy[] = {50.0f};
        DrawGridLines(drawList, clip, gx, gy, IM_COL32_WHITE);
    });
    session.Frame();
    CHECK(vertices > 20000);
    // The first segment crosses the baseline: two triangles; the second: a quad of two.
    CHECK(areaVertices == 4 * 3);
    CHECK(crossingVertex);
}

TEST_CASE("The series palette moves its lightness until custom surfaces are reached too")
{
    // Surfaces far from the presets: a mid grey in light, a light grey in dark.
    facet::Theme light = facet::Theme::Light();
    light.colors.bgTertiary = facet::Rgb(0xB8B8BC);
    facet::Theme dark = facet::Theme::Dark();
    dark.colors.bgElevatedLevels[2] = facet::Rgb(0x5A5A5E);
    for (facet::Theme* theme : {&light, &dark})
    {
        for (int hueStep = 0; hueStep < 24; ++hueStep)
        {
            theme->ApplyAccent(facet::FromOklch({0.6f, 0.15f, 15.0f * static_cast<float>(hueStep)}));
            for (int i = 0; i < kSeriesPaletteSize; ++i)
                CHECK(MinSurfaceContrast(theme->colors, SeriesColor(theme->colors, i)) >= 3.0f);
        }
    }
}

TEST_CASE("The series palette reaches 3:1 on every chart surface for 24 accent hues in both themes")
{
    float lowestDistance = 10.0f;
    for (int dark = 0; dark < 2; ++dark)
    {
        for (int hueStep = 0; hueStep < 24; ++hueStep)
        {
            facet::Theme theme = dark != 0 ? facet::Theme::Dark() : facet::Theme::Light();
            theme.ApplyAccent(facet::FromOklch({0.6f, 0.15f, 15.0f * static_cast<float>(hueStep)}));
            ImVec4 palette[2 * kSeriesPaletteSize];
            SeriesPalette(theme.colors, palette);
            for (int i = 0; i < 2 * kSeriesPaletteSize; ++i)
            {
                // Measured on the color as drawn: rounded to 8 bits per channel.
                const ImVec4 drawn = facet::FromU32(facet::ToU32(palette[i]));
                INFO("theme ", dark != 0 ? "dark" : "light", ", hue ", 15 * hueStep, ", series ", i);
                CHECK(MinSurfaceContrast(theme.colors, drawn) >= 3.0f);
            }
            for (int i = 0; i < kSeriesPaletteSize; ++i)
            {
                for (int j = i + 1; j < kSeriesPaletteSize; ++j)
                    lowestDistance = std::min(lowestDistance, OklabDistance(palette[i], palette[j]));
            }
            // The first series keeps the accent hue.
            const float accentHue = facet::ToOklch(theme.colors.accent).h;
            const float firstHue = facet::ToOklch(palette[0]).h;
            const float hueGap = std::fabs(std::remainder(firstHue - accentHue, 360.0f));
            CHECK(hueGap < 8.0f);
        }
    }
    // Any two of the first eight series differ clearly (OKLab distance; 0.02 is about the
    // smallest difference people notice side by side).
    MESSAGE("lowest OKLab distance between two series: ", lowestDistance);
    CHECK(lowestDistance >= 0.06f);
}
