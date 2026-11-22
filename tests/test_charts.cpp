// Behaviour of the charts family (facet/widgets/charts.h), and of what the charts added to the
// chart core block (double x values, readers with gaps, runs, decimation indices, bands). The rest
// of the chart core block is tested in tests/test_chart_core.cpp.
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks are in tests/test_api_charts.cpp.

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <span>
#include <string>
#include <vector>

#include <crtdbg.h>
#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/chart_core.h"
#include "internal/focus.h"
#include "internal/popup_host.h"
#include "support/session.h"
#include "widgets/charts/chart_parts.h"
#include "widgets/display/display_look.h"

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

// ---- Legend ----------------------------------------------------------------------------------------

TEST_CASE("A legend entry flips hidden on click, Space and Enter, and each entry is a Tab stop")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::LegendItem items[] = {{"Series 1"}, {"Series 2"}, {"Series 3"}};
    int writes = 0;
    ImGuiID legendId = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        legendId = ImGui::GetID("legend");
        if (facet::Legend("legend", items))
            ++writes;
        session.Track("legend");
    });
    session.Frame();
    session.Click(fi::ChartLegendEntryRect(legendId, 1).GetCenter());
    CHECK(items[1].hidden);
    CHECK_FALSE(items[0].hidden);
    CHECK(writes == 1);

    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == fi::ChartEntryId(legendId, 0, 0));
    session.KeyPress(ImGuiKey_Space);
    CHECK(items[0].hidden);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == fi::ChartEntryId(legendId, 0, 2));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(items[2].hidden);
    CHECK(writes == 3);
}

TEST_CASE("Alt with a legend entry shows it alone, and again shows every entry")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::LegendItem items[] = {{"Series 1"}, {"Series 2", 0, true}, {"Series 3"}};
    ImGuiID legendId = 0;
    session.SetUi([&] {
        legendId = ImGui::GetID("legend");
        facet::Legend("legend", items);
    });
    session.Frame();
    const ImVec2 second = fi::ChartLegendEntryRect(legendId, 1).GetCenter();
    session.KeyDown(ImGuiMod_Alt);
    session.Click(second);
    session.KeyUp(ImGuiMod_Alt);
    CHECK(items[0].hidden);
    CHECK_FALSE(items[1].hidden);
    CHECK(items[2].hidden);
    session.KeyDown(ImGuiMod_Alt);
    session.Click(second);
    session.KeyUp(ImGuiMod_Alt);
    CHECK_FALSE(items[0].hidden);
    CHECK_FALSE(items[1].hidden);
    CHECK_FALSE(items[2].hidden);

    // The same from the keyboard: Alt with Space or Enter on the focused entry.
    session.KeyPress(ImGuiKey_Tab);  // after the click, the first Tab shows the focus on entry 1
    REQUIRE(ImGui::GetFocusID() == fi::ChartEntryId(legendId, 0, 1));
    session.KeyPress(ImGuiKey_Space | ImGuiMod_Alt);
    CHECK(items[0].hidden);
    CHECK_FALSE(items[1].hidden);
    CHECK(items[2].hidden);
    session.KeyPress(ImGuiKey_Enter | ImGuiMod_Alt);
    CHECK_FALSE(items[0].hidden);
    CHECK_FALSE(items[2].hidden);
}

TEST_CASE("A read-only legend stays focusable and writes nothing; a disabled or passive one is no Tab stop")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::LegendItem readOnly[] = {{"Series 1"}, {"Series 2", 0, true}};
    facet::LegendItem disabled[] = {{"Series 1"}};
    facet::LegendItem passive[] = {{"Series 1"}};
    bool written = false;
    ImGuiID readOnlyId = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        readOnlyId = ImGui::GetID("read only");
        written = facet::Legend("read only", readOnly, {.readOnly = true}) || written;
        ImGui::BeginDisabled();
        written = facet::Legend("disabled", disabled) || written;
        ImGui::EndDisabled();
        written = facet::Legend("passive", passive, {.toggle = false}) || written;
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(fi::ChartLegendEntryRect(readOnlyId, 0).GetCenter());
    CHECK_FALSE(readOnly[0].hidden);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == fi::ChartEntryId(readOnlyId, 0, 0));
    session.KeyPress(ImGuiKey_Space);
    CHECK_FALSE(readOnly[0].hidden);
    CHECK(readOnly[1].hidden);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    // Past the second read-only entry: the disabled and the passive legend take no focus.
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    CHECK_FALSE(written);
    CHECK_FALSE(disabled[0].hidden);
}

TEST_CASE("Legend entries wrap within the legend width, cut long labels and keep ids under item ids")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::LegendItem items[] = {{"A long series label that cannot fit", 0, false, 7},
                                 {"Series 2", 0, false, 9},
                                 {"Series 3", 0, false, 11}};
    ImRect rect;
    ImGuiID legendId = 0;
    const float rowHeight = facet::ControlHeight(facet::ControlSize::Small);
    session.SetUi([&] {
        legendId = ImGui::GetID("legend");
        facet::Legend("legend", items, {.width = 90.0f});
        rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    });
    session.Frame();
    CHECK(rect.GetWidth() == doctest::Approx(facet::Px(90.0f)));
    CHECK(rect.GetHeight() >= 2.0f * rowHeight - 0.5f);
    for (int i = 0; i < 3; ++i)
    {
        const ImRect entry = fi::ChartLegendEntryRect(legendId, i);
        CHECK(entry.Min.x >= rect.Min.x - 0.5f);
        CHECK(entry.Max.x <= rect.Max.x + 0.5f);
    }
    // Entry ids follow item.id, so focus stays on an entry when the caller reorders the items.
    session.Click(fi::ChartLegendEntryRect(legendId, 2).GetCenter());
    CHECK(items[2].hidden);
    CHECK(ImGui::GetFocusID() == fi::ChartEntryId(legendId, 11, 2));
    std::swap(items[0], items[2]);
    session.Frame();
    CHECK(ImGui::GetFocusID() == fi::ChartEntryId(legendId, 11, 0));
}

// ---- Axes -------------------------------------------------------------------------------------------

namespace
{

int LongLabel(char* buffer, int size, double value, void*)
{
    return std::snprintf(buffer, static_cast<std::size_t>(size), "value %.1f units", value);
}

} // namespace

TEST_CASE("Chart axes: labels along x never overlap, and automatic ranges end on ticks")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        fi::ControlTextScope text(fi::kChartTickStyle);
        for (const float length : {40.0f, 120.0f, 300.0f, 900.0f})
        {
            INFO("length " << length);
            const facet::ChartAxis axis{.formatter = &LongLabel};
            const fi::ChartNumericAxis x = fi::ChartValueAxis(axis, {0.0, 1000.0, true}, length, false, 8.0f);
            int labelled = 0;
            int major = 0;
            for (int i = 0; i < x.count; ++i)
                if (x.ticks[i].major && major++ % x.labelEvery == 0)
                    ++labelled;
            // Neighbouring labels are labelEvery steps apart, at least one label and a gap.
            const float pitch = length / static_cast<float>(std::max(1, major - 1)) * static_cast<float>(x.labelEvery);
            if (labelled > 1)
                CHECK(pitch >= x.labelWidth + 8.0f - 0.5f);
            ++checked;
        }
        // Data from 1 to 6.4 with 5 % padding, about four ticks: the range ends on whole steps of its ticks.
        const facet::ChartAxis automatic;
        const fi::Range range = fi::ChartResolveRange(automatic, {1.0, 6.4, true}, {.pad = true, .nice = true}, 4);
        const fi::ChartNumericAxis y = fi::ChartValueAxis(automatic, range, 120.0f, true, 16.0f);
        REQUIRE(y.count >= 2);
        CHECK(y.ticks[0].value == doctest::Approx(range.min));
        CHECK(y.ticks[y.count - 1].value == doctest::Approx(range.max));
        // Bars and areas keep the baseline at 0, without padding below it.
        const fi::Range bars = fi::ChartResolveRange(automatic, {2.0, 7.0, true}, {.includeZero = true, .pad = true}, 4);
        CHECK(bars.min == 0.0);
        ++checked;
    });
    session.Frame();
    CHECK(checked == 5);
}

// ---- Charts with axes --------------------------------------------------------------------------------

namespace
{

const float kRamp[] = {1.0f, 2.0f, 4.0f, 3.0f, 5.0f, 2.0f, 6.0f, 4.0f, 3.0f, 5.0f};
const float kFlat[] = {2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f};
constexpr ImU32 kRed = IM_COL32(200, 30, 30, 255);
constexpr ImU32 kBlue = IM_COL32(30, 30, 200, 255);

// Whether every vertex the window draw list received since `from` lies inside `rect`, or belongs to
// a draw command whose clip rectangle lies inside it (so what reaches past it is cut there).
bool DrawnInside(int fromVertex, const ImRect& rect)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (const ImDrawCmd& cmd : drawList->CmdBuffer)
    {
        const bool clipped = cmd.ClipRect.x >= rect.Min.x - 0.5f && cmd.ClipRect.y >= rect.Min.y - 0.5f &&
                             cmd.ClipRect.z <= rect.Max.x + 0.5f && cmd.ClipRect.w <= rect.Max.y + 0.5f;
        for (unsigned int k = 0; k < cmd.ElemCount; ++k)
        {
            const unsigned int index = drawList->IdxBuffer[static_cast<int>(cmd.IdxOffset + k)] + cmd.VtxOffset;
            if (static_cast<int>(index) < fromVertex || clipped)
                continue;
            const ImVec2 pos = drawList->VtxBuffer[static_cast<int>(index)].pos;
            if (!ImRect(rect.Min.x - 1.0f, rect.Min.y - 1.0f, rect.Max.x + 1.0f, rect.Max.y + 1.0f).Contains(pos))
                return false;
        }
    }
    return true;
}

// The horizontal (x) or vertical (y) extent of the vertices of `color` since `from`.
ImVec2 ColorExtent(int from, ImU32 color, bool vertical)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 extent(FLT_MAX, -FLT_MAX);
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
    {
        if (drawList->VtxBuffer[i].col != color)
            continue;
        const float v = vertical ? drawList->VtxBuffer[i].pos.y : drawList->VtxBuffer[i].pos.x;
        extent.x = std::min(extent.x, v);
        extent.y = std::max(extent.y, v);
    }
    return extent;
}

} // namespace

TEST_CASE("A line chart fills the width at the chart height and is one Tab stop")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ChartSeries series[] = {{.label = "Series 1", .values = kRamp}};
    float available = 0.0f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        available = ImGui::GetContentRegionAvail().x;
        facet::LineChart("line", series);
        session.Track("line");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    CHECK(session.ItemRect("line").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("line").GetHeight() == doctest::Approx(std::round(facet::Px(facet::T().metrics.chartHeight))));
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("line"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("The line chart readout follows the pointer to the nearest x and steps with the keys")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ChartSeries series[] = {{.label = "Series 1", .values = kRamp}, {.label = "Series 2", .values = kFlat}};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::LineChart("line", series, {.width = 400.0f});
        session.Track("line");
    });
    session.Frame();
    const ImGuiID id = session.ItemId("line");
    const ImRect rect = session.ItemRect("line");
    // The plot spans most of the width: the pointer at 95 % reads the last sample.
    session.MouseMove(ImVec2(rect.Min.x + rect.GetWidth() * 0.97f, rect.GetCenter().y));
    CHECK(fi::ChartReadoutIndex(id) == 9);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == id);
    session.KeyPress(ImGuiKey_Home);
    CHECK(fi::ChartReadoutIndex(id) == 0);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(fi::ChartReadoutIndex(id) == 2);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(fi::ChartReadoutIndex(id) == 1);
    session.KeyPress(ImGuiKey_End);
    CHECK(fi::ChartReadoutIndex(id) == 9);
    CHECK(ImGui::GetFocusID() == id);  // the arrows stayed with the chart
}

TEST_CASE("A hidden series is neither drawn nor part of the range; colors follow the series index")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    int shown = -1;
    int hidden = -1;
    int palette = -1;
    session.SetUi([&] {
        facet::ChartSeries series[] = {{.label = "a", .values = kRamp, .color = kRed},
                                       {.label = "b", .values = kFlat, .color = kBlue}};
        int from = VertexCount();
        facet::LineChart("visible", series);
        shown = CountColor(from, kBlue);
        series[1].hidden = true;
        from = VertexCount();
        facet::LineChart("hidden", series);
        hidden = CountColor(from, kBlue);
        series[1].color = 0;
        series[1].hidden = false;
        from = VertexCount();
        facet::LineChart("palette", series);
        palette = CountColor(from, facet::ToU32(fi::SeriesColor(facet::T().colors, 1)));
    });
    session.Frame();
    CHECK(shown > 0);
    CHECK(hidden == 0);
    CHECK(palette > 0);
}

TEST_CASE("Charts with axes keep their size in Loading, Error and empty states and take no focus then")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ChartSeries series[] = {{.label = "a", .values = kRamp}};
    static const char* const categories[] = {"One", "Two"};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::LineChart("loading", series, {.state = facet::DataState::Loading});
        session.Track("loading");
        facet::BarChart("error", series, categories, {.state = facet::DataState::Error});
        session.Track("error");
        facet::LineChart("empty", std::span<const facet::ChartSeries>{});
        session.Track("empty");
        facet::DonutChart("zero", std::span<const facet::DonutSlice>{});
        session.Track("zero");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    const float height = std::round(facet::Px(facet::T().metrics.chartHeight));
    CHECK(session.ItemRect("loading").GetHeight() == doctest::Approx(height));
    CHECK(session.ItemRect("error").GetHeight() == doctest::Approx(height));
    CHECK(session.ItemRect("empty").GetHeight() == doctest::Approx(height));
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("Charts with extreme data and tiny sizes draw inside their box")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float one[] = {3.0f};
    static const float gaps[] = {kNaN, 1.0f, kNaN, 2.0f, 3.0f, kNaN};
    static const float nan[] = {kNaN, kNaN};
    static const float huge[] = {-3e30f, 1e30f, 2e30f};
    static const float negatives[] = {-1.0f, 0.5f, -3.0f, 2.0f};
    static const char* const categories[] = {"A category label that is far too long for its slot", "B", "C", "D"};
    static const facet::DonutSlice slices[] = {{"A", 1.0}, {"B", -2.0}, {"C", 0.0}, {"D", 1e-9}};
    int checked = 0;
    session.SetUi([&] {
        const auto check = [&](const char* name, auto&& draw) {
            INFO(name);
            const int from = VertexCount();
            draw();
            const ImRect rect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            CHECK(rect.GetWidth() >= 1.0f);
            CHECK(rect.GetHeight() >= 1.0f);
            CHECK(DrawnInside(from, rect));
            ++checked;
        };
        for (const float width : {3.0f, 40.0f, 300.0f})
        {
            const float height = width < 10.0f ? 4.0f : 0.0f;
            const facet::ChartSeries a[] = {{.label = "one", .values = one}};
            const facet::ChartSeries b[] = {{.label = "gaps", .values = gaps}, {.label = "nan", .values = nan}};
            const facet::ChartSeries c[] = {{.label = "huge", .values = huge}, {.label = "negative", .values = negatives}};
            check("one point", [&] { facet::LineChart("one", a, {.markers = true, .width = width, .height = height}); });
            check("gaps", [&] { facet::LineChart("gaps", b, {.area = true, .stacked = true, .legend = true, .width = width, .height = height}); });
            check("huge", [&] { facet::LineChart("huge", c, {.area = true, .width = width, .height = height}); });
            check("log", [&] {
                facet::LineChart("log", c, {.yAxis = {.scale = facet::ValueScale::Logarithmic}, .width = width, .height = height});
            });
            check("flat", [&] { facet::LineChart("flat", a, {.yAxis = {.min = 2.0, .max = 2.0}, .width = width, .height = height}); });
            check("bars", [&] { facet::BarChart("bars", c, categories, {.stacked = true, .legend = true, .width = width, .height = height}); });
            check("horizontal", [&] {
                facet::BarChart("horizontal", c, categories,
                                {.orientation = facet::Orientation::Horizontal, .width = width, .height = height});
            });
            check("brush", [&] {
                double low = -5.0, high = 1e9;
                facet::ChartRangeBrush("brush", &low, &high, 0.0, 3.0, c, {.width = width, .height = height});
            });
            check("frames", [&] { facet::FrameTimeGraph("frames", gaps, {.width = width, .height = height}); });
            check("meter", [&] { facet::LevelMeter("Meter", negatives, -1.0, 1.0, {.segments = true, .width = width}); });
        }
        check("donut", [&] { facet::DonutChart("donut", slices, {.minSliceFraction = 0.2f, .legend = true, .diameter = 3.0f}); });
        check("gauge", [&] { facet::Gauge("Gauge", 1e12, 0.0, 1.0, {.diameter = 3.0f}); });
    });
    session.Frame();
    CHECK(checked == 32);
}

TEST_CASE("A bar chart picks categories by pointer and keys; horizontal bars take Up and Down too")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float values[] = {1.0f, 2.0f, 3.0f};
    static const char* const categories[] = {"A", "B", "C", "D"};  // one more label than values
    const facet::ChartSeries series[] = {{.label = "a", .values = values}};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::BarChart("bars", series, categories, {.width = 400.0f});
        session.Track("bars");
        facet::BarChart("rows", series, categories, {.orientation = facet::Orientation::Horizontal, .width = 400.0f});
        session.Track("rows");
    });
    session.Frame();
    const ImRect bars = session.ItemRect("bars");
    session.MouseMove(ImVec2(bars.Max.x - 4.0f, bars.GetCenter().y));
    CHECK(fi::ChartReadoutIndex(session.ItemId("bars")) == 3);  // four categories
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("bars"));
    session.KeyPress(ImGuiKey_Home);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(fi::ChartReadoutIndex(session.ItemId("bars")) == 1);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("rows"));
    session.KeyPress(ImGuiKey_Home);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(fi::ChartReadoutIndex(session.ItemId("rows")) == 2);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(fi::ChartReadoutIndex(session.ItemId("rows")) == 1);
}

TEST_CASE("Stacked bar segments follow each other with a gap and never overlap")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float lower[] = {3.0f};
    static const float upper[] = {2.0f};
    static const char* const categories[] = {"A"};
    ImVec2 red, blue;
    session.SetUi([&] {
        const facet::ChartSeries series[] = {{.label = "lower", .values = lower, .color = kRed},
                                             {.label = "upper", .values = upper, .color = kBlue}};
        const int from = VertexCount();
        facet::BarChart("bars", series, categories, {.stacked = true, .width = 300.0f});
        red = ColorExtent(from, kRed, true);
        blue = ColorExtent(from, kBlue, true);
    });
    session.Frame();
    REQUIRE(red.x < red.y);
    REQUIRE(blue.x < blue.y);
    // Screen y grows downward: the upper segment ends above the lower one, a bar gap apart.
    CHECK(blue.y <= red.x - facet::Px(facet::T().charts.barGap) + 1.0f);
}

// ---- Donut chart ------------------------------------------------------------------------------------

TEST_CASE("A donut chart picks slices by pointer and keys and merges small slices into one")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::DonutSlice slices[] = {{"A", 50.0}, {"B", 30.0}, {"C", 1.0}, {"D", 1.0}, {"E", 18.0}};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::DonutChart("donut", slices, {.minSliceFraction = 0.05f, .diameter = 160.0f});
        session.Track("donut");
    });
    session.Frame();
    const ImGuiID id = session.ItemId("donut");
    const ImRect rect = session.ItemRect("donut");
    const float d = facet::Px(160.0f);
    // Just right of the top of the ring: the first slice.
    session.MouseMove(ImVec2(rect.Min.x + d * 0.55f, rect.Min.y + d * 0.06f));
    CHECK(fi::ChartReadoutIndex(id) == 0);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == id);
    // A, B, E kept and the merged C and D: four slices.
    session.KeyPress(ImGuiKey_End);
    CHECK(fi::ChartReadoutIndex(id) == 3);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(fi::ChartReadoutIndex(id) == 2);
}

// ---- Gauge and level meter ----------------------------------------------------------------------------

TEST_CASE("A gauge springs to its value, jumps there with reduced motion, and is not a Tab stop")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    double value = 0.1;
    float right = 0.0f;
    const ImU32 fill = facet::ToU32(fi::DisplayProgressColor(facet::Tone::Accent));
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        const int from = VertexCount();
        facet::Gauge("Gauge", value, 0.0, 1.0, {.diameter = 120.0f});
        session.Track("gauge");
        right = ColorExtent(from, fill, false).y;
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frames(30);
    const float low = right;
    value = 1.0;
    session.Frame();
    CHECK(right < session.ItemRect("gauge").Max.x - facet::Px(20.0f));  // still on its way
    session.Seconds(1.0f);
    CHECK(right > low + facet::Px(40.0f));

    facet::Theme reduced = facet::T();
    reduced.motion.reduceMotion = true;
    facet::SetTheme(reduced, false);
    value = 0.1;
    session.Frame();
    CHECK(right == doctest::Approx(low).epsilon(0.02));

    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("A level meter smooths its value and holds its peak, then lets it fall")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    float level = 1.0f;
    float peakX = 0.0f;
    const ImU32 mark = facet::ToU32(facet::T().colors.label);
    session.SetUi([&] {
        const int from = VertexCount();
        facet::LevelMeter("##meter", std::span<const float>(&level, 1), 0.0, 1.0, {.width = 300.0f});
        peakX = ColorExtent(from, mark, false).x;
    });
    session.Seconds(0.5f);
    const float top = peakX;
    level = 0.1f;
    session.Seconds(facet::T().charts.peakHold * 0.5f);
    CHECK(peakX == doctest::Approx(top));  // held
    session.Seconds(facet::T().charts.peakHold + 2.0f);
    CHECK(peakX < top - facet::Px(100.0f));  // fallen
}

// ---- Frame time graph -------------------------------------------------------------------------------

TEST_CASE("The frame time graph marks frames over the target and the subsystem records every frame")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float fast[] = {0.010f, 0.012f, 0.011f};
    static const float slow[] = {0.010f, 0.030f, 0.011f};
    int fastWarnings = -1;
    int slowWarnings = -1;
    session.SetUi([&] {
        const ImU32 warning = facet::ToU32(facet::T().colors.warning);
        int from = VertexCount();
        facet::FrameTimeGraph("fast", fast);
        fastWarnings = CountColor(from, warning);
        from = VertexCount();
        facet::FrameTimeGraph("slow", slow);
        slowWarnings = CountColor(from, warning);
        facet::FrameTimeGraph("own", {.compact = true});
    });
    fi::ResetChartFrameTimes();
    session.Frames(12);
    CHECK(fastWarnings == 0);
    CHECK(slowWarnings > 0);
    const fi::SeriesView times = fi::ChartFrameTimes();
    CHECK(times.count == 12);
    CHECK(times.YAt(11) == doctest::Approx(session.Options().deltaTime));
    session.Frames(fi::kChartFrameHistory);
    CHECK(fi::ChartFrameTimes().count == fi::kChartFrameHistory);
}

// ---- Range brush ----------------------------------------------------------------------------------------

TEST_CASE("The range brush keeps its edges apart, drags its window and steps each edge by keys")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    double low = 20.0;
    double high = 40.0;
    int writes = 0;
    ImGuiID brushId = 0;
    const facet::ChartSeries series[] = {{.label = "a", .values = kRamp}};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        brushId = ImGui::GetID("Range");
        if (facet::ChartRangeBrush("Range", &low, &high, 0.0, 100.0, series, {.minGap = 10.0, .width = 400.0f}))
            ++writes;
        session.Track("brush");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("brush");
    // The low edge dragged past the high one stops one gap before it.
    session.Drag({fi::ChartBrushHandleRect(brushId, 0).GetCenter(), ImVec2(rect.Max.x - 2.0f, rect.GetCenter().y)});
    CHECK(low == doctest::Approx(30.0));
    CHECK(high == doctest::Approx(40.0));
    // The window body drags both edges by the same amount.
    const ImVec2 body((fi::ChartBrushHandleRect(brushId, 0).GetCenter().x + fi::ChartBrushHandleRect(brushId, 1).GetCenter().x) * 0.5f,
                      rect.GetCenter().y);
    session.Drag({body, ImVec2(body.x + rect.GetWidth() * 0.2f, body.y)});
    CHECK(high - low == doctest::Approx(10.0));
    CHECK(low > 40.0);
    // Each edge is a Tab stop; the arrows step it, Escape during a held key restores it.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == fi::ChartBrushHandleId(brushId, 0));
    const double before = low;
    // Off the keyboard grid (a hundredth of the range), the first step lands on the grid point below.
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(low == doctest::Approx(std::ceil(before) - 2.0));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == fi::ChartBrushHandleId(brushId, 1));
    session.KeyPress(ImGuiKey_End);
    CHECK(high == doctest::Approx(100.0));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    CHECK(writes >= 4);
}

TEST_CASE("The range brush window keeps its look under the pointer; only a grip shows hover")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    double low = 30.0;
    double high = 70.0;
    ImGuiID brushId = 0;
    std::vector<ImDrawVert> drawn;
    const facet::ChartSeries series[] = {{.label = "a", .values = kRamp}};
    session.SetUi([&] {
        brushId = ImGui::GetID("Range");
        const int from = VertexCount();
        (void)facet::ChartRangeBrush("Range", &low, &high, 0.0, 100.0, series, {.width = 400.0f});
        session.Track("brush");
        const ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawn.assign(drawList->VtxBuffer.Data + from, drawList->VtxBuffer.Data + drawList->VtxBuffer.Size);
    });
    // Reduced motion leaves no hover fade in between, so each state draws its final look.
    facet::Theme theme = facet::T();
    theme.motion.reduceMotion = true;
    facet::SetTheme(theme, false);
    const auto same = [](const std::vector<ImDrawVert>& a, const std::vector<ImDrawVert>& b) {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (a[i].col != b[i].col || a[i].pos.x != b[i].pos.x || a[i].pos.y != b[i].pos.y)
                return false;
        return true;
    };
    session.MouseMove(ImVec2(2.0f, 2.0f));
    session.Frames(3);
    const std::vector<ImDrawVert> rest = drawn;
    REQUIRE_FALSE(rest.empty());
    // Over the window body, half way between the edges and clear of both grips.
    const ImRect rect = session.ItemRect("brush");
    const ImRect lowGrip = fi::ChartBrushHandleRect(brushId, 0);
    const ImRect highGrip = fi::ChartBrushHandleRect(brushId, 1);
    session.MouseMove(ImVec2((lowGrip.Max.x + highGrip.Min.x) * 0.5f, rect.Min.y + 2.0f));
    session.Frames(3);
    CHECK(same(drawn, rest));
    // Over a grip: the grip changes, so the draw differs.
    session.MouseMove(lowGrip.GetCenter());
    session.Frames(3);
    CHECK_FALSE(same(drawn, rest));
}

TEST_CASE("The range brush zooms with Alt and the wheel, and a read-only brush writes nothing")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    double low = 20.0, high = 60.0;
    double lockedLow = 20.0, lockedHigh = 60.0;
    const facet::ChartSeries series[] = {{.label = "a", .values = kRamp}};
    session.SetUi([&] {
        facet::ChartRangeBrush("Range", &low, &high, 0.0, 100.0, series, {.width = 400.0f});
        session.Track("brush");
        facet::ChartRangeBrush("Locked", &lockedLow, &lockedHigh, 0.0, 100.0, series, {.width = 400.0f, .readOnly = true});
        session.Track("locked");
    });
    session.Frame();
    session.MouseMove(session.ItemCenter("brush"));
    session.KeyDown(ImGuiMod_Alt);
    session.Wheel(session.ItemCenter("brush"), 1.0f);
    session.Frame();
    CHECK(high - low == doctest::Approx(40.0 * 0.8));
    session.Wheel(session.ItemCenter("locked"), 1.0f);
    session.KeyUp(ImGuiMod_Alt);
    const ImRect locked = session.ItemRect("locked");
    session.Drag({ImVec2(locked.Min.x + 4.0f, locked.GetCenter().y), ImVec2(locked.Max.x - 4.0f, locked.GetCenter().y)});
    CHECK(lockedLow == 20.0);
    CHECK(lockedHigh == 60.0);
}

// ---- Steady state --------------------------------------------------------------------------------------

TEST_CASE("Charts that are shown allocate nothing per frame in the steady state")
{
    ChartGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float channels[] = {0.3f, 0.8f};
    static const char* const categories[] = {"A", "B"};
    static const facet::DonutSlice slices[] = {{"A", 2.0}, {"B", 1.0}};
    static facet::LegendItem legend[] = {{"a"}, {"b"}};
    double low = 1.0, high = 5.0;
    const facet::ChartSeries series[] = {{.label = "a", .values = kRamp}, {.label = "b", .values = kFlat}};
    session.SetUi([&] {
        facet::Sparkline("spark", kRamp);
        facet::Legend("legend", legend);
        facet::LineChart("line", series, {.area = true, .stacked = true, .markers = true, .legend = true});
        facet::BarChart("bars", series, categories, {.legend = true});
        facet::DonutChart("donut", slices, {.legend = true});
        facet::Gauge("gauge", 0.4, 0.0, 1.0);
        facet::LevelMeter("meter", channels, 0.0, 1.0, {.segments = true});
        facet::FrameTimeGraph("frames");
        facet::ChartRangeBrush("brush", &low, &high, 0.0, 9.0, series);
    });
    session.Frames(10);
    session.MouseMove(ImVec2(300.0f, 300.0f));  // hover something
    session.Frames(30);

    static int allocations = 0;
    allocations = 0;
    ImGuiMemAllocFunc oldAlloc = nullptr;
    ImGuiMemFreeFunc oldFree = nullptr;
    void* oldUser = nullptr;
    ImGui::GetAllocatorFunctions(&oldAlloc, &oldFree, &oldUser);
    ImGui::SetAllocatorFunctions(
        [](size_t size, void*) -> void* {
            ++allocations;
            return std::malloc(size);
        },
        [](void* pointer, void*) { std::free(pointer); });
#ifdef _DEBUG
    static long crtAllocations = 0;
    crtAllocations = 0;
    const _CRT_ALLOC_HOOK previous =
        _CrtSetAllocHook([](int type, void*, size_t, int, long, const unsigned char*, int) -> int {
            if (type == _HOOK_ALLOC || type == _HOOK_REALLOC)
                ++crtAllocations;
            return 1;
        });
#endif
    session.Frames(20);
#ifdef _DEBUG
    _CrtSetAllocHook(previous);
    CHECK(crtAllocations == 0);
#endif
    ImGui::SetAllocatorFunctions(oldAlloc, oldFree, oldUser);
    CHECK(allocations == 0);
}
