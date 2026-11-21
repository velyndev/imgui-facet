# Display

Widgets that show text, states and data: styled and truncated text, section headers, banners
and alerts, empty states and data state views, progress, spinners and skeletons, badges and
status marks, key caps, code and log views, key-value lists, stat tiles and data tables.

Header: `facet/widgets/display.h`. Every declaration follows [the API conventions](../api-conventions.md),
and the header comments are the full reference (parameters, units, keys, state kept per id).
The demo gallery shows every widget on the page Widgets > Display.

Every widget below is declared, compiles and can be called, and each entry ends with its status. A
**planned** widget draws a placeholder of its default size (a dashed box that names the widget)
and returns the neutral result (false, `None`, nothing written); a **ready** one works as its
header describes.

## Widgets

### StyledText

```cpp
void StyledText(const char* text, const StyledTextOptions& options = {});
```

Text in a theme text style (size and weight from `T().type`) and a tone: Neutral draws `label`
(`labelSecondary` with `secondary`), Accent `accentText`, the status tones their status color, so the
text reaches 4.5:1 on the content surfaces. One line per line break by default, at its own width (a
line longer than a set width runs past it); with `wrap` it wraps at the item width, which is then the
available width unless `width` says otherwise, and `align` places each line inside that width. Text
sits on the line's text offset like `ImGui::Text`, so it lines up after a control on the same line.
Not interactive; inside `ImGui::BeginDisabled` it draws in `labelTertiary`. Long texts are measured
and drawn in batches of lines without allocating. Status: ready.

### TruncatedText

```cpp
void TruncatedText(const char* text, const TruncatedTextOptions& options = {});
```

Text cut with an ellipsis so it fits its width (the available width by default): at the end, in the
middle (keeps the end, for paths and file names) or at the start. With `maxLines` above 1 (End mode
only) the text wraps and the last line ends in the ellipsis. Cuts fall on character boundaries of
UTF-8 text, and widths of a few pixels draw what fits (or nothing) without trouble. While the text is
cut, resting the pointer on it shows the whole text as a tooltip; a text that fits shows none. With
`focusable` it is a Tab stop (never activated) with a focus ring, and keyboard focus shows the
tooltip too. A line break ends a one-line text as if the rest did not fit. Status: ready.

### SectionHeader

```cpp
bool SectionHeader(const char* label, const SectionHeaderOptions& options = {});
```

A Title3 heading with an optional link-style action at the end of its line (`accentText`, Body,
underlined while hovered or pressed so the state does not rest on color), a wrapped Footnote
description in `labelSecondary` and an optional hairline divider. The action takes at most half the
width; the heading gives way and is cut with an ellipsis, with the whole heading on hover and with the
action's keyboard focus. The action is the only Tab stop and activates by click, Enter or Space;
without an action the header is not a Tab stop. Width: the available width. Status: ready.

### Banner

```cpp
BannerAction Banner(const char* label, bool* open, const char* message, const BannerOptions& options = {});
```

Full-width strip for view-wide conditions, with up to two actions and a close button. Status: planned.

### InlineAlert

```cpp
InlineAlertAction InlineAlert(const char* label, bool* open, const char* message, const InlineAlertOptions& options = {});
```

Tinted callout inside the content with one optional action. Status: planned.

### EmptyState

```cpp
bool EmptyState(const char* label, const EmptyStateOptions& options = {});
```

Centered placeholder for a view without content, with one action. Status: planned.

### DataStateView

```cpp
[[nodiscard]] bool BeginDataStateView(const char* id, DataState state, int count, const DataStateViewOptions& options = {});
void EndDataStateView();
bool DataStateViewRetryPressed();
class DataStateViewScope;
```

Switches a data view between skeleton rows, error with retry, empty state and content. Status: planned.

### ProgressBar

```cpp
void ProgressBar(const char* label, float fraction, const ProgressBarOptions& options = {});
```

Linear progress: a track with the value's fill, an optional lighter buffer fill ahead of it, and the
label line above it with the percentage at its end (`showPercent`). Not interactive and not a Tab
stop. `tone` colors the fill (Accent by default, through the 3:1 focusRing color); `size` sets the
track thickness (4, 6 or 8 units). Without a label line the track is centered in one control height,
so it lines up in a settings row. A new value eases in over `motion.durationStandard`;
`kIndeterminate` sweeps a segment on the shared loading clock, which becomes a slow fade in place
with reduced motion. Values outside 0 to 1 are drawn clamped. Status: ready.

### ProgressRing

```cpp
void ProgressRing(const char* label, float fraction, const ProgressRingOptions& options = {});
```

A circular progress arc with round caps, from the top clockwise, over a `fill[0]` track, with the
visible label part to its right. The diameter is the control height of `size` or `diameter`; the
stroke is an eighth of it. The tone colors the arc as in ProgressBar (3:1 on the content surfaces).
`centerText`, or the percentage with `showPercent`, sits inside the ring in Footnote or Caption,
whichever fits; a ring too small for it shows it after the label instead. A new value eases in over
`motion.durationStandard`; `kIndeterminate` turns a quarter arc on the shared loading clock, which
stands at the top and fades in place with reduced motion. The ring sits so its label meets the text
of the line around it. Not interactive; inside `ImGui::BeginDisabled` it draws in the disabled
colors. Status: ready.

### Spinner

```cpp
void Spinner(const char* label, const SpinnerOptions& options = {});
```

Busy indicator on the shared spinner clock. Status: planned.

### Skeleton

```cpp
void Skeleton(const SkeletonOptions& options = {});
```

Loading placeholder lines, blocks or circles with the shared shimmer. Status: planned.

### Badge

```cpp
void Badge(const char* text, const BadgeOptions& options = {});
```

A small label in a box with the element radius, for a real status or category: by default the
tone's wash with the text in `label`, with `solid` the tone's fill with its readable text color
(`onAccent`, `onSuccess`, `onWarning`, `onDestructive`). An optional leading icon takes the tone's
glyph color. The text is Footnote, centered in the badge, and the badge sits on the baseline of the
Body text around it, so it reads as part of a sentence at every scale (see "Marks on a line of text"
below). Not interactive. Status: ready.

### CountBadge

```cpp
void CountBadge(int count, const CountBadgeOptions& options = {});
```

A counter in a solid pill of the tone (Accent by default): a circle for one digit, wider for more,
"99+" above `maxCount`. Negative counts draw as 0; a count of 0 submits an empty item unless
`showZero`. It sits on the text baseline like a badge. Not interactive. Status: ready.

### StatusDot

```cpp
void StatusDot(const char* label, const StatusDotOptions& options = {});
```

A dot of `metrics.statusDotSize` in the tone's glyph color (`labelSecondary` for Neutral), centered on
the capitals of the line, with the visible label part after it. `pulse` adds a ring that grows out of
the dot and fades once per `motion.pulsePeriod`; with reduced motion the ring is left out and the dot
stands still. A dot alone (`"##Online"`) shows the text after `"##"` as its tooltip on hover. Not
interactive. Status: ready.

### StatusPill

```cpp
void StatusPill(const char* label, const StatusPillOptions& options = {});
```

A pill in the tone's wash with the visible label part in `label` and an optional leading icon, or a
pulsing live dot, in the tone's glyph color, in three sizes (Caption, Footnote or Body text). A tone
change crossfades the wash and the glyph over `motion.durationFast` under the id (at once with
reduced motion). It sits on the text baseline like a badge. Not interactive. Status: ready.

### KeyCap

```cpp
void KeyCap(ImGuiKeyChord chord, const KeyCapOptions& options = {});
```

A chord drawn as key caps, optionally live. Status: planned.

### ShortcutHint

```cpp
void ShortcutHint(const char* text, ImGuiKeyChord chord, const ShortcutHintOptions& options = {});
void ShortcutHint(const char* text, ShortcutId shortcut, const ShortcutHintOptions& options = {});
```

Muted text with key caps at the end of the row. Status: planned.

### CharacterCounter

```cpp
void CharacterCounter(int count, int max, const CharacterCounterOptions& options = {});
```

Text length against a limit, warning near it and destructive over it. Status: planned.

### InlineCode

```cpp
bool InlineCode(const char* label, const InlineCodeOptions& options = {});
```

Inline code in a tinted box, optionally copied on activation. Status: planned.

### CodeView

```cpp
bool CodeView(const char* id, std::string_view code, const CodeViewOptions& options = {});
```

Read-only code block with line numbers, light syntax tint and copy. Status: planned.

### LogView

```cpp
void LogView(const char* id, std::span<const LogEntry> entries, const LogViewOptions& options = {});
void LogView(const char* id, int entryCount, LogGetter getter, void* user, const LogViewOptions& options = {});
```

Virtualized log with level colors, time column and follow mode. Status: planned.

### KeyValueList

```cpp
void BeginKeyValueList(const char* id, const KeyValueListOptions& options = {});
bool KeyValue(const char* label, const char* value, const KeyValueOptions& options = {});
void EndKeyValueList();
class KeyValueListScope;
```

Read-only labelled values aligned with settings rows. Status: planned.

### StatTile

```cpp
void StatTile(const char* label, double value, const StatTileOptions& options = {});
```

A value with caption, delta chip and trend line. Status: planned.

### DataTable

```cpp
[[nodiscard]] bool BeginDataTable(const char* id, std::span<const DataTableColumn> columns, int rowCount, ImGuiSelectionBasicStorage* selection, const DataTableOptions& options = {});
bool DataTableRow(int index);
void EndDataTable();
class DataTableScope;
```

Sortable, resizable table with a sticky header, frozen columns, virtualized rows and selection. Status: planned.

### DataTable cell edit

```cpp
[[nodiscard]] bool BeginDataTableCellEdit();
void EndDataTableCellEdit();
class DataTableCellEditScope;
```

Edits one cell in place with any editor widget. Status: planned.

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
    {"Name", 0.0f, facet::Align::Start, true},           // editable
    {"Size", 80.0f, facet::Align::End},
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

### Tones

Every tone is drawn with a second cue: an icon, a word or a shape. Text in a tone is readable (4.5:1)
on the content surfaces (`bg`, `bgSecondary`, `bgTertiary`); on a tone's wash the text is `label`,
and the tone shows in its icon or dot (3:1). A test (`tests/test_display.cpp`) computes these pairs
from the theme tokens in both themes.

## Planned, not declared yet

These belong to the family but are not part of v1; they get declarations in a later release.

- **EventTimeline**: a vertical list of events with a connecting rail, status nodes, timestamps
  and expandable details.
- **DataTable additions**: rows that expand into detail views, tree rows, filter popovers per
  column and row reordering by drag.
- **SectionHeader pinning**: a header that stays at the top of its scroll area while its section
  is visible.
- **Badge attached to another item**: a counter drawn over the corner of the previous item.
- **LogView wrapping**: wrapped lines of variable height. Dear ImGui's list clipper needs equal
  row heights, so this needs a height cache per entry; until then each log row is one line.
