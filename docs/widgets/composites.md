# Composites

Widgets built from the other families and the drag and drop core: editors for curves, easing and
gradients, property grids, console views, search and filter bars, settings search, key bind and
preset lists, theme controls, inventory grids, item tiles, slots and detail cards, media cards,
user chips and profile switchers, preview canvases, layer lists, onboarding, and the public drag
and drop calls.

Header: `facet/widgets/composites.h`. It includes `display.h` (for `LogEntry`) and `media.h`
(for `AvatarItem`). Every declaration follows [the API conventions](../api-conventions.md), and
the header comments are the full reference. The demo gallery shows every widget on the page
Widgets > Composites.

**Status: planned.** Each widget below is declared, compiles and can be called; until Phase 2
implements it, a call draws a placeholder of the widget's default size and returns the neutral
result. The drag and drop calls wait for the drag and drop core of Phase 1c part B.

## Who owns the data

Item data always belongs to the caller and is read during the call.

- **Per-item state the widget edits** comes as a non-const span, and the widget writes only the
  members documented as written: `FilterChip::active`, `KeybindListItem::bind`,
  `LayerItem::visible` and `LayerItem::locked`.
- **Growing and shrinking collections** (curve points, gradient stops) come as the caller's
  storage plus a count: the span size is the capacity, `*count` the number in use.
- **Rearranging the caller's data** (moving an inventory item, reordering layers, renaming a
  preset) is reported as a result struct, and the caller applies it: `InventoryResult`,
  `LayerListResult`, `PresetListResult`.
- **Entry ids**: every item struct has an `ImGuiID id` member. Entries get ids under the widget
  id, `item.id` when it is non-zero and the index otherwise, so a caller that reorders its items
  sets `id` and the state of each entry (hover, animations) follows the item, not the position.

## Widgets

| Widget | Declaration | What it does | Status |
|---|---|---|---|
| CurveEditor | `bool CurveEditor(const char* label, std::span<ImVec2> points, int* count, ImVec2 min, ImVec2 max, const CurveEditorOptions& options = {})` | Response curve through draggable, addable control points | planned |
| BezierEditor | `bool BezierEditor(const char* label, CubicBezier* curve, const BezierEditorOptions& options = {})` | Cubic easing editor with two handles, presets and numeric fields | planned |
| GradientEditor | `bool GradientEditor(const char* label, std::span<GradientStop> stops, int* count, const GradientEditorOptions& options = {})` | Gradient bar with draggable color stops | planned |
| PropertyGrid | `void BeginPropertyGrid(const char* id, const PropertyGridOptions& options = {})`, `void EndPropertyGrid()`, `[[nodiscard]] bool BeginPropertyCategory(...)`, `void EndPropertyCategory()`, `[[nodiscard]] bool BeginPropertyRow(...)`, `void EndPropertyRow()`, with scopes | Inspector of labelled rows in collapsible categories with a filter | planned |
| ConsoleView | `bool ConsoleView(const char* id, std::string* command, std::span<const LogEntry> entries, const ConsoleViewOptions& options = {})`, overloads with a getter and with a `char*` buffer | Log view with a command field, history and Tab completion | planned |
| SearchFilterBar | `bool SearchFilterBar(const char* label, std::string* query, std::span<FilterChip> filters, const SearchFilterBarOptions& options = {})`, overload with a `char*` buffer | Search field with toggleable filter chips and a result count | planned |
| SettingsSearch | `void BeginSettingsSearch(const char* id, const SettingsSearchOptions& options = {})`, `void EndSettingsSearch()`, `bool SettingsSearchMatch(const char* label, const SettingsSearchMatchOptions& options = {})`, `int SettingsSearchHitCount(const char* id, const char* section)`, `SettingsSearchScope` | Search over settings rows with hit counts per section | planned |
| KeybindList | `bool KeybindList(const char* id, const KeybindListOptions& options = {})` (the shortcut registry), overload with `std::span<KeybindListItem>` | Editable table of key binds with conflict marks and reset | planned |
| PresetList | `PresetListResult PresetList(const char* id, int* active, std::span<const PresetItem> presets, const PresetListOptions& options = {})` | Preset manager with create, save, rename, duplicate, delete, import and export | planned |
| ThemeModeControl | `bool ThemeModeControl(const char* label, ThemeMode* mode, const ThemeModeControlOptions& options = {})` | Light, Dark and System segments that can apply the mode | planned |
| AccentPicker | `bool AccentPicker(const char* label, ImVec4* accent, const AccentPickerOptions& options = {})` | Accent swatches plus a custom color that can apply the accent | planned |
| UiScaleControl | `bool UiScaleControl(const char* label, float* scale, float min, float max, const UiScaleControlOptions& options = {})` | Stepped UI scale with optional Ctrl zoom keys | planned |
| InventoryGrid | `InventoryResult InventoryGrid(const char* id, int* selected, std::span<const InventoryItem> slots, const InventoryGridOptions& options = {})` | Slot grid with drag to move, swap, merge and split | planned |
| ItemTile | `bool ItemTile(const char* id, const InventoryItem& item, const ItemTileOptions& options = {})` | One item tile with count, category bar, lock and cooldown | planned |
| ItemSlot | `InventoryResult ItemSlot(const char* label, const InventoryItem& item, const ItemSlotOptions& options = {})` | Single slot that accepts inventory drops | planned |
| ItemDetailCard | `int ItemDetailCard(const char* label, ImageHandle image, const ItemDetailCardOptions& options = {})` | Detail card with stats and differences, tags and actions | planned |
| MediaCard | `bool MediaCard(const char* label, ImageHandle cover, const MediaCardOptions& options = {})` | Card with a cover image, title, detail and badges | planned |
| UserChip | `bool UserChip(const char* label, ImageHandle avatar, const UserChipOptions& options = {})` | Avatar, name and detail in a pill for the signed-in account | planned |
| ProfileSwitcher | `ProfileSwitcherAction ProfileSwitcher(const char* id, int* current, std::span<const AvatarItem> profiles, const ProfileSwitcherOptions& options = {})` | List of profiles with add and manage entries | planned |
| PreviewCanvas | `void BeginPreviewCanvas(const char* id, ImVec2 referenceSize, const PreviewCanvasOptions& options = {})`, `void EndPreviewCanvas()`, `PreviewCanvasScope` | Letterboxed preview of a target resolution with grid, safe area and guides | planned |
| CanvasElement | `bool CanvasElement(const char* label, CanvasElementState* element, const CanvasElementOptions& options = {})` | Anchored, draggable element inside a preview canvas | planned |
| LayerList | `LayerListResult LayerList(const char* id, int* selected, std::span<LayerItem> layers, const LayerListOptions& options = {})` | Ordered layer rows with visibility and lock toggles and reordering | planned |
| Onboarding | `[[nodiscard]] bool BeginOnboarding(const char* label, bool* open, int* step, int stepCount, const OnboardingOptions& options = {})`, `void EndOnboarding()`, `OnboardingScope` | Paged welcome panel with progress dots and skip | planned |
| Drag and drop | `[[nodiscard]] bool BeginDragSource(const char* type, const void* data, std::size_t size, const DragSourceOptions& options = {})`, `void EndDragSource()`, `[[nodiscard]] bool BeginDropTarget()`, `const ImGuiPayload* AcceptDrop(const char* type, const AcceptDropOptions& options = {})`, `void EndDropTarget()`, with scopes | Themed drag previews, drop indicators and edge scrolling on top of Dear ImGui drag and drop | planned |

## Usage

### Curve editor over fixed storage

```cpp
static ImVec2 points[16] = {{0.0f, 0.0f}, {1.0f, 1.0f}};
static int count = 2;
if (facet::CurveEditor("Response", points, &count, {0.0f, 0.0f}, {1.0f, 1.0f}, {.marker = input}))
    SaveCurve({points, static_cast<std::size_t>(count)});
```

### Property grid

```cpp
static char filter[64] = "";
facet::SearchField("##filter", filter, sizeof(filter));     // inputs.h
facet::BeginPropertyGrid("inspector", {.filter = filter});
if (facet::BeginPropertyCategory("Transform"))
{
    if (facet::BeginPropertyRow("Position", {.modified = position != defaultPosition}))
    {
        facet::VectorInput("##value", &position);              // inputs.h
        facet::EndPropertyRow();
    }
    facet::EndPropertyCategory();
}
facet::EndPropertyGrid();
```

### Console

```cpp
static std::string command;
if (facet::ConsoleView("console", &command, log, {.history = history, .commands = commandNames}))
{
    RunCommand(command);                   // the application's interpreter
    AddToHistory(command);
    command.clear();
}
```

### Settings search

The rows are the caller's settings rows (layout.h); the search decides which are submitted.

```cpp
facet::SearchField("##search", &query);
facet::BeginSettingsSearch("settings", {.filter = query.c_str()});
if (facet::SettingsSearchMatch("Vertical sync", {.keywords = "vsync tearing", .section = "Display"}))
{
    facet::BeginSettingsRow("Vertical sync");
    facet::Toggle("##value", &vsync);
    facet::EndSettingsRow();
}
facet::EndSettingsSearch();
// In the navigation, drawn before the page: hits of the last frame.
const int hits = facet::SettingsSearchHitCount("settings", "Display");
```

### Inventory grid

The widget never changes the slots; it reports what the user did.

```cpp
const facet::InventoryResult r = facet::InventoryGrid("bag", &selected, slots);
switch (r.action)
{
case facet::InventoryAction::Moved:
case facet::InventoryAction::Swapped: SwapSlots(r.source, r.from, r.to); break;
case facet::InventoryAction::Merged:
case facet::InventoryAction::Split: MoveCount(r.source, r.from, r.to, r.count); break;
case facet::InventoryAction::Activated: UseItem(r.to); break;
default: break;
}
```

`r.source` is the id of the widget the drag started in, so drags between two grids or from an
`ItemSlot` arrive with the source grid's id; compare it with `ImGui::GetID("bag")` taken in the
same id scope.

### Layer list

```cpp
const facet::LayerListResult r = facet::LayerList("layers", &selectedLayer, layerRows);
if (r.action == facet::LayerAction::Moved)
    MoveLayer(r.index, r.to);              // reorders the application's layers and layerRows
```

### Drag and drop between custom items

```cpp
facet::ItemTile("tile", item);
if (facet::BeginDragSource("app.item", &itemId, sizeof(itemId), {.previewText = item.label}))
    facet::EndDragSource();

facet::ImageView("Target", targetImage);
if (facet::BeginDropTarget())
{
    if (const ImGuiPayload* payload = facet::AcceptDrop("app.item"))
        Assign(*static_cast<const int*>(payload->Data));
    facet::EndDropTarget();
}
```

### Onboarding

```cpp
static bool showWelcome = firstRun;
static int step = 0;
if (facet::BeginOnboarding("Welcome", &showWelcome, &step, 3))
{
    DrawStep(step);                        // the application's step content
    facet::EndOnboarding();
}
```

## Planned, not declared yet

- **EnvelopeEditor**: a piecewise envelope (attack, hold, decay, sustain, release) with axis
  constrained vertices and draggable segment curvature.
- **FindReplaceBar**: a find bar with match count, next and previous, and an optional replace row.
- **ShapePreview**: a live preview of a parametric marker shape with handles for its parameters.
- **CardBoard**: scrolling columns of draggable cards with animated drop gaps.
- **SettingsSearch result jumps**: choosing a result scrolls to its row and flashes it.
- **SearchFilterBar sort menu**: a sort choice next to the chips; until then place a Dropdown
  beside the bar.
