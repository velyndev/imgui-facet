// Behaviour of the display family (facet/widgets/display.h, DataTable excepted). ProgressBar: its
// layout with and without a label line, the width rule, the eased fill, clamping, the
// indeterminate sweep and the disabled look. The API checks of every display widget are in
// tests/test_api_display.cpp.

#include <cfloat>
#include <cmath>
#include <limits>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/session.h"

namespace
{

// The rightmost and leftmost x of the vertices of `color` the window draw list received since
// `from`; FLT_MAX and -FLT_MAX when there are none.
struct Span
{
    float min = FLT_MAX;
    float max = -FLT_MAX;
    bool Empty() const { return max < min; }
};

Span ColorSpan(int from, ImU32 color)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    Span span;
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
    {
        const ImDrawVert& v = drawList->VtxBuffer[i];
        if (v.col != color)
            continue;
        span.min = std::fmin(span.min, v.pos.x);
        span.max = std::fmax(span.max, v.pos.x);
    }
    return span;
}

// The theme and the motion settings a test changes, restored at the end.
struct DisplayGuard
{
    facet::Theme saved = facet::T();
    bool follow = facet::FollowsSystemMotion();
    DisplayGuard()
    {
        facet::SetFollowSystemMotion(false);
        facet::SetTheme(facet::Theme::Light(), false);
    }
    ~DisplayGuard()
    {
        facet::SetTheme(saved, false);
        facet::SetFollowSystemMotion(follow);
    }
};

} // namespace

TEST_CASE("A progress bar is one control height without a label line and a line plus its track with one")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float available = 0.0f;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        facet::ProgressBar("##bare", 0.5f);
        session.Track("bare");
        facet::ProgressBar("Copying", 0.5f);
        session.Track("labelled");
        facet::ProgressBar("##percent", 0.5f, {.showPercent = true});
        session.Track("percent");
        facet::ProgressBar("Copying##large", 0.5f, {.size = facet::ControlSize::Large});
        session.Track("large");
        facet::ProgressBar("##units", 0.5f, {.width = 120.0f});
        session.Track("units");
    });
    session.Frame();
    const facet::Metrics& m = facet::T().metrics;
    CHECK(session.ItemRect("bare").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    const float line = session.ItemRect("labelled").GetHeight() - facet::PxSnapped(m.progressHeight);
    CHECK(line > 0.0f);
    CHECK(session.ItemRect("percent").GetHeight() == doctest::Approx(session.ItemRect("labelled").GetHeight()));
    CHECK(session.ItemRect("large").GetHeight() > session.ItemRect("labelled").GetHeight());
    // Field width by default (rule 6), units when asked.
    CHECK(session.ItemRect("bare").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("units").GetWidth() == doctest::Approx(facet::Px(120.0f)));
}

TEST_CASE("A progress bar is not a Tab stop and reports hover like any item")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool hovered = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::ProgressBar("Progress", 0.3f);
        session.Track("bar");
        hovered = ImGui::IsItemHovered();
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    session.MouseMove(session.ItemCenter("bar"));
    session.Frame();
    CHECK(hovered);
}

TEST_CASE("The progress fill eases to a new value and is clamped to the track")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DisplayGuard guard;
    const ImU32 tone = facet::ToU32(facet::T().colors.focusRing);
    float fraction = 0.0f;
    Span fill;
    Span over;
    Span nan;
    ImRect bar;
    session.SetUi([&] {
        int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ProgressBar("##eased", fraction);
        bar = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        fill = ColorSpan(from, tone);
        from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ProgressBar("##over", 2.0f);
        over = ColorSpan(from, tone);
        from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ProgressBar("##nan", std::numeric_limits<float>::quiet_NaN());
        nan = ColorSpan(from, tone);
    });
    session.Frame();
    CHECK(fill.Empty());
    CHECK(nan.Empty());
    CHECK(over.max == doctest::Approx(bar.Max.x).epsilon(0.01));
    fraction = 1.0f;
    session.Frame();
    CHECK_FALSE(fill.Empty());
    CHECK(fill.max < bar.Max.x - 1.0f);  // on its way
    session.Seconds(facet::T().motion.durationStandard + 0.1f);
    CHECK(fill.max == doctest::Approx(bar.Max.x).epsilon(0.01));
}

TEST_CASE("An indeterminate progress bar sweeps a segment along the track")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DisplayGuard guard;
    const ImU32 tone = facet::ToU32(facet::T().colors.focusRing);
    Span segment;
    ImRect bar;
    session.SetUi([&] {
        const int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ProgressBar("##busy", facet::kIndeterminate);
        bar = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        segment = ColorSpan(from, tone);
    });
    // Sample the segment over one sweep: it moves, and never leaves the track.
    float first = 0.0f;
    bool moved = false;
    for (int i = 0; i < 12; ++i)
    {
        session.Seconds(0.1f);
        if (segment.Empty())
            continue;
        CHECK(segment.min >= bar.Min.x - 1.0f);
        CHECK(segment.max <= bar.Max.x + 1.0f);
        if (first == 0.0f)
            first = segment.min;
        else if (std::fabs(segment.min - first) > 1.0f)
            moved = true;
    }
    CHECK(moved);
}

TEST_CASE("A disabled progress bar draws no tone color")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    DisplayGuard guard;
    const ImU32 tone = facet::ToU32(facet::T().colors.focusRing);
    Span fill;
    session.SetUi([&] {
        ImGui::BeginDisabled();
        const int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::ProgressBar("Disabled", 1.0f);
        fill = ColorSpan(from, tone);
        ImGui::EndDisabled();
    });
    session.Frame();
    CHECK(fill.Empty());
}
