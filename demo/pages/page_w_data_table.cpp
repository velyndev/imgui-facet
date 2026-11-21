// Demo page: Data table. Shows DataTable of facet/widgets/display.h: a hundred thousand generated
// placeholder rows with sorting, selection, resizing, reordering, hiding, a pinned column and
// cell editing; the loading, error and empty states; and a narrow table that scrolls sideways
// under its pinned column. The rest of the display family is on the Display page.

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <numeric>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"
#include "widgets/display/data_table_internal.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::SectionHeader;

// Example data: generated placeholders, not real content.
constexpr int kRows = 100000;
constexpr int kProbedRows = 12;  // rows recorded as probes for the selftest
constexpr int kColumnCount = 5;
constexpr int kGroups = 5;
const char* const kGroupNames[kGroups] = {"Group A", "Group B", "Group C", "Group D", "Group E"};

const facet::DataTableColumn kColumns[kColumnCount] = {
    {.label = "#", .width = 72.0f, .align = facet::Align::End, .flags = ImGuiTableColumnFlags_NoHide},
    {.label = "Name", .width = 180.0f, .editable = true},
    {.label = "Group", .width = 120.0f},
    {.label = "Value", .width = 104.0f, .align = facet::Align::End, .editable = true},
    {.label = "Notes", .width = 280.0f, .flags = ImGuiTableColumnFlags_NoSort},
};

const facet::DataTableColumn kNarrowColumns[] = {
    {.label = "#", .width = 56.0f, .align = facet::Align::End},
    {.label = "Column 2", .width = 120.0f},
    {.label = "Column 3", .width = 120.0f},
    {.label = "Column 4", .width = 120.0f},
    {.label = "Column 5", .width = 120.0f},
};
constexpr int kNarrowRows = 24;

struct DataTableUi
{
    // Row -> item, sorted by the header; the items' names and values, editable in their cells.
    std::vector<int> order;
    std::vector<std::string> names;
    std::vector<int> values;
    ImGuiSelectionBasicStorage selection;
    bool selectable = true;
    bool readOnly = false;
    bool disabled = false;
    int rowSize = 1;  // Small, Regular, Large
    bool resetColumns = false;
    int activations = 0;
    int commits = 0;
    int cancels = 0;
    // Read back from the table every frame, for the notes and the selftest.
    bool shown[kColumnCount] = {true, true, true, true, true};
    int displayOrder[kColumnCount] = {0, 1, 2, 3, 4};
    float firstWidth = 0.0f;  // pixels, the "#" column
    float narrowScroll = 0.0f;
    float narrowPinnedLeft = 0.0f;
    float narrowPinnedAtRest = 0.0f;
};

DataTableUi ui;

void EnsureData()
{
    if (!ui.order.empty())
        return;
    ui.order.resize(kRows);
    ui.names.resize(kRows);
    ui.values.resize(kRows);
    char text[32];
    for (int i = 0; i < kRows; ++i)
    {
        ui.order[static_cast<size_t>(i)] = i;
        std::snprintf(text, sizeof(text), "Item %06d", i + 1);
        ui.names[static_cast<size_t>(i)] = text;
        ui.values[static_cast<size_t>(i)] = static_cast<int>((static_cast<unsigned>(i) * 2654435761u) % 1000u);
    }
    // The selection keys rows by item, so it stays on the items when the order changes.
    ui.selection.UserData = &ui;
    ui.selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int row) {
        const auto* state = static_cast<const DataTableUi*>(self->UserData);
        return static_cast<ImGuiID>(state->order[static_cast<size_t>(row)] + 1);
    };
}

void ResetData()
{
    for (int i = 0; i < kRows; ++i)
    {
        ui.order[static_cast<size_t>(i)] = i;
        char text[32];
        std::snprintf(text, sizeof(text), "Item %06d", i + 1);
        ui.names[static_cast<size_t>(i)] = text;
        ui.values[static_cast<size_t>(i)] = static_cast<int>((static_cast<unsigned>(i) * 2654435761u) % 1000u);
    }
}

void SortRows(const ImGuiTableSortSpecs* specs)
{
    std::stable_sort(ui.order.begin(), ui.order.end(), [specs](int a, int b) {
        for (int s = 0; s < specs->SpecsCount; ++s)
        {
            const ImGuiTableColumnSortSpecs& spec = specs->Specs[s];
            int order = 0;
            switch (spec.ColumnIndex)
            {
            case 1:
                order = ui.names[static_cast<size_t>(a)].compare(ui.names[static_cast<size_t>(b)]);
                break;
            case 2:
                order = a % kGroups - b % kGroups;
                break;
            case 3:
                order = ui.values[static_cast<size_t>(a)] - ui.values[static_cast<size_t>(b)];
                break;
            default:
                order = a - b;
                break;
            }
            if (order != 0)
                return spec.SortDirection == ImGuiSortDirection_Ascending ? order < 0 : order > 0;
        }
        return a < b;
    });
}

// Records `rect` (a header cell or a cell of the current table) as a probe, cut to the part of the
// table that shows.
void ProbeTableRect(facet::demo::PageCtx& ctx, const char* name, ImRect rect)
{
    const ImGuiTable* table = ImGui::GetCurrentTable();
    rect.ClipWithFull(table->InnerClipRect);
    facet::demo::ProbeRect(ctx, name, 0, rect.Min, rect.Max);
}

void ProbeHeaders(facet::demo::PageCtx& ctx)
{
    const ImGuiTable* table = ImGui::GetCurrentTable();
    const float top = table->OuterRect.Min.y;
    const float bottom = top + facet::Px(facet::T().table.headerHeight);
    // Probes count as visible inside the current clip rect: here the rows', below the header.
    ImGui::PushClipRect(table->InnerClipRect.Min, table->InnerClipRect.Max, false);
    char name[32];
    for (int n = 0; n < kColumnCount; ++n)
    {
        const ImGuiTableColumn& column = table->Columns[n];
        std::snprintf(name, sizeof(name), "table.header.%d", n);
        ProbeTableRect(ctx, name, ImRect(column.MinX, top, column.MaxX, bottom));
        ui.shown[n] = column.IsEnabled;
        ui.displayOrder[n] = column.DisplayOrder;
    }
    ImGui::PopClipRect();
    ui.firstWidth = table->Columns[0].MaxX - table->Columns[0].MinX;
}

void MainTable(facet::demo::PageCtx& ctx)
{
    using facet::demo::ForcedItemState;
    if (ctx.capture)
    {
        switch (ctx.captureState)
        {
        case 1:
            ctx.ForceNextItemState(ForcedItemState::Hovered, 2);
            break;
        case 2:
            ctx.ForceNextItemState(ForcedItemState::Focused, 1);
            break;
        case 3:
            ctx.ForceNextItemState(ForcedItemState::Hovered, facet::internal::kDataTableHeaderPart + 1);
            break;
        case 4:
            ctx.ForceNextItemState(ForcedItemState::Open, facet::internal::kDataTableHeaderPart + 2);
            break;
        default:
            break;
        }
    }
    const facet::ControlSize sizes[] = {facet::ControlSize::Small, facet::ControlSize::Regular,
                                        facet::ControlSize::Large};
    facet::PushControlSize(sizes[std::clamp(ui.rowSize, 0, 2)]);
    if (ui.disabled)
        ImGui::BeginDisabled();
    const facet::DataTableOptions options{.sortable = true,
                                          .multiSort = true,
                                          .resizable = true,
                                          .reorderable = true,
                                          .hideable = true,
                                          .frozenColumns = 1,
                                          .height = 240.0f,
                                          .readOnly = ui.readOnly};
    if (facet::BeginDataTable("table", kColumns, kRows, ui.selectable ? &ui.selection : nullptr, options))
    {
        if (ui.resetColumns)
        {
            ImGui::GetCurrentTable()->IsResetAllRequest = true;  // applied by the next BeginDataTable
            ui.resetColumns = false;
        }
        if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs != nullptr && specs->SpecsDirty)
        {
            SortRows(specs);
            specs->SpecsDirty = false;
        }
        ProbeHeaders(ctx);
        facet::RowRange rows;
        char text[48];
        while (facet::NextVisibleRows(&rows))
        {
            for (int row = rows.first; row < rows.last; ++row)
            {
                const int item = ui.order[static_cast<size_t>(row)];
                if (facet::DataTableRow(row))
                    ++ui.activations;
                const ImRect rowRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                if (row < kProbedRows)
                {
                    std::snprintf(text, sizeof(text), "table.row.%d", row);
                    ctx.Probe(text);
                }
                if (row == kRows - 1)
                    ctx.Probe("table.row.last");
                if (row == 3)
                {
                    // The Name cell of row 3, for the double click of the selftest.
                    const ImGuiTableColumn& name = ImGui::GetCurrentTable()->Columns[1];
                    ProbeTableRect(ctx, "table.cell.3.name", ImRect(name.MinX, rowRect.Min.y, name.MaxX, rowRect.Max.y));
                }

                ImGui::TableNextColumn();
                std::snprintf(text, sizeof(text), "%d", item + 1);
                facet::StyledText(text, {.secondary = true, .align = facet::Align::End, .width = facet::kFill});
                ImGui::TableNextColumn();
                if (facet::BeginDataTableCellEdit())
                {
                    facet::TextField("##edit", &ui.names[static_cast<size_t>(item)]);
                    ui.commits += facet::IsItemEditCommitted() ? 1 : 0;
                    ui.cancels += facet::IsItemEditCanceled() ? 1 : 0;
                    facet::EndDataTableCellEdit();
                }
                else
                {
                    facet::StyledText(ui.names[static_cast<size_t>(item)].c_str());
                }
                ImGui::TableNextColumn();
                facet::StyledText(kGroupNames[item % kGroups]);
                ImGui::TableNextColumn();
                if (facet::BeginDataTableCellEdit())
                {
                    facet::NumberInput("##edit", &ui.values[static_cast<size_t>(item)], {.min = 0.0, .max = 999.0});
                    ui.commits += facet::IsItemEditCommitted() ? 1 : 0;
                    ui.cancels += facet::IsItemEditCanceled() ? 1 : 0;
                    facet::EndDataTableCellEdit();
                }
                else
                {
                    std::snprintf(text, sizeof(text), "%d", ui.values[static_cast<size_t>(item)]);
                    facet::StyledText(text, {.align = facet::Align::End, .width = facet::kFill});
                }
                ImGui::TableNextColumn();
                std::snprintf(text, sizeof(text), "Placeholder note for item %d", item + 1);
                facet::TruncatedText(text, {.secondary = true});
            }
        }
        facet::EndDataTable();
    }
    ctx.Probe("table.main");
    if (ctx.capture && (ctx.captureState == 1 || ctx.captureState == 2 || ctx.captureState == 3))
        ctx.RevealInCapture(0.4f);
    if (ui.disabled)
        ImGui::EndDisabled();
    facet::PopControlSize();
}

void StateTables(facet::demo::PageCtx& ctx)
{
    static const facet::DataTableColumn columns[] = {{.label = "Name"}, {.label = "Value", .width = 72.0f}};
    const float width = facet::demo::DemoWidth(220.0f);
    const float right = facet::demo::ContentRight();
    struct StateTable
    {
        const char* id;
        const char* probe;
        facet::DataState state;
        int rows;
    };
    const StateTable tables[] = {
        {"loading", "table.loading", facet::DataState::Loading, 8},
        {"error", "table.error", facet::DataState::Error, 0},
        {"empty", "table.empty", facet::DataState::Ready, 0},
    };
    for (int i = 0; i < 3; ++i)
    {
        if (i > 0)
            facet::demo::SameLineIfFits(right, facet::Px(width));
        const StateTable& table = tables[i];
        if (facet::BeginDataTable(table.id, columns, table.rows, nullptr,
                                  {.state = table.state,
                                   .errorText = "Placeholder error text",
                                   .width = width,
                                   .height = 168.0f}))
            facet::EndDataTable();  // never reached: these states draw in place of the rows
        ctx.Probe(table.probe);
    }
}

void NarrowTable(facet::demo::PageCtx& ctx)
{
    if (facet::BeginDataTable("narrow", kNarrowColumns, kNarrowRows, nullptr,
                              {.resizable = true, .frozenColumns = 1, .width = facet::demo::DemoWidth(320.0f),
                               .height = 200.0f}))
    {
        ImGuiTable* table = ImGui::GetCurrentTable();
        if (ctx.capture && ctx.captureState == 6 && ctx.captureFramesLeft > 2)
            ImGui::SetScrollX(table->InnerWindow, facet::Px(140.0f));
        ui.narrowScroll = table->InnerWindow->Scroll.x;
        ui.narrowPinnedLeft = table->Columns[0].MinX;
        if (ui.narrowScroll == 0.0f)
            ui.narrowPinnedAtRest = ui.narrowPinnedLeft;
        facet::RowRange rows;
        char text[40];
        while (facet::NextVisibleRows(&rows))
        {
            for (int row = rows.first; row < rows.last; ++row)
            {
                (void)facet::DataTableRow(row);
                if (row == 1)
                    ctx.Probe("table.narrow.row.1");
                ImGui::TableNextColumn();
                std::snprintf(text, sizeof(text), "%d", row + 1);
                facet::StyledText(text, {.secondary = true, .align = facet::Align::End, .width = facet::kFill});
                for (int column = 2; column <= 5; ++column)
                {
                    ImGui::TableNextColumn();
                    std::snprintf(text, sizeof(text), "Cell %d.%d", row + 1, column);
                    facet::StyledText(text);
                }
            }
        }
        facet::EndDataTable();
    }
    ctx.Probe("table.narrow");
    if (ctx.capture && ctx.captureState == 6)
        ctx.RevealInCapture(0.5f);
}

} // namespace

FACET_DEMO_PAGE("data-table", "Data table", "Widgets", 112, "default", "row-hover", "row-focus", "header-hover",
                "menu", "states", "narrow")
{
    EnsureData();
    facet::demo::PageHeader("Data table",
                            "A Dear ImGui table with Facet headers: sorting, resizing, reordering and hiding columns, "
                            "a pinned column, virtualized rows, selection, keyboard navigation by row and by cell, "
                            "and cell editing. Every row holds generated placeholder data.");
    facet::demo::ReadyFamilyNote("display");

    SectionHeader("Rows and columns");
    facet::demo::Paragraph("A hundred thousand rows; only the visible ones are submitted each frame. Click a header "
                           "to sort (Shift adds a column), drag it to move the column, drag its edge to resize, and "
                           "right-click it for the column menu. Arrows move by row, Left and Right by cell; F2, "
                           "Enter on an editable cell or a double click edits Name and Value.");
    LabeledRow("Selection");
    facet::Toggle("##selection", &ui.selectable);
    ctx.Probe("table.toggle.selection");
    LabeledRow("Read-only");
    facet::Toggle("##readonly", &ui.readOnly);
    ctx.Probe("table.toggle.readonly");
    LabeledRow("Disabled");
    facet::Toggle("##disabled", &ui.disabled);
    ctx.Probe("table.toggle.disabled");
    LabeledRow("Row size");
    static const char* const kSizes[] = {"Small", "Regular", "Large"};
    facet::SegmentedControl("##rowsize", &ui.rowSize, kSizes);
    ctx.Probe("table.rowsize");
    LabeledRow("Columns");
    if (facet::Button("Reset columns"))
        ui.resetColumns = true;
    ctx.Probe("table.reset");
    MainTable(ctx);
    facet::demo::Note("Rows activated: %d. Rows selected: %d. Edits committed: %d, canceled: %d.", ui.activations,
                      ui.selection.Size, ui.commits, ui.cancels);

    SectionHeader("Data states");
    facet::demo::Paragraph("Loading, error and empty draw under the header in place of the rows.");
    StateTables(ctx);
    if (ctx.capture && ctx.captureState == 5)
        ctx.RevealInCapture(0.5f);

    SectionHeader("Narrow table with a pinned column");
    facet::demo::Paragraph("Narrower than its columns, the table scrolls sideways; the first column stays in view.");
    NarrowTable(ctx);
}

FACET_DEMO_SELFTEST("data-table")
{
    static std::vector<int> selectedItems;
    test.Do([] {
        EnsureData();
        ResetData();
        ui.selection.Clear();
        ui.selectable = true;
        ui.readOnly = false;
        ui.disabled = false;
        ui.rowSize = 1;
        ui.resetColumns = true;
        ui.activations = 0;
        ui.commits = 0;
        ui.cancels = 0;
    });
    test.Wait(3);
    // A table out of sight submits nothing: bring it into view before aiming at its rows.
    test.Reveal("table.main");
    test.WaitSeconds(0.4f);

    // Mouse selection: a click, Ctrl+click and Shift+click.
    test.Click("table.row.0");
    test.Expect("a click selects the row", [] {
        return ui.selection.Size == 1 && ui.selection.Contains(static_cast<ImGuiID>(ui.order[0] + 1));
    });
    test.KeyDown(ImGuiMod_Ctrl);
    test.Click("table.row.2");
    test.KeyUp(ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+click adds a row", [] { return ui.selection.Size; }, 2.0, 0.0);
    test.KeyDown(ImGuiMod_Shift);
    test.Click("table.row.4");
    test.KeyUp(ImGuiMod_Shift);
    test.ExpectNear("Shift+click selects the range from the last click", [] { return ui.selection.Size; }, 3.0, 0.0);
    test.Do([] {
        selectedItems.clear();
        for (int row = 2; row <= 4; ++row)
            selectedItems.push_back(ui.order[static_cast<size_t>(row)]);
    });

    // Sorting by a header click keeps the selection on the same items. "#" is pinned, so its
    // header shows at every size; the table starts sorted by it, so a click turns the order.
    test.Click("table.header.0");
    test.Wait(2);
    test.Expect("a header click sorts the other way", [] { return ui.order[0] == kRows - 1; });
    test.Expect("the selection follows its items across the sort", [] {
        if (ui.selection.Size != 3)
            return false;
        for (const int item : selectedItems)
            if (!ui.selection.Contains(static_cast<ImGuiID>(item + 1)))
                return false;
        return true;
    });
    test.Click("table.header.0");
    test.Wait(2);
    test.Expect("a second click sorts ascending again", [] { return ui.order[0] == 0; });

    // Keyboard: rows, the ends, activation.
    test.Click("table.row.1");
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectFocused("table.row.2");
    test.Expect("Down selects the row it moves to", [] {
        return ui.selection.Size == 1 && ui.selection.Contains(static_cast<ImGuiID>(ui.order[2] + 1));
    });
    test.KeyPress(ImGuiKey_End);
    test.Wait(2);
    test.ExpectVisible("table.row.last");
    test.KeyPress(ImGuiKey_Home);
    test.Wait(2);
    test.ExpectVisible("table.row.0");
    test.ExpectFocused("table.row.0");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Enter activates the focused row", [] { return ui.activations; }, 1.0, 0.0);

    // Cell navigation and editing: Right twice reaches Name, Enter edits it, Enter commits.
    test.Do([] { ui.commits = 0; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(3);
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.Type("Edited");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.ExpectText("Enter commits the edited name", [] { return ui.names[static_cast<size_t>(ui.order[0])]; },
                    "Edited");
    test.ExpectNear("one edit committed", [] { return ui.commits; }, 1.0, 0.0);
    test.ExpectFocused("table.row.0");
    // F2 edits again; Escape restores the name.
    test.KeyPress(ImGuiKey_F2);
    test.Wait(3);
    test.Type(" more");
    test.KeyPress(ImGuiKey_Escape);
    test.Wait(2);
    test.ExpectText("Escape cancels the edit", [] { return ui.names[static_cast<size_t>(ui.order[0])]; }, "Edited");
    test.ExpectNear("one edit canceled", [] { return ui.cancels; }, 1.0, 0.0);

    // A double click on a Name cell edits it; Enter commits.
    test.DoubleClick("table.cell.3.name");
    test.Wait(3);
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.Type("Clicked");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.ExpectText("a double click edits the Name cell", [] { return ui.names[static_cast<size_t>(ui.order[3])]; },
                    "Clicked");
    test.ExpectFocused("table.row.3");

    // Right twice more reaches Value, scrolled into view when the table is narrow; Enter edits
    // the number.
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_RightArrow);
    test.WaitSeconds(0.5f);
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(3);
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.Type("42");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.ExpectNear("Enter commits the typed value", [] { return ui.values[static_cast<size_t>(ui.order[3])]; },
                    42.0, 0.0);

    // The column menu of Value hides and shows a column (keyboard inside the menu).
    test.Click("table.header.3", ImGuiMouseButton_Right);
    test.Wait(3);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_RightArrow);
    test.Wait(3);
    // The focus starts on Name: "#" cannot be hidden, so its row is disabled.
    for (int i = 0; i < 3; ++i)
        test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(3);
    test.Expect("the column menu hides Notes", [] { return !ui.shown[4] && ui.shown[3]; });
    test.Click("table.header.3", ImGuiMouseButton_Right);
    test.Wait(3);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_RightArrow);
    test.Wait(3);
    for (int i = 0; i < 3; ++i)
        test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(3);
    test.Expect("the column menu shows Notes again", [] { return ui.shown[4]; });

    // Reordering by drag: Value dragged left past its own left edge swaps with Group.
    using T = facet::demo::Target;
    test.Drag({T::Item("table.header.3"), T::Item("table.header.3", 0.0f, 0.5f).Offset(-8.0f, 0.0f)}, 0.4f);
    test.Wait(2);
    test.Expect("dragging Value past Group moves it", [] { return ui.displayOrder[3] < ui.displayOrder[2]; });
    // Resizing the pinned "#" column stops at the narrowest column width.
    test.Drag({T::Item("table.header.0", 1.0f, 0.5f), T::Item("table.header.0", 1.0f, 0.5f).Offset(-200.0f, 0.0f)},
              0.4f);
    test.Wait(2);
    test.ExpectNear("a resize stops at the narrowest width (units)",
                    [] { return ui.firstWidth / facet::Px(1.0f); }, facet::T().table.minColumnWidth, 1.5);

    // Read-only and disabled (the toggles sit above the table; it comes back into view after each).
    test.Click("table.toggle.readonly");
    test.Reveal("table.main");
    test.WaitSeconds(0.4f);
    test.Click("table.row.1");
    test.KeyPress(ImGuiKey_F2);
    test.Wait(3);
    test.ExpectFocused("table.row.1");
    test.Click("table.toggle.readonly");
    test.Do([] { ui.selection.Clear(); });
    test.Click("table.toggle.disabled");
    test.Reveal("table.main");
    test.WaitSeconds(0.4f);
    test.Click("table.row.1");
    test.ExpectNear("a disabled table selects nothing", [] { return ui.selection.Size; }, 0.0, 0.0);
    test.Click("table.toggle.disabled");

    // The data states and the narrow table with its pinned column.
    for (const char* probe : {"table.loading", "table.error", "table.empty"})
    {
        test.Reveal(probe);
        test.WaitSeconds(0.4f);
        test.ExpectVisible(probe);
    }
    test.Reveal("table.narrow");
    test.WaitSeconds(0.4f);
    test.Click("table.narrow.row.1");
    for (int i = 0; i < 5; ++i)
        test.KeyPress(ImGuiKey_RightArrow);
    test.WaitSeconds(0.5f);
    test.Expect("Right scrolls the narrow table sideways", [] { return ui.narrowScroll > 0.0f; });
    test.ExpectNear("the pinned column stays in place", [] { return ui.narrowPinnedLeft - ui.narrowPinnedAtRest; },
                    0.0, 0.5);
    test.Do([] { ui.resetColumns = true; });
    test.Wait(2);
}
