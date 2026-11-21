# Layout

Stacks, grids, flow layout, split views, panels, list and detail, settings rows, dependent groups,
separators and item grids, declared in `facet/widgets/layout.h`. Every declaration follows
[the API conventions](../api-conventions.md); the header comments are the full reference. See
[the widget index](../widgets.md) for the other families.

A widget whose status is planned is declared and callable: it draws a dashed placeholder with its
name, shows the content of a container inside it, returns the neutral result and never writes a
value. Phase 2 implements each widget and changes its status to ready.

The responsive decisions of these layouts use the width classes of `facet/core/layout.h`
(`WidthClass`, `WidthClassOf`, `AvailableWidthUnits`, `AvailableWidthClass`) and
`LayoutLabeledRow`, which are implemented today. Apps use the same queries for their own narrow
fallbacks; this family adds no second breakpoint system.

When End is called: a Begin that returns `void` always lays out its content and its End is always
called. A Begin that returns `[[nodiscard]] bool` may skip its content, and its End is called only
after true. Every pair has a scope class with the same rule.

## Widgets

### BeginStack

```cpp
void BeginStack(const char* id, const StackOptions& options = {});
void StackSpacer();
void EndStack();
class StackScope;
```

Items in a column or a row with a gap token between them. Dear ImGui lays the items out itself, so
any item works inside, stock or Facet:

- Vertical (the default): the gap is the vertical item spacing while the items are laid out. The
  stack is `width` units wide (0 fills the available width, rule 6 otherwise) and narrows the
  window's work rect to that width, so items that fill the available width stop at its edge.
- Horizontal: the window switches to Dear ImGui's horizontal layout type (as a menu bar does), so
  each item follows the previous one on the line with the gap between them; the vertical layout
  comes back at `EndStack`. `width` 0 fits the items.
- `gap`: `Gap::None` 0, `Tight` `spacing.xs`, `Item` `spacing.sm` (the window's own item spacing),
  `Loose` `spacing.lg`, `Section` `spacing.xxl`. The gap is also the item spacing of stock content
  nested inside an item of the stack, until a nested stack sets its own.
- `StackSpacer()` splits the items into runs. With a set length on the main axis (`width` of a
  horizontal stack, `height` of a vertical one) every spacer takes an equal share of the free space,
  so the items after the last spacer end at the stack's end; with no set length a spacer only
  separates runs. Up to 16 runs are measured; further spacers join their items to the last run.
- `align` places each run on the cross axis, from its size in the last frame: Start, Center or End
  of the stack's cross size (the width of a vertical stack; a set `height`, else the tallest run, of
  a horizontal one). The items inside one run share the run's start; to center controls of different
  heights on one line, put a spacer between them (with no set width it takes no room). In a vertical
  stack, `Stretch` also pushes the stack width as the item width, so fields and buttons fill it; in
  a horizontal stack it is Start.
- The run sizes of the last frame are kept under the id, so the first frame places runs at the start.
- The stack is one group, the last item after `EndStack`, as large as its set lengths or its items.
  Stacks nest in each other and in every other container. `StackSpacer` outside a stack, or inside a
  container nested in one, is reported as a usage error and ignored.

Status: ready.

### BeginGrid

```cpp
void BeginGrid(const char* id, const GridOptions& options = {});
void GridNext(const GridCellOptions& options = {});
void EndGrid();
class GridScope;
```

Equal columns that reflow with the width. The grid is `width` units wide (0 fills the available
width); its column count is the most columns of at least `minCellWidth` units (0:
`layout.minRowControl`) that fit with the gap between them, at least one and at most `maxColumns`
when that is set, so a narrower region shows fewer, wider columns. At a width of a few pixels the
grid has one column of at least one pixel.

- `GridNext()` starts the next cell at the next free column. A cell's item width and the window's work
  rect are narrowed to the cell, so fields and fill-width items fill it.
- `columnSpan` covers that many columns (clamped to the column count); a span that does not fit on
  the current row starts the next row.
- A row starts one gap below the tallest cell of the row above, measured as its cells end in the
  same frame, so cells never overlap, also when one grows. The grid keeps no state across frames.
- Arrow keys move between the items of the cells through Dear ImGui's spatial navigation, and Tab
  follows the order the cells were submitted in.
- The grid is one group, the last item after `EndGrid`; with no cell it is as tall as nothing.
  `GridNext` outside a grid, or inside a container nested in one, is reported and ignored.

Status: ready.

### BeginFlow

```cpp
void BeginFlow(const char* id, const FlowOptions& options = {});
[[nodiscard]] bool FlowNext();
void EndFlow();
class FlowScope;
```

Items of different widths that wrap onto new lines, such as chips and tags. The flow is `width`
units wide (0 fills the available width) and narrows the window's work rect to it.

- `FlowNext()` before each item places the cursor and starts the item; every item is a group, so it
  may be made of several items. The sizes of the items in the last frame are kept under the id:
  `BeginFlow` breaks them into lines with the gap between items and between lines, and a line starts
  one gap below the tallest item of the line above, measured in the same frame. An item the last
  frame did not know (the first frame, or one added since) goes after the previous item, or on a new
  line when that one already reached the width; the next frame places it properly. An item wider
  than the flow gets a line of its own.
- `align`: Start, Center or End places each line; Stretch spreads the items of every line but the
  last over the whole width.
- `maxLines`: from the first item that does not fit, `FlowNext` returns false for every later item;
  skip them. On the last allowed line an item must leave room for the chip when more items follow
  (the first item of a line is always shown). `EndFlow` then draws the chip after the last item
  shown: a neutral `Badge` with `Strings::moreCount` ("+{count}") and the number of skipped items.
  The chip is not interactive.
- The item sizes live in a vector under the id that grows with the item count and is not touched
  otherwise. Sizes measured at another scale are dropped, so a scale change takes one frame to
  settle.
- `EndFlow` is always called; the flow is one group, the last item after it. `FlowNext` outside a
  flow, or inside a container nested in one, is reported and returns false.

Status: ready.

### BeginSplitView

```cpp
void BeginSplitView(const char* id, float* ratio, const SplitViewOptions& options = {});
void SplitViewNext();
void EndSplitView();
class SplitViewScope;
```

Two panes with a divider between them. The view is `width` x `height` units (0 fills the available
width and height; with no height left it takes `metrics.panelHeight`). The divider is a band of
`layout.dividerBand` between the panes: a hairline in `separator` with a short grip in the middle in
`scrollThumb` (`scrollThumbHover` while hovered, dragged or focused), the part that reaches 3:1, and
the resize cursor over it.

- Sizes: the first pane gets `*ratio` of the room (the view less the band), kept between `minFirst`
  and the room less `minSecond` (0: `layout.minRowControl` each). When the room is smaller than both
  minimums together, the panes share it in the proportion of their minimums and the divider moves
  nothing. The ratio is read every frame, so the panes keep their shares when the view is resized.
- Mouse: dragging the divider writes `*ratio` while it moves; with `collapsible`, dragging past half a
  minimum collapses that pane (`*ratio` 0 or 1). A double click goes back to the ratio the view had
  the first time it was shown.
- Keyboard: the divider is a Tab stop between the panes (Tab goes from the first pane's items to the
  divider to the second pane's). The arrows of its axis move it by `spacing.lg` (Shift `spacing.xs`,
  Ctrl four times `spacing.xxxl`), Home and End to the first and second pane's minimum. With
  `collapsible`, Enter or Space collapses the first pane, and on a collapsed pane brings back the
  ratio it had. Escape during a drag puts back the ratio from the drag's start.
- Each pane is a scroll area of the scroll core: a child window with Facet's overlay bars whose
  keyboard navigation is part of the view's window. Its content keeps its ids under the pane. A
  collapsed pane is a hidden child window: the content is submitted but clipped away and takes no
  input.
- Compact width: a horizontal split shows its panes one above the other (the divider then moves on
  y with Up and Down).
- Remembered: the view keeps the last ratio the user set, the ratio before the last collapse and its
  first ratio under its id in the Dear ImGui ini file. In a later run, the first `BeginSplitView`
  writes the saved ratio to `*ratio` once, so the split survives a restart even when the caller
  starts from a constant; later writes of the caller stand.
- A null `ratio` shows the panes at 0.5 and writes nothing. Values outside 0 to 1 are shown clamped
  (NaN as 0.5) and written only by an edit.
- `SplitViewNext` starts the second pane; a view that never calls it gets an empty second pane.
  `EndSplitView` is always called; the view is one group, the last item after it. Views nest in
  their panes.

Status: ready.

### BeginPanels, BeginPanel

```cpp
void BeginPanels(const char* id, std::span<float> sizes, const PanelsOptions& options = {});
void EndPanels();
[[nodiscard]] bool BeginPanel(const char* label, const PanelOptions& options = {});
void EndPanel();
class PanelsScope;
class PanelScope;
```

Resizable panels with headers, side by side (or stacked with `Orientation::Vertical`) in a view of
`width` x `height` units (0 fills the available room; with no height left, `metrics.panelHeight`).
Submit one `BeginPanel` per entry of `sizes`, in order; further panels are reported and skipped, as
are panels past 16.

- Sizes: a collapsed panel takes a strip of `layout.panelHeaderHeight`; the expanded panels share the
  rest in the proportion of their `sizes` entries (normalized; negative entries count as 0, and all 0
  share equally). Between neighbours sits a band of `layout.dividerBand`.
- Dividers: the same divider as `BeginSplitView` between two expanded neighbours, submitted with the
  second one, so Tab goes from a panel's content to the divider to the next panel's header. Dragging
  it, the arrows of its axis (Shift fine, Ctrl coarse), Home and End move room between its two
  neighbours only: their two entries of `sizes` are written, keeping their sum, and neither gets
  shorter than `layout.minRowControl` while both fit it. The new sizes show from the next frame.
  Beside a collapsed panel only the hairline is drawn.
- Header: a row of `layout.panelHeaderHeight` with the icon and the title in Headline, cut with an
  ellipsis and a tooltip when narrow, and a hairline under it. With `collapsible` (the default) the
  header is the containers family's header row with its chevron: a click, Enter or Space collapses
  the panel, and the hover fill and focus ring show it is an item. Without `collapsible` the title is
  drawn as text, not an item.
- Collapsed: side by side, the strip shows an icon button with a right chevron whose tooltip and
  name are the title (`Strings::expand` without a title); it expands the panel. Stacked, the header
  row stays and expands it. The panel collapses and expands over `motion.durationStandard` (at once
  with reduced motion), and its neighbours grow and shrink with it. The collapsed state is kept under
  the panel's id in the Dear ImGui ini file.
- `BeginPanel` returns true while the panel is expanded: its content goes into a scroll area of the
  scroll core under the header (ids under the panel), and `EndPanel` follows; false while collapsed or
  out of sight. Panel labels are hashed under the view's id.
- Compact width: horizontal panels stack. `EndPanels` is always called; the view is one group, the
  last item after it. Views nest in their panels.

Status: ready.

### BeginListDetail

```cpp
void BeginListDetail(const char* id, bool* detailOpen, const ListDetailOptions& options = {});
[[nodiscard]] bool BeginListPane();
void EndListPane();
[[nodiscard]] bool BeginDetailPane();
void EndDetailPane();
void EndListDetail();
class ListDetailScope;
class ListPaneScope;
class DetailPaneScope;
```

A list pane and a detail pane in the available width, `height` units tall (0 fills the available
height; with no height left, `metrics.panelHeight`).

- Regular and Expanded widths: the list pane is `listWidth` units wide (0: `layout.labelColumn` +
  `layout.minRowControl`), at most the width less the band and `layout.minRowControl`; then a band of
  `layout.dividerBand` with a hairline in `separator`, and the detail pane in the rest. The divider
  does not move; use `BeginSplitView` for a resizable split.
- Compact width: one pane at the full size, the list while `*detailOpen` is false, the detail while
  it is true. The caller writes `*detailOpen = true` when an entry is chosen. The detail pane starts
  with a plain back button (icon "chevron-left", `Strings::back`) that writes `*detailOpen = false`;
  when the detail opens while the keyboard focus was in the list pane and the last input was the
  keyboard, the focus moves to the back button, so it is not left on an entry that went away.
- Each pane is a scroll area of the scroll core: a child window with Facet's overlay bars whose
  keyboard navigation belongs to the window around it; its content keeps its ids under the pane.
- A pane Begin returns false while its pane is hidden (the Compact rule) and while it is out of sight;
  its End only after true. `EndListDetail` is always called; the view is one group, the last item
  after it. List-details nest in their panes. A null `detailOpen` counts as false and is never
  written.

Status: ready.

### BeginSettingsRow

```cpp
void BeginSettingsRow(const char* label, const SettingsRowOptions& options = {});
void EndSettingsRow();
bool SettingsRowResetPressed();
class SettingsRowScope;
```

One labelled row of the row layout block (`src/internal/row_layout.h`): the label in a label column,
the control (whatever the caller submits between Begin and End) in the rest of the available width.

- Columns: the label column is `labelWidth` units, or the `layout.labelColumn` token, with the
  `spacing.sm` gap included, so in a column of settings rows every control starts at one x whatever
  the label lengths. A label too long for its column ends in an ellipsis and shows in full as a
  tooltip on hover, and under the label while the row's control has the keyboard focus. The rows of
  one window under one id stack top form a column: each keeps the widest reset slot of the column
  (as measured in the last frame) free at its end, so the controls also end in one line (rows
  indented by a dependent group included) and every row of the column stacks at the same width.
- Narrow widths: when the column and `layout.minRowControl` do not fit, the label goes on its own
  line and the control takes the whole width under it.
- The control slot pushes its width as the item width and narrows the window's work rect, so Facet
  fields and stock items with a width of `-FLT_MIN` fill the slot and stop before the reset button.
- Baseline: the label is drawn in the text style of the control and on the line of the control's
  text. The row measures the height of its control (the last item of the slot, when it starts at the
  slot's top) and keeps it under its id; when it is the height of the Small, Regular or Large
  control size, the next frame lines the label up with a control of that size, otherwise with the
  inherited size (`PushControlSize`). The first frame of a row uses the inherited size.
- `description`: a Footnote line in `labelSecondary` under the label; it wraps across the control
  column when nothing sits under the control, else within the label column. `icon` sits before the
  label in `labelSecondary`.
- `modified` puts a dot after the label; with `resetButton` the row keeps a square slot at its end,
  where the reset button (icon "reset", tooltip `Strings::reset`) shows while `modified` is on. It is
  a Tab stop after the control and activates with a click, Enter or Space;
  `SettingsRowResetPressed()` reports it for the last row ended in the frame.
- Ids: the label is pushed on the id stack until `EndSettingsRow`, so every control can use
  `"##value"`. Two rows with the same visible text need `###`.
- Disabled: inside `ImGui::BeginDisabled` the label, mark, icon and description turn
  `labelTertiary` with the control.
- The row is one group, the last item after `EndSettingsRow`, so rows stack with the window's item
  spacing (`spacing.sm`) and work in table cells. Rows nest up to the block's depth of 8.

The options have no `error` or `helpText`: the control in the row draws its own message.

Status: ready.

### BeginDependentGroup

```cpp
[[nodiscard]] bool BeginDependentGroup(const char* id, bool enabled, const DependentGroupOptions& options = {});
void EndDependentGroup();
class DependentGroupScope;
```

Settings that only apply while a parent setting is on; `enabled` is the parent's state.

- With `indent` (the default) the content is indented by `spacing.lg`, and a hairline in `separator`
  runs down the middle of the indent along the content, so the rows read as belonging to the setting
  above.
- While `enabled` is false the content is disabled through `ImGui::BeginDisabled`: Facet widgets
  draw their disabled look and show the not-allowed cursor, and disabled items are no Tab stops, so
  the keyboard goes from the parent straight to the first item after the group. Settings rows inside
  draw their labels in `labelTertiary` too.
- `DependentMode::Disable` (the default) keeps the content visible and always returns true.
  `DependentMode::Hide` also collapses it: the collapse region of the containers family animates its
  height to 0 over `motion.durationStandard` (at once with reduced motion), clips the content
  meanwhile and keeps the closing items out of the keyboard focus; once closed, Begin returns false.
  Turning the parent on opens it the same way. The animated height is kept under the id.
- `EndDependentGroup` only after true. Groups nest (a dependent group inside a dependent group), and
  rows, stacks and any other content work inside. Outside a frame Begin returns false.

Status: ready.

### Separator, SeparatorText

```cpp
void Separator(const SeparatorOptions& options = {});
void SeparatorText(const char* text, const SeparatorTextOptions& options = {});
```

Hairline dividers, not interactive (no id, never focused, skipped by Tab). The hairline is
`metrics.hairline` rounded to whole pixels (at least one) and snapped to the pixel grid, in
`separator`, or `separatorOpaque` with `opaque` where a translucent line would double up.

- `Separator()` is an item as wide as the available width and one hairline tall; the window's item
  spacing gives the room above and below it.
- `Orientation::Vertical` goes on the current line after `ImGui::SameLine`: as tall as the items
  before it on that line, or one control height (the inherited size) on an empty line.
- `SeparatorText(text)` is one Footnote line: the text in `labelSecondary` (`labelTertiary` inside
  `ImGui::BeginDisabled`) at the start, or in the middle with `centered`, and the hairline on the free
  sides, centered on the text. A text too long for the width ends in an ellipsis and shows in full as
  a tooltip on hover; the text is read during the call.

Status: ready.

### BeginItemGrid

Status: planned

```cpp
[[nodiscard]] bool BeginItemGrid(const char* id, int itemCount, ImGuiSelectionBasicStorage* selection,
                                 const ItemGridOptions& options = {});
bool ItemGridTile(int index);
void EndItemGrid();
class ItemGridScope;
```

A virtualized, reflowing grid of equal tiles with two-dimensional selection.

## Usage examples

A settings section. Every row pushes its label on the id stack, so each control uses `"##value"`;
the dependent group disables its rows while the parent toggle is off:

```cpp
facet::BeginSettingsRow("Shadows", {.description = "Soft shadows under floating layers"});
ImGui::Checkbox("##value", &shadows);                       // or any Facet control
facet::EndSettingsRow();
if (facet::BeginDependentGroup("shadow options", shadows))
{
    facet::BeginSettingsRow("Strength", {.modified = strength != kDefaultStrength, .resetButton = true});
    ImGui::SliderFloat("##value", &strength, 0.0f, 2.0f);
    facet::EndSettingsRow();
    if (facet::SettingsRowResetPressed())
        strength = kDefaultStrength;
    facet::EndDependentGroup();
}
```

A split view whose ratio the app keeps (and saves) itself:

```cpp
static float ratio = 0.3f;
facet::BeginSplitView("browser", &ratio, {.minFirst = 160.0f, .collapsible = true});
DrawFolders();
facet::SplitViewNext();
DrawFiles();
facet::EndSplitView();
```

Panels with headers, one share per panel:

```cpp
static float sizes[3] = {0.25f, 0.5f, 0.25f};
facet::BeginPanels("workspace", sizes);
if (facet::BeginPanel("Layers")) { DrawLayers(); facet::EndPanel(); }
if (facet::BeginPanel("Canvas", {.collapsible = false})) { DrawCanvas(); facet::EndPanel(); }
if (facet::BeginPanel("Properties")) { DrawProperties(); facet::EndPanel(); }
facet::EndPanels();
```

List and detail. The app opens the detail when an entry is chosen; in a Compact width the back
button closes it again:

```cpp
static bool detailOpen = false;
facet::BeginListDetail("messages", &detailOpen);
if (facet::BeginListPane())
{
    if (DrawMessageList(&current))
        detailOpen = true;
    facet::EndListPane();
}
if (facet::BeginDetailPane())
{
    DrawMessage(current);
    facet::EndDetailPane();
}
facet::EndListDetail();
```

A flow of chips limited to two lines:

```cpp
facet::BeginFlow("tags", {.maxLines = 2});
for (const Tag& tag : tags)
    if (facet::FlowNext())
        DrawTag(tag);
facet::EndFlow();
```

A grid over a large set of tiles (thumbnails, files); only the visible tiles are submitted, and the
tile content is the caller's own drawing:

```cpp
static ImGuiSelectionBasicStorage selection;
if (facet::BeginItemGrid("tiles", itemCount, &selection, {.minTileWidth = 72.0f}))
{
    facet::RowRange tiles;
    while (facet::NextVisibleRows(&tiles))
        for (int i = tiles.first; i < tiles.last; ++i)
        {
            if (facet::ItemGridTile(i))
                Use(i);
            DrawTileContent(i);   // fills the tile's area
        }
    facet::EndItemGrid();
}
```

## Later

No widget of this family is in the later tier.
