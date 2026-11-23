# Overlays

Modals and confirm dialogs, popovers, context menus, tooltips, toasts, drawers, loading overlays,
the command palette, watermark overlays and the unsaved changes bar, declared in
`facet/widgets/overlays.h`. Every declaration follows [the API conventions](../api-conventions.md);
the header comments are the full reference. See [the widget index](../widgets.md) for the other
families.

Every layer is a level of one popup host, so they share their placement (next to the anchor,
flipped to the other side where there is no room, then moved into the display), the Escape order,
the focus that moves in and comes back, the open and close motion (at once under reduced motion),
the scrim and the surfaces.

Every floating layer (menu, popover, dialog, drawer, tooltip, toast, command palette, loading card,
unsaved changes bar) is its elevated surface with one `separator` hairline along its edge. In the
dark theme the elevated levels are near-black greys a step above the page, so the lighter grey and
the hairline show the layer's height where a shadow cannot darken black; no edge is a light grey.
Rows in menus and in the command palette follow the row look of the lists: `fill[2]` under the
pointer or on the current row, and the focus ring inside the row only after the keys moved there.

How layers open and how results come back:

- Layers the caller opens (`BeginModal`, `ConfirmDialog`, `BeginPopover`, `BeginDrawer`,
  `CommandPalette`) take `bool* open`. The caller writes true to open; Facet writes false when the
  layer is dismissed. The call is made every frame, open or not. A null `open` shows nothing and
  returns the closed result.
- Layers opened by input (`BeginContextMenu`, tooltips) open themselves over the last item.
- Results come back as return values (`DialogResult`, a written index) or from query functions; there
  are no callbacks.
- Escape closes the topmost layer only, after an active edit was canceled: a list opened from a
  popover closes before the popover, a confirm dialog before the modal it opened over.

## Widgets

### BeginModal

```cpp
[[nodiscard]] bool BeginModal(const char* label, bool* open, const ModalOptions& options = {});
void EndModal();
class ModalScope;
```

A dialog centered in the display over the scrim, `overlay.modalWidth` wide (`width` in units
otherwise, never wider than the display), on the elevated surface of level 3 with the window
radius. The visible label part is the title (Title3, cut with an ellipsis and shown in full on
hover); with `closeButton` a Plain, Small close button named `Strings::close` sits at the end of the
title row.

- **Content:** submitted between `BeginModal` and `EndModal`, with ids under the dialog. The dialog is
  as tall as its content; content taller than the display scrolls inside it with Facet's overlay
  bars, and the dialog stays inside the display.
- **Focus:** when it opens, the keyboard focus moves to the first item of the content (or the item
  that called `ImGui::SetItemDefaultFocus`); Tab and the arrows stay inside the dialog; when it
  closes, however it closes, the focus goes back to the item that had it before.
- **Input:** the page under the scrim takes no hover and no click. A press on the scrim never reaches
  what is under it.
- **Closing:** with `dismissible`, Escape and a click on the scrim close it and write `*open = false`;
  without, both do nothing and the caller closes it. The close button closes it as well.
- **Stacking:** a modal opened inside another modal's content (a `ConfirmDialog` too) stacks above
  it and Escape closes the top one first. Two modals opened at the same level of the page replace
  each other, as two popups of one level do, so open the second one from inside the first.
- **Motion:** it scales in from `motion.popupScaleFrom` and fades in over `motion.popupOpen`, and
  fades out over `motion.popupClose`; the scrim fades with it.

Status: ready.

### ConfirmDialog

```cpp
DialogResult ConfirmDialog(const char* label, bool* open, const char* message,
                           const ConfirmDialogOptions& options = {});
```

A modal (`BeginModal`, without a close button) with the title, the message in Body and
labelSecondary, and two buttons at the end of the last row: Cancel (Secondary, `cancelLabel` or
`Strings::cancel`) and the confirm button (Primary, or Destructive with `destructive`; `confirmLabel`
or `Strings::confirm`). Where the two do not fit side by side, they stack at full width.

- **Result:** `Confirmed` or `Canceled` on the frame it is decided, with `*open = false` and the focus
  back on the item that opened it; `None` on every other frame.
- **Keys:** the focus starts on the confirm button, or on Cancel when the dialog is destructive.
  Enter confirms from anywhere in the dialog unless it is destructive, must be held, or the focus is
  on Cancel; while the focus ring is not shown yet (the dialog opened from a click), Enter in a
  destructive dialog cancels. Escape and a click on the scrim cancel.
- **Hold to confirm:** with `holdToConfirm`, the confirm button must stay held by the mouse, Space or
  Enter for `motion.durationSlow`; a fill grows across it while it is held, and a shorter press does
  nothing. `Strings::holdToConfirm` says so above the buttons.
- Over a modal, call it inside the modal's content, so it stacks above it.

Status: ready.

### BeginPopover

```cpp
[[nodiscard]] bool BeginPopover(const char* id, bool* open, const PopoverOptions& options = {});
void EndPopover();
class PopoverScope;
```

A card next to the last item (or where the pointer was when it opened, with `PopoverAnchor::Mouse`),
on the blurred surface (the opaque tint where blur is not available), with the popover radius and
`spacing.md` of padding.

- **Placement:** the side of `placement`; `Auto` opens beside the anchor and centered on it, on the
  trailing side first and on the leading side when only that one has room, and takes below or above
  (whichever has more room) when the card fits on neither side. It flips to the opposite side where it
  does not fit, moves into the display, and follows its anchor while the anchor scrolls or moves.
  The arrow (`arrow`) points at the anchor's center from the facing edge, `overlay.arrowSize` long.
- **Size:** `width` in units, or the width of its content; as tall as its content, which scrolls with
  Facet's overlay bars once it outgrows the room on its side.
- **Input:** the page around it stays usable: it takes hover, the wheel and clicks. With
  `dismissible`, a press outside the card and its anchor closes it and still reaches what was
  pressed, and Escape closes it (after a list opened from its content, before the layers below it).
  A press on the anchor is the anchor's own, so a button that toggles the flag (`open = !open`)
  closes the popover on its second click. Without `dismissible`, only the caller closes it.
- **Focus:** the keyboard focus moves into it when it opens and goes back to the anchor when it
  closes by Escape or by the flag.
- **Open on hover:** with `openOnHover`, resting on the anchor for `overlay.submenuDelay` opens it
  without taking the focus, and leaving both the anchor and the card closes it again.
- Content ids are under the card. Motion: it grows from the side that faces the anchor.

Status: ready.

### BeginContextMenu

```cpp
[[nodiscard]] bool BeginContextMenu(const char* id, const ContextMenuOptions& options = {});
void EndContextMenu();
class ContextMenuScope;
```

A menu that opens on a right click (released over the target), or on the Menu key or Shift+F10 while
the target has keyboard focus, read through key ownership so they work without
`ImGuiConfigFlags_NavEnableKeyboard`. `ContextTarget::LastItem` is the item submitted right before
the call; `Window` is empty space of the current window. It opens at the pointer, or below the focused
item for the keys, at least `overlay.menuMinWidth` wide, flipped and moved into the display.

Its rows are menu rows (`facet::MenuItem`, `BeginMenu` of the navigation family, on the shared menu
row block): a row that runs its action closes the whole menu, a submenu opens after a short rest.
Escape closes one level per press and a click outside closes it; the focus moves to its first row
and comes back to the target.

Status: ready.

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

```cpp
ToastId ShowToast(const char* text, const ToastOptions& options = {});
void UpdateToast(ToastId toast, const char* text, const ToastOptions& options = {});
bool ToastActionPressed(ToastId toast);
bool IsToastVisible(ToastId toast);
void DismissToast(ToastId toast);
void SetToastCorner(ScreenCorner corner);
```

Queued notifications that `facet::EndFrame` draws in a corner of the display, after every page.

- **Queue:** 16 entries; the texts are copied (text up to 255 bytes, description 511, action label 63),
  so nothing is allocated and the caller's strings need to live only during the call. When the queue
  is full, the oldest toast is dropped at once. `ShowToast` outside a frame returns an invalid id.
- **Stack:** `overlay.toastMaxCount` toasts show at a time, newest nearest the corner,
  `overlay.toastStackGap` apart and `overlay.toastMargin` from the corner; the others wait and show
  in the order they were queued. A toast is as wide as its content on one line, so a short message
  is a short toast, and at most `overlay.toastWidth` wide (less on a narrow display), where its text
  wraps; one with a progress bar takes that width. All keep to the edge of their corner. A toast
  that leaves keeps its place while it fades, then the others move into the room on a spring, so the
  stack never jumps. A `WatermarkOverlay` strip in the half of the display the stack grows from that
  crosses the stack's column (on a narrow display) moves the stack past it, one stack gap away.
  Toasts stay in front of the page windows and behind open popups; under a modal dialog they show
  but take no input.
- **A toast:** the tone's icon (`Tone::Success`, `Warning`, `Destructive`, `Accent`; none for `Neutral`)
  or `icon`, the text in Body, `description` in Footnote, an optional progress bar
  (`progressBar`, `progress` or `kIndeterminate`), an optional action button (`actionLabel`) and a close
  button named `Strings::close`.
- **Timer:** `duration` seconds (0 for `overlay.toastDuration`; below 0 until dismissed), times
  `motion.scale`. It runs only while the toast shows, and pauses while the pointer is over it, while
  it has keyboard focus and while it is dragged. `UpdateToast` replaces the text and options of a
  waiting or shown toast and restarts its timer.
- **Dismissing:** the close button, Escape while the toast has focus, `DismissToast`, the timer, or a
  horizontal drag toward the near display edge past half the toast's width (or faster than
  `drag.flickSpeed`); a shorter drag springs back. The action button dismisses the toast too, and
  `ToastActionPressed` is true for it on the next frame.
- **Keyboard:** toast windows take part in Dear ImGui's window switching (Ctrl+Tab with
  `ImGuiConfigFlags_NavEnableKeyboard`): the newest toast is the first window it reaches, with the
  focus on its first button; Tab moves between its buttons. When a focused toast goes, the focus
  returns to the window under it.
- `IsToastVisible` is true from `ShowToast` until the toast is dismissed, also while it still waits in
  the queue. `SetToastCorner` applies from the next frame.
- **Motion:** a toast fades and slides in from the edge over `motion.popupOpen` and fades out over
  `motion.popupClose`; under reduced motion it appears, moves and leaves at once.

Status: ready.

### BeginDrawer

```cpp
[[nodiscard]] bool BeginDrawer(const char* label, bool* open, const DrawerOptions& options = {});
void EndDrawer();
class DrawerScope;
```

A panel flush with an edge of the display (`side`: the trailing or leading edge, a bottom sheet or the
top) over the whole of that side, `extent` units wide or tall (`overlay.drawerWidth` or
`overlay.drawerHeight` by default, at most nine tenths of the display), with the window radius on its
two inner corners. It slides in from its edge. The visible label part is the title row (Title3), with
a close button while it is dismissible; the content scrolls with Facet's overlay bars in the room
below.

- **Modal** (the default): a scrim behind it, the page takes no input, and the focus stays inside and
  comes back to the item that had it when it closes. **Not modal:** no scrim, the page beside it stays
  usable, a click there leaves it open; the focus moves into it when it opens and comes back when it
  closes while the focus is inside.
- **Closing** with `dismissible`: Escape (in its place of the Escape order), a click on the scrim, the
  close button, and a drag of the title row toward the edge past a third of the extent or faster than
  `drag.flickSpeed` (a shorter drag springs back). Without, only the caller closes it.
- **Resizing** with `resizable`: a band of `layout.dividerBand` along the inner edge drags the extent,
  between `layout.minRowControl` and nine tenths of the display side, kept under the id in units. The
  band is a mouse affordance and takes no keyboard focus.
- It follows a resize of the display at once. Content ids are under the drawer.

Status: ready.

### BeginLoadingOverlay

```cpp
void BeginLoadingOverlay(const char* id, const LoadingOverlayOptions& options = {});
void EndLoadingOverlay();
class LoadingOverlayScope;
```

Wraps a region: its content is always laid out, in one group, so the region keeps its size whether it
loads or not, and `EndLoadingOverlay` is always called. While `loading` is set, the content runs
inside `ImGui::BeginDisabled` (Facet widgets show their disabled look, nothing takes a click and Tab
passes over it), and over the region lie the scrim and a small card with the spinner, or a bar while
`progress` is 0 to 1, and the message (`Strings::loading` by default, cut with an ellipsis), so the
message keeps AA contrast over any content. The scrim and the card fade over
`motion.durationStandard`; the blocking starts and ends at once. After the call the region is the last
item.

Status: ready.

### CommandPalette

```cpp
bool CommandPalette(const char* label, bool* open, int* picked, std::span<const ChoiceItem> commands,
                    const CommandPaletteOptions& options = {});
bool CommandPalette(const char* label, bool* open, int* picked, std::span<const char* const> commands,
                    const CommandPaletteOptions& options = {});
bool CommandPalette(const char* label, bool* open, ShortcutId* picked, const CommandPaletteOptions& options = {});
```

A launcher centered `overlay.commandPaletteTop` below the top of the display,
`overlay.commandPaletteWidth` wide (narrower on a narrow display): the visible label part as a small
heading, a search field (`placeholder`, `Strings::search` by default) and the list of entries.

- **Rows:** while the field is empty, the entries in their order with a header before each run of
  equal `ChoiceItem::group`, and with `recentFirst` the recent picks first under `Strings::recent`;
  while it has text, the fuzzy matches best first, their matched characters highlighted. A row shows
  the icon, the label and `detail` at the end; in the shortcut form, the chord as key caps.
  Disabled entries show and cannot be picked. The list is as tall as its rows, `maxVisibleRows`
  (`list.popupRows` by default) at most and never past the display, and shows `emptyText`
  (`Strings::noResults`) when nothing matches.
- **Keys:** the field keeps the keyboard focus while the list takes Up, Down, Page Up and Page Down
  (Home and End stay with the caret); Enter or Tab picks the active row; Escape closes. A click on a row
  picks it; a click anywhere else in the palette gives the focus back to the field.
- **Result:** on a pick it writes `*picked` and `*open = false`, returns true, and the focus goes back
  to the item that had it; the shortcut form also calls `TriggerShortcut`. A null `picked` shows the
  palette but picks nothing.
- `openShortcut`, a registered shortcut, writes `*open = true` when it fires. Escape and a click
  outside close it. Recent picks (5) are kept under the id in the ini file, by the label (or the
  shortcut name), so they survive a change of the entries' order.

Status: ready.

### WatermarkOverlay

```cpp
void WatermarkOverlay(const char* id, std::span<const WatermarkField> fields, const WatermarkOptions& options = {});
```

A strip of fields in a corner of the display, `overlay.viewportMargin` from the corner of the work
area, drawn over every window and taking no input, so every click and key reaches what is under it.
Each field shows its icon and label in labelSecondary and its value in label (Footnote), on the
elevated surface of level 1 with a hairline, so the text keeps AA contrast over any content; a strip
wider than the display is cut with an ellipsis.

With `movable`, the strip takes the mouse while an overlay menu shows (`BeginOverlay`): a drag moves it
and the release snaps it to the corner nearest the pointer; with keyboard focus (Dear ImGui's window
switching) the arrow keys move it to the next corner that way. The corner is kept under the id and
saved in the ini file. Outside an overlay menu a movable strip stays where it is.

Status: ready.

### UnsavedChangesBar

```cpp
UnsavedChangesAction UnsavedChangesBar(const char* id, int changeCount, const UnsavedChangesBarOptions& options = {});
```

A bar pinned `spacing.md` above the bottom of the current window's visible area, over the page
content, as wide as that area less `spacing.md` on each side, on the elevated surface of level 1 with
the card radius. It slides up while `changeCount` is above 0 and down when it reaches 0
(`motion.durationStandard`), with the message (`message`, or `Strings::unsavedChange` /
`Strings::unsavedChanges` with `{count}` replaced; cut with an ellipsis and shown in full on hover) and
the Revert (Secondary) and Apply (Primary) buttons at the end.

- **Result:** `Apply` or `Revert` on the frame a button is activated; `applyShortcut` applies from
  anywhere while there are changes; Escape reverts while one of its buttons has the keyboard focus,
  after every layer in front of it. While it slides down it keeps the last message and takes no input.
- Tab reaches its buttons from the page. The window's content grows by the bar's room, so the last
  rows can still be scrolled above it. Call it after the window's other content, so it draws over the
  page's own child windows.

Status: ready.

## Usage examples

A popover anchored to the button that opens it. A click on the anchor item does not count as a
click outside, so the button may toggle the flag: the first click opens the popover and the second
closes it.

```cpp
static bool filtersOpen = false;
if (facet::Button("Filters"))
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
if (facet::Button("Delete"))
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
