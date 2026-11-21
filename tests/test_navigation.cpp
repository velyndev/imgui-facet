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

ImVec2 Nudge(ImVec2 p)
{
    return ImVec2(p.x + 1.0f, p.y);
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

// --- Sidebar ------------------------------------------------------------------------------------

TEST_CASE("Sidebar items are one Tab stop, Up and Down move the focus and Enter activates")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int page = 0;
    int activations = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::BeginSidebar("nav", {.mode = facet::SidebarMode::Expanded, .height = 300.0f}))
        {
            for (int i = 0; i < 3; ++i)
            {
                char label[16];
                std::snprintf(label, sizeof(label), "Page %d", i + 1);
                if (facet::SidebarItem(label, facet::Icon("list"), {.selected = page == i}))
                {
                    page = i;
                    ++activations;
                }
                session.Track(label);
            }
            facet::EndSidebar();
        }
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frames(2);
    session.Click(session.ItemCenter("Page 3"));
    CHECK(page == 2);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("Page 3"));  // the selected item is the Tab stop
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(ImGui::GetFocusID() == session.ItemId("Page 2"));
    session.KeyPress(ImGuiKey_Home);
    CHECK(ImGui::GetFocusID() == session.ItemId("Page 1"));
    // Down moves one item, though the items after the focused one come later in the frame.
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(ImGui::GetFocusID() == session.ItemId("Page 2"));
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(page == 0);
    CHECK(activations == 2);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("An Auto sidebar is a rail in a narrow width and names its items in tooltips")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float columnWidth = 0.0f;
    bool tooltip = false;
    bool narrow = true;
    session.SetUi([&] {
        ImGui::BeginChild("frame", ImVec2(facet::Px(narrow ? 300.0f : 780.0f), facet::Px(320.0f)));
        if (facet::BeginSidebar("nav", {.height = 300.0f}))
        {
            facet::SidebarItem("Settings", facet::Icon("settings"), {.selected = true, .badge = 3});
            session.Track("item");
            // The tooltip levels are queried inside the frame, where the popup host answers.
            tooltip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
            facet::EndSidebar();
        }
        columnWidth = ImGui::GetItemRectSize().x;
        ImGui::EndChild();
    });
    session.Seconds(1.0f);
    CHECK(columnWidth == doctest::Approx(std::round(facet::Px(facet::T().containers.railWidth))));
    session.MouseMove(session.ItemCenter("item"));
    session.MouseMove(Nudge(session.ItemCenter("item")));
    session.Seconds(facet::T().overlay.tooltipDelay * facet::T().motion.scale + 0.3f);
    CHECK(tooltip);
    narrow = false;
    session.MouseMove(ImVec2(1.0f, 1.0f));
    session.Seconds(1.0f);
    CHECK(columnWidth == doctest::Approx(std::round(facet::Px(facet::T().containers.sidebarWidth))));
}

TEST_CASE("A collapsible sidebar section toggles on a click and on Left and Right; Hidden draws nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool shown = false;
    bool hidden = true;
    session.SetUi([&] {
        hidden = facet::BeginSidebar("off", {.mode = facet::SidebarMode::Hidden});
        if (facet::BeginSidebar("nav", {.mode = facet::SidebarMode::Expanded, .height = 300.0f}))
        {
            shown = facet::BeginSidebarSection("Group", {.collapsible = true, .defaultOpen = false});
            session.Track("header");
            if (shown)
            {
                facet::SidebarItem("Inside", facet::Icon("list"));
                facet::EndSidebarSection();
            }
            facet::EndSidebar();
        }
    });
    session.Frames(2);
    CHECK_FALSE(hidden);
    CHECK_FALSE(shown);
    session.Click(session.ItemCenter("header"));
    CHECK(shown);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK_FALSE(shown);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(shown);
}

// --- Menus --------------------------------------------------------------------------------------

namespace
{

struct MenuProbe
{
    bool option = false;
    int radio = 0;
    int opens = 0;
    int deletes = 0;
    int deep = 0;
    bool fileOpen = false;
    bool editOpen = false;
    bool subOpen = false;
    ImGuiID bar = 0;
    ImGuiID fileTitle = 0;
    ImGuiID editTitle = 0;

    void Draw(facet::test::Session& session)
    {
        fileOpen = editOpen = subOpen = false;
        bar = ImGui::GetID("bar");
        if (facet::BeginMenuBar("bar"))
        {
            fileTitle = ImGui::GetID("File");
            if (facet::BeginMenu("File"))
            {
                fileOpen = true;
                if (facet::MenuItem("Open"))
                    ++opens;
                session.Track("open");
                facet::MenuItem("Details");
                ImGui::BeginDisabled();
                facet::MenuItem("Duplicate");
                session.Track("duplicate");
                ImGui::EndDisabled();
                if (facet::MenuItem("Delete", {.destructive = true}))
                    ++deletes;
                session.Track("delete");
                if (facet::BeginMenu("More options"))
                {
                    subOpen = true;
                    if (facet::MenuItem("Deep"))
                        ++deep;
                    session.Track("deep");
                    facet::MenuItem("Deeper");
                    facet::MenuItem("Deepest");
                    facet::EndMenu();
                }
                session.Track("more");
                facet::MenuItem("Last row");
                session.Track("last");
                facet::EndMenu();
            }
            editTitle = ImGui::GetID("Edit");
            if (facet::BeginMenu("Edit"))
            {
                editOpen = true;
                facet::MenuItem("Cut", {.shortcut = ImGuiMod_Ctrl | ImGuiKey_X});
                facet::MenuCheckItem("Wrap", &option);
                session.Track("wrap");
                facet::MenuRadioItem("Small", &radio, 0);
                facet::MenuRadioItem("Large", &radio, 1);
                session.Track("large");
                facet::EndMenu();
            }
            facet::EndMenuBar();
        }
    }

    ImVec2 Title(int index) const { return NavigationPartRect(bar, index).GetCenter(); }
};

} // namespace

TEST_CASE("A menu bar title opens its menu, an item activates and closes it, hovering switches menus")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    MenuProbe probe;
    session.SetUi([&] { probe.Draw(session); });
    session.Frame();
    session.Click(probe.Title(0));
    session.Seconds(0.3f);
    CHECK(probe.fileOpen);
    session.Click(session.ItemCenter("open"));
    session.Frames(2);
    CHECK(probe.opens == 1);
    CHECK_FALSE(probe.fileOpen);
    // Open File, then move onto Edit: Edit opens in its place.
    session.Click(probe.Title(0));
    session.Seconds(0.3f);
    session.MouseMove(probe.Title(1));
    session.MouseMove(Nudge(probe.Title(1)));
    session.Seconds(0.3f);
    CHECK(probe.editOpen);
    CHECK_FALSE(probe.fileOpen);
    session.KeyPress(ImGuiKey_Escape);
    session.Seconds(0.3f);
    CHECK_FALSE(probe.editOpen);
}

TEST_CASE("In a bar menu Left and Right switch menus, Escape closes it and the focus returns to the title")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    MenuProbe probe;
    session.SetUi([&] { probe.Draw(session); });
    session.Frame();
    session.Click(probe.Title(0));
    session.Seconds(0.3f);
    REQUIRE(probe.fileOpen);
    session.MouseMove(ImVec2(1.0f, 500.0f));
    session.KeyPress(ImGuiKey_DownArrow);  // the focus is in the menu
    session.KeyPress(ImGuiKey_RightArrow);
    session.Seconds(0.3f);
    CHECK(probe.editOpen);
    CHECK_FALSE(probe.fileOpen);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.Seconds(0.3f);
    CHECK(probe.fileOpen);
    session.KeyPress(ImGuiKey_Escape);
    session.Seconds(0.3f);
    CHECK_FALSE(probe.fileOpen);
    CHECK(ImGui::GetFocusID() == probe.fileTitle);
    // Right on a title moves to the next title; Down opens its menu.
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ImGui::GetFocusID() == probe.editTitle);
    session.KeyPress(ImGuiKey_DownArrow);
    session.Seconds(0.3f);
    CHECK(probe.editOpen);
}

TEST_CASE("A submenu opens on Right and closes on Left, and the aim test keeps it open on the way there")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    MenuProbe probe;
    session.SetUi([&] { probe.Draw(session); });
    session.Frame();
    session.Click(probe.Title(0));
    session.Seconds(0.3f);
    REQUIRE(probe.fileOpen);
    // The pointer rests on the submenu row: it opens.
    session.MouseMove(session.ItemCenter("more"));
    session.MouseMove(Nudge(session.ItemCenter("more")));
    session.Seconds(0.5f);
    REQUIRE(probe.subOpen);
    // On its way toward the submenu across the next row, for longer than the submenu delay, the
    // submenu stays open: the pointer aims at it.
    const ImRect deep = session.ItemRect("deep");
    const ImVec2 from = session.ItemCenter("more");
    const ImVec2 last = session.ItemCenter("last");
    const ImVec2 toward(from.x + (deep.Min.x - from.x) * 0.7f, last.y);
    constexpr int kSteps = 24;
    for (int i = 1; i <= kSteps; ++i)
    {
        const float f = static_cast<float>(i) / static_cast<float>(kSteps);
        session.MouseMove(ImVec2(from.x + (toward.x - from.x) * f, from.y + (toward.y - from.y) * f));
    }
    CHECK(probe.subOpen);
    session.MouseMove(session.ItemCenter("deep"));
    session.Seconds(0.1f);
    session.Click(session.ItemCenter("deep"));
    session.Frames(2);
    CHECK(probe.deep == 1);
    CHECK_FALSE(probe.fileOpen);  // the whole chain closed
    // Resting on another row of the menu, away from the submenu, closes it.
    session.Click(probe.Title(0));
    session.Seconds(0.3f);
    session.MouseMove(session.ItemCenter("more"));
    session.MouseMove(Nudge(session.ItemCenter("more")));
    session.Seconds(0.5f);
    REQUIRE(probe.subOpen);
    session.MouseMove(session.ItemCenter("open"));
    session.MouseMove(Nudge(session.ItemCenter("open")));
    session.Seconds(0.5f);
    CHECK_FALSE(probe.subOpen);
    // Keyboard: Right on the submenu row opens it, Left closes it again.
    session.MouseMove(ImVec2(1.0f, 500.0f));
    session.KeyPress(ImGuiKey_Escape);
    session.Seconds(0.3f);
    session.KeyPress(ImGuiKey_DownArrow);
    session.Seconds(0.3f);
    REQUIRE(probe.fileOpen);
    for (int i = 0; i < 3; ++i)
        session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_RightArrow);
    session.Seconds(0.3f);
    CHECK(probe.subOpen);
    CHECK(probe.fileOpen);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.Seconds(0.3f);
    CHECK_FALSE(probe.subOpen);
    CHECK(probe.fileOpen);
}

TEST_CASE("Typing in a menu moves the focus to the next row that starts with the text, past disabled rows")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    MenuProbe probe;
    session.SetUi([&] { probe.Draw(session); });
    session.Frame();
    session.Click(probe.Title(0));
    session.Seconds(0.3f);
    REQUIRE(probe.fileOpen);
    session.MouseMove(ImVec2(1.0f, 500.0f));
    // "Details" comes first; typing d again skips the disabled "Duplicate" and lands on "Delete".
    session.Type("d");
    session.Frames(2);
    session.Type("d");
    session.Frames(2);
    CHECK(ImGui::GetFocusID() == session.ItemId("delete"));
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(probe.deletes == 1);
    CHECK_FALSE(probe.fileOpen);
}

TEST_CASE("Check and radio items write their values; a command item triggers its shortcut")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    MenuProbe probe;
    const facet::ShortcutId command = facet::RegisterShortcut("test.nav.command", ImGuiMod_Ctrl | ImGuiKey_K);
    int fired = 0;
    bool inline_ = false;
    session.SetUi([&] {
        probe.Draw(session);
        if (facet::ShortcutPressed(command))
            ++fired;
        // A row outside any menu draws inline and activates.
        inline_ = facet::MenuItem("Inline", {.command = command}) || inline_;
        session.Track("inline");
    });
    session.Frame();
    session.Click(probe.Title(1));
    session.Seconds(0.3f);
    session.Click(session.ItemCenter("wrap"));
    CHECK(probe.option);
    session.Click(probe.Title(1));
    session.Seconds(0.3f);
    session.Click(session.ItemCenter("large"));
    CHECK(probe.radio == 1);
    // The command row triggers its shortcut; the chord runs it with every menu closed as well.
    session.Click(session.ItemCenter("inline"));
    session.Frames(2);
    CHECK(inline_);
    CHECK(fired == 1);
    session.KeyPress(ImGuiKey_K | ImGuiMod_Ctrl);
    session.Frames(2);
    CHECK(fired == 2);
    facet::UnregisterShortcut(command);
}

TEST_CASE("Menus that do not fit the bar move into its more menu, whose rows open them")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool editOpen = false;
    int cuts = 0;
    ImGuiID bar = 0;
    float width = 0.0f;
    session.SetUi([&] {
        bar = ImGui::GetID("bar");
        editOpen = false;
        if (facet::BeginMenuBar("bar", {.width = width}))
        {
            if (facet::BeginMenu("File"))
            {
                facet::MenuItem("Open");
                facet::EndMenu();
            }
            if (facet::BeginMenu("Edit"))
            {
                editOpen = true;
                if (facet::MenuItem("Cut"))
                    ++cuts;
                session.Track("cut");
                facet::EndMenu();
            }
            facet::EndMenuBar();
        }
    });
    session.Frames(2);
    CHECK(NavigationPartRect(bar, 1).GetWidth() > 0.0f);
    width = (NavigationPartRect(bar, 0).GetWidth() + facet::ControlHeight(facet::ControlSize::Regular)) / facet::Px(1.0f) +
            facet::T().spacing.xs * 2.0f;
    session.Frames(3);
    CHECK(NavigationPartRect(bar, 1).GetWidth() == 0.0f);
    const ImRect more = NavigationPartRect(bar, facet::internal::kNavigationPartMore);
    REQUIRE(more.GetWidth() > 0.0f);
    session.Click(more.GetCenter());
    session.Seconds(0.3f);
    const ImRect row = NavigationPartRect(bar, facet::internal::NavigationMenuRowPart(1));
    REQUIRE(row.GetWidth() > 0.0f);
    session.Click(row.GetCenter());
    session.Seconds(0.3f);
    CHECK(editOpen);
    session.Click(session.ItemCenter("cut"));
    session.Frames(2);
    CHECK(cuts == 1);
}

TEST_CASE("Alt alone moves the focus to the first title of the menu bar")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    MenuProbe probe;
    session.SetUi([&] {
        ImGui::Button("content");
        session.Track("content");
        probe.Draw(session);
    });
    session.Frame();
    session.Click(session.ItemCenter("content"));
    session.KeyPress(ImGuiKey_LeftAlt);
    session.Frame();
    CHECK(ImGui::GetFocusID() == probe.fileTitle);
    CHECK(facet::internal::KeyboardModality());
}

