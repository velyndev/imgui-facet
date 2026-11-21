// BeginDataTable, DataTableRow, EndDataTable and the cell edit pair (facet/widgets/display.h).
//
// A DataTable is a Dear ImGui table (BeginTableEx with ScrollX and ScrollY, so the header row and
// the frozen columns stay in view) on the grouped surface, with:
//   - Facet headers (data_table_header.cpp) instead of TableHeadersRow: sorting through
//     TableSetColumnSortDirection, reordering by drag through TableQueueSetColumnDisplayOrder,
//     resize bands of the table.resizeBand token, and a Facet column menu;
//   - the overlay bars of the scroll core on the table's inner window, below the header;
//   - rows from the row block of the list view (ListRowGroup in list_view.h): one row item per row
//     that spans every column, placed in the table's background channel with the background clip
//     rectangle, so its hover and selection fill and its focus ring cover all columns, also while
//     the table is scrolled sideways, and never cover the cells; the rows are virtualized by the row
//     ranges of the list core (an ImGuiListClipper), so only visible rows are submitted and the
//     cost of a frame does not grow with the row count;
//   - the selection of the list core (ImGui::BeginMultiSelect over the caller's storage), with the
//     Shift range start carried across a sort (ListRangeAnchor);
//   - keys while a row has the focus: the arrows, Page Up and Down move through Dear ImGui
//     navigation; Home and End go to the first and last row (ListJumpToEnd); Left and Right move a
//     focused cell along the columns in their display order; F2, or Enter on an editable cell,
//     starts the cell edit session; Tab enters and leaves the rows like any Facet list;
//   - the loading, error and empty states drawn under the header in place of the rows.
// Every pixel length comes from the tokens through Px; colors from the theme.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_core.h"
#include "internal/list_view.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/display/data_table_internal.h"

namespace facet
{
namespace internal
{
namespace
{

constexpr const char* kTableScope = "BeginDataTable";
constexpr const char* kEditScope = "BeginDataTableCellEdit";

// Frames an editor may take to become active after it was asked to take the keyboard focus
// (SetKeyboardFocusHere lands one frame later, activation another); then the session is dropped.
constexpr int kDataTableEditStartFrames = 4;

struct DataTableStack
{
    DataTableFrame frames[kDataTableDepth];
    int depth = 0;
};

DataTableStack& Tables()
{
    static DataTableStack stack;
    return stack;
}

ImGuiID RowKeyOf(const DataTableFrame& frame, int index)
{
    return frame.selection != nullptr ? frame.selection->GetStorageIdFromIndex(index) : static_cast<ImGuiID>(index);
}

ImGuiID RowIdOf(const DataTableFrame& frame, ImGuiID key)
{
    return ImHashData(&key, sizeof(key), frame.seed);
}

// The top of row 0 in screen pixels: right under the header row, at the current scroll offset.
// Rows advance exactly rowHeight (DataTableRow clamps them), so row i starts i rows lower.
double RowsTop(const DataTableFrame& frame)
{
    return static_cast<double>(frame.table->WorkRect.Min.y) + static_cast<double>(frame.headerHeight);
}

ImRect RowRectOf(const DataTableFrame& frame, int index)
{
    const float top = static_cast<float>(RowsTop(frame) + static_cast<double>(index) * frame.rowHeight);
    return DataTableRowSpan(frame, top, top + frame.rowHeight);
}

// Scrolls a row into the part of the table below the header.
void RevealRow(const DataTableFrame& frame, const ImRect& rect, bool animate)
{
    ImRect target = rect;
    target.Min.y -= frame.headerHeight;
    ScrollAreaRevealRect(frame.window, target, ScrollAlign::Nearest, animate);
}

void FocusRowIndex(DataTableFrame& frame, int index)
{
    const ImGuiID id = RowIdOf(frame, RowKeyOf(frame, index));
    const ImRect rect = RowRectOf(frame, index);
    ListFocusRow(frame.window, id, frame.focusScope, rect);
    frame.state->focusIndex = index;
    frame.state->focusedId = id;
    RevealRow(frame, rect, false);
}

bool ColumnUsable(const DataTableFrame& frame, int column)
{
    return column >= 0 && column < frame.columnCount && frame.table->Columns[column].IsEnabled;
}

bool CellEditable(const DataTableFrame& frame, int column)
{
    return !frame.readOnly && !frame.disabled && ColumnUsable(frame, column) && DataTableColumnEditable(frame, column);
}

// The cell of `column` in a row from `top` to `bottom`: the column's extent (its padding included),
// the gap of a row fill kept above and below.
ImRect CellRect(const DataTableFrame& frame, int column, float top, float bottom)
{
    const ImGuiTableColumn& c = frame.table->Columns[column];
    const float outset = FocusRingOutset();
    const float gap = HairlineThickness();
    return ImRect(c.MinX + outset, top + gap, std::max(c.MinX + outset + 1.0f, c.MaxX - outset), bottom - gap);
}

// Left and Right while a row has the focus: the focused cell moves along the enabled columns in
// their display order; Left from the first column gives the focus back to the whole row.
void MoveCellFocus(DataTableFrame& frame, int direction)
{
    ImGuiTable* table = frame.table;
    DataTableState& state = *frame.state;
    const int order = ColumnUsable(frame, state.focusColumn) ? table->Columns[state.focusColumn].DisplayOrder : -1;
    int target = state.focusColumn;
    if (direction > 0)
    {
        for (int o = order + 1; o < frame.columnCount; ++o)
        {
            const int column = table->DisplayOrderToIndex[o];
            if (table->Columns[column].IsEnabled)
            {
                target = column;
                break;
            }
        }
    }
    else
    {
        target = -1;
        for (int o = order - 1; o >= 0; --o)
        {
            const int column = table->DisplayOrderToIndex[o];
            if (table->Columns[column].IsEnabled)
            {
                target = column;
                break;
            }
        }
    }
    state.focusColumn = target;
    SetKeyboardModality(true);
    if (target < 0 || state.focusIndex < 0)
        return;
    // Bring the cell into view sideways; a scrolling column must clear the frozen ones.
    const ImGuiTableColumn& c = table->Columns[target];
    const ImRect row = RowRectOf(frame, state.focusIndex);
    ImRect cell(c.MinX, row.Min.y - frame.headerHeight, c.MaxX, row.Max.y);
    if (table->FreezeColumnsRequest > 0 && c.DisplayOrder >= table->FreezeColumnsRequest)
    {
        const ImGuiTableColumn& last = table->Columns[table->DisplayOrderToIndex[table->FreezeColumnsRequest - 1]];
        cell.Min.x -= std::max(0.0f, last.MaxX - table->InnerRect.Min.x);
    }
    ScrollAreaRevealRect(frame.window, cell, ScrollAlign::Nearest, true);
}

void StartEdit(DataTableFrame& frame, ImGuiID key, int column)
{
    DataTableState& state = *frame.state;
    state.editPhase = DataTableState::EditPhase::Starting;
    state.editKey = key;
    state.editColumn = column;
    state.editFrames = 0;
    state.editorId = 0;
    state.focusColumn = column;
}

// Ends the edit session; with `refocus` the keyboard focus goes back to the edited row, its
// cell still focused, as after the keys that end an edit (Enter, Tab, Escape).
void EndEdit(DataTableFrame& frame, bool refocus)
{
    DataTableState& state = *frame.state;
    state.editPhase = DataTableState::EditPhase::None;
    state.editorId = 0;
    if (refocus && frame.row >= 0)
    {
        ListFocusRow(frame.window, frame.rowId, frame.focusScope, frame.rowRect);
        state.focusIndex = frame.row;
        state.focusedId = frame.rowId;
    }
}

// The keys a focused row reads (see the top of this file), before the rows are submitted.
void ReadRowKeys(DataTableFrame& frame)
{
    DataTableState& state = *frame.state;
    const ImGuiID owner = DataTableKeyOwner();
    const ImGuiKey owned[] = {ImGuiKey_Home, ImGuiKey_End, ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_F2};
    for (const ImGuiKey key : owned)
        ImGui::SetKeyOwner(key, owner);
    if (!ColumnUsable(frame, state.focusColumn))
        state.focusColumn = -1;
    const bool editableCell = state.focusColumn >= 0 && CellEditable(frame, state.focusColumn);
    if (editableCell)
    {
        // Enter edits the cell instead of activating the row.
        ImGui::SetKeyOwner(ImGuiKey_Enter, owner);
        ImGui::SetKeyOwner(ImGuiKey_KeypadEnter, owner);
    }

    const bool home = ImGui::IsKeyPressed(ImGuiKey_Home, ImGuiInputFlags_Repeat, owner);
    const bool end = ImGui::IsKeyPressed(ImGuiKey_End, ImGuiInputFlags_Repeat, owner);
    if (home != end)
    {
        ListJumpDesc jump;
        jump.rowCount = frame.rowCount;
        jump.selection = frame.selection;
        jump.single = frame.single;
        jump.focusScope = frame.focusScope;
        jump.focusIndex = state.focusIndex;
        const int target = ListJumpToEnd(jump, end, nullptr);
        if (target >= 0)
            FocusRowIndex(frame, target);
    }
    const bool left = ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat, owner);
    const bool right = ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat, owner);
    if (left != right)
        MoveCellFocus(frame, right ? 1 : -1);

    if (state.focusIndex < 0 || frame.readOnly || frame.disabled)
        return;
    const bool f2 = ImGui::IsKeyPressed(ImGuiKey_F2, ImGuiInputFlags_None, owner);
    const bool enter = editableCell && (ImGui::IsKeyPressed(ImGuiKey_Enter, ImGuiInputFlags_None, owner) ||
                                        ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, ImGuiInputFlags_None, owner));
    if (!f2 && !enter)
        return;
    int column = ColumnUsable(frame, state.focusColumn) ? state.focusColumn : -1;
    if (column < 0 || !CellEditable(frame, column))
    {
        // F2 on the whole row edits its first editable cell in display order.
        column = -1;
        for (int o = 0; o < frame.columnCount && column < 0; ++o)
            if (CellEditable(frame, frame.table->DisplayOrderToIndex[o]))
                column = frame.table->DisplayOrderToIndex[o];
    }
    if (column >= 0)
        StartEdit(frame, RowKeyOf(frame, state.focusIndex), column);
}

// The part of the table under the header: where the rows scroll and the data states show.
ImRect BodyRect(const DataTableFrame& frame)
{
    ImRect body = frame.table->InnerClipRect;
    body.Min.y = std::min(body.Max.y, frame.table->OuterRect.Min.y + frame.headerHeight);
    return body;
}

// Closes the Dear ImGui table and the bars, draws the lines over it, and lays the widget out as
// one item of the frame's size (rule 11: the last item covers the whole widget).
void FinishTable(DataTableFrame& frame)
{
    ImGuiTable* table = frame.table;
    ImGuiWindow* inner = table->InnerWindow;
    const ImRect innerRect = inner->Rect();
    const bool scrolledSideways = inner->Scroll.x > 0.0f && table->FreezeColumnsRequest > 0;
    float frozenEdge = 0.0f;
    if (scrolledSideways)
    {
        const ImGuiTableColumn& last = table->Columns[table->DisplayOrderToIndex[table->FreezeColumnsRequest - 1]];
        frozenEdge = std::round(last.MaxX);
    }
    ImGui::EndTable();
    ImGui::PopStyleVar();
    EndScrollAreaBars();
    DataTableApplyResize(frame);

    // Over the rows: the frozen columns end in a hairline while columns scroll under them.
    if (scrolledSideways && frozenEdge > innerRect.Min.x && frozenEdge < innerRect.Max.x)
    {
        ImDrawList* drawList = inner->DrawList;
        drawList->PushClipRect(innerRect.Min, innerRect.Max, true);
        const float hairline = HairlineThickness();
        FillRect(drawList, ImVec2(frozenEdge, innerRect.Min.y), ImVec2(frozenEdge + hairline, innerRect.Max.y),
                 ToU32(T().colors.separatorOpaque), 0.0f);
        drawList->PopClipRect();
    }

    ImGuiWindow* outer = frame.outerWindow;
    outer->DC.CursorPos = frame.cursorStart;
    outer->DC.IsSameLine = false;
    ImGui::ItemSize(frame.frame.GetSize());
    ImGui::ItemAdd(frame.frame, frame.id, nullptr, ImGuiItemFlags_NoNav);
}

} // namespace

DataTableFrame* CurrentDataTable()
{
    DataTableStack& stack = Tables();
    return stack.depth > 0 ? &stack.frames[stack.depth - 1] : nullptr;
}

bool DataTableColumnEditable(const DataTableFrame& frame, int column)
{
    if (column < 0 || column >= IMGUI_TABLE_MAX_COLUMNS)
        return false;
    return (frame.editable[column / 32] & (1u << (column % 32))) != 0;
}

ImRect DataTableRowSpan(const DataTableFrame& frame, float top, float bottom)
{
    // From the first column (or the visible left edge) to the visible right edge, also where the
    // columns end before it, so every row reads as one band across the table.
    const ImGuiTable* table = frame.table;
    const float outset = FocusRingOutset();
    const float left =
        std::max(table->Columns[table->LeftMostEnabledColumn].MinX, table->InnerClipRect.Min.x + outset);
    const float right = std::max(table->InnerClipRect.Max.x - outset, left + 1.0f);
    return ImRect(left, top, right, bottom);
}

ImRect DataTableColumnClip(const DataTableFrame& frame, int column)
{
    const ImGuiTable* table = frame.table;
    const ImGuiTableColumn& c = table->Columns[column];
    const float outset = FocusRingOutset();
    // Where the column is cut (by the frozen columns or the table edge) the clip stays at the
    // cut; elsewhere it grows by the ring outset so a ring around the cell stays whole.
    const float minX = c.ClipRect.Min.x > c.MinX ? c.ClipRect.Min.x : c.ClipRect.Min.x - outset;
    const float maxX = c.ClipRect.Max.x < c.MaxX ? c.ClipRect.Max.x : c.ClipRect.Max.x + outset;
    ImRect clip(minX, table->InnerClipRect.Min.y, maxX, table->InnerClipRect.Max.y);
    clip.ClipWithFull(table->InnerClipRect);
    return clip;
}

void DataTableBeginBackground(const DataTableFrame& frame)
{
    ImGuiTable* table = frame.table;
    ImGuiWindow* window = table->InnerWindow;
    ImGui::SetWindowClipRectBeforeSetChannel(window, table->Bg2ClipRectForDrawCmd);
    table->DrawSplitter->SetCurrentChannel(window->DrawList, table->Bg2DrawChannelCurrent);
}

void DataTableEndBackground(const DataTableFrame& frame)
{
    ImGuiTable* table = frame.table;
    if (table->CurrentColumn >= 0)
        ImGui::TablePushColumnChannel(table->CurrentColumn);
}

ImGuiID DataTableKeyOwner()
{
    static const ImGuiID owner = ImHashStr("##facet.datatable.keys");
    return owner;
}

void DataTableEndFrame()
{
    // Begin without End: the scope stack reports it by name; the frames are dropped here so the
    // next frame starts with no open table.
    Tables().depth = 0;
}

} // namespace internal

using namespace internal;

bool BeginDataTable(const char* id, std::span<const DataTableColumn> columns, int rowCount,
                    ImGuiSelectionBasicStorage* selection, const DataTableOptions& options)
{
    // 1. Entry.
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    DataTableStack& stack = Tables();
    if (stack.depth >= kDataTableDepth)
    {
        ReportUsageError("BeginDataTable nested deeper than %d; the table is not opened", kDataTableDepth);
        return false;
    }
    ImGuiContext& g = *GImGui;
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const Theme& theme = T();
    const ImGuiID tableId = window->GetID(id);
    const int columnCount =
        static_cast<int>(std::min<std::size_t>(columns.size(), static_cast<std::size_t>(IMGUI_TABLE_MAX_COLUMNS - 1)));
    if (rowCount < 0)
        rowCount = 0;

    // 2. Measure: the width rule, and the height (0 fills what is left, at least a header and
    // three rows).
    const float rowHeight = options.rowHeight > 0.0f ? Px(options.rowHeight) : ListRowHeight(ControlSize::Inherit);
    const float headerHeight = Px(theme.table.headerHeight);
    const float inset = Px(theme.spacing.xs);
    const float width = ResolveItemWidth(options.width, next, kFill);
    float height = 0.0f;
    if (options.height > 0.0f)
        height = Px(options.height);
    else
        height = std::max(ImGui::GetContentRegionAvail().y, headerHeight + rowHeight * 3.0f + inset * 2.0f);
    height = std::max(height, headerHeight + inset * 2.0f);
    const ImVec2 cursorStart = window->DC.CursorPos;
    const ImRect frameRect(cursorStart, ImVec2(cursorStart.x + width, cursorStart.y + height));

    // Out of sight, or nothing to show a table of: one item of the size.
    if (columnCount <= 0 || ImGui::IsClippedEx(frameRect, 0))
    {
        if (columnCount <= 0)
            ReportUsageError("BeginDataTable \"%s\" without columns", id);
        ImGui::ItemSize(frameRect.GetSize());
        ImGui::ItemAdd(frameRect, tableId, nullptr, ImGuiItemFlags_NoNav);
        return false;
    }

    // The grouped surface, behind the table's inner window.
    FillRect(window->DrawList, frameRect.Min, frameRect.Max, ToU32(theme.colors.bgSecondary), Px(theme.radii.card));
    StrokeRect(window->DrawList, frameRect.Min, frameRect.Max, ToU32(theme.colors.separator), Px(theme.radii.card),
               HairlineThickness());

    // 3. The Dear ImGui table: its inner window without stock bars or background, flattened into
    // the page's navigation so Tab reaches the headers and rows like any other item.
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_PadOuterX |
                            ImGuiTableFlags_NoBordersInBody;
    if (options.resizable)
        flags |= ImGuiTableFlags_Resizable;
    if (options.reorderable)
        flags |= ImGuiTableFlags_Reorderable;
    if (options.hideable)
        flags |= ImGuiTableFlags_Hideable;
    if (options.sortable)
        flags |= ImGuiTableFlags_Sortable;
    if (options.sortable && options.multiSort)
        flags |= ImGuiTableFlags_SortMulti;
    window->DC.CursorPos = ImVec2(frameRect.Min.x + inset, frameRect.Min.y + inset);
    const ImVec2 tableSize(std::max(1.0f, width - inset * 2.0f), std::max(1.0f, height - inset * 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(Px(theme.spacing.md), 0.0f));
    // The stock bars take no room (a zero bar size; ImGuiWindowFlags_NoScrollbar would also stop
    // the content from being wider than the window), and Dear ImGui leaves the wheel to the scroll
    // core, whose overlay bars BeginScrollAreaBars adds below.
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 0.0f);
    ImGui::SetNextWindowFlags(ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground, true);
    ImGui::SetNextWindowChildFlags(ImGuiChildFlags_NavFlattened, true);
    const bool tableOpen = ImGui::BeginTableEx(id, tableId, columnCount, flags, tableSize, 0.0f);
    ImGui::PopStyleVar();
    if (!tableOpen)
    {
        ImGui::PopStyleVar();
        window->DC.CursorPos = cursorStart;
        window->DC.IsSameLine = false;
        ImGui::ItemSize(frameRect.GetSize());
        // Dear ImGui already added an item with the table id for its clipped table.
        ImGui::ItemAdd(frameRect, 0, nullptr, ImGuiItemFlags_NoNav);
        return false;
    }
    ImGuiTable* table = g.CurrentTable;

    DataTableFrame& frame = stack.frames[stack.depth++];
    frame = DataTableFrame();
    frame.id = tableId;
    frame.table = table;
    frame.outerWindow = window;
    frame.window = table->InnerWindow;
    frame.state = &GetState<DataTableState>(tableId);
    frame.frame = frameRect;
    frame.cursorStart = cursorStart;
    frame.rowCount = rowCount;
    frame.columnCount = columnCount;
    frame.selection = selection;
    frame.single = options.singleSelect;
    frame.readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    frame.sortable = options.sortable;
    frame.multiSort = options.sortable && options.multiSort;
    frame.hideable = options.hideable;
    frame.reorderable = options.reorderable;
    frame.resizable = options.resizable;
    frame.disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    frame.rowHeight = rowHeight;
    frame.headerHeight = headerHeight;
    frame.forced = forced;

    // Columns: a sizing flag the caller set wins (Dear ImGui accepts one per column).
    ImGui::TableSetupScrollFreeze(std::clamp(options.frozenColumns, 0, columnCount), 1);
    for (int i = 0; i < columnCount; ++i)
    {
        const DataTableColumn& column = columns[static_cast<std::size_t>(i)];
        ImGuiTableColumnFlags columnFlags = column.flags;
        if ((columnFlags & ImGuiTableColumnFlags_WidthMask_) == 0)
            columnFlags |= column.width > 0.0f ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthStretch;
        const bool fixed = (columnFlags & ImGuiTableColumnFlags_WidthFixed) != 0;
        ImGui::TableSetupColumn(column.label != nullptr ? column.label : "", columnFlags,
                                fixed && column.width > 0.0f ? Px(column.width) : 0.0f);
        if (column.editable)
            frame.editable[i / 32] |= 1u << (i % 32);
    }
    // Dear ImGui turns on its inner vertical borders for a resizable table; the header draws its own
    // dividers, so the stock lines (in stock colors) are left out.
    table->Flags &= ~ImGuiTableFlags_BordersInnerV;
    // The layout, with the narrowest column of the table.minColumnWidth token: Dear ImGui takes
    // its minimum column width (without the spacing and padding around the cell) from
    // FramePadding.x.
    const float cellExtra = table->CellSpacingX1 + table->CellSpacingX2 + table->CellPaddingX * 2.0f;
    const float minWidth = std::max(1.0f, Px(theme.table.minColumnWidth) - cellExtra);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(minWidth, g.Style.FramePadding.y));
    ImGui::TableUpdateLayout(table);
    ImGui::PopStyleVar();

    // The bars take the pointer before the header and the rows.
    ScrollAreaBarsDesc bars;
    bars.horizontal = true;
    bars.insetTop = headerHeight;
    if (table->FreezeColumnsRequest > 0)
    {
        const ImGuiTableColumn& last = table->Columns[table->DisplayOrderToIndex[table->FreezeColumnsRequest - 1]];
        bars.insetLeft = std::max(0.0f, last.MaxX - frame.window->Pos.x);
    }
    BeginScrollAreaBars(bars);

    // 4. The header row, then the rows or the state in their place.
    DataTableHeaderRow(frame, columns.first(static_cast<std::size_t>(columnCount)));
    ImGuiWindow* inner = frame.window;
    inner->DC.NavLayerCurrent = static_cast<ImGuiNavLayer>(table->NavLayer);
    if (options.state != DataState::Ready || rowCount == 0)
    {
        DataTableBeginBackground(frame);
        const ImRect body = BodyRect(frame);
        DrawListState(body, options.state, options.emptyText, options.errorText);
        DataTableEndBackground(frame);
        frame.state->editPhase = DataTableState::EditPhase::None;
        FinishTable(frame);
        --stack.depth;
        return false;
    }

    DataTableState& state = *frame.state;
    if (state.focusIndex >= rowCount)
        state.focusIndex = -1;
    // The caller sorts between Begin and End when the specs are dirty, after the rows' order was
    // read here: the Shift range start is then not recorded at End, so the next frame finds its
    // entry at the new place (ListRangeAnchor::Remap).
    frame.sortPending = frame.sortable && (table->IsSortSpecsDirty || table->SortSpecs.SpecsDirty);
    frame.seed = ImHashStr("##rows", 0, tableId);
    frame.outerScope = g.CurrentFocusScopeId;
    frame.focusScope = selection != nullptr ? inner->IDStack.back() : g.CurrentFocusScopeId;
    frame.rowFocused = g.NavId != 0 && g.NavId == state.focusedId;
    const bool editorActive = state.editPhase == DataTableState::EditPhase::Active && state.editorId != 0 &&
                              g.ActiveId == state.editorId;
    frame.listFocused = frame.rowFocused || editorActive;

    // Tab: the rows of a selection are in its own focus scope, entered and left through a tab stop.
    bool enterByTab = false;
    if (selection != nullptr && !frame.disabled)
    {
        DataTableBeginBackground(frame);
        const ImRect body = BodyRect(frame);
        const bool rowScope = g.NavId != 0 && g.NavFocusScopeId == frame.focusScope;
        if (!rowScope)
            enterByTab = ListTabProxy(tableId, body);
        else if (frame.rowFocused)
            ListClaimTabExit(inner, DataTableKeyOwner(), tableId, frame.outerScope, body);
        DataTableEndBackground(frame);
    }
    if (frame.rowFocused && !frame.disabled)
        ReadRowKeys(frame);

    if (selection != nullptr)
    {
        state.anchor.Remap(selection, frame.focusScope, rowCount);
        ListSelectionOptions selectionOptions;
        selectionOptions.single = options.singleSelect;
        selectionOptions.boxSelect = true;
        // readOnly keeps the cells from editing; the selection still works (display.h).
        BeginListSelection(selection, rowCount, selectionOptions);
    }
    frame.tabStop = state.focusIndex;
    if (enterByTab)
    {
        // The focus moves on from the tab stop to the row Tab lands on; the selection stays.
        const int target = frame.tabStop >= 0 ? frame.tabStop : 0;
        FocusRowIndex(frame, target);
        frame.tabStop = target;
        frame.listFocused = true;
    }
    BeginListRows({.rowCount = rowCount, .rowHeight = rowHeight, .columns = 1, .keepAlive = frame.tabStop});
    frame.group.Begin(frame.listFocused, &frame.forced);
    frame.rowsOpen = true;
    PushScope(kTableScope);
    return true;
}

namespace
{

// Ends the row being submitted at exactly one row height, so the next row starts where the row
// ranges expect it whatever the cells drew.
void EndCurrentRow(DataTableFrame& frame)
{
    ImGuiTable* table = frame.table;
    if (!table->IsInsideRow)
        return;
    if (table->CurrentColumn != -1)
    {
        ImGui::TableEndCell(table);
        table->CurrentColumn = -1;
    }
    table->RowPosY2 = table->RowPosY1 + frame.rowHeight;
}

} // namespace

bool DataTableRow(int index)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    // Outside an open table nothing happens.
    DataTableFrame* framePtr = CurrentDataTable();
    if (framePtr == nullptr || !framePtr->rowsOpen)
        return false;
    DataTableFrame& frame = *framePtr;
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = frame.table;
    if (g.CurrentTable != table || g.CurrentWindow != frame.window || index < 0 || index >= frame.rowCount)
        return false;
    DataTableState& state = *frame.state;

    EndCurrentRow(frame);
    ImGui::TableNextRow(ImGuiTableRowFlags_None, frame.rowHeight);
    const float top = table->RowPosY1;
    const float bottom = top + frame.rowHeight;
    // Text in the cells sits centered on the row (TableBeginCell hands this baseline on).
    table->RowTextBaseline = std::max(0.0f, CenteredTextY(top, frame.rowHeight) - top);

    const ImGuiID key = RowKeyOf(frame, index);
    const ImGuiID rowId = RowIdOf(frame, key);
    const ImRect rect = DataTableRowSpan(frame, top, bottom);
    bool tabStop = false;
    if (frame.tabStop >= 0)
        tabStop = index == frame.tabStop;
    else if (!frame.tabStopGiven)
        tabStop = true;
    frame.tabStopGiven = frame.tabStopGiven || tabStop;
    const bool focusedBefore = g.NavId == rowId;
    const bool cellFocus = focusedBefore && ColumnUsable(frame, state.focusColumn);

    // The row item lives in a cell of the row (any enabled column) and draws into the background
    // channel, under every cell; the caller's first TableNextColumn then starts column 0 afresh.
    ImGui::TableSetColumnIndex(table->LeftMostEnabledColumn);
    DataTableBeginBackground(frame);
    ListRowDesc desc;
    if (frame.selection != nullptr)
    {
        desc.selection = index;
        desc.selected = IsListEntrySelected(frame.selection, index);
    }
    desc.tabStop = tabStop;
    desc.part = index;
    desc.inPlace = true;
    desc.allowOverlap = true;
    desc.ownRing = cellFocus;
    const ListRowState row = frame.group.Row(rowId, rect, desc);
    // The background clip stays until the caller's first TableNextColumn sets the cell's own, so
    // the row item counts as shown wherever along the row it is (probes, ImGui::IsItemVisible).
    ImGui::TableEndCell(table);
    table->CurrentColumn = -1;

    frame.row = index;
    frame.rowKey = key;
    frame.rowId = rowId;
    frame.rowRect = rect;
    if (row.focused)
    {
        if (state.focusIndex != index || state.focusedId != rowId)
            RevealRow(frame, rect, true);
        state.focusIndex = index;
        state.focusedId = rowId;
        if (ColumnUsable(frame, state.focusColumn))
        {
            frame.cellRing = true;
            frame.cellRingRect = CellRect(frame, state.focusColumn, top, bottom);
            frame.cellRingClip = DataTableColumnClip(frame, state.focusColumn);
            frame.cellRingAmount = row.item.focusAmount;
        }
    }

    bool activated = row.activated;
    if (activated && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        // A double click on an editable cell edits it instead of activating the row.
        const int column = ImGui::TableGetHoveredColumn();
        if (CellEditable(frame, column))
        {
            StartEdit(frame, key, column);
            activated = false;
        }
    }
    return activated;
}

void EndDataTable()
{
    if (!PopScope(kTableScope))
        return;
    DataTableFrame* framePtr = CurrentDataTable();
    if (framePtr == nullptr)
        return;
    DataTableFrame& frame = *framePtr;
    ImGuiContext& g = *GImGui;
    DataTableStack& stack = Tables();
    if (g.CurrentTable != frame.table || g.CurrentWindow != frame.window)
    {
        ReportUsageError("EndDataTable called outside the table its BeginDataTable opened");
        --stack.depth;
        return;
    }
    DataTableState& state = *frame.state;
    EndCurrentRow(frame);
    EndListRows();
    if (frame.selection != nullptr)
    {
        (void)EndListSelection();
        if (!frame.sortPending)
            state.anchor.Record(frame.selection, frame.focusScope, frame.rowCount);
    }
    // The focus ring of the focused row, or of its focused cell, over every row.
    DataTableBeginBackground(frame);
    frame.group.End();
    if (frame.cellRing && frame.cellRingAmount > 0.0f)
    {
        ImDrawList* drawList = frame.window->DrawList;
        drawList->PushClipRect(frame.cellRingClip.Min, frame.cellRingClip.Max, true);
        DrawFocusRing(drawList, frame.cellRingRect, Px(T().radii.control), frame.cellRingAmount);
        drawList->PopClipRect();
    }
    DataTableEndBackground(frame);
    // A session whose editor did not show up in time (its row is gone) ends quietly.
    if (state.editPhase == DataTableState::EditPhase::Starting && ++state.editFrames > kDataTableEditStartFrames)
        state.editPhase = DataTableState::EditPhase::None;
    FinishTable(frame);
    --stack.depth;
}

bool BeginDataTableCellEdit()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    DataTableFrame* framePtr = CurrentDataTable();
    if (framePtr == nullptr || !framePtr->rowsOpen || framePtr->editOpen)
        return false;
    DataTableFrame& frame = *framePtr;
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = frame.table;
    if (g.CurrentTable != table || !table->IsInsideRow || table->CurrentColumn < 0 || frame.row < 0)
        return false;
    DataTableState& state = *frame.state;
    if (state.editPhase == DataTableState::EditPhase::None || state.editKey != frame.rowKey ||
        state.editColumn != table->CurrentColumn)
        return false;
    if (!CellEditable(frame, table->CurrentColumn))
    {
        state.editPhase = DataTableState::EditPhase::None;
        return false;
    }
    frame.editOpen = true;
    PushScope(kEditScope);
    // The editor fills the cell, centered on the row.
    ImGuiWindow* window = frame.window;
    const float editorHeight = ControlHeight(ControlSize::Inherit);
    window->DC.CursorPos.y = table->RowPosY1 + std::max(0.0f, (frame.rowHeight - editorHeight) * 0.5f);
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (state.editPhase == DataTableState::EditPhase::Starting)
        ImGui::SetKeyboardFocusHere();
    return true;
}

void EndDataTableCellEdit()
{
    if (!PopScope(kEditScope))
        return;
    DataTableFrame* framePtr = CurrentDataTable();
    if (framePtr == nullptr)
        return;
    DataTableFrame& frame = *framePtr;
    frame.editOpen = false;
    DataTableState& state = *frame.state;
    ImGuiContext& g = *GImGui;
    if (state.editPhase == DataTableState::EditPhase::Starting)
    {
        if (ImGui::IsItemActive())
        {
            state.editPhase = DataTableState::EditPhase::Active;
            state.editorId = g.LastItemData.ID;
        }
        else if (++state.editFrames > kDataTableEditStartFrames)
        {
            state.editPhase = DataTableState::EditPhase::None;
        }
        return;
    }
    if (state.editPhase != DataTableState::EditPhase::Active)
        return;
    if (ImGui::IsItemActive())
    {
        state.editorId = g.LastItemData.ID;
        return;
    }
    // The editor was deactivated: Enter, Tab or Escape give the focus back to the cell; a click
    // elsewhere leaves it where the click put it.
    const ImGuiIO& io = g.IO;
    bool clicked = false;
    for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
        clicked = clicked || io.MouseClicked[button];
    if (!clicked && ImGui::IsKeyPressed(ImGuiKey_Tab, ImGuiInputFlags_None, ImGuiKeyOwner_Any))
        ImGui::NavMoveRequestCancel();
    EndEdit(frame, !clicked);
}

} // namespace facet
