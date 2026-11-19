// The blocks of src/internal/ working together through the calls they offer each other: a list
// view inside a popup level, the drag and drop core scrolling a scroll core area, a reorder in a
// list view, and a text field handing its list keys to a completion level.

#include <cstdio>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/drag_drop.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/scroll.h"
#include "internal/text_field.h"
#include "support/session.h"

namespace fi = facet::internal;

namespace
{

struct CombinationGuard
{
    CombinationGuard()
    {
        facet::SetFollowSystemMotion(false);
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
    ~CombinationGuard()
    {
        facet::SetFollowSystemMotion(true);
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
};

const ImGuiID kComboLevel = ImHashStr("combo.level");
const char* const kComboRowType = "combo.row";

ImRect ComboLastItemRect()
{
    return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
}

} // namespace

TEST_CASE("A list view inside a popup level scrolls with the wheel and keeps it from the page")
{
    CombinationGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool open = false;
    ImGuiID listWindowId = 0;
    ImGuiID pageWindowId = 0;
    session.SetUi([&] {
        pageWindowId = ImGui::GetCurrentWindow()->ID;
        if (ImGui::Button("open"))
            fi::OpenPopupLevel(kComboLevel, {.rect = ComboLastItemRect(), .side = fi::PopupSide::Below});
        session.Track("open");
        if (fi::BeginPopupLevel(kComboLevel))
        {
            fi::ListViewDesc desc;
            desc.rowCount = 40;
            desc.areaSize = ImVec2(220.0f, 160.0f);
            if (fi::BeginListView(ImGui::GetID("list"), desc))
            {
                listWindowId = ImGui::GetCurrentWindow()->ID;
                facet::RowRange rows;
                while (facet::NextVisibleRows(&rows))
                    for (int i = rows.first; i < rows.last; ++i)
                    {
                        (void)fi::ListViewRow(i);
                        if (i == 0)
                            session.Track("row0");
                    }
                (void)fi::EndListView();
            }
            fi::EndPopupLevel();
        }
        open = fi::IsPopupLevelOpen(kComboLevel);
        // The page behind the level is long enough to scroll.
        ImGui::Dummy(ImVec2(10.0f, 2000.0f));
    });
    session.Frames(2);
    session.Click(session.ItemCenter("open"));
    session.Frames(3);
    REQUIRE(open);
    ImGuiWindow* list = ImGui::FindWindowByID(listWindowId);
    ImGuiWindow* page = ImGui::FindWindowByID(pageWindowId);
    REQUIRE(list != nullptr);
    REQUIRE(page != nullptr);
    REQUIRE(list->ScrollMax.y > 0.0f);
    const ImVec2 center = list->InnerRect.GetCenter();
    session.MouseMove(center);
    session.Frames(2);
    session.Wheel(center, -2.0f);
    session.Seconds(0.6f);
    CHECK(list->Scroll.y > 0.0f);
    CHECK(page->Scroll.y == 0.0f);
    CHECK(open);
    // Past the end of the list the wheel still stays in the level: the page never scrolls.
    for (int i = 0; i < 20; ++i)
        session.Wheel(center, -5.0f);
    session.Seconds(0.6f);
    CHECK(list->Scroll.y == doctest::Approx(list->ScrollMax.y));
    CHECK(page->Scroll.y == 0.0f);
    CHECK(open);
}

TEST_CASE("A drag resting at the edge of a scroll area scrolls it and drops where the rows moved to")
{
    CombinationGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    constexpr int kRows = 30;
    constexpr float kRowHeight = 24.0f;
    ImGuiID areaId = 0;
    ImRect areaRect;
    float areaScroll = 0.0f;
    std::vector<ImRect> slots;
    fi::DragDropZoneResult drop;
    bool delivered = false;
    int deliveredIndex = -1;
    int deliveredSource = -1;
    session.SetUi([&] {
        slots.clear();
        if (fi::BeginScrollAreaCore("area", {.size = ImVec2(300.0f, 150.0f)}))
        {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            areaId = window->ID;
            areaRect = window->InnerRect;
            areaScroll = window->Scroll.y;
            char name[16];
            for (int i = 0; i < kRows; ++i)
            {
                ImGui::PushID(i);
                const fi::ItemState row = fi::ItemBehavior(ImGui::GetID("row"), ImVec2(260.0f, kRowHeight));
                slots.push_back(row.rect);
                std::snprintf(name, sizeof(name), "row%d", i);
                session.Track(name);
                if (fi::DragDropBeginSource(kComboRowType, &i, sizeof(i)))
                    fi::DragDropEndSource();
                ImGui::PopID();
            }
            drop = fi::DragDropZone(ImGui::GetID("zone"), areaRect, kComboRowType, {.slots = slots});
            if (drop.delivered && drop.payload != nullptr)
            {
                delivered = true;
                deliveredIndex = drop.insertIndex;
                deliveredSource = *static_cast<const int*>(drop.payload->Data);
            }
            fi::EndScrollAreaCore();
        }
    });
    session.Frames(2);
    REQUIRE(areaId != 0);
    const ImVec2 start = session.ItemCenter("row0");
    session.MouseMove(start);
    session.MouseDown();
    session.MouseMove(ImVec2(start.x, start.y + 12.0f));
    const ImVec2 nearBottom(start.x, areaRect.Max.y - 4.0f);
    session.MouseMove(nearBottom);
    session.Seconds(0.5f);
    CHECK(fi::DragDropRequestedEdgeScroll(areaId));
    CHECK(areaScroll > 0.0f);
    // The area scrolled, so the row under the pointer is one that was out of sight at the start.
    const int visibleAtStart = static_cast<int>(150.0f / kRowHeight);
    REQUIRE(drop.hovered);
    CHECK(drop.insertIndex > visibleAtStart);
    const int shownIndex = drop.insertIndex;
    session.MouseUp();
    session.Frames(2);
    REQUIRE(delivered);
    CHECK(deliveredSource == 0);
    // The drop lands in the gap the last hovered frame showed, give or take the one row the area
    // may scroll on the release frame.
    CHECK(deliveredIndex >= shownIndex);
    CHECK(deliveredIndex <= shownIndex + 1);
    CHECK_FALSE(fi::DragDropActive(nullptr, nullptr));
}

TEST_CASE("A row dragged within a list view reports the move the caller applies")
{
    CombinationGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::vector<int> order = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<ImRect> slots;
    facet::ReorderMove move;
    int moves = 0;
    session.SetUi([&] {
        fi::ListViewDesc desc;
        desc.rowCount = static_cast<int>(order.size());
        desc.areaSize = ImVec2(300.0f, 400.0f);
        if (fi::BeginListView(ImGui::GetID("list"), desc))
        {
            slots.clear();
            int first = -1;
            facet::RowRange rows;
            char name[16];
            while (facet::NextVisibleRows(&rows))
                for (int i = rows.first; i < rows.last; ++i)
                {
                    const fi::ListRowState row = fi::ListViewRow(i);
                    if (first < 0)
                        first = i;
                    slots.push_back(row.rect);
                    std::snprintf(name, sizeof(name), "row%d", i);
                    session.Track(name);
                    if (fi::DragDropBeginSource(kComboRowType, &i, sizeof(i)))
                        fi::DragDropEndSource();
                }
            const fi::DragDropZoneResult drop = fi::DragDropZone(
                ImGui::GetID("zone"), ImGui::GetCurrentWindow()->InnerRect, kComboRowType, {.slots = slots});
            if (drop.delivered && drop.payload != nullptr)
            {
                const int source = *static_cast<const int*>(drop.payload->Data);
                move = fi::DragDropReorderMove(source, first + drop.insertIndex);
                ++moves;
            }
            (void)fi::EndListView();
        }
        if (move.IsValid())
        {
            facet::ApplyReorderMove(order, move);
            move = {};
        }
    });
    session.Frames(2);
    // All ten rows are visible, so the slots are the rows 0 to 9 in order.
    REQUIRE(slots.size() == order.size());
    const ImRect target = session.ItemRect("row5");
    const ImVec2 from = session.ItemCenter("row1");
    const ImVec2 to(target.GetCenter().x, target.Min.y + target.GetHeight() * 0.25f);
    session.DragAndHold({from, ImVec2(from.x, from.y + 12.0f), to}, 6, 4);
    session.Frames(2);
    CHECK(moves == 1);
    // Row 1 dropped before row 5 lands at index 4.
    CHECK(order == std::vector<int>{0, 2, 3, 4, 1, 5, 6, 7, 8, 9});
    // A drop onto its own place reports no move.
    const ImVec2 back = session.ItemCenter("row2");
    session.DragAndHold({back, ImVec2(back.x, back.y + 12.0f), ImVec2(back.x, back.y + 4.0f)}, 6, 4);
    session.Frames(2);
    CHECK(moves == 2);
    CHECK(order == std::vector<int>{0, 2, 3, 4, 1, 5, 6, 7, 8, 9});
}

TEST_CASE("A text field hands its list keys to an open completion level and keeps typing")
{
    CombinationGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string text;
    bool listOpen = false;
    bool fieldActive = false;
    int downs = 0;
    int escapes = 0;
    session.SetUi([&] {
        fi::TextFieldParams params;
        params.completionPopup = kComboLevel;
        const fi::TextFieldResult field = fi::TextFieldBehavior("field", &text, params);
        session.Track("field");
        fieldActive = field.active;
        if (field.edited && !text.empty())
            fi::OpenPopupLevel(kComboLevel, fi::TextFieldPopupAnchor(field, fi::TextFieldAnchor::Field),
                               {.takeFocus = false});
        if (fi::BeginPopupLevel(kComboLevel))
        {
            ImGui::TextUnformatted("Placeholder row");
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_None, kComboLevel))
                ++downs;
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, kComboLevel))
            {
                ++escapes;
                fi::ClosePopupLevel(kComboLevel);
            }
            fi::EndPopupLevel();
        }
        listOpen = fi::IsPopupLevelOpen(kComboLevel);
    });
    session.Frames(2);
    session.Click(session.ItemCenter("field"));
    REQUIRE(fieldActive);
    session.Type("a");
    session.Frames(2);
    REQUIRE(listOpen);
    CHECK(fieldActive);
    // Typing goes on in the field while the level is open.
    session.Type("b");
    session.Frames(2);
    CHECK(text == "ab");
    CHECK(listOpen);
    CHECK(fieldActive);
    // Down goes to the level.
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(downs == 1);
    CHECK(text == "ab");
    CHECK(fieldActive);
    // The first Escape closes the level and leaves the edit alone; the second cancels the edit.
    session.KeyPress(ImGuiKey_Escape);
    CHECK(escapes == 1);
    CHECK_FALSE(listOpen);
    CHECK(fieldActive);
    CHECK(text == "ab");
    session.KeyPress(ImGuiKey_Escape);
    CHECK(escapes == 1);
    CHECK_FALSE(fieldActive);
    CHECK(text.empty());
}
