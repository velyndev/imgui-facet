# Composites

Widgets built from the other families and the drag and drop core: inventory grids, item tiles,
slots and detail cards, media cards, user chips and profile switchers, and the public drag and drop
calls; search and filter bars, settings search, key bind and preset lists, theme controls,
onboarding and console views; editors for curves, easing and gradients, property grids, preview
canvases and layer lists.

Header: `facet/widgets/composites.h`. It includes `display.h` (for `LogEntry`), `lists.h` (for
`ReorderMove`) and `media.h` (for `AvatarItem`). Every declaration follows
[the API conventions](../api-conventions.md), and the header comments are the full reference. The
demo gallery shows them on three pages: Inventory and cards, Search and app controls, and Editors
and canvas.

Each entry ends with its status. A **planned** widget is declared, compiles and can be called: a
call draws a placeholder of the widget's default size and returns the neutral result; the drag and
drop calls return false and null. A **ready** one works as its header describes.

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

## Inventory, cards and people

### InventoryGrid

```cpp
InventoryResult InventoryGrid(const char* id, int* selected, std::span<const InventoryItem> slots, const InventoryGridOptions& options = {});
```

Slot grid with drag to move, swap, merge and split. Status: planned.

### InventoryTile

```cpp
bool InventoryTile(const char* id, const InventoryItem& item, const InventoryTileOptions& options = {});
```

One item tile with count, category bar, lock and cooldown. Status: planned.

### InventorySlot

```cpp
InventoryResult InventorySlot(const char* label, const InventoryItem& item, const InventorySlotOptions& options = {});
```

Single slot that accepts inventory drops. Status: planned.

### InventoryDetailCard

```cpp
int InventoryDetailCard(const char* label, ImageHandle image, const InventoryDetailCardOptions& options = {});
```

Detail card with stats and differences, tags and actions. Status: planned.

### MediaCard

```cpp
bool MediaCard(const char* label, ImageHandle cover, const MediaCardOptions& options = {});
```

Card with a cover image, title, description and badges. Status: planned.

### UserChip

```cpp
bool UserChip(const char* label, ImageHandle avatar, const UserChipOptions& options = {});
```

Avatar, name and description in a pill for the signed-in account. Status: planned.

### ProfileSwitcher

```cpp
ProfileSwitcherAction ProfileSwitcher(const char* id, int* current, std::span<const AvatarItem> profiles, const ProfileSwitcherOptions& options = {});
```

List of profiles with add and manage entries. Status: planned.

### Drag and drop

```cpp
[[nodiscard]] bool BeginDragSource(const char* type, const void* data, std::size_t size, const DragSourceOptions& options = {});
void EndDragSource();
[[nodiscard]] bool BeginDropTarget();
const ImGuiPayload* AcceptDrop(const char* type, const AcceptDropOptions& options = {});
void EndDropTarget();
```

Themed drag previews, drop indicators and edge scrolling on top of Dear ImGui drag and drop;
both pairs have their scope class (`DragSourceScope`, `DropTargetScope`). Status: planned.

### Usage of the inventory, cards and drag and drop

#### Inventory grid

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

`r.source` is the id of the grid, tile or slot the drag started in; compare it with
`ImGui::GetID` of that widget's id or label (`ImGui::GetID("bag")`), taken in the same id scope.

#### Drag and drop between custom items

An `InventoryTile` is an inventory drag source by default; one item has one drag source, so the tile
turns its own off before it becomes the source of an application payload.

```cpp
facet::InventoryTile("tile", item, {.dragSource = false});
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

## Search, presets and app controls

### SearchFilterBar

```cpp
bool SearchFilterBar(const char* label, std::string* query, std::span<FilterChip> filters, const SearchFilterBarOptions& options = {});
bool SearchFilterBar(const char* label, char* buffer, std::size_t bufferSize, std::span<FilterChip> filters, const SearchFilterBarOptions& options = {});
```

Search field with toggleable filter chips and a result count. Status: planned.

### SettingsSearch

```cpp
void BeginSettingsSearch(const char* id, const SettingsSearchOptions& options = {});
void EndSettingsSearch();
bool SettingsSearchMatch(const char* label, const SettingsSearchMatchOptions& options = {});
int SettingsSearchHitCount(const char* id, const char* section);
class SettingsSearchScope;
```

Search over settings rows with hit counts per section. Status: planned.

### KeybindList

```cpp
bool KeybindList(const char* id, const KeybindListOptions& options = {});
bool KeybindList(const char* id, std::span<KeybindListItem> items, const KeybindListOptions& options = {});
```

Editable table of key binds with conflict marks and reset; the first overload edits the shortcut
registry. Status: planned.

### PresetList

```cpp
PresetListResult PresetList(const char* id, int* active, std::span<const PresetItem> presets, const PresetListOptions& options = {});
```

Preset manager with create, save, rename, duplicate, delete, import and export. Status: planned.

### ThemeModeControl

```cpp
bool ThemeModeControl(const char* label, ThemeMode* mode, const ThemeModeControlOptions& options = {});
```

Light, Dark and System segments that can apply the mode. Status: planned.

### AccentPicker

```cpp
bool AccentPicker(const char* label, bool* themeDefault, ImVec4* accent, const AccentPickerOptions& options = {});
```

Accent swatches with a theme default and a custom color that can apply the accent. Status: planned.

### UiScaleControl

```cpp
bool UiScaleControl(const char* label, float* scale, float min, float max, const UiScaleControlOptions& options = {});
```

Stepped UI scale with optional Ctrl zoom keys. Status: planned.

### Onboarding

```cpp
[[nodiscard]] bool BeginOnboarding(const char* label, bool* open, int* step, int stepCount, const OnboardingOptions& options = {});
void EndOnboarding();
class OnboardingScope;
```

Paged welcome panel with progress dots and skip. Status: planned.

### ConsoleView

```cpp
bool ConsoleView(const char* id, std::string* command, std::span<const LogEntry> entries, const ConsoleViewOptions& options = {});
bool ConsoleView(const char* id, std::string* command, int entryCount, LogGetter getter, void* user, const ConsoleViewOptions& options = {});
bool ConsoleView(const char* id, char* buffer, std::size_t bufferSize, std::span<const LogEntry> entries, const ConsoleViewOptions& options = {});
bool ConsoleView(const char* id, char* buffer, std::size_t bufferSize, int entryCount, LogGetter getter, void* user, const ConsoleViewOptions& options = {});
```

Log view with a command field, history and Tab completion. Status: planned.

### Usage of the search and app controls

#### Settings search

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
// In the navigation, drawn before the page and in another window: hits of the last frame.
const int hits = facet::SettingsSearchHitCount("settings", "Display");
```

The search name `"settings"` is global, not hashed on the id stack, so the navigation finds the
counts from any window. Two searches open in the same frame need different names.

#### Accent picker in saved settings

"Each theme's own accent" is not one color, so the picker reports it as a value of its own. An
application that saves settings stores both values and applies them itself.

```cpp
if (facet::AccentPicker("##accent", &settings.themeAccent, &settings.accent, {.apply = false}))
{
    if (settings.themeAccent)
        facet::ClearAccent();              // each theme shows its own accent again
    else
        facet::SetAccent(settings.accent);
    SaveSettings();
}
```

#### Onboarding

```cpp
static bool showWelcome = firstRun;
static int step = 0;
if (facet::BeginOnboarding("Welcome", &showWelcome, &step, 3))
{
    DrawStep(step);                        // the application's step content
    facet::EndOnboarding();
}
```

#### Console

```cpp
static std::string command;
if (facet::ConsoleView("console", &command, log, {.history = history, .commands = commandNames}))
{
    RunCommand(command);                   // the application's interpreter
    AddToHistory(command);
    command.clear();
}
```

## Editors and canvas

### CurveEditor

```cpp
bool CurveEditor(const char* label, std::span<ImVec2> points, int* count, ImVec2 min, ImVec2 max, const CurveEditorOptions& options = {});
bool CurveEditor(const char* label, std::vector<ImVec2>* points, ImVec2 min, ImVec2 max, const CurveEditorOptions& options = {});
```

Response curve through draggable, addable control points. Status: planned.

### BezierEditor

```cpp
bool BezierEditor(const char* label, CubicBezier* curve, const BezierEditorOptions& options = {});
```

Cubic easing editor with two handles, presets and numeric fields. Status: planned.

### GradientEditor

```cpp
bool GradientEditor(const char* label, std::span<GradientStop> stops, int* count, const GradientEditorOptions& options = {});
bool GradientEditor(const char* label, std::vector<GradientStop>* stops, const GradientEditorOptions& options = {});
```

Gradient bar with draggable color stops. Status: planned.

### PropertyGrid

```cpp
void BeginPropertyGrid(const char* id, const PropertyGridOptions& options = {});
void EndPropertyGrid();
[[nodiscard]] bool BeginPropertyCategory(const char* label, const PropertyCategoryOptions& options = {});
void EndPropertyCategory();
[[nodiscard]] bool BeginPropertyRow(const char* label, const PropertyRowOptions& options = {});
void EndPropertyRow();
```

Inspector of labelled rows in collapsible categories with a filter; every pair has its scope
class. Status: planned.

### PreviewCanvas

```cpp
void BeginPreviewCanvas(const char* id, ImVec2 referenceSize, const PreviewCanvasOptions& options = {});
void EndPreviewCanvas();
class PreviewCanvasScope;
```

Letterboxed preview of a target resolution with grid, safe area and guides. Status: planned.

### CanvasElement

```cpp
bool CanvasElement(const char* label, CanvasElementState* element, const CanvasElementOptions& options = {});
```

Anchored, draggable element inside a preview canvas. Status: planned.

### LayerList

```cpp
LayerListResult LayerList(const char* id, int* selected, std::span<LayerItem> layers, const LayerListOptions& options = {});
```

Ordered layer rows with visibility and lock toggles and reordering. Status: planned.

### Usage of the editors and canvas

#### Curve editor over fixed storage

```cpp
static ImVec2 points[16] = {{0.0f, 0.0f}, {1.0f, 1.0f}};
static int count = 2;
if (facet::CurveEditor("Response", points, &count, {0.0f, 0.0f}, {1.0f, 1.0f}, {.marker = input}))
    SaveCurve({points, static_cast<std::size_t>(count)});
```

#### Property grid

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

#### Layer list

```cpp
const facet::LayerListResult r = facet::LayerList("layers", &selectedLayer, layerRows);
if (r.action == facet::LayerAction::Moved)
    facet::ApplyReorderMove(layers, r.move);  // the application's layers, in the order of layerRows
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
