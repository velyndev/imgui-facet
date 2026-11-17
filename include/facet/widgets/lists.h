#pragma once

// Lists: dropdowns and multi-select dropdowns, combo boxes, list boxes, selectable and
// reorderable lists, tree views, checkbox trees, transfer lists and searchable checklists.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families, and docs/widgets/lists.md has usage examples. The family is planned:
// its widgets are declared here and draw a placeholder until Phase 2 implements them
// (docs/widgets.md).
//
// Two kinds of multiple choice use two kinds of storage:
//   - A selection (highlighted rows that the next command acts on) lives in a caller-owned
//     ImGuiSelectionBasicStorage and is driven by ImGui::BeginMultiSelect, so Ctrl, Shift, box
//     select and Ctrl+A behave as in every other multi-select list. The storage keys an entry
//     by AdapterIndexToStorageId(index): the index itself unless the caller sets the adapter,
//     for example to an item id that survives reordering.
//   - Checked values (a set of options that are on) are a std::span<bool> with one entry per
//     item, written like any other edited value. std::vector<bool> packs its bits and does not
//     convert to a span; keep the flags in std::array<bool, N>, a bool array or
//     std::unique_ptr<bool[]> instead.
// Popup lists (Dropdown, MultiDropdown, BeginCombo) open on input only.
//
// Item sources: a span of ChoiceItem, a span of plain labels (const char* const) for items that
// need only a label, or a count plus a ChoiceGetter for generated and long lists. A getter is
// called during the widget call only, possibly many times. The strings it writes into the item
// need to stay valid only until the next getter call: a widget copies what it keeps (a filtered
// order keeps indices, not labels) or calls the getter again, so a getter may format its labels
// into one static buffer.

#include <algorithm>
#include <iterator>
#include <ranges>
#include <span>

#include <imgui.h>

#include "facet/widgets/common.h"

namespace facet
{

// --- Dropdowns -------------------------------------------------------------------------------

struct DropdownOptions
{
    const char* placeholder = nullptr; // shown while *index is -1; null uses Strings::select
    bool searchable = false;           // filter field at the top of the list, fuzzy matched
    int maxVisibleItems = 0;           // list height in rows; 0 for the token default
    const char* emptyText = nullptr;   // null uses Strings::empty, or Strings::noResults while filtering
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fills the available width
    bool readOnly = false;
    bool loading = false;              // the list shows the loading state; the field keeps its value
    bool mixed = false;                // the field shows Strings::mixed until a choice is made
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A field that opens a list of `items` below it, or above it when there is no room below.
// The field shows the chosen item's icon and label; ChoiceItem::group adds group headers, and
// disabled items are shown but cannot be chosen. Click, Enter, Space or Alt+Down opens; arrows
// move; typing jumps to the next label that starts with the typed text (filters instead when
// searchable); Enter or a click chooses and closes; Escape closes without writing.
// Writes *index when an item is chosen and returns true on that frame. *index -1 means none;
// an index outside the items is shown as none and written only by a choice. Items, labels and
// the getter are read during the call only. The label's visible part is drawn in front of the
// field (rule 6). Default width: the available width. While the list is open, its scroll
// position and filter text are kept under the id.
bool Dropdown(const char* label, int* index, std::span<const ChoiceItem> items,
              const DropdownOptions& options = {});
bool Dropdown(const char* label, int* index, std::span<const char* const> items,
              const DropdownOptions& options = {});
// `itemCount` items from `getter`, which is called for the visible rows and while filtering,
// so a long list costs only what is shown.
bool Dropdown(const char* label, int* index, int itemCount, ChoiceGetter getter, void* user,
              const DropdownOptions& options = {});

// How a multi-select field sums up the checked items.
enum class MultiDropdownSummary
{
    List,   // the checked labels, comma separated, truncated with an ellipsis
    Count,  // the number of checked items
};

struct MultiDropdownOptions
{
    const char* placeholder = nullptr; // shown while nothing is checked; null uses Strings::select
    MultiDropdownSummary summary = MultiDropdownSummary::List;  // what the closed field shows
    bool searchable = false;           // filter field at the top of the list, fuzzy matched
    bool selectAllRow = false;         // a first row that checks or clears every enabled item
    int maxVisibleItems = 0;           // list height in rows; 0 for the token default
    const char* emptyText = nullptr;   // null uses Strings::empty or Strings::noResults
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fills the available width
    bool readOnly = false;
    bool loading = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A field whose list has a check mark per item and stays open while items are toggled.
// `checked` holds one entry per item; extra entries are ignored, missing ones count as false
// and are never written. Space, Enter or a click toggles the row under the cursor; Escape or a
// click outside closes. Writes the toggled entry at once and returns true on that frame.
// Opening keys, width and kept state as Dropdown.
bool MultiDropdown(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                   const MultiDropdownOptions& options = {});
bool MultiDropdown(const char* label, std::span<bool> checked, std::span<const char* const> items,
                   const MultiDropdownOptions& options = {});

// --- Combo with caller-drawn rows --------------------------------------------------------------

struct ComboOptions
{
    Icon icon;                         // shown in the field before the preview text
    int maxVisibleItems = 0;           // list height in rows; 0 for the token default
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fills the available width
    bool readOnly = false;             // the field does not open
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// The field of a combo whose rows the caller submits, for rows a ChoiceItem cannot describe.
// `preview` is the field text (read during the call; null shows Strings::select). Opens with
// the keys of Dropdown. Returns true while the list is open: submit rows with ComboItem (or
// any widget), then call EndCombo. Returns false while closed; call EndCombo only after true.
// Default width: the available width. The open state and scroll position are kept under the id.
[[nodiscard]] bool BeginCombo(const char* label, const char* preview, const ComboOptions& options = {});
void EndCombo();

struct ComboItemOptions
{
    Icon icon;
    const char* detail = nullptr;      // secondary text at the end of the row; null for none
};

// A row of the open combo list. `selected` shows the check mark and is where the list scrolls
// to when it opens. Returns true when the row is activated (click, Enter); the list then
// closes. Wrap a row in ImGui::BeginDisabled to show it but refuse it. Only between BeginCombo
// and EndCombo.
bool ComboItem(const char* label, bool selected, const ComboItemOptions& options = {});

class [[nodiscard]] ComboScope
{
public:
    ComboScope(const char* label, const char* preview, const ComboOptions& options = {})
        : open_(BeginCombo(label, preview, options)) {}
    ~ComboScope() { if (open_) EndCombo(); }
    ComboScope(const ComboScope&) = delete;
    ComboScope& operator=(const ComboScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- List box and selectable list ----------------------------------------------------------------

struct ListBoxOptions
{
    int visibleRows = 0;               // height in rows; 0 for the token default
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;   // null uses Strings::empty
    const char* errorText = nullptr;   // null uses Strings::loadFailed
    ControlSize size = ControlSize::Inherit;   // row height
    float width = 0.0f;                // units; 0 fills the available width
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A framed, scrolling list with one chosen row. Rows show the item icon, label and detail;
// ChoiceItem::group adds group headers. The list is one Tab stop: arrows, Page Up and Down,
// Home and End move the choice, typing jumps to the next matching label. Writes *index as soon
// as the choice moves and returns true on that frame. *index -1 means none. Only the visible
// rows are submitted, so the getter overload scales to long lists. While `state` is Loading or
// Error, or with no items, the frame keeps its height and shows the loading, error or empty
// state instead of the rows and returns false. `helpText` sits under the list, and a non-null
// `error` outlines the frame and replaces it (rule 7). The label's visible part is drawn above
// the list, which is taller than a label line. Default width: the available width. The scroll
// position and the typeahead text are kept under the id.
bool ListBox(const char* label, int* index, std::span<const ChoiceItem> items, const ListBoxOptions& options = {});
bool ListBox(const char* label, int* index, std::span<const char* const> items,
             const ListBoxOptions& options = {});
bool ListBox(const char* label, int* index, int itemCount, ChoiceGetter getter, void* user,
             const ListBoxOptions& options = {});

struct SelectableListOptions
{
    bool boxSelect = true;             // a drag that starts in empty space selects the rows it covers
    int visibleRows = 0;               // height in rows; 0 for the token default
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;   // null uses Strings::empty
    const char* errorText = nullptr;   // null uses Strings::loadFailed
    ControlSize size = ControlSize::Inherit;   // row height
    float width = 0.0f;                // units; 0 fills the available width
    bool readOnly = false;             // the selection is shown but cannot change
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A scrolling list with multiple selection through ImGui::BeginMultiSelect: a click selects,
// Ctrl+click toggles, Shift+click and Shift+arrows extend, Ctrl+A selects all, Escape clears.
// Writes `selection` (caller-owned and kept across frames; null shows the rows without
// selection) and returns true on the frame it changed. The row activated by double click or
// Enter is reported by SelectableListActivatedRow. Label, width, kept state, the not-Ready and
// empty states, error and help text as ListBox.
bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, std::span<const ChoiceItem> items,
                    const SelectableListOptions& options = {});
bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, std::span<const char* const> items,
                    const SelectableListOptions& options = {});
bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, int itemCount, ChoiceGetter getter,
                    void* user, const SelectableListOptions& options = {});

// The row activated (double click, Enter) in the last SelectableList call of this frame, or -1.
int SelectableListActivatedRow();

// --- Reorderable list ----------------------------------------------------------------------------

// One move in a reorderable list of n rows: the row at index `from` (in the list before the
// move) ends up at index `to` (in the list after the move); the rows in between shift by one
// toward the gap it left. Both are in 0 to n - 1. Moving row 1 of five rows to index 3 turns
// {0, 1, 2, 3, 4} into {0, 2, 3, 1, 4}. The caller moves its own data, for example with
// ApplyReorderMove.
struct ReorderMove
{
    int from = -1;
    int to = -1;
    constexpr bool IsValid() const { return from >= 0 && to >= 0 && from != to; }
};

// Applies `move` to a random-access container of the caller's (std::vector, std::array, a C
// array, a std::span): items[move.from] goes to index move.to and the elements in between shift
// by one. Does nothing for an invalid move or for indices outside the container.
template <class Range>
    requires std::ranges::random_access_range<Range> && std::ranges::sized_range<Range>
void ApplyReorderMove(Range&& items, const ReorderMove& move)
{
    const auto count = static_cast<long long>(std::ranges::size(items));
    if (!move.IsValid() || move.from >= count || move.to >= count)
        return;
    const auto first = std::ranges::begin(items);
    const auto from = std::ranges::next(first, move.from);
    const auto to = std::ranges::next(first, move.to);
    if (move.from < move.to)
        std::rotate(from, std::ranges::next(from), std::ranges::next(to));
    else
        std::rotate(to, from, std::ranges::next(from));
}

struct ReorderableListOptions
{
    bool handleOnly = false;           // only the grip starts a drag; the rest of the row stays clickable
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;   // null uses Strings::empty
    const char* errorText = nullptr;   // null uses Strings::loadFailed
    ControlSize size = ControlSize::Inherit;   // row height
    float width = 0.0f;                // units; 0 fills the available width
    bool readOnly = false;             // no grips, no moves
};

// Rows that are dragged by their grip (or anywhere on the row) to a new place while the other
// rows move aside to show the gap. With the keyboard, Alt+Up and Alt+Down move the focused
// row. Writes *move and returns true on the frame a row lands on a new place; *move is left
// alone otherwise. A row keeps its animation state under ChoiceItem::id (the index when 0), so
// give items stable ids when the caller moves its data; rows of the label overload are keyed
// by index. While `state` is Loading or Error, or with no items, one row shows the loading,
// error or empty state instead and nothing can move. Default width: the available width.
bool ReorderableList(const char* id, ReorderMove* move, std::span<const ChoiceItem> items,
                     const ReorderableListOptions& options = {});
bool ReorderableList(const char* id, ReorderMove* move, std::span<const char* const> items,
                     const ReorderableListOptions& options = {});

// --- Tree view -----------------------------------------------------------------------------------

struct TreeViewOptions
{
    bool indentGuides = true;          // hairlines that join the children of a node
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;   // drawn by EndTreeView when no node was submitted; null uses Strings::empty
    const char* errorText = nullptr;   // null uses Strings::loadFailed
    float width = 0.0f;                // units; 0 fills the available width
    float height = 0.0f;               // units; 0 fits the nodes without own scrolling, > 0 scrolls
};

// A tree whose nodes are submitted between BeginTreeView and EndTreeView with BeginTreeNode and
// TreeLeaf. `selection` (caller-owned, null for no selection) holds the selected nodes keyed by
// TreeNodeOptions::id, or by the node's id on the id stack when that is 0; click, Ctrl+click
// and Shift+click select through ImGui::BeginMultiSelect over the visible nodes. Arrows move,
// Right opens or goes to the first child, Left closes or goes to the parent, typing jumps.
// Returns false while the state is not Ready (the state is drawn instead) or while the view is
// out of sight; call EndTreeView only after true. The view keeps its scroll position under the id.
[[nodiscard]] bool BeginTreeView(const char* id, ImGuiSelectionBasicStorage* selection,
                                 const TreeViewOptions& options = {});
void EndTreeView();

// Whether the last EndTreeView of this frame wrote its selection storage.
bool TreeViewSelectionChanged();

struct TreeNodeOptions
{
    Icon icon;
    const char* detail = nullptr;      // secondary text at the end of the row; null for none
    bool defaultOpen = false;          // open state the first time the node appears
    ImGuiID id = 0;                    // stable key in the selection storage; 0 uses the node's id
    bool loading = false;              // an open node shows a loading row under its children
};

// A node with children, inside a tree view. Returns true while it is open: submit the
// children, then call EndTreeNode. Returns false while closed; call EndTreeNode only after
// true. The open state is kept under the node's id; ImGui::SetNextItemOpen before the call sets
// it, and ImGui::IsItemToggledOpen after the call reports a toggle (the moment to start loading
// children for a lazy tree, with `loading` on until they arrive). The node row is the last
// item, so stock drag and drop on it works for moving nodes. The node's children get ids under
// the node's id.
[[nodiscard]] bool BeginTreeNode(const char* label, const TreeNodeOptions& options = {});
void EndTreeNode();

// A node without children, inside a tree view. Returns true when it is activated (double
// click, Enter).
bool TreeLeaf(const char* label, const TreeNodeOptions& options = {});

class [[nodiscard]] TreeViewScope
{
public:
    TreeViewScope(const char* id, ImGuiSelectionBasicStorage* selection, const TreeViewOptions& options = {})
        : open_(BeginTreeView(id, selection, options)) {}
    ~TreeViewScope() { if (open_) EndTreeView(); }
    TreeViewScope(const TreeViewScope&) = delete;
    TreeViewScope& operator=(const TreeViewScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

class [[nodiscard]] TreeNodeScope
{
public:
    explicit TreeNodeScope(const char* label, const TreeNodeOptions& options = {})
        : open_(BeginTreeNode(label, options)) {}
    ~TreeNodeScope() { if (open_) EndTreeNode(); }
    TreeNodeScope(const TreeNodeScope&) = delete;
    TreeNodeScope& operator=(const TreeNodeScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Checkbox tree -------------------------------------------------------------------------------

// One node of a checkbox tree, in depth-first order: a node's children follow it with depth + 1.
struct CheckTreeItem
{
    const char* label = nullptr;       // required; "##" hides the visible part as for widget labels
    Icon icon;
    int depth = 0;                     // 0 for top-level nodes
    ImGuiID id = 0;                    // key of the node's open state; 0 uses the index
};

struct CheckboxTreeOptions
{
    bool cascade = true;               // a parent sets all its children; children make the parent On, Off or Mixed
    bool defaultOpen = true;           // open state of parent nodes the first time they appear
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;   // null uses Strings::empty
    const char* errorText = nullptr;   // null uses Strings::loadFailed
    ControlSize size = ControlSize::Inherit;   // row height
    float width = 0.0f;                // units; 0 fills the available width
    float height = 0.0f;               // units; 0 fits the rows, > 0 scrolls
    bool readOnly = false;
};

// A tree with a tri-state checkbox per node. `states` holds one CheckState per item; missing
// entries count as Off and are never written. Space or a click toggles a node: Off and Mixed
// become On, On becomes Off. With cascade, the new state goes down to every descendant and
// every ancestor becomes On, Off or Mixed from its children. Returns true on the frame any
// state was written. Arrows move and open or close as in a tree view. While `state` is Loading
// or Error, or with no items, the tree area shows the loading, error or empty state instead and
// no state is written. The open states are kept under the id.
bool CheckboxTree(const char* id, std::span<CheckState> states, std::span<const CheckTreeItem> items,
                  const CheckboxTreeOptions& options = {});

// --- Transfer list and searchable checklist ------------------------------------------------------

struct TransferListOptions
{
    const char* sourceTitle = nullptr; // heading of the source list; null for none
    const char* targetTitle = nullptr; // heading of the target list; null for none
    bool searchable = true;            // a filter field above each list
    int visibleRows = 0;               // height of each list in rows; 0 for the token default
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;   // shown by each list without rows; null uses Strings::empty
    const char* errorText = nullptr;   // null uses Strings::loadFailed
    float width = 0.0f;                // units; 0 fills; in a Compact width the lists stack
    bool readOnly = false;
};

// Two lists with move buttons between them: items whose `inTarget` entry is false are in the
// source list, the others in the target list. Selected rows move with the buttons, by double
// click or Enter, or by dragging them across. Writes the moved entries of `inTarget` and
// returns true on that frame. Missing entries count as false and are never written. While
// `state` is Loading or Error, both lists show that state and nothing moves; a list without rows
// (no items, every item on the other side, or a filter that matches none) shows the empty
// state. The selection and filter text of both lists are kept under the id.
bool TransferList(const char* id, std::span<bool> inTarget, std::span<const ChoiceItem> items,
                  const TransferListOptions& options = {});
bool TransferList(const char* id, std::span<bool> inTarget, std::span<const char* const> items,
                  const TransferListOptions& options = {});

struct SearchableChecklistOptions
{
    const char* placeholder = nullptr; // placeholder of the filter field; null uses Strings::search
    bool selectAllRow = true;          // a row that checks or clears every visible enabled item
    bool checkedFirst = false;         // checked items are listed before the others
    int visibleRows = 0;               // height in rows; 0 for the token default
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;   // null uses Strings::empty or Strings::noResults
    const char* errorText = nullptr;   // null uses Strings::loadFailed
    float width = 0.0f;                // units; 0 fills the available width
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// An inline checklist for long lists: a filter field (fuzzy matched), a select-all row for the
// visible items, the number of checked items and a scrolling list. Writes the toggled entries
// of `checked` and returns true on that frame; missing entries count as false and are never
// written. Label above the list, as ListBox. The filter text is kept under the id.
bool SearchableChecklist(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                         const SearchableChecklistOptions& options = {});
bool SearchableChecklist(const char* label, std::span<bool> checked, std::span<const char* const> items,
                         const SearchableChecklistOptions& options = {});
bool SearchableChecklist(const char* label, std::span<bool> checked, int itemCount, ChoiceGetter getter,
                         void* user, const SearchableChecklistOptions& options = {});

} // namespace facet
