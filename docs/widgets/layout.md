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

Status: planned

```cpp
void BeginStack(const char* id, const StackOptions& options = {});
void StackSpacer();
void EndStack();
class StackScope;
```

Items in a row or a column with a gap token, cross-axis alignment and flexible spacers.

### BeginGrid

Status: planned

```cpp
void BeginGrid(const char* id, const GridOptions& options = {});
void GridNext(const GridCellOptions& options = {});
void EndGrid();
class GridScope;
```

Equal columns that reflow with the width from a minimum cell width, with column spans.

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

Status: planned

```cpp
void BeginSettingsRow(const char* label, const SettingsRowOptions& options = {});
void EndSettingsRow();
bool SettingsRowResetPressed();
class SettingsRowScope;
```

A label column and a control column that stack when narrow, with description, modified mark and
reset button.

### BeginDependentGroup

Status: planned

```cpp
[[nodiscard]] bool BeginDependentGroup(const char* id, bool enabled, const DependentGroupOptions& options = {});
void EndDependentGroup();
class DependentGroupScope;
```

Settings that are disabled or collapse while their parent setting is off.

### Separator, SeparatorText

Status: planned

```cpp
void Separator(const SeparatorOptions& options = {});
void SeparatorText(const char* text, const SeparatorTextOptions& options = {});
```

A pixel-snapped hairline, horizontal or vertical, optionally with a text.

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
