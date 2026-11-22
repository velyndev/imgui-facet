// Focus ring and keyboard modality (src/internal/focus.h).

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "support/session.h"

using facet::internal::ItemBehavior;
using facet::internal::ItemState;
using facet::internal::KeyboardModality;

namespace
{

// Motion and theme as the presets have them, whatever the Windows animation setting; restored
// at the end of the case.
struct ThemeGuard
{
    ThemeGuard() { facet::SetFollowSystemMotion(false); }
    ~ThemeGuard()
    {
        facet::SetFollowSystemMotion(true);
        facet::ClearAccent();
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
};

// The positions of the vertices drawn into `drawList` from index `from` on, in index order.
std::vector<ImVec2> Positions(const ImDrawList* drawList, int from)
{
    std::vector<ImVec2> positions;
    for (const ImDrawCmd& cmd : drawList->CmdBuffer)
    {
        const int end = static_cast<int>(cmd.IdxOffset + cmd.ElemCount);
        for (int k = std::max(static_cast<int>(cmd.IdxOffset), from); k < end; ++k)
            positions.push_back(drawList->VtxBuffer[static_cast<int>(cmd.VtxOffset + drawList->IdxBuffer[k])].pos);
    }
    return positions;
}

} // namespace

TEST_CASE("Keyboard modality: navigation keys turn it on, the mouse turns it off")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] { ImGui::Button("a"); session.Track("a"); ImGui::Button("b"); });
    session.Frames(2);
    CHECK_FALSE(KeyboardModality());  // a fresh start
    session.KeyPress(ImGuiKey_Tab);
    CHECK(KeyboardModality());
    // Typing text and modifiers alone keep it.
    session.Type("abc");
    session.KeyPress(ImGuiMod_Shift);
    CHECK(KeyboardModality());
    // Mouse movement keeps it, a click ends it.
    session.MouseMove(ImVec2(300.0f, 300.0f));
    CHECK(KeyboardModality());
    session.Click(session.ItemCenter("a"));
    CHECK_FALSE(KeyboardModality());
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(KeyboardModality());
    session.Wheel(ImVec2(300.0f, 300.0f), -1.0f);
    CHECK_FALSE(KeyboardModality());
    session.KeyPress(ImGuiKey_Space);
    CHECK(KeyboardModality());
}

TEST_CASE("Keyboard modality is reset by a new start")
{
    {
        facet::test::Session session;
        REQUIRE(session.Ready());
        session.SetUi([] { ImGui::Button("a"); });
        session.KeyPress(ImGuiKey_Tab);
        CHECK(KeyboardModality());
    }
    facet::test::Session session;
    REQUIRE(session.Ready());
    CHECK_FALSE(KeyboardModality());
}

TEST_CASE("Focus ring: the color reaches 3:1 on every surface for every accent in both themes")
{
    ThemeGuard guard;
    for (const bool dark : {false, true})
    {
        for (const facet::AccentPreset& preset : facet::AccentPresets())
        {
            facet::Theme theme = dark ? facet::Theme::Dark() : facet::Theme::Light();
            theme.ApplyAccent(preset.color);
            facet::SetTheme(theme, false);
            const ImVec4 ring = facet::internal::FocusRingColor();
            const facet::Colors& c = facet::T().colors;
            const ImVec4* surfaces[] = {&c.bg, &c.bgSecondary, &c.bgTertiary, &c.bgElevated, &c.bgElevatedLevels[0],
                                        &c.bgElevatedLevels[1], &c.bgElevatedLevels[2]};
            for (const ImVec4* surface : surfaces)
            {
                INFO((dark ? "dark " : "light ") << preset.name);
                CHECK(facet::ContrastRatio(ring, *surface) >= 3.0f);
            }
        }
    }
}

TEST_CASE("Focus ring: it sits just outside the control and fades with the amount")
{
    ThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    facet::SetUiScale(1.0f);
    const ImRect control(100.0f, 100.0f, 200.0f, 128.0f);
    const ImRect ring = facet::internal::FocusRingRect(control);
    const float offset = facet::HairlineThickness() + facet::PxSnapped(facet::T().metrics.focusRingWidth);
    CHECK(ring.Min.x == control.Min.x - offset);
    CHECK(ring.Max.y == control.Max.y + offset);

    int verticesNone = -1;
    int verticesFull = -1;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int start = drawList->VtxBuffer.Size;
        facet::internal::DrawFocusRing(drawList, control, 8.0f, 0.0f);
        verticesNone = drawList->VtxBuffer.Size - start;
        facet::internal::DrawFocusRing(drawList, control, 8.0f, 1.0f);
        verticesFull = drawList->VtxBuffer.Size - start;
    });
    session.Frame();
    CHECK(verticesNone == 0);
    CHECK(verticesFull > 0);
}

TEST_CASE("Focus ring: the round shape draws circular arcs around a circle")
{
    ThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    facet::SetUiScale(1.0f);
    // A knob drawn as a circle of radius 12, like the slider knobs. Its round ring must be a
    // circular-arc stroke of the ring rectangle at the knob radius plus the outset (no halo),
    // which for a square with half its side as the radius is a circle concentric to the knob.
    // The theme's corners would make a squircle, whose gap to the knob is wider at the diagonals.
    const ImRect knob(100.0f, 100.0f, 124.0f, 124.0f);
    const float ringRadius = 12.0f + facet::internal::FocusRingOutset();
    const float width = facet::PxSnapped(facet::T().metrics.focusRingWidth);
    std::vector<ImVec2> ring;
    std::vector<ImVec2> expected;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImRect outer = facet::internal::FocusRingRect(knob);
        const ImU32 color = facet::ToU32(facet::internal::FocusRingColor());
        int from = drawList->IdxBuffer.Size;
        facet::internal::DrawFocusRing(drawList, knob, 12.0f, 1.0f, 0, facet::internal::RingShape::Round);
        ring = Positions(drawList, from);
        from = drawList->IdxBuffer.Size;
        facet::StrokeRoundRect(drawList, outer.Min, outer.Max, color, ringRadius, width);
        expected = Positions(drawList, from);
    });
    session.Frames(3);
    REQUIRE_FALSE(ring.empty());
    REQUIRE(ring.size() == expected.size());
    for (size_t i = 0; i < ring.size(); ++i)
    {
        INFO(i);
        CHECK(ring[i].x == expected[i].x);
        CHECK(ring[i].y == expected[i].y);
    }
}

TEST_CASE("Focus ring: an item shows it only after keyboard input, animated, and clipped by its window")
{
    ThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    ItemState state;
    ImVec4 childClip;
    ImVec4 ringClip;
    session.SetUi([&] {
        ImGui::Button("first");
        ImGui::BeginChild("child", ImVec2(200.0f, 100.0f));
        state = ItemBehavior(ImGui::GetID("item"), ImVec2(80.0f, 24.0f));
        session.Track("item");
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        childClip = drawList->_ClipRectStack.back();
        facet::internal::DrawItemFocusRing(state, 8.0f);
        ringClip = drawList->CmdBuffer.back().ClipRect;
        ImGui::EndChild();
    });
    session.Frames(2);
    // Mouse: focused, no ring.
    session.Click(session.ItemCenter("item"));
    CHECK(state.focused);
    CHECK_FALSE(state.focusVisible);
    CHECK(state.focusAmount == 0.0f);
    // Keyboard: the ring fades in over the fast duration.
    facet::internal::SetKeyboardModality(true);
    session.Frame();
    CHECK(state.focusVisible);
    CHECK(state.focusAmount > 0.0f);
    CHECK(state.focusAmount < 1.0f);
    session.Seconds(facet::T().motion.durationFast + 0.05f);
    CHECK(state.focusAmount == 1.0f);
    // Drawn with the child window's clip rectangle.
    CHECK(ringClip.x == childClip.x);
    CHECK(ringClip.z == childClip.z);
    CHECK(ringClip.w == childClip.w);

    // With reduced motion the ring is there at once.
    facet::Theme theme = facet::Theme::Light();
    theme.motion.reduceMotion = true;
    facet::SetTheme(theme, false);
    session.Click(ImVec2(500.0f, 500.0f));  // focus elsewhere, modality off
    session.Frame();
    session.Click(session.ItemCenter("item"));
    facet::internal::SetKeyboardModality(true);
    session.Frame();
    CHECK(state.focusAmount == 1.0f);
}

namespace
{

// A theme whose ring color no other part of the tested widgets uses, so every vertex in it is a ring
// (or the accent outline of a focused field, which the tests keep out of their count by position).
void SetRingProbeTheme()
{
    facet::Theme theme = facet::Theme::Light();
    theme.colors.focusRing = facet::Rgb(0xFF00FF);
    facet::SetTheme(theme, false);
}

// Vertices in the ring color (at any alpha, so a ring fading in counts) drawn into any window this
// frame below `belowY` (screen pixels).
int RingVerticesBelow(float belowY)
{
    const ImU32 ring = facet::ToU32(facet::T().colors.focusRing);
    const ImDrawData* data = ImGui::GetDrawData();
    int count = 0;
    if (data == nullptr)
        return 0;
    for (const ImDrawList* list : data->CmdLists)
        for (const ImDrawVert& v : list->VtxBuffer)
            count += ((v.col & ~IM_COL32_A_MASK) == (ring & ~IM_COL32_A_MASK) && (v.col & IM_COL32_A_MASK) != 0 && v.pos.y > belowY) ? 1 : 0;
    return count;
}

// Vertices in `color` drawn into any window this frame below `belowY`.
int ColorVerticesBelow(ImU32 color, float belowY)
{
    const ImDrawData* data = ImGui::GetDrawData();
    int count = 0;
    if (data == nullptr)
        return 0;
    for (const ImDrawList* list : data->CmdLists)
        for (const ImDrawVert& v : list->VtxBuffer)
            count += (v.col == color && v.pos.y > belowY) ? 1 : 0;
    return count;
}

} // namespace

TEST_CASE("Focus ring: two units wide, no halo, and inside rows it stays within the row")
{
    ThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    facet::SetUiScale(1.0f);
    CHECK(facet::T().metrics.focusRingWidth == 2.0f);
    CHECK(facet::internal::FocusRingWidth() == 2.0f);
    CHECK(facet::internal::FocusRingOutset() == facet::HairlineThickness() + 2.0f);
    const ImRect control(100.0f, 100.0f, 200.0f, 128.0f);
    std::vector<ImVec2> outside;
    std::vector<ImVec2> inside;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        int from = drawList->IdxBuffer.Size;
        facet::internal::DrawFocusRing(drawList, control, 8.0f, 1.0f);
        outside = Positions(drawList, from);
        from = drawList->IdxBuffer.Size;
        facet::internal::DrawFocusRingInside(drawList, control, 8.0f, 1.0f);
        inside = Positions(drawList, from);
    });
    session.Frame();
    REQUIRE_FALSE(outside.empty());
    REQUIRE_FALSE(inside.empty());
    // Without a halo nothing reaches past the ring (half a pixel of anti-aliasing aside), and the
    // ring keeps clear of the control by one hairline.
    const ImRect ring = facet::internal::FocusRingRect(control);
    for (const ImVec2& p : outside)
    {
        CHECK(p.x >= ring.Min.x - 1.0f);
        CHECK(p.x <= ring.Max.x + 1.0f);
        CHECK(p.y >= ring.Min.y - 1.0f);
        CHECK(p.y <= ring.Max.y + 1.0f);
        // Along the straight top and bottom edges (the rounded corners cut into the bounding box).
        if (p.x > control.Min.x + 20.0f && p.x < control.Max.x - 20.0f)
            CHECK_FALSE((p.y > control.Min.y + 0.5f && p.y < control.Max.y - 0.5f));
    }
    // Inside: within the rectangle and no deeper than the ring width.
    for (const ImVec2& p : inside)
    {
        CHECK(p.x >= control.Min.x - 1.0f);
        CHECK(p.x <= control.Max.x + 1.0f);
        CHECK(p.y >= control.Min.y - 1.0f);
        CHECK(p.y <= control.Max.y + 1.0f);
    }
}

TEST_CASE("Focus ring: a dropdown opened by a click shows none on hovered rows; an arrow key shows it")
{
    ThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetRingProbeTheme();
    static const char* const kItems[] = {"Compact", "Regular", "Spacious", "Wide"};
    int index = 1;
    session.SetUi([&] {
        facet::Dropdown("##density", &index, kItems);
        session.Track("field");
    });
    session.Frames(2);
    const ImRect field = session.ItemRect("field");
    // Below the field and its own ring and outline.
    const float listTop = field.Max.y + facet::internal::FocusRingOutset() + 1.0f;
    session.Click(field.GetCenter());
    session.Frames(4);
    const float row = facet::internal::ListRowHeight(facet::ControlSize::Regular);
    for (int target = 0; target < 4; ++target)
    {
        session.MouseMove(ImVec2(field.GetCenter().x, field.Max.y + row * (static_cast<float>(target) + 0.75f)));
        session.Frames(3);
        CAPTURE(target);
        CHECK(RingVerticesBelow(listTop) == 0);
    }
    // The hovered row shows its fill: the list is open and the pointer is on it.
    CHECK(ColorVerticesBelow(facet::ToU32(facet::T().colors.fill[2]), listTop) > 0);
    // An arrow key moves the current row (up from the last one the pointer rested on): now the keys
    // put it there, and the ring shows.
    session.KeyPress(ImGuiKey_UpArrow);
    session.Frames(2);
    CHECK(RingVerticesBelow(listTop) > 0);
}

TEST_CASE("Focus ring: a click on a list row shows none; an arrow key shows it inside the row")
{
    ThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetRingProbeTheme();
    static const char* const kItems[] = {"First", "Second", "Third", "Fourth"};
    int index = -1;
    session.SetUi([&] {
        facet::ListBox("##list", &index, kItems);
        session.Track("list");
    });
    session.Frames(2);
    const ImRect list = session.ItemRect("list");
    const float row = facet::internal::ListRowHeight(facet::ControlSize::Regular);
    session.Click(ImVec2(list.GetCenter().x, list.Min.y + row * 1.5f));
    session.Frames(3);
    CHECK(index == 1);
    CHECK(RingVerticesBelow(list.Min.y - 10.0f) == 0);
    session.KeyPress(ImGuiKey_DownArrow);
    session.Frames(3);
    CHECK(index == 2);
    CHECK(RingVerticesBelow(list.Min.y - 10.0f) > 0);
    // The ring of a row stays inside the list's area.
    const ImU32 ring = facet::ToU32(facet::T().colors.focusRing);
    for (const ImDrawList* drawList : ImGui::GetDrawData()->CmdLists)
        for (const ImDrawVert& v : drawList->VtxBuffer)
            if ((v.col & ~IM_COL32_A_MASK) == (ring & ~IM_COL32_A_MASK) && (v.col & IM_COL32_A_MASK) != 0)
            {
                CHECK(v.pos.x >= list.Min.x - 0.5f);
                CHECK(v.pos.x <= list.Max.x + 0.5f);
            }
}

TEST_CASE("Focus ring: a click on a data table row shows none")
{
    ThemeGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SetRingProbeTheme();
    static const facet::DataTableColumn kColumns[] = {{"Name"}, {"Value"}};
    ImGuiSelectionBasicStorage selection;
    session.SetUi([&] {
        if (facet::BeginDataTable("table", kColumns, 20, &selection, {.height = 240.0f}))
        {
            facet::RowRange rows;
            while (facet::NextVisibleRows(&rows))
                for (int i = rows.first; i < rows.last; ++i)
                {
                    facet::DataTableRow(i);
                    ImGui::TableNextColumn();
                    ImGui::Text("Placeholder %d", i);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", i * 3);
                }
            facet::EndDataTable();
        }
        session.Track("table");
    });
    session.Frames(2);
    const ImRect table = session.ItemRect("table");
    session.Click(ImVec2(table.GetCenter().x, table.Min.y + facet::T().table.headerHeight +
                                                  facet::internal::ListRowHeight(facet::ControlSize::Regular) * 2.5f));
    session.Frames(3);
    CHECK(selection.Size == 1);
    CHECK(RingVerticesBelow(table.Min.y - 10.0f) == 0);
    session.KeyPress(ImGuiKey_DownArrow);
    session.Frames(3);
    CHECK(RingVerticesBelow(table.Min.y - 10.0f) > 0);
}
