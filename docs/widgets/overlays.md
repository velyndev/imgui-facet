# Overlays

Modals and confirm dialogs, popovers, context menus, tooltips, toasts, drawers, loading overlays,
the command palette, watermark overlays and the unsaved changes bar, declared in
`facet/widgets/overlays.h`. Every declaration follows [the API conventions](../api-conventions.md);
the header comments are the full reference. See [the widget index](../widgets.md) for the other
families.

A widget whose status is planned is declared and callable. Layers open nothing yet: while the caller
asks for one to be open, it draws one dashed placeholder line with its name in the page flow; layers
opened by input (context menus, tooltips) and toasts show nothing. Every planned widget returns the
neutral result (false, None, an invalid id) and never writes a value. Phase 2 implements each widget
and changes its status to ready.

How layers open and how results come back:

- Layers the caller opens (`BeginModal`, `ConfirmDialog`, `BeginPopover`, `BeginDrawer`,
  `CommandPalette`) take `bool* open`. The caller writes true to open; Facet writes false when the
  layer is dismissed. The call is made every frame, open or not. A null `open` shows nothing and
  returns the closed result.
- Layers opened by input (`BeginContextMenu`, tooltips) open themselves over the last item.
- Results come back as return values (`DialogResult`, a written index) or from query functions; there
  are no callbacks.
- Escape closes the topmost layer only, after an active edit was canceled.

## Widgets

### BeginModal

Status: planned

```cpp
[[nodiscard]] bool BeginModal(const char* label, bool* open, const ModalOptions& options = {});
void EndModal();
class ModalScope;
```

A dialog over a scrim with a focus trap, stacking for nested modals and focus return on close.

### ConfirmDialog

Status: planned

```cpp
DialogResult ConfirmDialog(const char* label, bool* open, const char* message,
                           const ConfirmDialogOptions& options = {});
```

A compact yes or no modal that returns Confirmed or Canceled, with a destructive tone and an
optional hold to confirm.

### BeginPopover

Status: planned

```cpp
[[nodiscard]] bool BeginPopover(const char* id, bool* open, const PopoverOptions& options = {});
void EndPopover();
class PopoverScope;
```

A floating card anchored to the last item or the pointer that flips and clamps to the display, with
light dismiss and an optional open on hover.

### BeginContextMenu

Status: planned

```cpp
[[nodiscard]] bool BeginContextMenu(const char* id, const ContextMenuOptions& options = {});
void EndContextMenu();
class ContextMenuScope;
```

A menu that opens on right click, the Menu key or Shift+F10 over an item or empty window space.

### ItemTooltip, BeginRichTooltip

```cpp
void ItemTooltip(const char* fmt, ...);
[[nodiscard]] bool BeginRichTooltip(const TooltipOptions& options = {});
void EndRichTooltip();
class RichTooltipScope;
```

A themed tooltip for the last item, called right after it every frame. `ItemTooltip` formats its
text (printf, cut at 1024 bytes, never allocated) in Footnote and the label color, wrapped at
`overlay.tooltipMaxWidth`; `BeginRichTooltip` takes any content until `EndRichTooltip`, wrapped at
`TooltipOptions::maxWidth` when set.

- **When it shows:** after the pointer rests on the item for `overlay.tooltipDelay`, below and after
  the pointer so the cursor never covers it, and after the same delay while the item has visible
  keyboard focus (below the item). Within `overlay.tooltipGrace` after a tooltip was visible, the
  next item shows its own at once. A mouse press hides it until the pointer leaves the item, a key
  press until the pointer moves. It flips and moves to stay inside the display.
- **When it never shows:** for a disabled item (`ImGui::BeginDisabled`); there is no option that
  changes this. Also not while another item is active or a popup above blocks the item, the same
  rule as `ImGui::IsItemHovered`.
- **Input:** a tooltip never takes the keyboard focus or a key, and a plain one lets every click
  through. With `interactive`, a rich tooltip takes the mouse and stays while the pointer moves
  into it (inside the box between where the pointer was and the tooltip), so its content can be
  hovered and clicked; it still takes no keyboard focus, and a click inside gives Dear ImGui's window
  focus to it.
- **Look and motion:** the elevated surface at level 1 with a hairline, the control radius, padding
  `spacing.sm` by `spacing.xs`; it fades in and out with the popup motion, at once under reduced
  motion.
- Text items without an id (`ImGui::TextUnformatted`) are named by their rectangle, as Dear ImGui
  names them for its own tooltips. One tooltip shows at a time.

Status: ready.

### ShowToast

Status: planned

```cpp
ToastId ShowToast(const char* text, const ToastOptions& options = {});
void UpdateToast(ToastId toast, const char* text, const ToastOptions& options = {});
bool ToastActionPressed(ToastId toast);
bool IsToastVisible(ToastId toast);
void DismissToast(ToastId toast);
void SetToastCorner(ScreenCorner corner);
```

Queued notifications with tones, one action, progress, auto-dismiss, hover pause and swipe dismiss,
drawn by `facet::EndFrame`.

### BeginDrawer

Status: planned

```cpp
[[nodiscard]] bool BeginDrawer(const char* label, bool* open, const DrawerOptions& options = {});
void EndDrawer();
class DrawerScope;
```

A panel or bottom sheet that slides in from an edge, with a scrim, a resize edge and flick to close.

### BeginLoadingOverlay

Status: planned

```cpp
void BeginLoadingOverlay(const char* id, const LoadingOverlayOptions& options = {});
void EndLoadingOverlay();
class LoadingOverlayScope;
```

Dims and blocks a region while it loads, with a spinner or a progress bar and a message.

### CommandPalette

Status: planned

```cpp
bool CommandPalette(const char* label, bool* open, int* picked, std::span<const ChoiceItem> commands,
                    const CommandPaletteOptions& options = {});
bool CommandPalette(const char* label, bool* open, int* picked, std::span<const char* const> commands,
                    const CommandPaletteOptions& options = {});
bool CommandPalette(const char* label, bool* open, ShortcutId* picked, const CommandPaletteOptions& options = {});
```

A floating launcher with fuzzy search over the app's commands or the registered shortcuts, recent
picks first.

### WatermarkOverlay

Status: planned

```cpp
void WatermarkOverlay(const char* id, std::span<const WatermarkField> fields, const WatermarkOptions& options = {});
```

A strip of name and value fields in a display corner that lets input through.

### UnsavedChangesBar

Status: planned

```cpp
UnsavedChangesAction UnsavedChangesBar(const char* id, int changeCount, const UnsavedChangesBarOptions& options = {});
```

A bar that slides in while settings have unsaved changes and returns Apply or Revert.

## Usage examples

A popover anchored to the button that opens it. A click on the anchor item does not count as a
click outside, so the button may toggle the flag: the first click opens the popover and the second
closes it, instead of the press closing it and the release opening it again.

```cpp
static bool filtersOpen = false;
if (ImGui::Button("Filters"))
    filtersOpen = !filtersOpen;
if (facet::BeginPopover("filters", &filtersOpen, {.placement = facet::PopoverPlacement::Below}))
{
    DrawFilterControls();
    facet::EndPopover();
}
```

A destructive action behind a confirm dialog. The dialog is called every frame; the result arrives
on the deciding frame:

```cpp
static bool confirmDelete = false;
if (ImGui::Button("Delete"))
    confirmDelete = true;
if (facet::ConfirmDialog("Delete the preset?", &confirmDelete, "The preset cannot be restored.",
                         {.confirmLabel = "Delete", .destructive = true}) == facet::DialogResult::Confirmed)
    DeletePreset();
```

A context menu on a row, and a rich tooltip on the same row:

```cpp
ImGui::Selectable(file.name);
if (facet::BeginContextMenu("file menu"))
{
    if (facet::MenuItem("Rename")) StartRename(file);
    if (facet::MenuItem("Delete", {.destructive = true})) Delete(file);
    facet::EndContextMenu();
}
if (facet::BeginRichTooltip())
{
    DrawFilePreview(file);
    facet::EndRichTooltip();
}
```

A toast with an action and a toast that reports progress. The action press arrives on the frame
after it happened:

```cpp
static facet::ToastId undoToast, exportToast;
if (Deleted())
    undoToast = facet::ShowToast("Item deleted", {.actionLabel = "Undo"});
if (facet::ToastActionPressed(undoToast))
    UndoDelete();

if (exportStarted)
    exportToast = facet::ShowToast("Exporting", {.duration = -1.0f, .progressBar = true, .progress = 0.0f});
if (facet::IsToastVisible(exportToast))
    facet::UpdateToast(exportToast, "Exporting", {.duration = -1.0f, .progressBar = true, .progress = Progress()});
```

A command palette over the registered shortcuts, opened with Ctrl+K. Picking an entry triggers its
shortcut, so the same handler runs as for the key chord:

```cpp
static bool paletteOpen = false;
facet::ShortcutId picked;
static const facet::ShortcutId openPalette = facet::RegisterShortcut("app.palette", ImGuiMod_Ctrl | ImGuiKey_K,
                                                                   {.label = "Command palette"});
facet::CommandPalette("Commands", &paletteOpen, &picked, {.openShortcut = openPalette});
```

An unsaved changes bar at the bottom of a settings page:

```cpp
switch (facet::UnsavedChangesBar("settings changes", ChangedSettingCount()))
{
case facet::UnsavedChangesAction::Apply: ApplySettings(); break;
case facet::UnsavedChangesAction::Revert: RevertSettings(); break;
default: break;
}
```

## Later

These widgets are planned for a later release and are not declared yet:

- **Coachmark**: an onboarding spotlight that dims everything but a target item, with a callout,
  step dots and a remembered seen state.
- **RadialMenu**: a pie menu that picks by direction from its center, opened by holding a key and
  chosen on release.
