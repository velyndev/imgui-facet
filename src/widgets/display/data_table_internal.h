#pragma once

// Links between the files of DataTable (facet/widgets/display.h): data_table.cpp opens the table,
// runs the rows, the keys and the cell edit session; data_table_header.cpp draws the header row and
// its column menu; data_table_frame.cpp resets the open tables at the end of a frame. Not for other
// modules.

#include <cstdint>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/display.h"
#include "internal/item.h"
#include "internal/list_core.h"
#include "internal/list_view.h"

namespace facet::internal
{

// Tables open at once (a table in a cell of another table counts).
inline constexpr int kDataTableDepth = 4;

// The forced capture states of a table: part i is row i (part -1 the first row), and the header
// cell of column n is kDataTableHeaderPart + n (Hovered, Pressed, Focused; Open shows its menu).
inline constexpr int kDataTableHeaderPart = 1 << 20;

// What a table keeps under its id between frames.
struct DataTableState
{
    int focusIndex = -1;       // the row with the keyboard focus (or the last one that had it)
    ImGuiID focusedId = 0;     // its row item id
    int focusColumn = -1;      // the focused cell's column index; -1 while the whole row has the focus
    ListRangeAnchor anchor;    // the Shift range start across a sort
    // The column menu: the column it was opened for (-1: below no column), and an action chosen
    // inside it, applied in the table once its level is closed.
    int menuColumn = -1;
    // Column resizing with the band of data_table_header.cpp.
    int resizeColumn = -1;     // the column being resized, -1 for none
    float resizeStartWidth = 0.0f;
    float resizeStartMouse = 0.0f;
    float resizeWidth = 0.0f;  // the width it asks for while the band is held
    // The cell edit session: the row is kept by its storage key, so the session follows the item
    // when the caller sorts.
    enum class EditPhase
    {
        None,
        Starting,  // the editor was asked to take the keyboard focus; it has not become active yet
        Active,    // the editor is active
    };
    EditPhase editPhase = EditPhase::None;
    ImGuiID editKey = 0;
    int editColumn = -1;
    int editFrames = 0;        // frames in Starting
    ImGuiID editorId = 0;      // the editor item, once it was seen
};

// One open table between BeginDataTable and EndDataTable.
struct DataTableFrame
{
    ImGuiID id = 0;                 // the widget id (and the Dear ImGui table id)
    ImGuiTable* table = nullptr;
    ImGuiWindow* outerWindow = nullptr;
    ImGuiWindow* window = nullptr;  // the table's inner window, where the rows are
    DataTableState* state = nullptr;
    ImRect frame;                   // the grouped surface around the table, screen pixels
    ImVec2 cursorStart;             // where the widget started in the outer window
    int rowCount = 0;
    int columnCount = 0;
    ListSelection* selection = nullptr;
    bool single = false;
    bool readOnly = false;
    bool sortable = false;
    bool multiSort = false;
    bool hideable = false;
    bool reorderable = false;
    bool resizable = false;
    bool disabled = false;          // ImGui::BeginDisabled at Begin
    float rowHeight = 0.0f;         // pixels
    float headerHeight = 0.0f;      // pixels
    ImGuiID seed = 0;               // row ids are hashed from the row key with it
    ImGuiID focusScope = 0;         // the selection scope of the rows (the table id stack top)
    ImGuiID outerScope = 0;         // the focus scope around it
    bool rowFocused = false;        // a row item has the keyboard focus (not a cell editor)
    bool listFocused = false;       // the rows hold the keyboard focus (rows or a cell editor)
    bool rowsOpen = false;          // the rows, selection and ranges are open (Ready with rows)
    bool sortPending = false;       // the sort specs are dirty: the caller reorders the rows this frame
    ForcedItemRequest forced;
    ListRowGroup group;
    int tabStop = -1;               // the row Tab lands on
    bool tabStopGiven = false;
    std::uint32_t editable[(IMGUI_TABLE_MAX_COLUMNS + 31) / 32] = {};  // bit per editable column
    // The row being submitted.
    int row = -1;
    ImGuiID rowKey = 0;
    ImGuiID rowId = 0;
    ImRect rowRect;
    // The cell ring: the focused cell of the focused row, drawn after the rows.
    bool cellRing = false;
    ImRect cellRingRect;
    ImRect cellRingClip;
    float cellRingAmount = 0.0f;
    // Between BeginDataTableCellEdit and EndDataTableCellEdit.
    bool editOpen = false;
};

// The innermost open table, or null.
DataTableFrame* CurrentDataTable();

// Whether column `column` of the table was declared editable.
bool DataTableColumnEditable(const DataTableFrame& frame, int column);

// The screen rectangle a row covers: across every enabled column to the right edge of the part of
// the table that shows (the inner clip rectangle less the focus ring outset at the edges), so the
// fill and the ring span all columns, also while the table is scrolled sideways.
ImRect DataTableRowSpan(const DataTableFrame& frame, float top, float bottom);

// The clip rectangle for drawing over column `column` in the background channel: its own columns'
// clip rect grown by the focus ring outset, never over the columns that stay in view when it
// scrolls under them.
ImRect DataTableColumnClip(const DataTableFrame& frame, int column);

// Draws into the table's background channel (below the cells, above the row fills of Dear ImGui)
// with its clip rectangle until DataTableEndBackground.
void DataTableBeginBackground(const DataTableFrame& frame);
void DataTableEndBackground(const DataTableFrame& frame);

// The header row (data_table_header.cpp): the Facet header cells with sorting, reordering by
// drag, the resize bands and the column menu. Called once per table right after the layout.
// `columns` are the caller's columns (read during BeginDataTable only).
void DataTableHeaderRow(DataTableFrame& frame, std::span<const DataTableColumn> columns);
// After EndTable: applies a resize the band asked for (data_table_header.cpp).
void DataTableApplyResize(DataTableFrame& frame);

// The key owner every table reads its keys with (only one table has the keyboard focus).
ImGuiID DataTableKeyOwner();

// data_table_frame.cpp: closes what a table left open at the end of the frame.
void DataTableEndFrame();

} // namespace facet::internal
