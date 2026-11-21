// Demo page: Data table. Shows DataTable of facet/widgets/display.h with generated placeholder rows.
// The widget is planned: until Phase 2 implements it, the placeholder opens a plain Dear ImGui table
// so the rows below land in it. The rest of the display family is on the Display page.

#include <iterator>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::SectionHeader;

// Example data: neutral placeholders, not real content.
const facet::DataTableColumn kColumns[] = {
    {"Column 1"},
    {"Column 2"},
    {"Column 3", 0.0f, facet::Align::End},
};
constexpr int kTableRows = 3;

struct DataTableUi
{
    ImGuiSelectionBasicStorage selection;
    int activations = 0;
};

DataTableUi ui;

} // namespace

FACET_DEMO_PAGE("data-table", "Data table", "Widgets", 112)
{
    facet::demo::PageHeader("Data table",
                            "A sortable, resizable table with a sticky header, frozen columns, virtualized rows, "
                            "selection and cell editing. Every cell holds generated placeholder text.");
    facet::demo::PlannedFamilyNote("display");

    SectionHeader("Rows and columns");
    LabeledRow("Data table");
    if (facet::BeginDataTable("table", kColumns, kTableRows, &ui.selection,
                              {.sortable = true, .frozenColumns = 1, .height = facet::T().spacing.xxxl * 4.0f}))
    {
        facet::RowRange rows;
        while (facet::NextVisibleRows(&rows))
        {
            for (int row = rows.first; row < rows.last; ++row)
            {
                if (facet::DataTableRow(row))
                    ++ui.activations;
                for (int column = 0; column < static_cast<int>(std::size(kColumns)); ++column)
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("Row %d, column %d", row + 1, column + 1);
                }
            }
        }
        facet::EndDataTable();
    }
    ctx.Probe("table.view");
    facet::demo::Note("Rows activated: %d. Rows selected: %d.", ui.activations, ui.selection.Size);
}

FACET_DEMO_SELFTEST("data-table")
{
    test.Do([] {
        ui.selection.Clear();
        ui.activations = 0;
    });
    // The planned table is a placeholder: a click on its area and the keys select and activate
    // nothing.
    test.Click("table.view");
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("the table placeholder selects nothing", [] { return ui.selection.Size; }, 0.0, 0.0);
    test.ExpectNear("the table placeholder activates nothing", [] { return ui.activations; }, 0.0, 0.0);
}
