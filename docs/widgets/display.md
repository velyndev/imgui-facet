# Display

Widgets that show text, states and data: styled and truncated text, section headers, banners
and alerts, empty states and data state views, progress, spinners and skeletons, badges and
status marks, key caps, code and log views, key-value lists, stat tiles and data tables.

Header: `facet/widgets/display.h`. Every declaration follows [the API conventions](../api-conventions.md),
and the header comments are the full reference (parameters, units, keys, state kept per id).
The demo gallery shows every widget on the page Widgets > Display.

**Status: planned.** Each widget below is declared, compiles and can be called; until Phase 2
implements it, a call draws a placeholder of the widget's default size (a hairline box that
names the widget) and returns the neutral result (false, `None`, nothing written).

## Widgets

| Widget | Declaration | What it does | Status |
|---|---|---|---|
| StyledText | `void StyledText(const char* text, const StyledTextOptions& options = {})` | Text in a theme text style and a semantic tone, with wrapping and alignment | planned |
| TruncatedText | `void TruncatedText(const char* text, const TruncatedTextOptions& options = {})` | Text cut with an ellipsis at the end, middle or start; full text as tooltip when cut | planned |
| SectionHeader | `bool SectionHeader(const char* label, const SectionHeaderOptions& options = {})` | Section heading with subtitle, trailing action and divider; true when the action is activated | planned |
| Banner | `BannerAction Banner(const char* label, bool* open, const char* message, const BannerOptions& options = {})` | Full-width strip for view-wide conditions, with up to two actions and a close button | planned |
| InlineAlert | `bool InlineAlert(const char* label, bool* open, const char* message, const InlineAlertOptions& options = {})` | Tinted callout inside the content with one optional action | planned |
| EmptyState | `bool EmptyState(const char* label, const EmptyStateOptions& options = {})` | Centered placeholder for a view without content, with one action | planned |
| DataStateView | `[[nodiscard]] bool BeginDataStateView(const char* id, DataState state, int count, const DataStateViewOptions& options = {})`, `void EndDataStateView()`, `bool DataStateViewRetryPressed()`, `DataStateViewScope` | Switches a data view between skeleton rows, error with retry, empty state and content | planned |
| ProgressBar | `void ProgressBar(const char* label, float fraction, const ProgressBarOptions& options = {})` | Linear progress with an optional buffer fill; `kIndeterminate` sweeps | planned |
| ProgressRing | `void ProgressRing(const char* label, float fraction, const ProgressRingOptions& options = {})` | Circular progress arc with center text | planned |
| Spinner | `void Spinner(const char* label, const SpinnerOptions& options = {})` | Busy indicator on the shared spinner clock | planned |
| Skeleton | `void Skeleton(const SkeletonOptions& options = {})` | Loading placeholder lines, blocks or circles with the shared shimmer | planned |
| Badge | `void Badge(const char* text, const BadgeOptions& options = {})` | Small toned label for a real status or category | planned |
| CountBadge | `void CountBadge(int count, const CountBadgeOptions& options = {})` | Counter pill with a "99+" cap | planned |
| StatusDot | `void StatusDot(const char* label, const StatusDotOptions& options = {})` | Status dot with an optional label and a pulsing live ring | planned |
| StatusPill | `void StatusPill(const char* label, const StatusPillOptions& options = {})` | Toned pill for the state of a connection, job or device; tone changes crossfade | planned |
| KeyCap | `void KeyCap(ImGuiKeyChord chord, const KeyCapOptions& options = {})` | A chord drawn as key caps, optionally live | planned |
| ShortcutHint | `void ShortcutHint(const char* text, ImGuiKeyChord chord, const ShortcutHintOptions& options = {})`, overload with `ShortcutId` | Muted text with key caps at the end of the row | planned |
| CharacterCounter | `void CharacterCounter(int count, int max, const CharacterCounterOptions& options = {})` | Text length against a limit, warning near it and destructive over it | planned |
| InlineCode | `bool InlineCode(const char* label, const InlineCodeOptions& options = {})` | Inline code in a tinted box, optionally copied on activation | planned |
| CodeView | `bool CodeView(const char* id, std::string_view code, const CodeViewOptions& options = {})` | Read-only code block with line numbers, light syntax tint and copy | planned |
| LogView | `void LogView(const char* id, std::span<const LogEntry> entries, const LogViewOptions& options = {})`, overload with count and `LogGetter` | Virtualized log with level colors, time column and follow mode | planned |
| KeyValueList | `void BeginKeyValueList(const char* id, const KeyValueListOptions& options = {})`, `bool KeyValue(const char* label, const char* value, const KeyValueOptions& options = {})`, `void EndKeyValueList()`, `KeyValueListScope` | Read-only labelled values aligned with settings rows | planned |
| StatTile | `void StatTile(const char* label, double value, const StatTileOptions& options = {})` | A value with caption, delta chip and trend line | planned |
| DataTable | `[[nodiscard]] bool BeginDataTable(const char* id, std::span<const DataTableColumn> columns, int rowCount, ImGuiSelectionBasicStorage* selection, const DataTableOptions& options = {})`, `bool DataTableRow(int index)`, `void EndDataTable()`, `DataTableScope` | Sortable, resizable table with a sticky header, frozen columns, virtualized rows and selection | planned |
| DataTable cell edit | `[[nodiscard]] bool BeginDataTableCellEdit()`, `void EndDataTableCellEdit()`, `DataTableCellEditScope` | Edits one cell in place with any editor widget | planned |

## Usage

### Data state view

The caller owns the state and the data. The view draws every state but Ready-with-content
itself, so the content block needs no branches of its own.

```cpp
if (facet::BeginDataStateView("files", state, static_cast<int>(files.size()), {.retryLabel = "Try again"}))
{
    for (const File& file : files)
        facet::StyledText(file.name.c_str());
    facet::EndDataStateView();
}
if (facet::DataStateViewRetryPressed())
    StartLoading();                        // the application's own function
```

### Banner

A dismissible banner keeps its own collapse animation, so call it every frame, also after it
was dismissed.

```cpp
static bool offlineBanner = true;
switch (facet::Banner("Offline##net", &offlineBanner, "Changes are kept until the connection returns.",
                      {.tone = facet::Tone::Warning, .primaryAction = "Retry"}))
{
case facet::BannerAction::Primary: Reconnect(); break;
default: break;
}
```

### Data table with sorting, selection and cell editing

The table is a Dear ImGui table: sort specs, column flags and `TableNextColumn` are the stock
calls. Rows come from `NextVisibleRows`, so only visible rows are submitted.

The selection storage keys rows through `AdapterIndexToStorageId`, which by default returns the
row index. A sorted table shows different items at the same index after each sort, so the
example maps the row index to a stable item id; then the selection follows the items.

```cpp
static ImGuiSelectionBasicStorage selection;
selection.UserData = &order;               // order[row] is the item shown in that row
selection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* self, int row) {
    const auto& shown = *static_cast<const std::vector<int>*>(self->UserData);
    return static_cast<ImGuiID>(itemIds[shown[row]]);   // the application's stable item ids
};
static const facet::DataTableColumn columns[] = {
    {"Name", 0.0f, facet::ColumnAlign::Start, true},           // editable
    {"Size", 80.0f, facet::ColumnAlign::End},
};
const int rowCount = static_cast<int>(order.size());
if (facet::BeginDataTable("files", columns, rowCount, &selection, {.sortable = true, .frozenColumns = 1}))
{
    if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty)
    {
        SortOrder(specs, &order);          // the application reorders `order`, not its items
        specs->SpecsDirty = false;
    }
    facet::RowRange rows;
    while (facet::NextVisibleRows(&rows))
        for (int i = rows.first; i < rows.last; ++i)
        {
            const int item = order[i];
            if (facet::DataTableRow(i))
                OpenItem(item);
            ImGui::TableNextColumn();
            if (facet::BeginDataTableCellEdit())
            {
                facet::TextField("##edit", &names[item]);
                if (facet::IsItemEditCommitted())
                    SaveName(item);
                facet::EndDataTableCellEdit();
            }
            else
                facet::StyledText(names[item].c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d KB", sizes[item]);
        }
    facet::EndDataTable();
}
```

### Log view over a filtered log

The log stays the caller's; filtering is a getter over the indices that pass the filter.

```cpp
struct Filtered { const std::vector<facet::LogEntry>* log; const std::vector<int>* shown; };
Filtered view{&log, &shownIndices};
facet::LogView("log", static_cast<int>(shownIndices.size()),
               [](void* user, int index, facet::LogEntry* out) {
                   const auto* f = static_cast<const Filtered*>(user);
                   *out = (*f->log)[(*f->shown)[index]];
                   return true;
               },
               &view, {.height = 240.0f});
```

### Key-value list

```cpp
facet::BeginKeyValueList("details", {.copyable = true});
facet::KeyValue("Path", path.c_str());
facet::KeyValue("Modified", modifiedText);
facet::EndKeyValueList();
```

## Planned, not declared yet

These belong to the family but are not part of v1; they get declarations in a later release.

- **EventTimeline**: a vertical list of events with a connecting rail, status nodes, timestamps
  and expandable details.
- **DataTable additions**: rows that expand into detail views, tree rows, filter popovers per
  column and row reordering by drag.
- **SectionHeader pinning**: a header that stays at the top of its scroll area while its section
  is visible.
- **Badge attached to another item**: a counter drawn over the corner of the previous item.
