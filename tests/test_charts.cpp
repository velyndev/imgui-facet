// Behaviour of the charts family (facet/widgets/charts.h), and of what the charts added to the
// chart core block (double x values, readers with gaps, runs, decimation indices, bands). The rest
// of the chart core block is tested in tests/test_chart_core.cpp.
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_charts.cpp.

#include <cmath>
#include <cstdlib>
#include <limits>
#include <span>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/chart_core.h"
#include "support/session.h"

namespace
{

namespace fi = facet::internal;

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// A reader over `values` where NaN is a gap, x = 10 * index.
bool ReadTens(const void* source, int slot, double* x, double* y)
{
    const float* values = static_cast<const float*>(source);
    if (std::isnan(values[slot]))
        return false;
    *x = 10.0 * slot;
    *y = values[slot];
    return true;
}

} // namespace

// ---- Chart core additions ----------------------------------------------------------------------

TEST_CASE("Chart core: double x values keep their precision")
{
    // Seconds since 1970 near 1.7e9: a float steps by 128 s there, a double by far less than 1 s.
    const double x[] = {1.7e9, 1.7e9 + 1.0, 1.7e9 + 2.0, 1.7e9 + 3.0};
    const float y[] = {1.0f, 2.0f, 3.0f, 4.0f};
    const fi::SeriesView view{.y = y, .count = 4, .xd = x};
    CHECK(view.XAt(1) == 1.7e9 + 1.0);
    CHECK(fi::IsSortedByX(view));
    CHECK(fi::NearestSampleByX(view, 1.7e9 + 2.2, true) == 2);
    const fi::Range range = fi::SeriesRangeX(view);
    CHECK(range.min == 1.7e9);
    CHECK(range.max == 1.7e9 + 3.0);
}

TEST_CASE("Chart core: a reader reads gaps as NaN and the runs between gaps")
{
    const float values[] = {1.0f, 2.0f, kNaN, kNaN, 5.0f, kNaN, 7.0f, 8.0f};
    const fi::SeriesView view{.count = 8, .reader = &ReadTens, .source = values};
    CHECK(view.HasY());
    CHECK(view.XAt(4) == 40.0);
    CHECK(view.YAt(4) == 5.0);
    CHECK(std::isnan(view.YAt(2)));
    const fi::Range range = fi::SeriesRangeY(view);
    CHECK(range.min == 1.0);
    CHECK(range.max == 8.0);

    // Runs: [0, 2), [4, 5), [6, 8).
    int runs[3][2] = {};
    int found = 0;
    for (int run = fi::NextUsableSample(view, 0, 8); run < 8 && found < 3;)
    {
        const int end = fi::NextGap(view, run, 8);
        runs[found][0] = run;
        runs[found][1] = end;
        ++found;
        run = fi::NextUsableSample(view, end, 8);
    }
    REQUIRE(found == 3);
    CHECK(runs[0][0] == 0);
    CHECK(runs[0][1] == 2);
    CHECK(runs[1][0] == 4);
    CHECK(runs[1][1] == 5);
    CHECK(runs[2][0] == 6);
    CHECK(runs[2][1] == 8);
}

TEST_CASE("Chart core: decimating a run reports the sample index of every point")
{
    std::vector<float> y(1000);
    for (int i = 0; i < 1000; ++i)
        y[static_cast<std::size_t>(i)] = static_cast<float>(std::sin(i * 0.05));
    const fi::SeriesView view{.y = y.data(), .count = 1000};
    const fi::Scale xs = fi::MakeScale(fi::ScaleKind::Linear, 0.0, 999.0, 0.0f, 100.0f);
    const fi::Scale ys = fi::MakeScale(fi::ScaleKind::Linear, -1.0, 1.0, 50.0f, 0.0f);
    std::vector<ImVec2> points(static_cast<std::size_t>(fi::DecimatedCapacity(100.0f)));
    std::vector<int> indices(points.size(), -1);
    const int n = fi::DecimatePolyline(view, xs, ys, points, true, 200, 600, indices);
    REQUIRE(n > 2);
    CHECK(n < 400);
    int previous = -1;
    for (int i = 0; i < n; ++i)
    {
        const int index = indices[static_cast<std::size_t>(i)];
        CHECK(index >= 200);
        CHECK(index < 600);
        CHECK(index > previous);
        CHECK(points[static_cast<std::size_t>(i)].x == doctest::Approx(xs.ToPixel(index)));
        CHECK(points[static_cast<std::size_t>(i)].y == doctest::Approx(ys.ToPixel(y[static_cast<std::size_t>(index)])));
        previous = index;
    }
    // The whole-series form gives the same points as the range form over all samples.
    std::vector<ImVec2> whole(points.size());
    std::vector<ImVec2> ranged(points.size());
    const int a = fi::DecimatePolyline(view, xs, ys, whole, true);
    const int b = fi::DecimatePolyline(view, xs, ys, ranged, true, 0, 1000, {});
    REQUIRE(a == b);
    for (int i = 0; i < a; ++i)
        CHECK(whole[static_cast<std::size_t>(i)].x == ranged[static_cast<std::size_t>(i)].x);
}

TEST_CASE("Chart core: a band fills two triangles between each pair of points")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 top[] = {{10.0f, 10.0f}, {20.0f, 12.0f}, {30.0f, 8.0f}};
        const ImVec2 bottom[] = {{10.0f, 30.0f}, {20.0f, 30.0f}, {30.0f, 30.0f}};
        const int vertices = drawList->VtxBuffer.Size;
        const int indices = drawList->IdxBuffer.Size;
        fi::DrawBand(drawList, top, bottom, IM_COL32(1, 2, 3, 255), ImRect(0.0f, 0.0f, 100.0f, 100.0f));
        CHECK(drawList->VtxBuffer.Size - vertices == 8);
        CHECK(drawList->IdxBuffer.Size - indices == 12);
    });
    session.Frame();
}
