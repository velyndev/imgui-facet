// The declared API of DataTable and its cell editing (facet/widgets/display.h): the options follow
// the conventions, every function is called inside a real Dear ImGui frame, in a window and again
// inside ImGui::BeginDisabled, and afterwards the id, window, group and Facet scope stacks are as
// before, the table is one item with the id of its label and the selection is unchanged by the
// calls themselves. Behaviour is tested in tests/test_data_table.cpp; the rest of display.h in
// tests/test_api_display.cpp.

#include <type_traits>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/scope_stack.h"
#include "support/content_stub_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::content;

static_assert(OptionsFollowConventions<facet::DataTableOptions>());
// A view of caller data has the data block of rule 7, and the table writes a selection.
static_assert(HasDataBlock<facet::DataTableOptions>());
static_assert(HasReadOnly<facet::DataTableOptions>);
// One alignment enum for styled text and table columns (StyledText: tests/test_api_display.cpp).
static_assert(std::is_same_v<decltype(facet::DataTableColumn::align), facet::Align>);

void DataTableCalls()
{
    using namespace facet;
    static const DataTableColumn columns[] = {{"Name"}, {"Size", 80.0f, Align::End}};
    ImGuiSelectionBasicStorage selection;
    selection.SetItemSelected(1, true);
    // Ready with rows: a Dear ImGui table is open between Begin and End, so the caller's rows and
    // cells work; End closes it again and leaves one item, the table.
    Check("BeginDataTable, Ready", Expect::Item, "table", [&] {
        ImGuiTable* const outer = ImGui::GetCurrentTable();
        const bool open = BeginDataTable("table", columns, 10, &selection,
                                         {.sortable = true, .frozenColumns = 1, .height = 60.0f});
        CHECK(open);
        if (open)
        {
            CHECK(ImGui::GetCurrentTable() != outer);
            CHECK(ImGui::TableGetColumnCount() == 2);
            CHECK_FALSE(DataTableRow(0));
            CHECK(ImGui::TableNextColumn());
            ImGui::TextUnformatted("cell");
            // No cell is in edit mode before a key or a double click asks for it.
            CHECK_FALSE(BeginDataTableCellEdit());
            CHECK(ImGui::TableNextColumn());
            EndDataTable();
        }
        CHECK(ImGui::GetCurrentTable() == outer);
    });
    // The data states draw in place of the rows and return false, with the header still there.
    for (const DataState state : {DataState::Loading, DataState::Error})
        Check("BeginDataTable, Loading or Error", Expect::Item, "table",
              [&] { CHECK_FALSE(BeginDataTable("table", columns, 10, &selection, {.state = state, .height = 60.0f})); });
    Check("BeginDataTable, empty", Expect::Item, "table",
          [&] { CHECK_FALSE(BeginDataTable("table", columns, 0, &selection, {.height = 60.0f})); });
    CHECK(selection.Size == 1);
    CHECK(selection.Contains(1));
    // Outside a table the row call does nothing and the cell edit never opens.
    Check("DataTableRow outside a table", Expect::NoItem, nullptr, [] { CHECK_FALSE(DataTableRow(0)); });
    Check("BeginDataTableCellEdit", Expect::NoItem, nullptr, [] { CHECK_FALSE(BeginDataTableCellEdit()); });
}

} // namespace

TEST_CASE("The data table keeps every stack and opens a Dear ImGui table for the caller's cells")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &DataTableCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("A column sizing flag set by the caller replaces the one its width implies")
{
    // Dear ImGui accepts one sizing flag per column (an assertion in debug builds), so a caller
    // flag must not be combined with the policy that `width` implies.
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        static const DataTableColumn columns[] = {
            {.label = "Fixed", .flags = ImGuiTableColumnFlags_WidthFixed},
            {.label = "Stretch", .width = 80.0f, .flags = ImGuiTableColumnFlags_WidthStretch},
            {.label = "Auto"},
            {.label = "Sized", .width = 40.0f},
        };
        const ImGuiTableColumnFlags expected[] = {ImGuiTableColumnFlags_WidthFixed, ImGuiTableColumnFlags_WidthStretch,
                                                  ImGuiTableColumnFlags_WidthStretch, ImGuiTableColumnFlags_WidthFixed};
        const bool open = BeginDataTable("sizing", columns, 1, nullptr);
        CHECK(open);
        if (!open)
            return;
        const ImGuiTable* table = ImGui::GetCurrentTable();
        for (int i = 0; i < 4; ++i)
        {
            INFO("column " << i);
            CHECK((table->Columns[i].Flags & ImGuiTableColumnFlags_WidthMask_) == expected[i]);
            ++checked;
        }
        EndDataTable();
    });
    session.Frame();
    CHECK(checked == 4);
}

TEST_CASE("A DataTable End without its Begin is reported and changes no Dear ImGui stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int reported = 0;
    session.SetUi([&] {
        const Stacks before = CurrentStacks();
        const int errors = facet::internal::UsageErrorCount();
        facet::EndDataTable();
        facet::EndDataTableCellEdit();
        reported = facet::internal::UsageErrorCount() - errors;
        Stacks after = CurrentStacks();
        after.usageErrors = before.usageErrors;
        CHECK(after == before);
    });
    session.Frame();
    CHECK(reported == 2);
}

TEST_CASE("A DataTable without columns is reported and returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool open = true;
    int reported = 0;
    session.SetUi([&] {
        const int errors = facet::internal::UsageErrorCount();
        open = facet::BeginDataTable("none", std::span<const facet::DataTableColumn>(), 3, nullptr);
        reported = facet::internal::UsageErrorCount() - errors;
    });
    session.Frame();
    CHECK_FALSE(open);
    CHECK(reported == 1);
}
