#pragma once

// List view: the rows, keyboard, typeahead and look that every list widget shares, built on the
// row ranges and selection of list_core.h, the scroll area of scroll.h and the item behaviour,
// focus ring and selection pill blocks. ListBox, SelectableList, VirtualList, LogView and the
// open lists of Dropdown, MultiDropdown and Autocomplete use BeginListView; TreeView,
// CheckboxTree and LayerList use BeginListTree; DataTable, ItemGrid and other containers that
// place their own rows use the row block (ListRowGroup) further down.
//
//     ListViewDesc desc{.rowCount = count, .size = options.size, .selection = selection,
//                       .entry = &EntryOf, .user = &items, .state = options.state};
//     if (BeginListView(ImGui::GetID(label), desc))
//     {
//         RowRange rows;
//         while (NextListRows(&rows))                  // or facet::NextVisibleRows
//             for (int i = rows.first; i < rows.last; ++i)
//             {
//                 const ListRowState row = ListViewRow(i);
//                 PushPillLabelStyle(TextStyle::Body, row.selected);     // selection_pill.h
//                 ... draw the label inside row.content (the cursor is there, on the row's line) ...
//                 PopTextStyle();
//             }
//         const ListViewResult result = EndListView();  // only after true
//     }
//
// What the view does, so the widgets do not:
//   - Rows: one fixed height per control size (the list tokens through Px; ListRowHeight), in a
//     scroll area, virtualized: only the visible rows are handed out, plus the row Tab lands on,
//     the keyboard focus and the range start of Shift selections, so a million rows cost what
//     fifty do and the focus survives scrolling. A row is placed at its exact position whatever
//     the caller drew in the row before it. Row ids are hashed from the entry id
//     (ListEntryInfo::id, else the selection storage id of the index, else the index) under the
//     view, so the focus and the selection follow an entry when the caller sorts or filters.
//   - Selection: a multiple selection in the caller's ImGuiSelectionBasicStorage, or a single
//     choice in an int (the index; ListBox). Both run on ImGui::BeginMultiSelect, so click,
//     Ctrl+click, Shift+click, Shift+arrows, Ctrl+A, Escape and box select behave as in a stock
//     multi-select list. Disabled entries are shown, not focusable, skipped by the arrows and by
//     typeahead, and left out of ranges and of select-all.
//   - Keyboard: the view is one Tab stop (the focused row, else the chosen row, else the first);
//     the arrows, Page Up and Down, Home and End move through Dear ImGui navigation; Enter or a
//     double click activates the row (ListRowState::activated, ListViewResult::activated).
//   - Typeahead: while the view has focus, typed characters gather into a text that moves the
//     focus (and a plain selection, as an arrow key would) to the next entry whose label starts
//     with it, case-insensitive in ASCII; the same letter typed again cycles through the entries
//     that start with it. A pause of list.typeaheadTimeout starts a new text.
//   - Look: the row look below (hover fill[2], the selection accentSubtle), or the sliding
//     selection pill for a single choice (`pill`); the focus ring on the focused row, drawn after
//     every row so neighbours never cover it; no hover while the area scrolls. The caller draws the label, and draws it with PushPillLabelStyle so the
//     selected row also differs by weight, not by color alone, and in labelTertiary when the row
//     is disabled.
//   - States: while `state` is Loading or Error, or with no rows, the area keeps its size, shows
//     the loading, error or empty state (DrawListState) and BeginListView returns false.
//   - Captures: ForceNextItemState (item.h) right before BeginListView applies to the row whose
//     index is the request's part (part -1: the first row).
//   - Driven lists (ListViewDesc::highlight): the keyboard focus stays on the driving item (a text
//     field typing a filter, a dropdown button), and the view keeps an active row instead, drawn
//     with the hover fill and scrolled into view when it moves. It shows the focus ring only while
//     the keyboard modality is on and the keys (or the driving item) put it there: a list opened by
//     Enter or moved by an arrow shows it, a list opened by a click or a row the pointer moved to
//     does not. With keyOwner
//     it reads Up and Down (from no row: the first or the last), Page Up and Down (a page of
//     visible rows), Home and End, and Enter, Keypad Enter or Tab (activate the active row) owned
//     by keyOwner, skipping disabled entries; Escape is the owner's (it closes the level). The
//     pointer moving over a row makes it the active row, and a click activates it. Activation sets
//     ListRowState::activated and ListViewResult::activated, and writes `choice` unless readOnly.
//     Typeahead is off: the driving field has the typed text.
//
//   - Tab: the rows of a selection or choice live in the focus scope of ImGui::BeginMultiSelect,
//     and Dear ImGui's tabbing only moves between items of the focus scope that holds the focus, so
//     on its own it never enters or leaves such a list. A view (and a tree with a selection) adds a
//     tab stop item in the surrounding scope before its rows: when Tab, Shift+Tab or an arrow key
//     lands on it, the focus moves on to the row Tab lands on (without changing the selection).
//     While a row has the focus, the view owns Tab; a press puts the focus back on that item and
//     ListCoreNewFrame starts Dear ImGui's tabbing from there in the next frame, so the focus moves
//     to the item after (or, with Shift, before) the list as from any other item.
//
// State kept under the view id (state_storage.h): the focused row, the typeahead text and its
// time. The scroll offset is the scroll area's.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/common.h"
#include "internal/item.h"
#include "internal/list_core.h"

namespace facet::internal
{

// What the view needs to know about entry `index` beyond its position: the label for typeahead
// (null: the entry never matches; text after "##" is ignored), whether it is disabled, and a
// stable id (0: the selection storage id of the index, else the index).
struct ListEntryInfo
{
    const char* label = nullptr;
    bool disabled = false;
    ImGuiID id = 0;
};

// Fills `out` (already default-initialized) for entry `index`. Called during the view's calls
// only, possibly many times per frame; the label must stay valid until the next call.
using ListEntryFn = void (*)(void* user, int index, ListEntryInfo* out);

// The row height in pixels of a list of `size`: list.rowHeightSmall, rowHeight or rowHeightLarge
// through Px (Inherit is resolved first).
float ListRowHeight(ControlSize size);

struct ListViewDesc
{
    int rowCount = 0;
    ControlSize size = ControlSize::Inherit;  // row height (ListRowHeight)
    float rowHeight = 0.0f;                   // pixels; > 0 replaces the height of `size`
    ImVec2 areaSize = ImVec2(0.0f, 0.0f);     // pixels, as ScrollAreaDesc::size
    ImVec4 fadeSurface = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);  // as ScrollAreaDesc::fadeSurface
    ListSelection* selection = nullptr;       // multiple selection, keyed through the storage's adapter
    int* choice = nullptr;                    // single choice (index, -1 none); ignored when selection is set
    bool single = false;                      // selection holds at most one entry
    bool boxSelect = false;                   // a drag selects the rows it covers (selection only)
    bool readOnly = false;                    // the selection or choice shows but is never written
    bool pill = false;                        // the chosen row sits on the selection pill (choice only)
    ListEntryFn entry = nullptr;              // labels, disabled entries and ids; null: none of them
    void* user = nullptr;
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;          // null uses Strings::empty
    const char* errorText = nullptr;          // null uses Strings::loadFailed
    int revealRow = -1;                       // scrolled into view at once (SetNextVirtualListScrollTo)
    // A list driven from an item that keeps the keyboard focus (the open list of Autocomplete, a
    // searchable Dropdown or a ComboBox; see "Driven lists" above). `highlight` is the active row
    // (index, -1 for none), written by the view; `keyOwner` is the owner the driving item gives
    // the list keys to (a text field's completionPopup level id); 0 reads no keys.
    int* highlight = nullptr;
    ImGuiID keyOwner = 0;
};

struct ListRowOptions
{
    bool disabled = false;  // in addition to ListEntryInfo::disabled and ImGui::BeginDisabled
};

struct ListRowState
{
    ItemState item;          // the row item (ItemBehavior); Dear ImGui's last item after the call
    ImRect rect;             // the whole row, screen pixels
    ImRect content;          // where the caller draws: the row less its padding (and tree indent)
    bool selected = false;   // drawn selected this frame
    bool activated = false;  // Enter or a double click this frame
    bool focused = false;    // has the keyboard focus
    bool disabled = false;
};

struct ListViewResult
{
    bool changed = false;  // the selection or choice was written this frame
    int activated = -1;    // the row activated this frame, -1 for none
};

// Opens the view under `id` at the cursor and its row ranges. Returns false while the area is out
// of sight, and while the state, empty or not Ready, is drawn instead of the rows; call
// EndListView only after true. Between the two, loop NextListRows (facet::NextVisibleRows) and
// start every row with ListViewRow. Views nest up to kListViewDepth deep.
inline constexpr int kListViewDepth = 4;
[[nodiscard]] bool BeginListView(ImGuiID id, const ListViewDesc& desc);
// Starts row `index` of the innermost view: places and registers the row, runs its selection and
// draws its background. The cursor is left at row.content on the row's line for the caller's
// content (ImGui::Text and friends are centered on the row). Rows outside 0 to rowCount - 1
// return a default state.
ListRowState ListViewRow(int index, const ListRowOptions& options = {});
// Closes the row ranges and the selection, draws the selection pill and the focus ring, closes
// the area.
ListViewResult EndListView();

// The typeahead search, on its own for tests and for widgets that keep their entries elsewhere:
// the first enabled entry from `from` on (wrapping) whose label starts with `typed`. A text of a
// single character, or of one character repeated, starts after `from` (so typing it again moves
// on); a longer text starts at `from`. -1 when nothing matches or `typed` is empty.
int ListTypeaheadFind(const char* typed, int count, int from, ListEntryFn entry, void* user);

// ---- Row block ------------------------------------------------------------------------------------
//
// The rows, keys and typeahead of the view on their own, for containers that place their rows
// themselves: DataTable rows in a Dear ImGui table, ItemGrid tiles, LogView lines. Such a
// container opens BeginListRows and BeginListSelection (list_core.h) itself and runs every row
// through one ListRowGroup, so its rows look and behave as a list view's: the hover and selection
// fills (the row look below), no hover while the area scrolls, the focus ring drawn over every
// row, Home and End to the first and last enabled entry with Shift ranges (ListJumpToEnd), and
// typeahead (ListTypeahead).
//
//     ListRowGroup group;
//     group.Begin();
//     RowRange rows;
//     while (NextListRows(&rows))
//         for (int i = rows.first; i < rows.last; ++i)
//         {
//             const ListRowState row = group.Row(RowIdOf(i), RowRectOf(i),
//                                                {.selection = i, .selected = IsListEntrySelected(sel, i)});
//             ... draw the cells inside row.content ...
//         }
//     group.End();                               // the focus ring of the focused row

// The typed text of a list and its search, as the view and the tree keep it under their id.
struct ListTypeahead
{
    static constexpr int kCapacity = 64;
    char text[kCapacity] = {};
    int length = 0;
    double time = -1.0e9;

    // Adds this frame's typed characters (call while the list has focus). A pause of
    // list.typeaheadTimeout starts a new text; a space counts only inside a text, so Space alone
    // still activates the focused row. Returns true when the text changed.
    bool Update();
    // ListTypeaheadFind with the text gathered so far.
    int Find(int count, int from, ListEntryFn entry, void* user) const;
};

struct ListRowDesc
{
    // The entry's selection data (usually its index) inside a selection scope (BeginListSelection
    // or ImGui::BeginMultiSelect): the row then runs Dear ImGui's multi-select hooks. Invalid
    // outside one.
    ImGuiSelectionUserData selection = ImGuiSelectionUserData_Invalid;
    bool selected = false;     // shown selected (inside a scope, a request of this frame may change it)
    bool disabled = false;     // shown, not focusable, skipped by the arrows
    bool tabStop = false;      // the row Tab lands on (one per list)
    // The active row of a driven list: the hover fill, and the focus ring only while the keyboard
    // modality is on (the pointer moving the row never shows the ring).
    bool highlighted = false;
    float indent = 0.0f;       // pixels before row.content (a tree depth)
    int part = -1;             // the part of the group's forced state this row answers to (captures)
    // A row over space the container laid out itself (a table row spanning its cells): the row is
    // a part (PartBehavior, no ItemSize) and the cursor is left where it was before the call.
    bool inPlace = false;
    // A later item over the row (a control in a table cell) may take the hover and the click.
    bool allowOverlap = false;
    // The container draws the focus ring of this row itself (a table draws it around the focused
    // cell instead of the row); End leaves the row out.
    bool ownRing = false;
    // A tile of a grid or a card rather than a row: a selected one adds the accent outline (the row
    // look, ListRowLook::tile). (since 0.1)
    bool tile = false;
};

// The rows of one list in one window, between Begin and End.
class ListRowGroup
{
public:
    // `forced`: a capture's forced state (TakeForcedItemState), applied to the row whose part
    // matches; null for none.
    void Begin(const ForcedItemRequest* forced = nullptr);
    // Places row `id` at `rect` (screen pixels, the whole row), runs its item behaviour and the
    // selection hooks, and draws its fill. The cursor is left at row.content on the row's line.
    ListRowState Row(ImGuiID id, const ImRect& rect, const ListRowDesc& desc);
    // Draws the focus ring of the focused row over every row.
    void End();

private:
    ImGuiWindow* window = nullptr;
    const ForcedItemRequest* forced = nullptr;
    bool hasRing = false;
    bool ringFromHighlight = false;
    ItemState ring;
};

// ---- Row look -------------------------------------------------------------------------------------
//
// The states of a row, a cell or a tile, one look for every list, menu, table and grid (the rows
// rule of DESIGN.md): the row under the pointer and the current row of a driven list or a menu show
// fill[2], a press blends toward fill[1]; a selected row shows accentSubtle (plus the check mark
// where the widget has one); a selected tile or card shows accentSubtle and the accent outline at
// the one border width. No state draws a thicker stroke, and no state draws the focus ring: the ring
// belongs to the keyboard focus alone and sits inside the row (DrawFocusRingInside, focus.h). The
// list view, the tree, the row block and the menu rows draw their rows through these two helpers;
// widgets that place their own rows, cells or tiles call them too.

struct ListRowLook
{
    float hoverAmount = 0.0f;  // 0 to 1: the pointer is over the row (item.hoverAmount)
    float pressAmount = 0.0f;  // 0 to 1: held down (item.pressAmount)
    bool highlighted = false;  // the current row of a driven list or a menu (an open submenu's row): fill[2]
    bool selected = false;     // accentSubtle instead of the hover fill
    bool disabled = false;     // no hover or press fill; a selection still shows
    bool tile = false;         // a tile or a card: a selected one adds the accent outline
};

// The rectangle a list row's fill covers: the row inset one hairline at the top and the bottom, so
// the fills of adjacent selected rows stay apart. Screen pixels.
ImRect ListRowFillRect(const ImRect& row);

// Draws the fill of `look` into `rect` (screen pixels, usually ListRowFillRect of the row) with
// corners of `radius` pixels, and for a selected tile its accent outline (DrawOutline, control.h).
void DrawListRowFill(ImDrawList* drawList, const ImRect& rect, float radius, const ListRowLook& look);

struct ListJumpDesc
{
    int rowCount = 0;
    ListSelection* selection = nullptr;  // the list's multiple selection; null for none
    bool single = false;
    bool readOnly = false;
    ListEntryFn entry = nullptr;         // disabled entries are skipped
    void* user = nullptr;
    ImGuiID focusScope = 0;              // the selection scope (the id stack top at BeginListSelection)
    int focusIndex = -1;                 // the focused entry: the range start when Dear ImGui has none
};

// Home (`end` false) and End for a list that places its own rows: the first or last enabled
// entry, -1 when there is none. With Shift held and a multiple selection the selection becomes the
// range from the range start to it (Ctrl keeps the rest), as navigation would; otherwise a
// multiple selection holds only it, as after a plain arrow key. *changed (may be null) tells
// whether the selection changed. The caller moves the focus there (ListFocusRow) and scrolls the
// row into view. Owners read Home and End themselves: Dear ImGui's navigation measures the ends in
// float window coordinates, which lose whole rows at the end of a million rows.
int ListJumpToEnd(const ListJumpDesc& desc, bool end, bool* changed);

// Moves the keyboard focus to row `id` of `focusScope` at `rect` (screen pixels) in `window`,
// also when the row is not submitted this frame, as navigation would, and shows the focus ring.
void ListFocusRow(ImGuiWindow* window, ImGuiID id, ImGuiID focusScope, const ImRect& rect);

// Tab for a container that runs its rows through the row block inside a selection scope of its own
// (ItemGrid, DataTable; see "Tab" at the top of this file), as the view does it. Call one of the two
// every frame before BeginListSelection. While no row has the focus, ListTabProxy submits the tab
// stop of list `listId` at `rect` (screen pixels, the list's visible area) in the surrounding focus
// scope and returns true when Tab, Shift+Tab or an arrow key has just landed on it from outside: the
// container then moves the focus on to the row Tab lands on (ListFocusRow). While a row has the
// focus, ListClaimTabExit owns Tab for `owner` every frame and, on a press, puts the focus back on
// that tab stop (`outerScope` is the focus scope around the selection), so the tabbing of the next
// frame moves on to the item after (with Shift, before) the list. (since 0.1)
bool ListTabProxy(ImGuiID listId, const ImRect& rect);
void ListClaimTabExit(ImGuiWindow* window, ImGuiID owner, ImGuiID listId, ImGuiID outerScope, const ImRect& rect);

// The start of a Shift range across a sort or filter, as the view keeps it. Dear ImGui keeps the
// range start as an index; when the caller reorders its rows, Remap (before BeginListSelection)
// moves it back to the entry it was set on, found by its storage id in one scan of `rowCount`
// entries on the frame the order changed only, or drops it when the entry is gone. Record (after
// EndListSelection) remembers that entry. `focusScope` is the selection scope.
struct ListRangeAnchor
{
    ImGuiID id = 0;      // storage id of the range start at the end of the last frame
    bool known = false;

    void Remap(ListSelection* selection, ImGuiID focusScope, int rowCount);
    void Record(ListSelection* selection, ImGuiID focusScope, int rowCount);
};

// ---- Trees ----------------------------------------------------------------------------------------

struct ListTreeDesc
{
    ListSelection* selection = nullptr;   // keyed by node key; Facet applies the requests itself
    ControlSize size = ControlSize::Inherit;
    ImVec2 areaSize = ImVec2(0.0f, 0.0f);  // pixels, as ScrollAreaDesc::size
    bool fitHeight = false;               // the area grows with the nodes instead of scrolling
    ImVec4 fadeSurface = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    bool indentGuides = true;             // hairlines down from an open node along its children
    bool readOnly = false;
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;      // drawn by EndListTree when no node was submitted
    const char* errorText = nullptr;
};

struct ListTreeNodeOptions
{
    ImGuiID key = 0;           // selection key; 0 uses the node's id
    bool leaf = false;         // no children: no expander, never open
    bool defaultOpen = false;  // open state the first time the node appears
    bool loading = false;      // an open node shows a loading row under its children
    bool disabled = false;
};

struct ListTreeNodeState
{
    ListRowState row;
    ImGuiID key = 0;
    bool open = false;     // submit the children, then call ListTreePop
    bool toggled = false;  // opened or closed this frame (also ImGui::IsItemToggledOpen)
};

// A tree over the caller's nodes, submitted between BeginListTree and EndListTree in depth-first
// order with ListTreeNode; not virtualized. Selection: ImGui::BeginMultiSelect over the visible
// nodes with the node key as selection data; Facet applies the requests itself (SetItemSelected
// and Clear, never ApplyRequests): Ctrl+A selects the visible nodes in last frame's order, Shift
// ranges run between two visible nodes in this frame's order, Escape and a plain click clear the
// whole storage (also selected nodes under closed parents), and closing a parent leaves its
// selected descendants selected. Keys: Up and Down move (Dear ImGui navigation); Right opens a
// closed node or moves to the first child of an open one; Left closes an open node or moves to
// the parent; Enter or a double click toggles a node and activates a leaf; typing jumps as in
// the list view. Same return rule as BeginListView. Trees nest up to kListViewDepth deep.
[[nodiscard]] bool BeginListTree(ImGuiID id, const ListTreeDesc& desc);
// A node whose id is `label` hashed on the id stack (the label is also its typeahead text; the
// caller draws it inside row.content). The open state is kept under that id in the window's
// state storage, so ImGui::SetNextItemOpen works. When `open` is true for a node with children,
// the node's id is pushed on the id stack and its children follow; close them with ListTreePop.
// Nesting deeper than kListTreeDepth levels is reported and shown closed.
inline constexpr int kListTreeDepth = 32;
ListTreeNodeState ListTreeNode(const char* label, const ListTreeNodeOptions& options = {});
// Ends the children of the innermost open node: draws its loading row and indent guide.
void ListTreePop();

struct ListTreeResult
{
    bool changed = false;    // the selection was written this frame
    ImGuiID activated = 0;   // the key of the leaf activated this frame, 0 for none
};
ListTreeResult EndListTree();

// ---- Shared pieces --------------------------------------------------------------------------------

// The loading (spinner and Strings::loading), error (the error glyph in the destructive token and
// the text in label) or empty (labelSecondary) look of a data view, centered in `rect` (screen
// pixels) in the current window. Ready draws the empty look. Null texts use the Strings table.
void DrawListState(const ImRect& rect, DataState state, const char* emptyText, const char* errorText);

// Called by the scroll module's frame hooks (the list core has no subsystem entry): reports row
// ranges, selections, views and trees still open at the end of the frame and resets them.
// ListCoreNewFrame starts the Tab move out of a list that the list asked for in the last frame
// (see "Tab" at the top of this file).
void ListCoreNewFrame();
void ListCoreEndFrame();
void ListCoreShutdown();

} // namespace facet::internal
