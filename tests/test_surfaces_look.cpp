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
