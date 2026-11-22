# Display

Widgets that show text, states and data: styled and truncated text, section headers, banners
and alerts, empty states and data state views, progress, spinners and skeletons, badges and
status marks, key caps, code and log views, key-value lists, stat tiles and data tables.

Header: `facet/widgets/display.h`. Every declaration follows [the API conventions](../api-conventions.md),
and the header comments are the full reference (parameters, units, keys, state kept per id).
The demo gallery shows every widget on the page Widgets > Display.

Each entry ends with its status; every widget of the family is ready and works as its header describes.

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

A full-width strip in the tone's wash (Neutral: the control fill) with the tone's icon in its glyph
color (or `icon`), a bold title (Headline) and the wrapped message in `label`, which stays readable on
every wash. Up to two actions as small buttons (the primary one Secondary, the other Plain) sit at the
end of the title line while the text keeps at least `layout.minRowControl`, and under the message
otherwise. With `open` a close button (`Strings::close` as its tooltip) ends the first line. Every
button is a Tab stop, also while the banner is scrolled out of sight, and Escape on a focused action
or on the close button dismisses it; dismissing writes `*open = false` and returns `Dismissed`. A
dismissed banner collapses over `motion.durationStandard` (at once with reduced motion; its buttons
are not active meanwhile) and then submits nothing; writing `*open = true` grows it back. Without
`open` it has no close button and stays. Inside `ImGui::BeginDisabled` the wash, icon and text take
the disabled colors. Status: ready.

### InlineAlert

```cpp
InlineAlertAction InlineAlert(const char* label, bool* open, const char* message, const InlineAlertOptions& options = {});
```

The callout box of Banner for a place inside the content: the same wash, icon, Headline title,
wrapped message and close button, with a hairline around it and its one action as a small Secondary
button under the message. The action and the close button are Tab stops; Escape on either one
dismisses it, which writes `*open = false`, returns `Dismissed` and collapses it like a banner.
Status: ready.

### EmptyState

```cpp
bool EmptyState(const char* label, const EmptyStateOptions& options = {});
```

A placeholder for a view without content: `icon` (`metrics.emptyIconSize`) in `labelSecondary`, or in
`destructive` with the Destructive tone; the title in Headline; the wrapped message in
`labelSecondary`; and one Secondary button. Everything is centered in the width, and in `height` when
it is set (otherwise the content plus `spacing.lg` above and below). `compact` puts it all on one
line: a small icon, the title, the message cut with an ellipsis, and a small button at the end. Only
the action is a Tab stop; it is reachable while the state is scrolled out of sight, and the call
returns true when it is activated. Status: ready.

### DataStateView

```cpp
[[nodiscard]] bool BeginDataStateView(const char* id, DataState state, int count, const DataStateViewOptions& options = {});
void EndDataStateView();
bool DataStateViewRetryPressed();
class DataStateViewScope;
```

Draws the state of a data view itself and opens a group for the content only when there is some:
Loading draws `skeletonRows` skeleton bars in the list row height; Error draws the error state (error
icon, `errorText` or `Strings::loadFailed`, and the `retryLabel` button); Ready with a count of 0
draws the empty state (`emptyText` or `Strings::empty`). Its height is `height` when set, otherwise
the height of the last Ready frame with content (kept under the id), so switching states does not
move what follows; before any Ready frame a state takes the room it needs. A state too low for the
full empty state draws the compact one-line form. A state that appears fades in over
`motion.durationFast` (at once with reduced motion); the caller's content is shown as it is.
`DataStateViewRetryPressed` is true on the frame the retry button of the last call was activated.
`EndDataStateView` without its Begin is reported and does nothing. Status: ready.

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

A busy indicator as tall as the text of its size, centered on the capitals of the line, with the
visible label part after it in `labelSecondary`. Neutral turns in `labelSecondary`, Accent in the 3:1
accent (`focusRing`), the status tones in their colors. Every spinner reads one loading clock; with
reduced motion it stands still as three quarters of a ring and fades in place. Not interactive;
keeps nothing per id. Status: ready.

### Skeleton

```cpp
void Skeleton(const SkeletonOptions& options = {});
```

A loading placeholder with the shared shimmer sweep, flat with reduced motion: Text draws `lines`
bars, each centered in a Body line (or `height`), the last of several ending early; Rect one block of
`width` by `height` (the control height by default) with the `corners` radius; Circle a true circle of
`height` (an avatar placeholder). Width: the available width (Circle: its height). Not interactive;
keeps no state. Status: ready.

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

A chord as key caps: one cap per modifier (Ctrl, Shift, Alt, Win, in that order) and one for the key,
named by `KeyNameText`; a modifier key alone folds into the modifiers. A cap is the faint control
fill with a 3:1 boundary hairline and the key name in `label`. `ImGuiKey_None` draws
`Strings::unbound` in `labelSecondary`. With `live` the caps show the held look (a stronger fill and
a `focusRing` outline) while the chord is down. Small, Regular and Large sizes; the caps sit on the
text baseline like a badge. Not interactive. Status: ready.

### ShortcutHint

```cpp
void ShortcutHint(const char* text, ImGuiKeyChord chord, const ShortcutHintOptions& options = {});
void ShortcutHint(const char* text, ShortcutId shortcut, const ShortcutHintOptions& options = {});
```

A text in `labelSecondary` at the start of its width (the available width by default) and the chord
as key caps at the end, on one baseline. When the width cannot hold both, the caps are left out
first; then the text is cut with an ellipsis and shows in full on hover. The `ShortcutId` form draws
the registered shortcut's current primary chord, and the text alone for an invalid or unknown id or a
shortcut without a chord. Not interactive. Status: ready.

### CharacterCounter

```cpp
void CharacterCounter(int count, int max, const CharacterCounterOptions& options = {});
```

The length of a text against its limit as "count / max" in Footnote, `labelSecondary` while there is
room. From `warnRemaining` characters left it turns to the warning tone with the warning icon, over
the limit to the destructive tone with the error icon, so the state does not rest on color alone.
`ring` draws a small ring filled to the share used instead, with the characters left beside it once
it warns (negative over the limit). `max` of 0 or less shows the count alone. It sits on the Footnote
baseline of the line. Not interactive. Status: ready.

### InlineCode

```cpp
bool InlineCode(const char* label, const InlineCodeOptions& options = {});
```

Code or an identifier (the visible label part) in a box with the faint control fill and the element
radius, sitting on the text baseline of the line like a badge. The face is the UI font: Facet has no
monospace face yet. With `copyOnActivate` it is a Tab stop: a click, Enter or Space copies the
visible text (without the `"##"` part) through `ImGui::SetClipboardText`, returns true and shows a
check and `Strings::copied` above it for `motion.feedbackHold` times `motion.scale`; hover and press step the fill. Without
it the item is not a Tab stop. Status: ready.

### CodeView

```cpp
bool CodeView(const char* id, std::string_view code, const CodeViewOptions& options = {});
```

A read-only code block on the grouped surface (`bgSecondary` with a hairline) with a line number
gutter that stays in place while the code scrolls sideways, and a light tint: keywords, strings and
numbers in the `code*` tokens, comments in `labelSecondary` (C++ with the preprocessor and block
comments across lines, JSON, INI; Plain draws everything in `label`). `highlightLines` get the
`accentSubtle` background. Long lines scroll sideways, or wrap with `wrap`. The height fits up to 20
lines and then scrolls (or `height`). Without wrapping only the visible lines are drawn; the line
table is built again only when the text changes. The view is one Tab stop with a focus ring: while it
has keyboard focus (also after a click in it) the arrows, Page Up and Down, Home and End scroll it and
Ctrl+C copies the whole text, showing `Strings::copied`. With `copyButton` a `CopyButton` sits in the
top-right corner. Returns true on the frame the text was copied. Not yet: a monospace face (tabs
advance four space widths of the UI font, so columns line up only roughly), selecting part of the
text, and more than 16 wrapped segments of one line (the rest of such a line is not drawn).
Status: ready.

### LogView

```cpp
void LogView(const char* id, std::span<const LogEntry> entries, const LogViewOptions& options = {});
void LogView(const char* id, int entryCount, LogGetter getter, void* user, const LogViewOptions& options = {});
```

A virtualized log on the list view block, on the grouped surface: only the visible entries are read,
so a hundred thousand rows cost what a screenful does. Each row is one Body line in the small list row
height: the time (`mm:ss.mmm`, or `h:mm:ss.mmm` from one hour on, in a column measured for the widest
time) with `timestamps`, the level (warning and error icons in their tone; trace and debug text in
`labelSecondary`), the source tag in `labelSecondary` and the text, cut at the view edge with the
whole line on hover and keyboard focus. Rows select like any Facet list (click, Ctrl, Shift, Shift
with the arrows, Ctrl+A, box select, Escape) and Ctrl+C copies the selected lines in log order as
`time  source  text`. The view is one Tab stop; the arrows, Page Up and Down, Home and End move the
focused row and scroll. While it shows the end it follows new entries; a scroll up (wheel, keys,
scrollbar) stops following and shows `Strings::jumpToLatest` under the log, which, like End or
scrolling back to the end, resumes it. Loading, Error and an empty log show their states. Lines
that wrap are not supported yet (see the end of this page). Status: ready.

### KeyValueList

```cpp
void BeginKeyValueList(const char* id, const KeyValueListOptions& options = {});
bool KeyValue(const char* label, const char* value, const KeyValueOptions& options = {});
void EndKeyValueList();
class KeyValueListScope;
```

Read-only labelled values whose columns come from `LayoutLabeledRow`, so keys line up with settings
rows (`labelWidth` or `layout.labelColumn`) and a narrow row puts the value under its key. Keys in
Body `labelSecondary`, cut with an ellipsis and shown in full on hover; values in `label` (or the
tone's text color), wrapped, or cut with an ellipsis and a tooltip without `wrap`. In a `copyable`
list each value is a Tab stop with a focus ring around its text: a click, Enter or Space copies it,
`KeyValue` returns true and `Strings::copied` shows above it. Content is always submitted; an End
without its Begin is reported and does nothing. Status: ready.

### StatTile

```cpp
void StatTile(const char* label, double value, const StatTileOptions& options = {});
```

A tile on the grouped surface (card radius, hairline) with the visible label part as a Footnote
caption, the value in Title1 (`format` or `formatter`, `"%.0f"` by default) counting to a new value on
the standard spring (jumping with reduced motion), and an optional delta chip after it: the success
wash for the good direction and the destructive wash for the bad one (a negative delta is good with
`invertDelta`), an arrow up or down (a dash for 0) and the signed text, so the direction never rests
on color alone. `trend` draws a line over its samples along the bottom of the tile in the 3:1 accent.
Loading shows a skeleton in place of the value, Error the error icon with `Strings::loadFailed`.
Width: the available width. Not interactive. Status: ready.

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

### Marks on a line of text

Badges, count badges, status pills, key caps and inline code are a little taller than the text around
them and put their text on the baseline of that text: after `ImGui::SameLine` they meet Body text,
a Facet control's label or a stock widget's text drawn before or after them, at every scale. Status
dots, spinners and character counters center their mark on the capitals of the line.

```cpp
facet::StyledText("Connection");
ImGui::SameLine();
facet::StatusPill("Online###net", {.tone = facet::Tone::Success, .liveDot = true});
ImGui::SameLine();
facet::KeyCap(ImGuiMod_Ctrl | ImGuiKey_R);
```

### Tones

Every tone is drawn with a second cue: an icon, a word or a shape. Text in a tone is readable (4.5:1)
on the content surfaces (`bg`, `bgSecondary`, `bgTertiary`); on a tone's wash the text is `label`,
and the tone shows in its icon or dot (3:1). A test (`tests/test_display.cpp`) computes these pairs
from the theme tokens in both themes.

## Data table

DataTable and its cell editing, the data views of this family with rows and columns. The demo gallery
shows them on the page Widgets > Data table.

### DataTable

```cpp
[[nodiscard]] bool BeginDataTable(const char* id, std::span<const DataTableColumn> columns, int rowCount, ImGuiSelectionBasicStorage* selection, const DataTableOptions& options = {});
bool DataTableRow(int index);
void EndDataTable();
class DataTableScope;
```

A Dear ImGui table (`BeginTableEx` with both scroll axes) on the grouped surface (`bgSecondary`, card
radius, hairline), with a Facet header row that stays in view, virtualized rows and an optional
selection. Column widths, order, visibility and sort are Dear ImGui's state of the table, kept under
the id and in its ini file.

- **Header:** each visible column has a header cell with the visible part of its label in Footnote,
  `labelSecondary`, or `label` while the column sorts, cut with an ellipsis (the whole label on hover
  and keyboard focus) and placed by the column's `align`. `align` places the header only; cells draw
  their own content (`StyledText` with `.align` and `.width = kFill` lines numbers up). Header cells
  are Tab stops in display order when the table sorts or has the column menu. Hover and press fill
  the cell inset by `spacing.xs`; the focus ring goes around that fill.
- **Sorting** (`sortable`): a click, Enter or Space on a header sorts by its column and the next one
  turns the direction (Dear ImGui's `TableSetColumnSortDirection`); with `multiSort`, Shift adds the
  column to the sort. A sorted column shows an arrow (up ascending, down descending) and, while more
  than one column sorts, its place in the order. Read `ImGui::TableGetSortSpecs` between Begin and
  End; its `SpecsDirty` is true once per change (and once for a new table), never on frames without
  one. `ImGuiTableColumnFlags_NoSort` and the other stock sort flags apply.
- **Resizing** (`resizable`): a band of `table.resizeBand` on the right edge of a header cell resizes
  the column by drag; a double click fits it to its content. A hairline divider sits between header
  cells and turns into a `focusRing` line while its band is hovered or dragged. No column gets
  narrower than `table.minColumnWidth` (its padding included). Resizing has no keys.
- **Reordering** (`reorderable`): dragging a header past a neighbour's edge moves the column there,
  never across the frozen columns (Dear ImGui's rule). There are no keys for it.
- **Column menu:** a right click on a header (or on the header row after the last column), or the
  Menu key or Shift+F10 on a focused header, opens a menu with `Strings::sortAscending` and
  `Strings::sortDescending` for a sortable column and, with `hideable`, the `Strings::columns`
  submenu, whose rows show and hide columns. A column with `ImGuiTableColumnFlags_NoHide`, and the
  last column shown, cannot be hidden. Without `sortable` and `hideable` there is no menu.
- **Frozen columns** (`frozenColumns`): the leading columns stay in place while the others scroll
  sideways under them; a hairline marks their edge while the table is scrolled.
- **Rows:** `rowHeight`, or the list row height of the inherited control size (`PushControlSize`).
  Rows are virtualized by the row ranges of the list core: only the visible rows, the focused row and
  the start of a Shift range are handed out by `NextVisibleRows`, so a hundred thousand rows cost what
  a screenful does, and a frame allocates nothing once the table is shown. Each row is exactly one row
  height; content taller than the row is not supported. Text in cells is centered on the row.
- **Row look:** `DataTableRow` submits one row item that spans every column, from the first column
  to the right edge of the visible table, also while the table is scrolled sideways. It draws in the
  table's background channel, under the cells: the hover fill (`fill[2]`), the selection
  (`accentSubtle` while the rows have the focus, `fill[1]` when not) and the focus ring around the
  whole row, or around the focused cell. A control in a cell takes the pointer before the row.
- **Selection** (`selection` set): click, Ctrl, Shift, Shift with the arrows, Ctrl+A, Escape and box
  select through `ImGui::BeginMultiSelect` and the storage; `singleSelect` keeps one row. The start of
  a Shift range stays on its item when the caller sorts (the selection storage keys rows through
  `AdapterIndexToStorageId`, see the usage below).
- **Keys** while a row has the focus: Up, Down, Page Up and Page Down move between rows (Dear ImGui
  navigation, the selection follows as in a list); Home and End go to the first and last row; Left
  and Right move a focused cell along the columns in display order, scrolling it into view, and Left
  from the first column gives the focus back to the whole row; Enter or a double click activates the
  row (`DataTableRow` returns true), except on an editable cell, which they edit. Tab enters the rows
  at the focused row (the first one before any) and leaves them for the next item; the rows are one
  Tab stop. Rows have no typeahead.
- **States:** `Loading`, `Error` (`errorText` or `Strings::loadFailed`) and `Ready` with no rows
  (`emptyText` or `Strings::empty`) draw under the header in place of the rows; BeginDataTable then
  returns false, the header still sorts, resizes and opens its menu.
- **Read-only and disabled:** `readOnly` (or a pushed `ImGuiItemFlags_ReadOnly`) keeps every cell
  out of edit mode; selection, sorting and the column menu still work, and a double click activates.
  Inside `ImGui::BeginDisabled` nothing is selected, sorted or edited and nothing is a Tab stop.
- **Size:** width by the rule of the conventions (the available width by default); `height` in
  units, 0 fills the available height but keeps at least the header and three rows. A table narrower
  than its columns scrolls sideways with the overlay bars of the scroll core, which the header and
  the frozen columns leave free.

Status: ready.

### DataTable cell edit

```cpp
[[nodiscard]] bool BeginDataTableCellEdit();
void EndDataTableCellEdit();
class DataTableCellEditScope;
```

Edits one cell in place with the caller's editor widget. A cell of a column declared `editable`
enters edit mode on F2 or Enter while it is the focused cell (F2 on a focused row edits its first
editable cell in display order), or on a double click. On that row and column BeginDataTableCellEdit
returns true: submit one editor labelled `"##edit"` (`TextField`, `NumberInput` or any other widget
on the text field and number cores); it fills the cell, sits centered on the row and takes the
keyboard focus. The editor writes as it always does (text per keystroke, numbers on Enter) and
reports the outcome through `IsItemEditCommitted` and `IsItemEditCanceled` right after it; Escape
restores the value from the start of the edit. The session ends when the editor is deactivated: Enter,
Tab and Escape give the focus back to the cell, a click elsewhere leaves it where the click went. A
session whose editor never becomes active within a few frames is dropped. `readOnly`, a disabled
table and columns that are not `editable` never enter edit mode. Status: ready.

### Usage: sorting, selection and cell editing

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
