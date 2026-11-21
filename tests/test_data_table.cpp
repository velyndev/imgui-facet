// Behaviour of DataTable and its cell editing (facet/widgets/display.h).
//
// One test case per behaviour; docs/writing-a-widget.md describes the pattern. The API checks are
// in tests/test_api_data_table.cpp. Every case builds its own Session and table, so it passes alone
// (facet_tests -tc="name") and in the full suite.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/scope_stack.h"
#include "internal/popup_host.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "support/session.h"
#include "widgets/display/data_table_internal.h"

namespace
{

namespace fi = facet::internal;

// Motion jumps, so positions and amounts do not depend on time; restored when the case ends.
struct MotionGuard
{
    facet::Theme saved = facet::T();
    MotionGuard()
    {
        facet::Theme theme = saved;
        theme.motion.reduceMotion = true;
        facet::SetTheme(theme, false);
    }
    ~MotionGuard() { facet::SetTheme(saved, false); }
};

// A table of `rows` placeholder items with four columns: Index (fixed, frozen with `frozen`),
// Name (editable), Group and Value (fixed widths, so the table scrolls sideways when narrow). The
// items are kept in `order` (row -> item); the selection keys rows by item through the adapter, so
// it follows the items across a sort.
struct TableUi
{
    int rows = 50;
    std::vector<int> order;
    std::vector<std::string> names;
    std::vector<int> values;
    ImGuiSelectionBasicStorage selection;
    facet::DataTableOptions options;
    bool useSelection = true;
    float width = 600.0f;   // units
    float height = 300.0f;  // units
    int visibleRowCalls = 0;
    int activations = 0;
    int lastActivated = -1;
    int sorts = 0;
    int commits = 0;
    int cancels = 0;
    int editFrames = 0;
    bool open = false;
    bool disabled = false;
    int columnOrder[4] = {0, 1, 2, 3};
    bool columnShown[4] = {true, true, true, true};
    float columnWidth[4] = {};
    ImRect rowRect[8];
    ImGuiID rowId[8] = {};
    bool rowSeen[8] = {};
    ImRect editorRect;
    ImGuiID editorId = 0;

    static constexpr facet::DataTableColumn kColumns[4] = {
        {.label = "Index", .width = 60.0f, .align = facet::Align::End},
        {.label = "Name", .width = 160.0f, .editable = true},
        {.label = "Group", .width = 120.0f},
        {.label = "Value", .width = 120.0f, .align = facet::Align::End},
    };

    explicit TableUi(int count = 50) : rows(count)
    {
        order.resize(static_cast<size_t>(rows));
        names.resize(static_cast<size_t>(rows));
        values.resize(static_cast<size_t>(rows));
        char text[32];
        for (int i = 0; i < rows; ++i)
        {
            order[static_cast<size_t>(i)] = i;
            std::snprintf(text, sizeof(text), "Item %d", i);
            names[static_cast<size_t>(i)] = text;
            values[static_cast<size_t>(i)] = (i * 37) % 101;
        }
        selection.UserData = this;
        selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int row) {
            const auto* ui = static_cast<const TableUi*>(self->UserData);
            return static_cast<ImGuiID>(ui->order[static_cast<size_t>(row)] + 1);
        };
        options.sortable = true;
        options.multiSort = true;
        options.reorderable = true;
        options.hideable = true;
    }

    // The item a storage id stands for.
    static int ItemOf(ImGuiID id) { return static_cast<int>(id) - 1; }

    void Sort(ImGuiTableSortSpecs* specs)
    {
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            for (int s = 0; s < specs->SpecsCount; ++s)
            {
                const ImGuiTableColumnSortSpecs& spec = specs->Specs[s];
                int lhs = a;
                int rhs = b;
                if (spec.ColumnIndex == 3)
                {
                    lhs = values[static_cast<size_t>(a)];
                    rhs = values[static_cast<size_t>(b)];
                }
                else if (spec.ColumnIndex == 2)
                {
                    lhs = a % 3;
                    rhs = b % 3;
                }
                if (lhs != rhs)
                    return spec.SortDirection == ImGuiSortDirection_Ascending ? lhs < rhs : lhs > rhs;
            }
            return a < b;
        });
        ++sorts;
    }

    void Ui(facet::test::Session& session)
    {
        std::fill(std::begin(rowSeen), std::end(rowSeen), false);
        visibleRowCalls = 0;
        editorId = 0;
        ImGui::Button("before");
        session.Track("before");
        if (disabled)
            ImGui::BeginDisabled();
        facet::DataTableOptions o = options;
        o.width = width;
        o.height = height;
        tableId = ImGui::GetID("table");
        open = facet::BeginDataTable("table", kColumns, rows, useSelection ? &selection : nullptr, o);
        if (open)
        {
            ImGuiTable* table = ImGui::GetCurrentTable();
            for (int n = 0; n < 4; ++n)
            {
                columnOrder[n] = table->Columns[n].DisplayOrder;
                columnShown[n] = table->Columns[n].IsEnabled;
                columnWidth[n] = table->Columns[n].MaxX - table->Columns[n].MinX;
            }
            if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs != nullptr && specs->SpecsDirty)
            {
                Sort(specs);
                specs->SpecsDirty = false;
            }
            facet::RowRange range;
            char name[16];
            while (facet::NextVisibleRows(&range))
                for (int i = range.first; i < range.last; ++i)
                {
                    ++visibleRowCalls;
                    const int item = order[static_cast<size_t>(i)];
                    if (facet::DataTableRow(i))
                    {
                        ++activations;
                        lastActivated = i;
                    }
                    if (i < 8)
                    {
                        rowRect[i] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                        rowId[i] = ImGui::GetItemID();
                        rowSeen[i] = true;
                    }
                    std::snprintf(name, sizeof(name), "row%d", i);
                    session.Track(name);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", item);
                    ImGui::TableNextColumn();
                    if (facet::BeginDataTableCellEdit())
                    {
                        ++editFrames;
                        facet::TextField("##edit", &names[static_cast<size_t>(item)]);
                        editorRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                        editorId = ImGui::GetItemID();
                        if (facet::IsItemEditCommitted())
                            ++commits;
                        if (facet::IsItemEditCanceled())
                            ++cancels;
                        facet::EndDataTableCellEdit();
                    }
                    else
                    {
                        ImGui::TextUnformatted(names[static_cast<size_t>(item)].c_str());
                    }
                    ImGui::TableNextColumn();
                    ImGui::Text("Group %d", item % 3);
                    ImGui::TableNextColumn();
                    ImGui::Text("%d", values[static_cast<size_t>(item)]);
                }
            facet::EndDataTable();
        }
        session.Track("table");
        // The popup host answers inside a frame only.
        menuOpen = fi::IsPopupLevelOpen(ImHashStr("##facet.columns", 0, tableId));
        listOpen = fi::IsPopupLevelOpen(ImHashStr("##facet.columns.list", 0, tableId));
        if (disabled)
            ImGui::EndDisabled();
        ImGui::Button("after");
        session.Track("after");
    }

    ImGuiID TableId() const { return tableId; }
    ImGuiTable* Table() const { return ImGui::TableFindByID(tableId); }
    fi::DataTableState& State() const { return fi::GetState<fi::DataTableState>(tableId); }

    // The header cell of column `column` in the latest frame (screen pixels).
    ImRect Header(int column) const
    {
        const ImGuiTable* table = Table();
        const ImGuiTableColumn& c = table->Columns[column];
        const float top = table->OuterRect.Min.y;
        return ImRect(c.MinX, top, c.MaxX, top + facet::Px(facet::T().table.headerHeight));
    }
    // The id of the header item of column `column`.
    ImGuiID HeaderId(int column) const
    {
        const ImGuiID columnSeed = ImHashData(&column, sizeof(column), tableId);
        return ImHashStr("##header", 0, columnSeed);
    }
    // The center of row `row`'s cell in column `column`.
    ImVec2 Cell(facet::test::Session& session, int row, int column) const
    {
        const ImRect rect = session.ItemRect("row" + std::to_string(row));
        return ImVec2(Header(column).GetCenter().x, rect.GetCenter().y);
    }

    ImGuiID tableId = 0;
    bool menuOpen = false;
    bool listOpen = false;
};

} // namespace

TEST_CASE("DataTable: a table with rows opens, and only the visible rows are submitted")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(100000);
    ui.options.sortable = false;
    session.SetUi([&] { ui.Ui(session); });
    session.Frame();
    CHECK(ui.open);
    session.Frames(2);
    CHECK(ui.open);
    CHECK(ui.visibleRowCalls > 0);
    CHECK(ui.visibleRowCalls < 30);
    CHECK(fi::OpenScopeCount() == 0);
    // The rows sit one row height apart, right under the header.
    const float rowHeight = facet::Px(facet::T().list.rowHeight);
    CHECK(ui.rowRect[1].Min.y - ui.rowRect[0].Min.y == doctest::Approx(rowHeight));
    CHECK(ui.rowRect[0].Min.y == doctest::Approx(ui.Header(0).Max.y));
    // The last row is reached and drawn at its place at the end of the scroll range.
    ImGuiWindow* inner = ui.Table()->InnerWindow;
    ImGui::SetScrollY(inner, inner->ScrollMax.y);
    session.Frames(3);
    CHECK(ui.visibleRowCalls < 30);
    CHECK(session.ItemVisible("row99999"));
}

TEST_CASE("DataTable: a frame with a hundred thousand rows allocates nothing")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(100000);
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(10);
    session.MouseMove(session.ItemCenter("row2"));  // a hovered row
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
    session.Frames(20);
    ImGui::SetAllocatorFunctions(oldAlloc, oldFree, oldUser);
    CHECK(allocations == 0);
}

TEST_CASE("DataTable: loading, error and empty show under the header and return false")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(20);
    for (const facet::DataState state : {facet::DataState::Loading, facet::DataState::Error, facet::DataState::Ready})
    {
        ui.options.state = state;
        ui.rows = state == facet::DataState::Ready ? 0 : 20;
        session.SetUi([&] { ui.Ui(session); });
        session.Frames(2);
        CHECK_FALSE(ui.open);
        CHECK(ui.visibleRowCalls == 0);
        CHECK(fi::OpenScopeCount() == 0);
        // The header stays: Tab from the item before the table reaches its first column.
        session.Click(session.ItemCenter("before"));
        session.KeyPress(ImGuiKey_Tab);
        session.KeyPress(ImGuiKey_Tab);
        CHECK(GImGui->NavId == ui.HeaderId(0));
        // The table is one item of its whole size.
        CHECK(session.ItemRect("table").GetHeight() == doctest::Approx(facet::Px(ui.height)));
    }
}

TEST_CASE("DataTable: a header click sorts, and the sort specs are dirty once per change")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(30);
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    const int initial = ui.sorts;  // Dear ImGui marks new specs dirty once
    CHECK(initial == 1);
    session.Click(ui.Header(3).GetCenter());
    session.Frames(5);
    CHECK(ui.sorts == initial + 1);
    for (int row = 1; row < 10; ++row)
        CHECK(ui.values[static_cast<size_t>(ui.order[static_cast<size_t>(row - 1)])] <=
              ui.values[static_cast<size_t>(ui.order[static_cast<size_t>(row)])]);
    // A second click turns the direction.
    session.Click(ui.Header(3).GetCenter());
    session.Frames(5);
    CHECK(ui.sorts == initial + 2);
    CHECK(ui.values[static_cast<size_t>(ui.order[0])] >= ui.values[static_cast<size_t>(ui.order[1])]);
    // Shift adds a second column with multiSort.
    session.KeyDown(ImGuiMod_Shift);
    session.Click(ui.Header(2).GetCenter());
    session.KeyUp(ImGuiMod_Shift);
    session.Frames(3);
    CHECK(ui.sorts == initial + 3);
    CHECK(ui.Table()->SortSpecsCount == 2);
    // A focused header sorts with Enter.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(GImGui->NavId == ui.HeaderId(0));
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(3);
    CHECK(ui.sorts == initial + 4);
    CHECK(ui.Table()->SortSpecsCount == 1);
    CHECK(ui.Table()->Columns[0].SortOrder == 0);
}

TEST_CASE("DataTable: click, Ctrl and Shift select rows, and the selection follows the items across a sort")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(30);
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    session.Click(session.ItemCenter("row2"));
    CHECK(ui.selection.Size == 1);
    CHECK(ui.selection.Contains(static_cast<ImGuiID>(ui.order[2] + 1)));
    session.KeyDown(ImGuiMod_Ctrl);
    session.Click(session.ItemCenter("row4"));
    session.KeyUp(ImGuiMod_Ctrl);
    CHECK(ui.selection.Size == 2);
    session.KeyDown(ImGuiMod_Shift);
    session.Click(session.ItemCenter("row6"));
    session.KeyUp(ImGuiMod_Shift);
    // Shift without Ctrl: the range from the last click (row 4) to row 6.
    CHECK(ui.selection.Size == 3);
    const int anchorItem = ui.order[4];
    std::vector<int> selected;
    for (int row = 4; row <= 6; ++row)
        selected.push_back(ui.order[static_cast<size_t>(row)]);

    // Sort by value: the same items stay selected wherever they are now.
    session.Click(ui.Header(3).GetCenter());
    session.Frames(3);
    CHECK(ui.selection.Size == 3);
    for (const int item : selected)
        CHECK(ui.selection.Contains(static_cast<ImGuiID>(item + 1)));
    // Shift+click after the sort runs from the item the range started at, at its new row.
    int anchorRow = -1;
    for (int row = 0; row < ui.rows; ++row)
        if (ui.order[static_cast<size_t>(row)] == anchorItem)
            anchorRow = row;
    REQUIRE(anchorRow >= 0);
    const int target = anchorRow > 3 ? 0 : 6;
    session.KeyDown(ImGuiMod_Shift);
    session.Click(session.ItemCenter("row" + std::to_string(target)));
    session.KeyUp(ImGuiMod_Shift);
    CHECK(ui.selection.Size == std::abs(anchorRow - target) + 1);
    CHECK(ui.selection.Contains(static_cast<ImGuiID>(anchorItem + 1)));
}

TEST_CASE("DataTable: the row spans every column, also while the table scrolls sideways")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(30);
    ui.width = 300.0f;  // narrower than its columns
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    const ImGuiTable* table = ui.Table();
    const float outset = fi::FocusRingOutset();
    const ImRect row = ui.rowRect[1];
    CHECK(row.Min.x == doctest::Approx(table->InnerClipRect.Min.x + outset));
    CHECK(row.Max.x == doctest::Approx(table->InnerClipRect.Max.x - outset));
    // Scrolled sideways, the row still covers the visible part of every column.
    ImGuiWindow* inner = table->InnerWindow;
    REQUIRE(inner->ScrollMax.x > 0.0f);
    ImGui::SetScrollX(inner, inner->ScrollMax.x * 0.5f);
    session.Frames(3);
    const ImRect scrolled = ui.rowRect[1];
    CHECK(scrolled.Min.x == doctest::Approx(ui.Table()->InnerClipRect.Min.x + outset));
    CHECK(scrolled.Max.x == doctest::Approx(ui.Table()->InnerClipRect.Max.x - outset));
    // A click anywhere along it selects it, the focus ring then shows around it after a key.
    session.Click(ImVec2(scrolled.Min.x + scrolled.GetWidth() * 0.75f, scrolled.GetCenter().y));
    CHECK(ui.selection.Size == 1);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(GImGui->NavId == ui.rowId[2]);
    CHECK(fi::KeyboardModality());
}

TEST_CASE("DataTable: the arrows, Home and End move the focused row, and Enter activates it")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(200);
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    session.Click(session.ItemCenter("row1"));
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(GImGui->NavId == ui.rowId[2]);
    CHECK(ui.State().focusIndex == 2);
    CHECK(ui.selection.Contains(static_cast<ImGuiID>(ui.order[2] + 1)));
    session.KeyPress(ImGuiKey_End);
    session.Frames(2);
    CHECK(ui.State().focusIndex == 199);
    CHECK(session.ItemVisible("row199"));
    CHECK(ui.selection.Size == 1);
    session.KeyPress(ImGuiKey_Home);
    session.Frames(2);
    CHECK(ui.State().focusIndex == 0);
    CHECK(session.ItemVisible("row0"));
    // The first row's cells are below the header, not under it.
    CHECK(session.ItemRect("row0").Min.y >= ui.Header(0).Max.y - 0.5f);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(ui.activations == 1);
    CHECK(ui.lastActivated == 0);
}

TEST_CASE("DataTable: Left and Right move the focused cell along the columns in display order")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(20);
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    session.Click(session.ItemCenter("row1"));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.State().focusColumn == 0);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.State().focusColumn == 2);
    // The row keeps the keyboard focus: Down moves to the next row in the same column.
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(GImGui->NavId == ui.rowId[2]);
    CHECK(ui.State().focusColumn == 2);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(ui.State().focusColumn == -1);  // back to the whole row
    // In display order: after Group moved in front of Index, Right goes to Group first.
    ImGui::TableSetColumnDisplayOrder(ui.Table(), 2, 0);
    session.Frames(2);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.State().focusColumn == 2);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ui.State().focusColumn == 0);
}

TEST_CASE("DataTable: F2 or Enter edit a cell; Enter commits and Escape cancels")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(20);
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    session.Click(session.ItemCenter("row1"));
    const int item = ui.order[1];
    // Enter on the whole row activates it; on the editable Name cell it edits.
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_RightArrow);
    REQUIRE(ui.State().focusColumn == 1);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(3);
    CHECK(ui.activations == 0);
    CHECK(ui.editorId != 0);
    CHECK(GImGui->ActiveId == ui.editorId);
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.Type("Renamed");
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(ui.names[static_cast<size_t>(item)] == "Renamed");
    CHECK(ui.commits == 1);
    CHECK(ui.editorId == 0);  // the cell draws its text again
    CHECK(GImGui->NavId == ui.rowId[1]);  // the focus is back on the row, its cell still focused
    CHECK(ui.State().focusColumn == 1);
    // F2 edits too; Escape restores the text from before the edit.
    session.KeyPress(ImGuiKey_F2);
    session.Frames(3);
    REQUIRE(GImGui->ActiveId == ui.editorId);
    session.Type(" more");
    CHECK(ui.names[static_cast<size_t>(item)] == "Renamed more");
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK(ui.names[static_cast<size_t>(item)] == "Renamed");
    CHECK(ui.cancels == 1);
    CHECK(ui.commits == 1);
    CHECK(GImGui->NavId == ui.rowId[1]);
}

TEST_CASE("DataTable: a double click edits an editable cell and activates the row elsewhere")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(20);
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    session.DoubleClick(ui.Cell(session, 2, 2));
    CHECK(ui.activations == 1);
    CHECK(ui.lastActivated == 2);
    session.DoubleClick(ui.Cell(session, 3, 1));
    session.Frames(3);
    CHECK(ui.activations == 1);
    CHECK(GImGui->ActiveId == ui.editorId);
    CHECK(ui.editorId != 0);
    // A click elsewhere ends the session; the focus stays where the click went.
    session.Click(session.ItemCenter("after"));
    session.Frames(2);
    CHECK(ui.editorId == 0);
    CHECK(GImGui->NavId == session.ItemId("after"));
}

TEST_CASE("DataTable: read-only and disabled tables edit nothing; read-only still selects and sorts")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(20);
    ui.options.readOnly = true;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    session.Click(session.ItemCenter("row1"));
    CHECK(ui.selection.Size == 1);
    session.KeyPress(ImGuiKey_F2);
    // A double click on the Name cell activates the row instead of editing it.
    session.DoubleClick(ui.Cell(session, 2, 1));
    session.Frames(3);
    CHECK(ui.editFrames == 0);
    CHECK(ui.activations == 1);
    const int sorts = ui.sorts;
    session.Click(ui.Header(3).GetCenter());
    CHECK(ui.sorts == sorts + 1);

    ui.options.readOnly = false;
    ui.disabled = true;
    ui.selection.Clear();
    session.Frames(2);
    session.Click(session.ItemCenter("row1"));
    session.DoubleClick(ui.Cell(session, 2, 1));
    session.Frames(3);
    CHECK(ui.selection.Size == 0);
    CHECK(ui.editFrames == 0);
    CHECK(ui.activations == 1);
    // A disabled header does not sort either.
    session.Click(ui.Header(2).GetCenter());
    CHECK(ui.sorts == sorts + 1);
}

TEST_CASE("DataTable: a resize band never makes a column narrower than the token")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(20);
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    const float before = ui.columnWidth[1];
    const ImRect header = ui.Header(1);
    // A drag on the band at the column's right edge, a little inside the band's edge.
    const float band = facet::Px(facet::T().table.resizeBand) * 0.5f;
    const ImVec2 start(header.Max.x + band * 0.75f, header.GetCenter().y);
    session.Drag({start, ImVec2(start.x + 40.0f, start.y)});
    session.Frames(2);
    CHECK(ui.columnWidth[1] == doctest::Approx(before + 40.0f).epsilon(0.02));
    session.Drag({ImVec2(ui.Header(1).Max.x, start.y), ImVec2(ui.Header(1).Min.x - 200.0f, start.y)});
    session.Frames(2);
    CHECK(ui.columnWidth[1] == doctest::Approx(facet::Px(facet::T().table.minColumnWidth)).epsilon(0.02));
}

TEST_CASE("DataTable: dragging a header past its neighbour moves the column")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(20);
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    CHECK(ui.columnOrder[2] == 2);
    const ImRect group = ui.Header(2);
    const ImRect value = ui.Header(3);
    session.Drag({group.GetCenter(), ImVec2(value.GetCenter().x, group.GetCenter().y),
                  ImVec2(value.Max.x + 4.0f, group.GetCenter().y)},
                 6);
    session.Frames(2);
    CHECK(ui.columnOrder[2] == 3);
    CHECK(ui.columnOrder[3] == 2);
    // The drag did not sort.
    CHECK(ui.Table()->Columns[2].SortOrder == -1);
}

TEST_CASE("DataTable: the column menu sorts, hides and shows columns")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(20);
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    session.Click(ui.Header(2).GetCenter(), ImGuiMouseButton_Right);
    session.Frames(3);
    REQUIRE(ui.menuOpen);
    CHECK(ui.State().menuColumn == 2);
    // Sort descending: the second row of the menu.
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(3);
    CHECK_FALSE(ui.menuOpen);
    CHECK(ui.Table()->Columns[2].SortOrder == 0);
    CHECK(ui.Table()->Columns[2].SortDirection == ImGuiSortDirection_Descending);
    // Columns > Value hides the Value column.
    session.Click(ui.Header(2).GetCenter(), ImGuiMouseButton_Right);
    session.Frames(3);
    REQUIRE(ui.menuOpen);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_RightArrow);
    session.Frames(3);
    REQUIRE(ui.listOpen);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(3);
    CHECK_FALSE(ui.columnShown[3]);
    CHECK(ui.columnShown[0]);
    // The Menu key on a focused header opens it too; Value comes back.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(GImGui->NavId == ui.HeaderId(0));
    session.KeyPress(ImGuiKey_Menu);
    session.Frames(3);
    REQUIRE(ui.menuOpen);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_RightArrow);
    session.Frames(3);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(3);
    CHECK(ui.columnShown[3]);
}

TEST_CASE("DataTable: frozen columns stay in place while the others scroll sideways")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(20);
    ui.width = 280.0f;
    ui.options.frozenColumns = 1;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    const float frozenLeft = ui.Header(0).Min.x;
    const float nameLeft = ui.Header(1).Min.x;
    ImGuiWindow* inner = ui.Table()->InnerWindow;
    REQUIRE(inner->ScrollMax.x > 0.0f);
    ImGui::SetScrollX(inner, 60.0f);
    session.Frames(3);
    CHECK(ui.Header(0).Min.x == doctest::Approx(frozenLeft));
    CHECK(ui.Header(1).Min.x == doctest::Approx(nameLeft - 60.0f));
    // Right to a column hidden to the right scrolls it into view past the frozen one.
    session.Click(session.ItemCenter("row1"));
    for (int i = 0; i < 4; ++i)
        session.KeyPress(ImGuiKey_RightArrow);
    session.Frames(10);
    CHECK(ui.State().focusColumn == 3);
    CHECK(ui.Header(3).Max.x <= inner->InnerRect.Max.x + 0.5f);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.Frames(10);
    CHECK(ui.State().focusColumn == 1);
    CHECK(ui.Header(1).Min.x >= ui.Header(0).Max.x - 0.5f);
}

TEST_CASE("DataTable: Tab enters the rows at the focused row and leaves them for the next item")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    TableUi ui(20);
    ui.options.sortable = false;
    ui.options.hideable = false;
    ui.options.reorderable = false;  // headers without an action are no Tab stops
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.Frames(2);
    CHECK(GImGui->NavId == ui.rowId[0]);
    CHECK(ui.selection.Size == 0);  // entering by Tab selects nothing
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Tab);
    session.Frames(2);
    CHECK(GImGui->NavId == session.ItemId("after"));
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    session.Frames(2);
    CHECK(GImGui->NavId == ui.rowId[1]);
}
