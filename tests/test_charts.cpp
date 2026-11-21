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
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/chart_core.h"
#include "internal/focus.h"
#include "internal/popup_host.h"
#include "support/session.h"
#include "widgets/charts/chart_parts.h"

namespace
{

namespace fi = facet::internal;

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

// The theme and the motion settings a test changes, restored at the end.
struct ChartGuard
{
    facet::Theme saved = facet::T();
    bool follow = facet::FollowsSystemMotion();
    ChartGuard()
    {
        facet::SetFollowSystemMotion(false);
        facet::SetTheme(facet::Theme::Light(), false);
    }
    ~ChartGuard()
    {
        facet::SetTheme(saved, false);
        facet::SetFollowSystemMotion(follow);
    }
};

// Vertices of exactly `color` the window draw list received since `from`.
int CountColor(int from, ImU32 color)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    int count = 0;
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
        count += drawList->VtxBuffer[i].col == color ? 1 : 0;
    return count;
}

int VertexCount()
{
    return ImGui::GetWindowDrawList()->VtxBuffer.Size;
}

// Whether every vertex since `from` lies inside `rect` grown by `slack` pixels (antialiasing
// fringes reach half a pixel out).
bool VerticesInside(int from, const ImRect& rect, float slack = 1.0f)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
    {
        const ImVec2 p = drawList->VtxBuffer[i].pos;
        if (p.x < rect.Min.x - slack || p.x > rect.Max.x + slack || p.y < rect.Min.y - slack || p.y > rect.Max.y + slack)
            return false;
    }
    return true;
}

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

// ---- Chart parts -----------------------------------------------------------------------------------

TEST_CASE("Readout values show four significant digits in their shortest form")
{
    char text[48];
    const facet::ChartAxis axis;
    const fi::TickSet none;
    fi::ChartReadoutText(axis, 4.25, none, text, sizeof(text));
    CHECK(std::string(text) == "4.25");
    fi::ChartReadoutText(axis, static_cast<double>(0.3f), none, text, sizeof(text));
    CHECK(std::string(text) == "0.3");
    fi::ChartReadoutText(axis, 1234567.0, none, text, sizeof(text));
    CHECK(std::string(text) == "1.235e6");
    fi::ChartReadoutText(axis, -0.0, none, text, sizeof(text));
    CHECK(std::string(text) == "0");
    // With the ticks of an axis: one digit more than the ticks need.
    fi::Tick ticks[fi::kMaxTicks];
    const fi::TickSet set = fi::LinearTicks(0.0, 10.0, 6, ticks);
    fi::ChartReadoutText(axis, 4.26, set, text, sizeof(text));
    CHECK(std::string(text) == "4.3");
    // The axis format wins.
    const facet::ChartAxis percent{.format = "%.0f %%"};
    fi::ChartReadoutText(percent, 42.4, set, text, sizeof(text));
    CHECK(std::string(text) == "42 %");
}

// ---- Sparkline ------------------------------------------------------------------------------------

TEST_CASE("A sparkline fills the available width at the control height in the accent series color")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float values[] = {1.0f, 3.0f, 2.0f, 5.0f, 4.0f};
    int lineVertices = 0;
    float available = 0.0f;
    ImVec2 size;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        const int from = VertexCount();
        facet::Sparkline("Trend", values);
        size = ImGui::GetItemRectSize();
        lineVertices = CountColor(from, facet::ToU32(fi::SeriesColor(facet::T().colors, 0)));
    });
    session.Frame();
    CHECK(size.x == doctest::Approx(available));
    CHECK(size.y == doctest::Approx(facet::ControlHeight(facet::ControlSize::Inherit)));
    CHECK(lineVertices > 0);
}

TEST_CASE("A sparkline with no values, one value, gaps, a zero range or a tiny size stays in its rect")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float one[] = {3.0f};
    static const float gaps[] = {1.0f, kNaN, 2.0f, 3.0f, kNaN, kNaN, 1.0f};
    static const float flat[] = {2.0f, 2.0f, 2.0f};
    static const float huge[] = {-3e30f, 1e30f, 2e30f};
    int checked = 0;
    session.SetUi([&] {
        const auto check = [&](const char* label, std::span<const float> values, const facet::SparklineOptions& options) {
            INFO(label);
            const int from = VertexCount();
            facet::Sparkline(label, values, options);
            const ImRect rect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            CHECK(rect.GetWidth() >= 1.0f);
            // Lines and dots reach past their points by half their width; the clip rect cuts them there.
            CHECK(VerticesInside(from, rect, facet::Px(facet::T().charts.lineWidth) + 1.0f));
            ++checked;
        };
        check("empty", {}, {});
        check("one", one, {});
        check("one bar", one, {.bars = true});
        check("gaps", gaps, {.area = true, .markMinMax = true});
        check("gap bars", gaps, {.bars = true, .markMinMax = true});
        check("flat", flat, {});
        check("huge", huge, {.area = true});
        check("fixed range", gaps, {.min = 5.0, .max = 5.0});
        check("tiny", gaps, {.width = 3.0f, .height = 2.0f});
    });
    session.Frame();
    CHECK(checked == 9);
}

TEST_CASE("A sparkline through a getter draws like the same values in a span")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float values[] = {1.0f, 4.0f, 2.0f, kNaN, 3.0f};
    const facet::ChartGetter getter = [](void*, int index, facet::ChartPoint* out) {
        if (std::isnan(values[index]))
            return false;
        out->x = index;
        out->y = values[index];
        return true;
    };
    int fromSpan = 0;
    int fromGetter = 0;
    session.SetUi([&] {
        int from = VertexCount();
        facet::Sparkline("span", values, {.width = 120.0f});
        fromSpan = VertexCount() - from;
        from = VertexCount();
        facet::Sparkline("getter", 5, getter, nullptr, {.width = 120.0f});
        fromGetter = VertexCount() - from;
    });
    session.Frame();
    CHECK(fromSpan > 0);
    CHECK(fromSpan == fromGetter);
}

TEST_CASE("A hovered sparkline names its nearest value in a tooltip and is not a Tab stop")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float values[] = {1.0f, 3.0f, 2.0f};
    bool tip = false;  // the popup host answers inside the frame
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::Sparkline("Trend", values, {.width = 120.0f});
        session.Track("spark");
        tip = fi::IsPopupLevelOpen(fi::PopupTooltipLevel(ImGui::GetItemID()));
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.MouseMove(session.ItemCenter("spark"));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK(tip);

    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}
