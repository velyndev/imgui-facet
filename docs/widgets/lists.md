# Lists

Dropdowns, combo boxes, list boxes, selectable and reorderable lists, tree views, checkbox trees,
transfer lists and searchable checklists, declared in `facet/widgets/lists.h`. Every declaration
follows [the API conventions](../api-conventions.md); the header comments are the full reference.
See [the widget index](../widgets.md) for the other families.

A widget whose status is planned is declared and callable: it draws a dashed placeholder with its
name, returns the neutral result (false, -1) and never writes a value. A widget whose status is ready works
as its header describes.

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

Status: planned

```cpp
bool Dropdown(const char* label, int* index, std::span<const ChoiceItem> items, const DropdownOptions& options = {});
bool Dropdown(const char* label, int* index, std::span<const char* const> items, const DropdownOptions& options = {});
bool Dropdown(const char* label, int* index, int itemCount, ChoiceGetter getter, void* user,
              const DropdownOptions& options = {});
```

A field that opens a list with icons, group headers, typeahead and an optional fuzzy filter, and
writes the chosen index. Items fetched from a slow source set `state`: the open list shows the
loading or error row, and the closed field marks an error, while the field keeps its value.

### MultiDropdown

Status: planned

```cpp
bool MultiDropdown(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                   const MultiDropdownOptions& options = {});
bool MultiDropdown(const char* label, std::span<bool> checked, std::span<const char* const> items,
                   const MultiDropdownOptions& options = {});
```

A field whose list stays open while items are checked, summed up in the field as a list or a count.

### BeginCombo, ComboItem, EndCombo

Status: planned

```cpp
[[nodiscard]] bool BeginCombo(const char* label, const char* preview, const ComboOptions& options = {});
void EndCombo();
bool ComboItem(const char* label, bool selected, const ComboItemOptions& options = {});
class ComboScope;
```

The field of a combo whose rows the caller submits, for rows a `ChoiceItem` cannot describe.

### ListBox

Status: planned

```cpp
bool ListBox(const char* label, int* index, std::span<const ChoiceItem> items, const ListBoxOptions& options = {});
bool ListBox(const char* label, int* index, std::span<const char* const> items, const ListBoxOptions& options = {});
bool ListBox(const char* label, int* index, int itemCount, ChoiceGetter getter, void* user,
             const ListBoxOptions& options = {});
```

A framed, scrolling list with one chosen row, typeahead and empty, loading and error states.

### SelectableList

Status: planned

```cpp
bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, std::span<const ChoiceItem> items,
                    const SelectableListOptions& options = {});
bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, std::span<const char* const> items,
                    const SelectableListOptions& options = {});
bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, int itemCount, ChoiceGetter getter,
                    void* user, const SelectableListOptions& options = {});
int SelectableListActivatedRow();
```

A scrolling list with Ctrl, Shift, box and Ctrl+A selection through `ImGui::BeginMultiSelect`.

### ReorderableList

Status: planned

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

Rows dragged by a grip (or moved with Alt+Up and Alt+Down) to a new place; the move comes back as a
result that the caller applies to its own data. `from` is the row's index before the move and `to`
its index after it. `ApplyReorderMove` is implemented and tested; it takes a `std::vector`, a
`std::array`, a C array or a `std::span` as it is.

### BeginTreeView, BeginTreeNode, TreeLeaf

Status: planned

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

A hierarchy with indent guides, icons, multiple selection, lazily loaded children and nodes that
stock drag and drop can move.

### CheckboxTree

Status: planned

```cpp
struct CheckTreeItem { const char* label; Icon icon; int depth; ImGuiID id; };
bool CheckboxTree(const char* id, std::span<CheckState> states, std::span<const CheckTreeItem> items,
                  const CheckboxTreeOptions& options = {});
```

A tree with a tri-state checkbox per node; a parent sets its children and shows Mixed when they differ.

### TransferList

Status: planned

```cpp
bool TransferList(const char* id, std::span<bool> inTarget, std::span<const ChoiceItem> items,
                  const TransferListOptions& options = {});
bool TransferList(const char* id, std::span<bool> inTarget, std::span<const char* const> items,
                  const TransferListOptions& options = {});
```

Two filterable lists with move buttons and drag between them; they stack in a Compact width.

### SearchableChecklist

Status: planned

```cpp
bool SearchableChecklist(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                         const SearchableChecklistOptions& options = {});
bool SearchableChecklist(const char* label, std::span<bool> checked, std::span<const char* const> items,
                         const SearchableChecklistOptions& options = {});
bool SearchableChecklist(const char* label, std::span<bool> checked, int itemCount, ChoiceGetter getter,
                         void* user, const SearchableChecklistOptions& options = {});
```

An inline checklist for long lists with a filter field, a select-all row and a count of checked items.

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
