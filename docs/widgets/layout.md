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

Status: planned

```cpp
void BeginFlow(const char* id, const FlowOptions& options = {});
[[nodiscard]] bool FlowNext();
void EndFlow();
class FlowScope;
```

Items of different widths that wrap onto new lines, with a "+N" chip past the line limit.

### BeginSplitView

Status: planned

```cpp
void BeginSplitView(const char* id, float* ratio, const SplitViewOptions& options = {});
void SplitViewNext();
void EndSplitView();
class SplitViewScope;
```

Two panes with a draggable, keyboard-movable divider, minimum sizes and optional collapse.

### BeginPanels, BeginPanel

Status: planned

```cpp
void BeginPanels(const char* id, std::span<float> sizes, const PanelsOptions& options = {});
void EndPanels();
[[nodiscard]] bool BeginPanel(const char* label, const PanelOptions& options = {});
void EndPanel();
class PanelsScope;
class PanelScope;
```

Several resizable panels with headers that collapse to a strip; a divider moves room only between
its two neighbours.

### BeginListDetail

Status: planned

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

A list and a detail pane side by side, or one at a time with a back button in a Compact width.

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

Status: planned

```cpp
[[nodiscard]] bool BeginDependentGroup(const char* id, bool enabled, const DependentGroupOptions& options = {});
void EndDependentGroup();
class DependentGroupScope;
```

Settings that are disabled or collapse while their parent setting is off.

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
