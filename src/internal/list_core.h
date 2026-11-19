#pragma once

// List and selection core: the visible row ranges of long lists, grids and tables (virtualized
// through ImGuiListClipper), the selection storage of the frozen list and table signatures,
// typeahead and the selection pill of list rows.
//
// This header holds what the other blocks and the Phase 2 containers call: facet::NextVisibleRows
// (common.h) reads the row ranges opened here, and VirtualList, ItemGrid, DataTable and the list
// widgets open them and drive their selection through it. The list core owns the rest
// (typeahead, row drawing, reordering through the drag and drop core, scrolling rows into view
// through the scroll core) and adds it to this file and to further list_* files.
//
// Row ranges: a container opens them before its row loop and closes them after it; the caller's
// loop in between is the one facet::NextVisibleRows documents:
//
//     BeginListRows({.rowCount = count, .rowHeight = rowHeight, .keepAlive = focusedRow});
//     RowRange rows;
//     while (facet::NextVisibleRows(&rows))      // forwards to NextListRows
//         for (int i = rows.first; i < rows.last; ++i)
//             ... row i ...
//     EndListRows();
//
// Ranges open on the current window and nest (a list inside a table cell) up to kListRowsDepth.
// The rows before and after the visible ones are skipped with their height, so the scroll range
// and the scrollbar stay right; keepAlive is handed out as an extra range even when it is out of
// view, so the keyboard focus and an active row survive scrolling.
//
// Selection: every multi-select list, grid and table takes the caller's
// ImGuiSelectionBasicStorage (lists.h, containers.h, display.h and layout.h freeze it) and drives
// it through ImGui::BeginMultiSelect, so Ctrl, Shift, box select and Ctrl+A behave as in every
// other Dear ImGui list. Index-based containers key an entry by
// storage->AdapterIndexToStorageId(index): the index itself unless the caller set the adapter.
// The tree view keys nodes by id and applies its selection itself (lists.h, BeginTreeView), so it
// does not use the pair below.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/common.h"

namespace facet::internal
{

// The selection storage type of every frozen list, grid and table signature.
using ListSelection = ImGuiSelectionBasicStorage;

inline constexpr int kListRowsDepth = 8;

struct ListRowsDesc
{
    int rowCount = 0;       // entries (rows, or tile indices for a grid)
    float rowHeight = 0.0f; // pixels from the top of one line of entries to the next; > 0
    int columns = 1;        // entries per line (grid tiles); ranges always cover whole lines
    int keepAlive = -1;     // an entry handed out even when it is out of view; -1 for none
};

// Opens row ranges for the current window. Every BeginListRows needs its EndListRows, also when
// the loop stops early. Deeper than kListRowsDepth is reported and hands out nothing.
void BeginListRows(const ListRowsDesc& desc);

// The next range of entries of the innermost open BeginListRows: the visible ones, then
// keepAlive. Returns false when every range was handed out, and when none is open (rows is then
// {0, 0}). facet::NextVisibleRows forwards here.
bool NextListRows(RowRange* rows);

// Closes the innermost BeginListRows and lays out the space of the entries that were not handed
// out. A call without an open range is reported and ignored.
void EndListRows();

struct ListSelectionOptions
{
    bool single = false;     // at most one selected entry
    bool boxSelect = true;   // a drag from empty space selects the entries it covers
    bool readOnly = false;   // the selection shows but requests leave the storage alone
};

// Starts multi-selection over `itemCount` index-keyed entries written to `selection` (null: the
// entries are not selectable, and the pair does nothing). Between the two calls every entry calls
// ImGui::SetNextItemSelectionUserData(index) before its item.
void BeginListSelection(ListSelection* selection, int itemCount, const ListSelectionOptions& options = {});

// Applies the selection requests of the frame to the storage. Returns true when the selection
// changed on this frame.
bool EndListSelection();

// Whether entry `index` of an index-keyed container is selected in `selection` (false for null).
// Not const, because Dear ImGui's adapter (GetStorageIdFromIndex) takes the storage as non-const.
bool IsListEntrySelected(ListSelection* selection, int index);

} // namespace facet::internal
