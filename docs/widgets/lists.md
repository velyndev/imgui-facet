# Lists

Dropdowns, combo boxes, list boxes, selectable and reorderable lists, tree views, checkbox trees,
transfer lists and searchable checklists, declared in `facet/widgets/lists.h`. Every declaration
follows [the API conventions](../api-conventions.md); the header comments are the full reference.
See [the widget index](../widgets.md) for the other families.

Each entry ends with its status; every widget of the family is ready and works as its header describes.

Two kinds of multiple choice use two kinds of storage. A selection (highlighted rows that the next
command acts on) is a caller-owned `ImGuiSelectionBasicStorage` driven by
`ImGui::BeginMultiSelect`. Checked values (options that are on) are a `std::span<bool>` with one
entry per item. `std::vector<bool>` packs its bits and does not convert to a span; keep such flags
in a `std::array<bool, N>`, a `bool` array or a `std::unique_ptr<bool[]>`.

Items come as a span of `ChoiceItem`, as a span of plain labels (`const char* const`) where a label
is all an item needs, or as a count and a `ChoiceGetter` for generated and long lists. A getter is
called during the widget call only, possibly many times, and the strings it writes need to stay
valid only until its next call: widgets copy what they keep or ask again, so one static buffer is
enough for a getter that formats its labels.

## Widgets

### Dropdown

```cpp
bool Dropdown(const char* label, int* index, std::span<const ChoiceItem> items, const DropdownOptions& options = {});
bool Dropdown(const char* label, int* index, std::span<const char* const> items, const DropdownOptions& options = {});
bool Dropdown(const char* label, int* index, int itemCount, ChoiceGetter getter, void* user,
              const DropdownOptions& options = {});
```

A field that opens a list and writes the chosen index. The field shows the chosen item's icon and
label, or the placeholder (`Strings::select` by default) in labelSecondary, cut with an ellipsis and
shown in full as a tooltip on hover and keyboard focus; the visible label part sits in front of the
field. Default width: the available width; the three control sizes set the field height and the row
height of the list.

- **Opening:** a click, Space, Enter, Down or Alt+Down. The list opens below the field, at least as
  wide as it, and above it when there is no room below; it follows the field when the page scrolls.
  The chosen row is scrolled into view and is the active row.
- **In the open list:** the field keeps the keyboard focus. Up and Down move the active row past
  disabled items and group headers, Page Up and Down move by a page, Home and End go to the ends,
  typing jumps to the next item whose label starts with the typed text, and Enter, Space, Tab or a
  click chooses the active row and closes the list. The pointer moving over a row makes it the
  active row. Escape, a click outside the list or a click on the field closes it without writing.
- **Typeahead on the closed field:** typing writes the next item whose label starts with the
  typed text; the same letter again cycles through the items that start with it.
- **Searchable:** a filter field at the top of the list takes the focus and the typing. Items are
  fuzzy matched on their visible label and ordered best first (a grouped list keeps its order under
  its headers); the matched letters are highlighted. The arrows, Enter and Tab still operate the
  rows; Escape closes the list and the focus goes back to the field. The filter starts empty on
  every opening.
- **Rows:** a check mark slot that shows the chosen item, the icon, the label and the detail at the
  end in labelSecondary. `ChoiceItem::group` adds a header row before each run of equal group text.
  Disabled items are shown in labelTertiary and cannot be chosen.
- **Look:** the field is a text field's frame: fill[2] with the controlBorder hairline, and the
  accent hairline while the list is open, never a thicker stroke. The list floats on the level 2
  elevated surface with the separator hairline. The active row (under the pointer or moved by the
  keys) shows the fill[2] row fill, the chosen row the accentSubtle fill with its check mark. The
  focus ring shows only after the keys moved the active row, inside the row; the pointer never
  draws it.
- **Long lists:** the rows are virtualized. The getter overload is called for the rows shown, for
  the chosen item on the closed field, and for every item when the list opens or the filter
  changes (to build the group headers and the filter result); ten thousand items stay fluid.
- **States:** disabled (`ImGui::BeginDisabled`: the disabled colors, not a Tab stop, does not
  open), read-only (shown, focusable, no chevron, does not open), `mixed` (shows `Strings::mixed`
  until a choice is made), `error` (destructive outline and message, recorded for
  `ValidationSummary`) and `helpText`. `state` describes the items: Loading shows the loading row and
  Error shows `errorText` in the open list, and the closed field shows the error glyph; the field
  keeps its value. No items in Ready show `emptyText` (`Strings::noResults` while filtering).
- **Result:** true on the frame a choice is made, also when it is the item already chosen;
  `ImGui::IsItemEdited` is true on that frame.

State kept under the id: the typeahead text and the mixed flag; under the list's popup level: the
rows, the active row and the filter text.

Status: ready.

### MultiDropdown

```cpp
bool MultiDropdown(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                   const MultiDropdownOptions& options = {});
bool MultiDropdown(const char* label, std::span<bool> checked, std::span<const char* const> items,
                   const MultiDropdownOptions& options = {});
```

A field whose list has a checkbox per row and stays open while items are toggled. It shares the
field, the opening keys, the placement, the group headers, the filter and the states with Dropdown.

- **Field:** the checked labels, comma separated and cut with an ellipsis (`summary` List), or
  `Strings::selectedCount` with the number of checked items (`summary` Count); the placeholder
  while nothing is checked.
- **In the open list:** Space, Enter or a click toggles the active row and writes its entry at
  once; the list stays open. Tab, Escape, a click outside or a click on the field closes it. In a
  searchable list Space types into the filter, and Enter toggles.
- **Select-all row** (`selectAllRow`): the first row, with a checkbox that shows On when every shown
  enabled item is checked, Mixed when some are and Off when none are; activating it checks every
  shown enabled item, or clears them when all were checked. While filtering it acts on the matches.
- **Entries:** `checked` has one entry per item; a missing entry shows as unchecked and is never
  written; disabled items are never toggled.
- **States:** disabled, read-only (does not open), `error`, `helpText`, and the item states as
  Dropdown.
- **Result:** true on each frame an entry was written; `ImGui::IsItemEdited` is true then.

Status: ready.

### BeginCombo, ComboItem, EndCombo

```cpp
[[nodiscard]] bool BeginCombo(const char* label, const char* preview, const ComboOptions& options = {});
void EndCombo();
bool ComboItem(const char* label, bool selected, const ComboItemOptions& options = {});
class ComboScope;
```

The field of a combo whose rows the caller submits, for rows a `ChoiceItem` cannot describe. The
field is the one of Dropdown: the label in front, `options.icon` and `preview` (or `Strings::select`
in labelSecondary), the chevron, the message line, the width rule and the three sizes.

- **Opening:** a click, Space, Enter or Down opens the list below the field (above when there is no
  room); BeginCombo then returns true and the caller submits rows until EndCombo. While closed it
  returns false and EndCombo must not be called. A read-only or disabled combo does not open.
- **The open list** takes the keyboard focus: the row passed `selected` gets it and is scrolled
  into view, the arrows move between the rows (Dear ImGui navigation), Enter, Space or a click on a
  ComboItem returns true from that call and closes the list, and Escape, a click outside or a click
  on the field closes it. The focus then goes back to the field.
- **Rows:** ComboItem draws the row as the rows of Dropdown: a check mark slot showing `selected`,
  `icon`, the label (semibold while selected, cut with an ellipsis and a tooltip) and `detail` at the
  end. Rows wrapped in `ImGui::BeginDisabled` are shown in labelTertiary and refused. Other widgets
  may be submitted between the rows.
- **Height:** as tall as the rows submitted while the list was last open, at most `maxVisibleRows`
  rows (`list.popupRows` by default); more rows scroll. The first opening uses the maximum.
- **Calls out of place:** ComboItem outside BeginCombo / EndCombo is reported and returns false; an
  EndCombo without its BeginCombo is reported and does nothing; a BeginCombo left open is reported
  by `facet::EndFrame`.

State kept under the id: the row count of the last opening; the list's scroll position under its
popup level.

Status: ready.

### ListBox

```cpp
bool ListBox(const char* label, int* index, std::span<const ChoiceItem> items, const ListBoxOptions& options = {});
bool ListBox(const char* label, int* index, std::span<const char* const> items, const ListBoxOptions& options = {});
bool ListBox(const char* label, int* index, int itemCount, ChoiceGetter getter, void* user,
             const ListBoxOptions& options = {});
```

A framed, scrolling list with one chosen row, which sits on the sliding selection pill and has a
semibold label. The visible label part is drawn above the frame (a list is taller than a label line),
cut with an ellipsis when the width is short. The frame has no fill and the controlBorder hairline, so
the row fills read on any surface; it is `visibleRows` rows tall (`list.inlineRows` by default) at
the row height of `size`, also with fewer items. Default width: the available width. The first time
the list is shown, the chosen row is scrolled into view.

- **Rows:** the icon, the label (cut with an ellipsis, shown in full as a tooltip on hover and
  keyboard focus) and the detail at the end in labelSecondary. `ChoiceItem::group` adds a header row
  before each run of equal group text; headers and disabled items cannot be chosen.
- **Keys:** the list is one Tab stop (the focused row, else the chosen row, else the first). The
  arrows move the choice past headers and disabled items and write it at once, Page Up and Down move
  by a page, Home and End go to the first and last enabled item, and typing jumps to the next item
  whose label starts with the typed text. A click chooses a row.
- **Long lists:** only the visible rows are submitted. The span overloads follow the caller's items
  every frame; the getter overload reads every item only when its count changes (for the group
  headers), and otherwise only the rows shown.
- **States:** disabled (`ImGui::BeginDisabled`: rows not focusable, nothing written), read-only
  (the focus moves, the choice is never written), `error` (destructive frame and message, recorded
  for `ValidationSummary`), `helpText`. While `state` is Loading or Error, or with no items, the
  frame keeps its height and shows the loading row, `errorText` or `emptyText`.
- **Result:** true on the frame `*index` was written; `ImGui::IsItemEdited` is true then.

State kept under the id: the focused row, the typeahead text, the scroll position and the rows.

Status: ready.

### SelectableList

```cpp
bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, std::span<const ChoiceItem> items,
                    const SelectableListOptions& options = {});
bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, std::span<const char* const> items,
                    const SelectableListOptions& options = {});
bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, int itemCount, ChoiceGetter getter,
                    void* user, const SelectableListOptions& options = {});
int SelectableListActivatedRow();
```

A framed, scrolling list with multiple selection through `ImGui::BeginMultiSelect`, with the label,
frame, rows, width, long-list behaviour, states and message line of ListBox. Each item is one row
(no group headers), so the storage keys row `i` by `AdapterIndexToStorageId(i)`.

- **Pointer:** a click selects one row, Ctrl+click toggles a row, Shift+click selects the range from
  the last clicked row, and a drag that starts in empty space selects the rows it covers
  (`boxSelect`). A click in empty space clears the selection.
- **Keys:** one Tab stop; the arrows move the focus and select the row, Shift with the arrows,
  Home or End extends the range, Ctrl with the arrows moves the focus alone, Space toggles, Ctrl+A
  selects every enabled row, Escape clears. Selected rows show accentSubtle, with a semibold label,
  whether or not the list has the focus.
- **Disabled rows** are shown in labelTertiary, are skipped by the arrows and left out of ranges,
  select-all and box selection.
- **Activation:** a double click or Enter on a row; `SelectableListActivatedRow()` returns that row
  after the call in the same frame, -1 otherwise.
- **Read-only:** the selection shows and the focus moves, but nothing is written; a null `selection`
  shows the rows without selection.
- **Result:** true on the frame the selection changed.

Status: ready.

### ReorderableList

```cpp
struct ReorderMove { int from = -1; int to = -1; constexpr bool IsValid() const; };
template <class Range>
    requires std::ranges::random_access_range<Range> && std::ranges::sized_range<Range>
void ApplyReorderMove(Range&& items, const ReorderMove& move);
bool ReorderableList(const char* id, ReorderMove* move, std::span<const ChoiceItem> items,
                     const ReorderableListOptions& options = {});
bool ReorderableList(const char* id, ReorderMove* move, std::span<const char* const> items,
                     const ReorderableListOptions& options = {});
```

Rows that move to a new place; the move comes back as a result that the caller applies to its own
data. `from` is the row's index before the move and `to` its index after it, in both directions.
`ApplyReorderMove` takes a `std::vector`, a `std::array`, a C array or a `std::span` as it is.

- **Rows:** one per item at the list row height, the full width (the available width by default):
  the grip, the icon, the label (cut with an ellipsis and a tooltip) and the detail at the end.
  A row under the pointer shows the fill[2] row fill of every list; the focus ring shows after
  keyboard input only, inside the row.
- **Pointer:** a press on a row (on its grip alone with `handleOnly`) becomes a drag after
  `drag.startThreshold`. The ghost follows the pointer, the other rows spring aside to open the gap
  where the row would land, and that place shows a fill[2] slot. Releasing over the list reports the
  move; releasing outside it, Escape or a right click cancels and the rows spring back. A drag that
  rests at the edge of a scroll area around the list scrolls it.
- **Keyboard:** each row is a Tab stop. Alt+Up and Alt+Down move the focused row at once. Space
  picks the focused row up, the arrows move it between the places it can land, Space or Enter drops
  it and Escape puts it back. The focus stays on the moved row.
- **Ids:** rows are keyed by `ChoiceItem::id`, so a row keeps its animation and its focus when the
  caller moves its data; give items stable ids. Rows of the label overload are keyed by their index:
  their positions do not animate after a move, and the focus follows the moved row by its new index.
- **States:** read-only (`readOnly`, or a null `move`): no grips, nothing moves; disabled items and
  rows inside `ImGui::BeginDisabled` are shown in labelTertiary and do not move. While `state` is
  Loading or Error, or with no items, one row shows the loading row, `errorText` or `emptyText`.
- **Result:** true on the frame a row lands on a new place; `*move` is written only then.
- **Limits:** at most 512 rows are laid out (a longer list is reported); rows are not virtualized.

Status: ready.

### BeginTreeView, BeginTreeNode, TreeLeaf

```cpp
[[nodiscard]] bool BeginTreeView(const char* id, ImGuiSelectionBasicStorage* selection,
                                 const TreeViewOptions& options = {});
void EndTreeView();
bool TreeViewSelectionChanged();
[[nodiscard]] bool BeginTreeNode(const char* label, const TreeNodeOptions& options = {});
void EndTreeNode();
bool TreeLeaf(const char* label, const TreeNodeOptions& options = {});
class TreeViewScope;
class TreeNodeScope;
```

A hierarchy over the caller's nodes, submitted in depth-first order between BeginTreeView and
EndTreeView. Default width: the available width; with `height` 0 the view grows with its nodes,
with a height it scrolls.

- **Rows:** one per node at the list row height, the full width of the view. The content is
  indented by `list.indent` per level after the chevron of a node with children (chevron-right
  closed, chevron-down open); then the icon, the label (cut with an ellipsis and a tooltip) and the
  detail at the end in labelSecondary. `indentGuides` draws a hairline from an open node down along
  its children.
- **Selection** (a non-null storage): click, Ctrl+click and Shift+click through
  `ImGui::BeginMultiSelect`, keyed by `TreeNodeOptions::id` or the node's id; Ctrl+A selects the
  visible nodes, Escape and a click in empty space clear. Closing a parent leaves its selected
  children selected. `TreeViewSelectionChanged()` is true after an EndTreeView that wrote the storage.
- **Keys:** the view is one Tab stop, also with a selection (Tab lands on the focused node, else the
  first). Up and Down move, Right opens a closed node or goes to the first child of an open one,
  Left closes an open node or goes to the parent, Enter or a double click opens or closes a node and
  activates a leaf (TreeLeaf returns true), and typing jumps to the next node whose label starts with
  the typed text. A click on the chevron opens or closes without selecting.
- **Lazy children:** `ImGui::IsItemToggledOpen()` right after BeginTreeNode reports the toggle;
  `loading` shows the loading row under the open node until the children arrive.
- **Nodes inside `ImGui::BeginDisabled`** are shown in labelTertiary, are not focusable and are
  left out of ranges and select-all.
- **States:** while `state` is Loading or Error, BeginTreeView draws the loading row or `errorText`
  in the view (one row tall when the view fits its nodes) and returns false; a tree without nodes
  shows `emptyText`.
- **Pairs:** BeginTreeView returns false while the view is out of sight or not Ready; call
  EndTreeView only after true. BeginTreeNode returns true while the node is open; call EndTreeNode
  only after true. An End without its Begin is reported and does nothing; a Begin left open is
  reported by `facet::EndFrame`. The node row is the last item after BeginTreeNode and TreeLeaf, so
  stock drag and drop on it works.

State kept: the open state of each node under its id (`ImGui::SetNextItemOpen` sets it), the
focused node and the typeahead text under the view id, the scroll position of a view with a height.

Status: ready.

### CheckboxTree

```cpp
struct CheckTreeItem { const char* label; Icon icon; int depth; ImGuiID id; };
bool CheckboxTree(const char* id, std::span<CheckState> states, std::span<const CheckTreeItem> items,
                  const CheckboxTreeOptions& options = {});
```

A tree with a tri-state checkbox per node, over items in depth-first order (a node's children follow
it with depth + 1). It is built on the tree of TreeView, so rows, indent, chevrons, indent guides,
keys and typeahead are the same; it has no selection. Default width: the available width; `height`
0 fits the rows, a height scrolls.

- **Checkboxes:** Off is the neutral box with the controlBorder hairline; On and Mixed are the accent box
  with the check or minus glyph in onAccent, so Mixed differs by its shape; read-only boxes are
  labelSecondary.
- **Toggling:** a click on a row or Space toggles its node: Off and Mixed become On, On becomes Off.
  With `cascade` the new state goes to every descendant and each ancestor becomes On or Off when all
  its children are, Mixed otherwise. Without it only the node changes. A click on the chevron, Enter,
  Left and Right open and close nodes without toggling.
- **Entries:** `states` holds one entry per item; a missing entry shows as Off, counts as Off for its
  parent and is never written.
- **Open state:** kept per node under `CheckTreeItem::id` (else the index); `defaultOpen` is the
  open state of a parent the first time it appears.
- **States:** read-only (shown, focusable, nothing toggles), nodes inside `ImGui::BeginDisabled`;
  while `state` is Loading or Error, or with no items, the tree shows the loading row, `errorText`
  or `emptyText` and nothing is written.
- **Result:** true on the frame a state was written; `ImGui::IsItemEdited` is true then.

Status: ready.

### TransferList

```cpp
bool TransferList(const char* id, std::span<bool> inTarget, std::span<const ChoiceItem> items,
                  const TransferListOptions& options = {});
bool TransferList(const char* id, std::span<bool> inTarget, std::span<const char* const> items,
                  const TransferListOptions& options = {});
```

Two lists with move buttons between them: items whose `inTarget` entry is false are in the source
list, the others in the target list. Default width: the available width; the lists sit side by side
with the buttons between them, and stack (source, a row of buttons, target) in a Compact width.

- **Each list:** the title above it (`sourceTitle`, `targetTitle`), a filter field when
  `searchable` (fuzzy matched, the matched letters highlighted), and the frame of SelectableList
  with `visibleRows` rows and its multiple selection (click, Ctrl and Shift click, Shift with the
  arrows, Ctrl+A, Escape). There is no box selection: a drag that starts on a row carries the row.
  The selection and filter text of both lists are kept under the id.
- **Moving:** Add (`Strings::add`) moves the selected rows of the source list, Remove
  (`Strings::remove`) those of the target list; each button is disabled while its list has no
  selection. A double click or Enter on a row moves the selected rows of its list (the row alone
  when it is not selected), and rows dragged into the other list move there (with the selection
  when the dragged row is selected; the drag preview shows the count). Moved rows leave the
  selection. The buttons are `facet::Button` with arrow icons.
- **Keys:** Tab goes through the source filter, the source list, Add, Remove, the target filter and
  the target list.
- **Entries:** disabled items never move; missing entries count as false (the item is in the source
  list) and are never written.
- **States:** read-only (nothing moves, the buttons are disabled), disabled; while `state` is
  Loading or Error both lists show that state and nothing moves; a list without rows shows
  `emptyText`, or `Strings::noResults` while its filter matches nothing.
- **Result:** true on the frame entries of `inTarget` were written.

The rows of both lists are built from the items every frame (the overloads take spans), so a list of
a few thousand items stays fluid, but very long lists belong in a virtualized view.

Status: ready.

### SearchableChecklist

```cpp
bool SearchableChecklist(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                         const SearchableChecklistOptions& options = {});
bool SearchableChecklist(const char* label, std::span<bool> checked, std::span<const char* const> items,
                         const SearchableChecklistOptions& options = {});
bool SearchableChecklist(const char* label, std::span<bool> checked, int itemCount, ChoiceGetter getter,
                         void* user, const SearchableChecklistOptions& options = {});
```

An inline checklist for long lists. From top to bottom: the label line with
`Strings::selectedCount` at its end, a filter field (search icon, clear button, `placeholder` or
`Strings::search`), and the frame of ListBox with `visibleRows` rows. Default width: the available
width.

- **Rows:** a checkbox per item, the icon, the label (the letters the filter matches are
  highlighted) and the detail. One row per item; `ChoiceItem::group` is not shown here.
- **Filter:** fuzzy matched on the visible label, best first; without a match the frame shows
  `emptyText` or `Strings::noResults`. The filter text is kept under the id.
- **Select-all row** (`selectAllRow`, on by default): checks every shown enabled item, or clears
  them when all were checked; its checkbox shows On, Off or Mixed for the shown items.
- **checkedFirst:** checked items are listed before the others. The order is rebuilt while the
  pointer is off the list and the list does not have the keyboard, so a row does not move away
  under a click.
- **Keys:** the filter field and the list are two Tab stops; in the list the arrows, Page Up and
  Down, Home and End move, typing jumps to a matching label, and Space, Enter or a click toggles
  the focused row.
- **Long lists:** the rows are virtualized. The getter overload reads every item when the filter or
  the item count changes, and otherwise only the rows shown; an unfiltered span list follows its
  items every frame.
- **Entries:** missing entries of `checked` show as unchecked and are never written.
- **States:** read-only (the filter works, nothing toggles), disabled, `error`, `helpText`; while
  `state` is Loading or Error the frame shows the loading row or `errorText`.
- **Result:** true on the frame an entry was written.

Status: ready.

## Usage examples

A dropdown over a long generated list. The getter fills one entry at a time and is called only for
the rows that are shown or filtered:

```cpp
static int font = -1;
bool FontName(void* user, int index, facet::ChoiceItem* out)
{
    out->label = static_cast<const FontList*>(user)->Name(index);   // app data, read during the call
    return true;
}
facet::Dropdown("Font", &font, fonts.Count(), &FontName, &fonts, {.searchable = true});
```

A combo with rows that a `ChoiceItem` cannot describe:

```cpp
if (facet::BeginCombo("Profile", profiles[current].name, {.icon = facet::Icon("user")}))
{
    for (int i = 0; i < profileCount; ++i)
    {
        ImGui::PushID(i);
        if (facet::ComboItem(profiles[i].name, i == current, {.detail = profiles[i].detail}))
            current = i;
        ImGui::PopID();
    }
    facet::EndCombo();
}
```

A selectable list whose selection survives sorting: the storage keys entries by an id from the
data instead of the row index.

```cpp
static ImGuiSelectionBasicStorage selection;
selection.UserData = &rows;
selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int index) {
    return static_cast<const std::vector<Row>*>(self->UserData)->at(index).id;
};
if (facet::SelectableList("Files", &selection, items))
    UpdateDetails(selection);
if (const int row = facet::SelectableListActivatedRow(); row >= 0)
    Open(rows[row]);
```

A reorderable list over the app's own vector. Items carry stable ids, so each row keeps its
animation state when the data moves:

```cpp
std::vector<facet::ChoiceItem> items = BuildItems(presets);   // label and id from each preset
facet::ReorderMove move;
if (facet::ReorderableList("presets", &move, items))
{
    facet::ApplyReorderMove(presets, move);   // the app's std::vector<Preset>
    facet::ApplyReorderMove(items, move);     // keeps the shown rows in step until the next rebuild
}
```

A tree with lazily loaded children. `ImGui::IsItemToggledOpen` right after the node call says when to
start loading:

```cpp
static ImGuiSelectionBasicStorage selection;
if (facet::BeginTreeView("assets", &selection, {.state = rootState}))
{
    for (Folder& folder : folders)
    {
        const bool open = facet::BeginTreeNode(folder.name, {.icon = facet::Icon("folder"),
                                                             .id = folder.id,
                                                             .loading = folder.loading});
        if (ImGui::IsItemToggledOpen() && !folder.loaded)
            StartLoading(folder);
        if (open)
        {
            for (const File& file : folder.files)
                if (facet::TreeLeaf(file.name, {.icon = facet::Icon("file"), .id = file.id}))
                    Open(file);
            facet::EndTreeNode();
        }
    }
    facet::EndTreeView();
}
```

A checkbox tree keeps its states next to the items, one per item:

```cpp
static const facet::CheckTreeItem items[] = {
    {.label = "Group 1"}, {.label = "Option 1", .depth = 1}, {.label = "Option 2", .depth = 1},
};
static facet::CheckState states[3] = {};
if (facet::CheckboxTree("filters", states, items))
    ApplyFilters(states);
```

## Later

No widget of this family is in the later tier.
