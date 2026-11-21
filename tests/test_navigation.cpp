// Behaviour of the navigation family (facet/widgets/navigation.h).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks are in tests/test_api_navigation.cpp. Parts of a widget (a tab, a close
// button, a page button) are found through the part records of
// src/widgets/navigation/navigation_parts.h, the same ones the demo page's selftest clicks.

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/popup_host.h"
#include "support/session.h"
#include "widgets/navigation/navigation_parts.h"
#include "widgets/selection/selection_parts.h"

namespace
{

using facet::internal::NavigationPartRect;

// Restores the theme a test changes.
struct ThemeGuard
{
    facet::Theme saved = facet::T();
    ~ThemeGuard() { facet::SetTheme(saved, false); }
};

// The bounds of the vertices of `color` the window draw list received since `from`.
ImRect ColorBounds(int from, ImU32 color)
{
    const ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImRect r(ImVec2(FLT_MAX, FLT_MAX), ImVec2(-FLT_MAX, -FLT_MAX));
    for (int i = from; i < drawList->VtxBuffer.Size; ++i)
        if (drawList->VtxBuffer[i].col == color)
            r.Add(drawList->VtxBuffer[i].pos);
    return r;
}

// A pill's fill carries an anti-aliasing fringe of half a pixel around a shape on whole pixels.
bool IsWholeEdge(float v)
{
    const float f = v - std::floor(v);
    return std::fabs(f - 0.5f) < 0.02f || f < 0.02f || f > 0.98f;
}

ImVec2 Center(const ImRect& r)
{
    return r.GetCenter();
}

const facet::ChoiceItem kTabs[] = {
    {.label = "Alpha", .id = 11}, {.label = "Beta", .id = 12}, {.label = "Gamma", .id = 13}, {.label = "Delta", .id = 14}};

} // namespace

// --- TabBar -------------------------------------------------------------------------------------

TEST_CASE("A tab bar is one Tab stop; Left and Right select and move the focus, Home and End, disabled tabs skipped")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::ChoiceItem kItems[] = {{"One"}, {.label = "Two", .disabled = true}, {"Three"}, {"Four"}};
    int index = 0;
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        writes += facet::TabBar("tabs", &index, kItems) ? 1 : 0;
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    // The one Tab stop is the selected tab.
    const ImGuiID first = ImGui::GetFocusID();
    CHECK(first != 0);
    CHECK(first != session.ItemId("after"));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(index == 2);  // the disabled tab is skipped
    CHECK(writes == 1);
    session.KeyPress(ImGuiKey_End);
    CHECK(index == 3);
    session.KeyPress(ImGuiKey_Home);
    CHECK(index == 0);
    CHECK(ImGui::GetFocusID() == first);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(index == 0);  // no wrap at the start
    // Tab leaves the bar in one step.
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("A click selects a tab once, a disabled tab or a disabled bar never")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::ChoiceItem kItems[] = {{"One"}, {.label = "Two", .disabled = true}, {"Three"}};
    int index = 0;
    int fixed = 0;
    int writes = 0;
    ImGuiID bar = 0;
    ImGuiID off = 0;
    session.SetUi([&] {
        bar = ImGui::GetID("tabs");
        writes += facet::TabBar("tabs", &index, kItems) ? 1 : 0;
        off = ImGui::GetID("off");
        ImGui::BeginDisabled();
        writes += facet::TabBar("off", &fixed, kItems) ? 1 : 0;
        ImGui::EndDisabled();
    });
    session.Frame();
    session.Click(Center(NavigationPartRect(bar, 2)));
    CHECK(index == 2);
    CHECK(writes == 1);
    session.Click(Center(NavigationPartRect(bar, 1)));
    CHECK(index == 2);
    session.Click(Center(NavigationPartRect(off, 2)));
    CHECK(fixed == 0);
    CHECK(writes == 1);
}

TEST_CASE("The tab pill moves with the spring of the segmented control, rests on whole pixels and jumps with reduced motion")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    static const char* const kLabels[] = {"One", "Two", "Three", "Four"};
    int tab = 0;
    int segment = 0;
    ImRect tabPill;
    ImRect segmentPill;
    session.SetUi([&] {
        const ImU32 subtle = facet::ToU32(facet::T().colors.accentSubtle);
        int from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::TabBar("tabs", &tab, kLabels, {.width = 401.0f});
        tabPill = ColorBounds(from, subtle);
        from = ImGui::GetWindowDrawList()->VtxBuffer.Size;
        facet::SegmentedControl("##segments", &segment, kLabels, {.width = 401.0f});
        segmentPill = ColorBounds(from, subtle);
    });
    session.Frame();
    const ImRect tabStart = tabPill;
    const ImRect segmentStart = segmentPill;
    REQUIRE(tabStart.Min.x < tabStart.Max.x);
    tab = 3;
    segment = 3;
    session.Frames(3);
    const ImRect tabMoving = tabPill;
    const ImRect segmentMoving = segmentPill;
    session.Seconds(1.5f);
    const ImRect tabEnd = tabPill;
    const ImRect segmentEnd = segmentPill;
    // The same spring: both covered the same share of their way in the same frames.
    const float tabShare = (tabMoving.Min.x - tabStart.Min.x) / (tabEnd.Min.x - tabStart.Min.x);
    const float segmentShare = (segmentMoving.Min.x - segmentStart.Min.x) / (segmentEnd.Min.x - segmentStart.Min.x);
    CHECK(tabShare > 0.05f);
    CHECK(tabShare < 0.95f);
    CHECK(tabShare == doctest::Approx(segmentShare).epsilon(0.05));
    CHECK(IsWholeEdge(tabEnd.Min.x));
    CHECK(IsWholeEdge(tabEnd.Max.x));
    // A scale change: the pill follows its tab with the spring and settles on whole pixels.
    const float savedScale = facet::UiScale();
    facet::SetUiScale(savedScale * 2.0f);
    session.Frames(2);
    const ImRect scaled = tabPill;
    session.Seconds(1.5f);
    CHECK(std::fabs(scaled.Min.x - tabPill.Min.x) > 1.0f);
    CHECK(IsWholeEdge(tabPill.Min.x));
    facet::SetUiScale(savedScale);
    session.Seconds(1.5f);
    // Reduced motion: at the new tab in the next frame.
    facet::Theme reduced = facet::T();
    reduced.motion.reduceMotion = true;
    facet::SetTheme(reduced, false);
    const ImRect rested = tabPill;
    tab = 0;
    session.Frame();
    CHECK(tabPill.Min.x == doctest::Approx(tabStart.Min.x));
    CHECK(tabPill.Min.x < rested.Min.x);
}

TEST_CASE("Tabs close by the close button, a middle click and Delete; the next tab takes over a closed selected tab")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::vector<facet::ChoiceItem> tabs(std::begin(kTabs), std::end(kTabs));
    int index = 1;
    int closed = -1;
    ImGuiID bar = 0;
    session.SetUi([&] {
        bar = ImGui::GetID("tabs");
        facet::TabBar("tabs", &index, tabs, {.closable = true});
        const int c = facet::TabBarClosedTab();
        if (c >= 0)
        {
            closed = c;
            tabs.erase(tabs.begin() + c);
        }
    });
    session.Frame();
    // The close button of the selected tab (Beta): Gamma, its next neighbour, takes over at index 1.
    session.Click(Center(NavigationPartRect(bar, facet::internal::NavigationClosePart(1))));
    CHECK(closed == 1);
    REQUIRE(tabs.size() == 3);
    CHECK(index == 1);
    CHECK(std::string(tabs[static_cast<std::size_t>(index)].label) == "Gamma");
    // A middle click on Alpha, before the selected tab: the selection stays on Gamma, now index 0.
    closed = -1;
    session.MouseMove(Center(NavigationPartRect(bar, 0)));
    session.MouseDown(ImGuiMouseButton_Middle);
    session.MouseUp(ImGuiMouseButton_Middle);
    session.Frame();
    CHECK(closed == 0);
    REQUIRE(tabs.size() == 2);
    CHECK(index == 0);
    CHECK(std::string(tabs[0].label) == "Gamma");
    // Delete on the focused last tab: the previous one takes over and gets the focus.
    session.Click(Center(NavigationPartRect(bar, 1)));
    CHECK(index == 1);
    session.KeyPress(ImGuiKey_Tab);  // the first Tab after a click shows the focus where it is
    closed = -1;
    session.KeyPress(ImGuiKey_Delete);
    session.Frame();
    CHECK(closed == 1);
    REQUIRE(tabs.size() == 1);
    CHECK(index == 0);
    CHECK(ImGui::GetFocusID() == facet::internal::SelectionEntryId(bar, tabs[0].id, 0));
    // The last tab closed: nothing is selected.
    session.KeyPress(ImGuiKey_Delete);
    session.Frame();
    CHECK(tabs.empty());
    CHECK(index == -1);
}

TEST_CASE("Tabs reorder by drag and by keyboard; the selected tab stays selected where it lands")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::vector<facet::ChoiceItem> tabs(std::begin(kTabs), std::end(kTabs));
    int index = 0;
    facet::ReorderMove moved;
    ImGuiID bar = 0;
    session.SetUi([&] {
        bar = ImGui::GetID("tabs");
        facet::TabBar("tabs", &index, tabs, {.reorderable = true});
        const facet::ReorderMove move = facet::TabBarMovedTab();
        if (move.IsValid())
        {
            moved = move;
            facet::ApplyReorderMove(tabs, move);
        }
    });
    session.Frame();
    // Drag Alpha (selected) past the middle of Gamma.
    const ImRect gamma = NavigationPartRect(bar, 2);
    session.Drag({Center(NavigationPartRect(bar, 0)), ImVec2(gamma.Max.x - 2.0f, gamma.GetCenter().y)}, 12);
    session.Frames(2);
    CHECK(moved.from == 0);
    CHECK(moved.to == 2);
    CHECK(std::string(tabs[2].label) == "Alpha");
    CHECK(index == 2);
    // Keyboard: Ctrl+Space picks up the focused tab, Left moves it, Space drops it.
    moved = {};
    session.Seconds(1.0f);
    session.Click(Center(NavigationPartRect(bar, 2)));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Space | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_Space);
    session.Frames(2);
    CHECK(moved.from == 2);
    CHECK(moved.to == 0);
    CHECK(std::string(tabs[0].label) == "Alpha");
    CHECK(index == 0);
    // Escape puts a picked-up tab back.
    moved = {};
    session.KeyPress(ImGuiKey_Space | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK_FALSE(moved.IsValid());
    CHECK(std::string(tabs[0].label) == "Alpha");
}

TEST_CASE("Tabs that do not fit move into the more menu and the selected tab always stays visible")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kLabels[] = {"First tab", "Second tab", "Third tab", "Fourth tab", "Fifth tab"};
    int index = 4;
    float width = 0.0f;
    ImGuiID bar = 0;
    session.SetUi([&] {
        bar = ImGui::GetID("tabs");
        facet::TabBar("tabs", &index, kLabels, {.width = width});
    });
    session.Frames(2);
    for (int i = 0; i < 5; ++i)
        CHECK(NavigationPartRect(bar, i).GetWidth() > 0.0f);
    CHECK(NavigationPartRect(bar, facet::internal::kNavigationPartMore).GetWidth() == 0.0f);
    // Room for about two tabs: the selected last tab stays and the more button shows.
    width = facet::T().tabs.maxWidth;
    session.Frames(3);
    CHECK(NavigationPartRect(bar, 4).GetWidth() > 0.0f);
    CHECK(NavigationPartRect(bar, 3).GetWidth() == 0.0f);
    REQUIRE(NavigationPartRect(bar, facet::internal::kNavigationPartMore).GetWidth() > 0.0f);
    // Narrower than the selected tab and the button: the tab shrinks but stays.
    width = facet::T().tabs.minWidth;
    session.Frames(3);
    CHECK(NavigationPartRect(bar, 4).GetWidth() > 0.0f);
    CHECK(NavigationPartRect(bar, 4).Max.x <= NavigationPartRect(bar, facet::internal::kNavigationPartMore).Min.x + 0.5f);
    // The menu lists the hidden tabs; choosing one selects it, and it shows from then on.
    width = facet::T().tabs.maxWidth;
    session.Frames(3);
    session.Click(Center(NavigationPartRect(bar, facet::internal::kNavigationPartMore)));
    session.Seconds(0.3f);
    const ImRect row = NavigationPartRect(bar, facet::internal::NavigationMenuRowPart(1));
    REQUIRE(row.GetWidth() > 0.0f);
    session.Click(Center(row));
    session.Frames(3);
    CHECK(index == 1);
    CHECK(NavigationPartRect(bar, 1).GetWidth() > 0.0f);
}

TEST_CASE("Ctrl+Page Down and Up switch the tabs of the bar while the focus is in its window")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kLabels[] = {"One", "Two", "Three"};
    int index = 0;
    session.SetUi([&] {
        facet::TabBar("tabs", &index, kLabels);
        ImGui::Button("content");
        session.Track("content");
    });
    session.Frame();
    session.Click(session.ItemCenter("content"));
    session.KeyPress(ImGuiKey_PageDown | ImGuiMod_Ctrl);
    CHECK(index == 1);
    session.KeyPress(ImGuiKey_PageDown | ImGuiMod_Ctrl);
    session.KeyPress(ImGuiKey_PageDown | ImGuiMod_Ctrl);
    CHECK(index == 0);  // wraps
    session.KeyPress(ImGuiKey_PageUp | ImGuiMod_Ctrl);
    CHECK(index == 2);
}

TEST_CASE("A tab bar with a null index draws read-only and writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const char* const kLabels[] = {"One", "Two"};
    bool written = false;
    int closed = -1;
    ImGuiID bar = 0;
    session.SetUi([&] {
        bar = ImGui::GetID("tabs");
        written = facet::TabBar("tabs", nullptr, kLabels, {.closable = true}) || written;
        closed = std::max(closed, facet::TabBarClosedTab());
    });
    session.Frame();
    session.Click(Center(NavigationPartRect(bar, 1)));
    session.Click(Center(NavigationPartRect(bar, facet::internal::NavigationClosePart(0))));
    CHECK_FALSE(written);
    CHECK(closed == -1);
}

