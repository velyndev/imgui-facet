// The quiet layers look of the surfaces families (lists, navigation, containers, layout): the row,
// tile and ring rules of DESIGN.md checked on what the widgets draw.

#include <algorithm>
#include <cfloat>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/list_view.h"
#include "support/session.h"

namespace
{

struct SurfacesLookGuard
{
    SurfacesLookGuard() { facet::SetFollowSystemMotion(false); }
    ~SurfacesLookGuard()
    {
        facet::SetFollowSystemMotion(true);
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
};

// The light preset with a focus ring color nothing else uses, so ring vertices can be counted.
void SetSurfacesProbeTheme()
{
    facet::Theme theme = facet::Theme::Light();
    theme.colors.focusRing = facet::Rgb(0xFF00FF);
    facet::SetTheme(theme, false);
}

bool SameRgb(ImU32 a, ImU32 b)
{
    return (a & ~IM_COL32_A_MASK) == (b & ~IM_COL32_A_MASK) && (a & IM_COL32_A_MASK) != 0;
}

// The box around every vertex drawn this frame in `color` (any alpha); empty (Min > Max) when none.
ImRect ColorBounds(ImU32 color)
{
    ImRect box(ImVec2(FLT_MAX, FLT_MAX), ImVec2(-FLT_MAX, -FLT_MAX));
    const ImDrawData* data = ImGui::GetDrawData();
    if (data == nullptr)
        return box;
    for (const ImDrawList* list : data->CmdLists)
        for (const ImDrawVert& v : list->VtxBuffer)
            if (SameRgb(v.col, color))
                box.Add(v.pos);
    return box;
}

int ColorCount(ImU32 color, bool exact)
{
    const ImDrawData* data = ImGui::GetDrawData();
    int count = 0;
    if (data == nullptr)
        return 0;
    for (const ImDrawList* list : data->CmdLists)
        for (const ImDrawVert& v : list->VtxBuffer)
            count += (exact ? v.col == color : SameRgb(v.col, color)) ? 1 : 0;
    return count;
}

ImU32 RingColor()
{
    return facet::ToU32(facet::T().colors.focusRing);
}

} // namespace

TEST_CASE("Lists look: the chosen value of an open dropdown shows the selected row fill")
{
    SurfacesLookGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetSurfacesProbeTheme();
    static const char* const kItems[] = {"Compact", "Regular", "Spacious", "Wide"};
    int index = 1;
    session.SetUi([&] {
        facet::Dropdown("##density", &index, kItems);
        session.Track("field");
    });
    session.Frames(2);
    const ImU32 subtle = facet::ToU32(facet::T().colors.accentSubtle);
    CHECK(ColorCount(subtle, true) == 0);
    const ImRect field = session.ItemRect("field");
    session.Click(field.GetCenter());
    session.Frames(4);
    // The pointer on another row: that row shows the hover fill, the chosen one the selected fill.
    const float row = facet::internal::ListRowHeight(facet::ControlSize::Regular);
    session.MouseMove(ImVec2(field.GetCenter().x, field.Max.y + row * 3.75f));
    session.Frames(3);
    CHECK(ColorCount(subtle, true) > 0);
    // The open field has the accent outline; below it, in the list, no ring.
    const float listTop = field.Max.y + facet::internal::FocusRingOutset() + 1.0f;
    CHECK(ColorBounds(RingColor()).Max.y <= listTop);
    CHECK(index == 1);
}

TEST_CASE("Lists look: a combo marks its chosen row by fill and draws no ring for the pointer")
{
    SurfacesLookGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetSurfacesProbeTheme();
    static const char* const kItems[] = {"First", "Second", "Third", "Fourth"};
    int index = 1;
    session.SetUi([&] {
        if (facet::BeginCombo("##combo", kItems[index]))
        {
            for (int i = 0; i < 4; ++i)
                if (facet::ComboItem(kItems[i], i == index))
                    index = i;
            facet::EndCombo();
        }
        session.Track("field");
    });
    session.Frames(2);
    const ImRect field = session.ItemRect("field");
    session.Click(field.GetCenter());
    session.Frames(4);
    const float row = facet::internal::ListRowHeight(facet::ControlSize::Regular);
    session.MouseMove(ImVec2(field.GetCenter().x, field.Max.y + row * 3.6f));
    session.Frames(3);
    CHECK(ColorCount(facet::ToU32(facet::T().colors.accentSubtle), true) > 0);
    const float listTop = field.Max.y + facet::internal::FocusRingOutset() + 1.0f;
    CHECK(ColorBounds(RingColor()).Max.y <= listTop);
}

TEST_CASE("Lists look: a reorderable row shows no ring after a click and an inside ring after a key")
{
    SurfacesLookGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetSurfacesProbeTheme();
    static const char* const kItems[] = {"First", "Second", "Third"};
    facet::ReorderMove move;
    session.SetUi([&] {
        (void)facet::ReorderableList("##order", &move, kItems);
        session.Track("list");
    });
    session.Frames(2);
    const ImRect list = session.ItemRect("list");
    const float row = facet::internal::ListRowHeight(facet::ControlSize::Regular);
    session.Click(ImVec2(list.GetCenter().x, list.Min.y + row * 0.5f));
    session.Frames(3);
    CHECK(ColorCount(RingColor(), false) == 0);
    session.KeyPress(ImGuiKey_DownArrow);
    session.Frames(3);
    REQUIRE(ColorCount(RingColor(), false) > 0);
    const ImRect ring = ColorBounds(RingColor());
    CHECK(ring.Min.x >= list.Min.x - 0.5f);
    CHECK(ring.Max.x <= list.Max.x + 0.5f);
}

TEST_CASE("Overlays look: the pointer over command palette rows draws fills, never the ring")
{
    SurfacesLookGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetSurfacesProbeTheme();
    static const char* const kCommands[] = {"First", "Second", "Third", "Fourth", "Fifth"};
    bool open = true;
    int picked = -1;
    session.SetUi([&] { (void)facet::CommandPalette("Commands", &open, &picked, kCommands); });
    session.Frames(6);
    REQUIRE(open);
    // What the open palette draws in the ring color before the pointer moves (its search field
    // may show the accent outline of a focused field); hovering rows must add nothing to it.
    const int baseline = ColorCount(RingColor(), false);
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    bool hoverFill = false;
    for (float y = display.y * 0.1f; y < display.y * 0.9f; y += 8.0f)
    {
        session.MouseMove(ImVec2(display.x * 0.5f, y));
        session.Frames(2);
        CAPTURE(y);
        CHECK(ColorCount(RingColor(), false) == baseline);
        hoverFill = hoverFill || ColorCount(facet::ToU32(facet::T().colors.fill[2]), true) > 0;
    }
    CHECK(hoverFill);
    CHECK(picked == -1);
    // The keys move the current row (up from the last row the pointer rested on): now the ring
    // shows on it.
    session.KeyPress(ImGuiKey_UpArrow);
    session.Frames(2);
    CHECK(ColorCount(RingColor(), false) > baseline);
}

TEST_CASE("Navigation look: a sidebar item shows no ring after a click and an inside ring after a key")
{
    SurfacesLookGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetSurfacesProbeTheme();
    int selected = 0;
    session.SetUi([&] {
        if (facet::BeginSidebar("##nav", {.mode = facet::SidebarMode::Expanded}))
        {
            if (facet::SidebarItem("First", facet::Icon("list"), {.selected = selected == 0}))
                selected = 0;
            session.Track("first");
            if (facet::SidebarItem("Second", facet::Icon("grid"), {.selected = selected == 1}))
                selected = 1;
            session.Track("second");
            facet::EndSidebar();
        }
    });
    session.Frames(2);
    const ImRect second = session.ItemRect("second");
    session.Click(second.GetCenter());
    session.Frames(3);
    CHECK(selected == 1);
    CHECK(ColorCount(RingColor(), false) == 0);
    session.KeyPress(ImGuiKey_UpArrow);
    session.Frames(3);
    REQUIRE(ColorCount(RingColor(), false) > 0);
    // Inside the row: the ring never reaches past the item's own rectangle.
    const ImRect first = session.ItemRect("first");
    const ImRect ring = ColorBounds(RingColor());
    CHECK(ring.Min.x >= first.Min.x - 0.5f);
    CHECK(ring.Max.x <= first.Max.x + 0.5f);
    CHECK(ring.Min.y >= first.Min.y - 0.5f);
    CHECK(ring.Max.y <= first.Max.y + 0.5f);
}

TEST_CASE("Layout look: a tile selected by a click shows the selected tile look and no ring")
{
    SurfacesLookGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetSurfacesProbeTheme();
    ImGuiSelectionBasicStorage selection;
    session.SetUi([&] {
        if (facet::BeginItemGrid("##tiles", 6, &selection, {.minTileWidth = 60.0f, .height = 200.0f}))
        {
            facet::RowRange range;
            while (facet::NextVisibleRows(&range))
                for (int i = range.first; i < range.last; ++i)
                {
                    (void)facet::ItemGridTile(i);
                    if (i == 0)
                        session.Track("tile0");
                }
            facet::EndItemGrid();
        }
    });
    session.Frames(2);
    const ImRect tile = session.ItemRect("tile0");
    // Unselected tiles draw nothing in the accent (the grid may have taken a first selection).
    selection.Clear();
    session.Frames(2);
    CHECK(ColorCount(RingColor(), false) == 0);
    session.Click(tile.GetCenter());
    session.Frames(3);
    REQUIRE(selection.Contains(0));
    CHECK(ColorCount(facet::ToU32(facet::T().colors.accentSubtle), true) > 0);
    // The accent outline of the selected tile, and nothing more in the accent: it lies within the
    // tile (a ring outside would reach past it), one hairline wide along each edge.
    REQUIRE(ColorCount(RingColor(), false) > 0);
    const ImRect accent = ColorBounds(RingColor());
    CHECK(accent.Min.x >= tile.Min.x - 0.5f);
    CHECK(accent.Max.x <= tile.Max.x + 0.5f);
    CHECK(accent.Min.y >= tile.Min.y - 0.5f);
    CHECK(accent.Max.y <= tile.Max.y + 0.5f);
}

TEST_CASE("Layout look: the ring follows the keys to an earlier tile at once, never the fading tile")
{
    SurfacesLookGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetSurfacesProbeTheme();
    session.SetUi([&] {
        if (facet::BeginItemGrid("##tiles", 6, nullptr, {.minTileWidth = 60.0f, .height = 200.0f}))
        {
            facet::RowRange range;
            while (facet::NextVisibleRows(&range))
                for (int i = range.first; i < range.last; ++i)
                {
                    (void)facet::ItemGridTile(i);
                    if (i == 3)
                        session.Track("tile3");
                }
            facet::EndItemGrid();
        }
    });
    session.Frames(2);
    // A click on tile 3, then Right: tile 4 has the focus and its ring at full strength.
    session.Click(session.ItemCenter("tile3"));
    session.KeyPress(ImGuiKey_RightArrow);
    session.Frames(12);
    REQUIRE(ColorCount(RingColor(), false) > 0);
    const ImRect before = ColorBounds(RingColor());
    REQUIRE((before.Min.x > session.ItemRect("tile3").Max.x - 0.5f || before.Min.y > session.ItemRect("tile3").Max.y - 0.5f));
    // Left moves the focus to tile 3 while the ring of tile 4, submitted after it, fades out: the
    // ring is drawn in tile 3 while tile 4 still fades (two frames, well inside durationFast).
    session.KeyPress(ImGuiKey_LeftArrow);
    session.Frames(2);
    const ImRect tile = session.ItemRect("tile3");
    REQUIRE(ColorCount(RingColor(), false) > 0);
    const ImRect ring = ColorBounds(RingColor());
    CHECK(ring.Min.x >= tile.Min.x - 0.5f);
    CHECK(ring.Max.x <= tile.Max.x + 0.5f);
    CHECK(ring.Min.y >= tile.Min.y - 0.5f);
    CHECK(ring.Max.y <= tile.Max.y + 0.5f);
}

TEST_CASE("Containers look: the inspector pane edge keeps its width under the pointer")
{
    SurfacesLookGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetSurfacesProbeTheme();
    session.SetUi([&] {
        facet::BeginInspectorPane("Details", nullptr, {.height = 200.0f});
        ImGui::TextUnformatted("Main");
        if (facet::InspectorPaneNext())
        {
            ImGui::TextUnformatted("Pane");
            session.Track("pane-text");
        }
        facet::EndInspectorPane();
    });
    session.Frames(3);
    CHECK(ColorCount(RingColor(), false) == 0);
    // The pane's leading edge sits left of its content by the content padding; walk toward it until
    // the edge takes the accent.
    const ImRect text = session.ItemRect("pane-text");
    bool lit = false;
    for (float x = text.Min.x; x > text.Min.x - 60.0f && !lit; x -= 1.0f)
    {
        session.MouseMove(ImVec2(x, text.GetCenter().y));
        session.Frames(1);
        lit = ColorCount(RingColor(), false) > 0;
    }
    REQUIRE(lit);
    const ImRect edge = ColorBounds(RingColor());
    // One hairline wide, as at rest: a state changes the color of an edge, not its width.
    CHECK(edge.GetWidth() <= facet::HairlineThickness() + 0.01f);
}
