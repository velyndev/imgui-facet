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

A grid of slots laid out with ItemGrid (`layout.h`): only the visible tiles are submitted, so ten
thousand slots cost what a screenful costs. The columns are `options.columns`, or as many slots of
`slotSize` (0: `metrics.slotSize`) as the width (`options.width`, 0 the available width) holds,
never more than there are slots, and the grid is as wide as its columns; the tiles
keep the slot size and shrink only when the set columns do not fit. The grid is as tall as its
rows, at most the visible height of the window it is in; further rows scroll inside it.

Each tile shows the item's image with the loading and failed states of the media family (or its
icon), the count, a lock chip, the cooldown as a pie in `scrim`, the category bar (`tint`,
`composites.inventoryTintHeight`) and the title in Caption, cut with an ellipsis and left out where
the tile is too small for it. Hover and keyboard focus show the full name and, for a locked item,
`Strings::locked`; an empty slot names itself `Strings::emptySlot`.

Selection goes through the list core: a click selects, Ctrl+click and Shift+click extend, the
arrows move in two dimensions, Escape clears. Selected tiles show the selection fill and an accent
ring inside the edge, so the selection does not rest on color alone. `*selected` is the focused
selected slot, else the first selected one, and is written when it changes; a value the caller
writes replaces the selection. Box selection is off, because a drag from a slot moves its item.
The grid is one Tab stop. Enter or a double click returns `Activated`.

Moving items: a drag (pointer, or Space on the focused slot for a keyboard drag that the arrows
carry and Space or Enter drops) onto an empty slot returns `Moved`, onto an item with the same
non-zero `stackKey` and room left `Merged`, onto any other item `Swapped`; Shift held at the drop
moves half the stack (`Split`, or `Merged` with half). A locked item cannot be dragged and refuses
drops (no highlight, the not-allowed cursor). Escape, a right click or a release outside any slot
cancels; the place an item came from stays faded while it is carried, and the ghost is the tile
itself at `drag.ghostOpacity`. While a keyboard drag is in progress the lines just above and below
the visible tiles also take it, so it can move on into rows scrolled out of view. Ctrl and an arrow
move the focused item to its neighbour at once by the same rules. The selection and the keyboard
focus follow a moved item. Drags cross into other grids, `InventorySlot` and `InventoryTile` of the
same payload type. The item's stack data is read from the source widget during the drag, so a
payload of this type from a custom source moves and swaps but never merges.

States: `Loading` draws skeleton slots, `Error` the error line, no slots the empty line. Disabled
(`ImGui::BeginDisabled`) draws the disabled look and takes no input. `readOnly` shows the selection
and writes nothing, starts no drag and accepts no drop. `InventoryItem::id` is not used yet: per-item
state (cooldown, count) is drawn from the item as passed. Status: ready.

### InventoryTile

```cpp
bool InventoryTile(const char* id, const InventoryItem& item, const InventoryTileOptions& options = {});
```

One tile outside a grid, a square of `slotSize` (a width set with `ImGui::SetNextItemWidth` keeps it
square), drawn as a grid tile. A click, Space or Enter activates it; hover and press show a wash
over it, the focus ring follows keyboard input, the item label is the tooltip. `selected` draws the
accent ring. With `dragSource` it is an inventory drag source (pointer, or Ctrl+Space for a keyboard
drag, since Space activates it); it is not a drop target. Status: ready.

### InventorySlot

```cpp
InventoryResult InventorySlot(const char* label, const InventoryItem& item, const InventorySlotOptions& options = {});
```

A single slot that takes inventory drops by the grid's rules (`to` is -1) and gives its item away
by a drag (Space picks it up for a keyboard drag). While a drag it takes hovers it, it shows the
accent target highlight. Empty, it draws `placeholderIcon` faintly and its visible label is the
tooltip. Enter or a double click returns `Activated` (`source` is the slot); a single click does
nothing. A keyboard drop leaves the focus on the slot. Disabled and a pushed
`ImGuiItemFlags_ReadOnly` take no drop and start no drag. Status: ready.

### InventoryDetailCard

```cpp
int InventoryDetailCard(const char* label, ImageHandle image, const InventoryDetailCardOptions& options = {});
```

A `Card` whose header is the visible label, with the image as an `ImageView` beside the description
(above it where the card is too narrow), the stats as a key-value list, the tags as badges and the
actions as buttons (the first one primary) in flows that wrap. A stat with a `compare` value shows
the difference after the value with its sign, in `success` when it is better and `destructive` when
it is worse (`lowerIsBetter` turns that around). An invalid image leaves the picture out. Width:
`metrics.cardWidth`, at most the available width. Returns the index of the action activated this
frame, or -1. Status: ready.

### MediaCard

```cpp
bool MediaCard(const char* label, ImageHandle cover, const MediaCardOptions& options = {});
```

A card surface with the cover as an `ImageView` (`mediaCoverAspect` of the width, with the loading
and failed states), the title in Headline cut to one line, the description in Footnote
`labelSecondary`, wrapped, and the badges in a flow. Narrower than `mediaCardHorizontalBelow` the
cover is a square beside the text. One Tab stop over the whole card: a click, Space or Enter
activates it; hover and press show a wash, `selected` an accent ring, a cut title shows in full on
hover and keyboard focus. Width: `metrics.cardWidth`, at most the available width. Status: ready.

### UserChip

```cpp
bool UserChip(const char* label, ImageHandle avatar, const UserChipOptions& options = {});
```

A pill with the avatar (initials of the name while there is no ready image, a skeleton while it
loads), the name and the description line; with a description the pill grows to hold both lines.
`showStatus` adds the status dot of `status` on the avatar. `compact` draws the avatar alone with the
name and description as its tooltip. A click, Space or Enter activates it; a name cut by a set width
shows in full on hover and focus. Status: ready.

### ProfileSwitcher

```cpp
ProfileSwitcherAction ProfileSwitcher(const char* id, int* current, std::span<const AvatarItem> profiles, const ProfileSwitcherOptions& options = {});
```

A field-like button with the current profile (avatar, name, description; `Strings::select` without
one) that opens a popover with the list. In the list the arrows move, Enter, Space or a click
switches and returns `Switched`, typing jumps to a name; the current profile has a check and a
semibold name, so it is marked by more than color. The add and manage entries return `Add` and
`Manage`. Switching or an entry closes the popover and puts the keyboard focus back on the button;
Escape closes it without switching. `Loading` shows skeleton rows, `Error` the error line, no
profiles the empty line; the entries stay usable. `readOnly` opens the list but switches nothing
and turns the entries off. Status: ready.

### Drag and drop

```cpp
[[nodiscard]] bool BeginDragSource(const char* type, const void* data, std::size_t size, const DragSourceOptions& options = {});
void EndDragSource();
[[nodiscard]] bool BeginDropTarget();
const ImGuiPayload* AcceptDrop(const char* type, const AcceptDropOptions& options = {});
void EndDropTarget();
```

Built on the drag and drop core. `BeginDragSource` makes the last item a source: a pointer drag
after `drag.startThreshold`, or a keyboard drag picked up with Ctrl+Space while the item has the
focus (Space alone stays the item's activation); the arrows carry it between targets of its type and
Space or Enter drops it, Escape cancels. The payload is copied at the start and may be at most 64
bytes; a larger one is a usage error and no drag starts. The ghost shows `previewText`, `icon` and
`image` on the elevated surface; content submitted between Begin and End appears in a floating layer
under the ghost. Disabled and read-only items never start a drag.

`BeginDropTarget` returns true while a drag of any type is in progress and the last item is visible,
not only while the drag hovers it: each `AcceptDrop` call declares the item as a target of its type,
so a keyboard drag can reach it. A drag of a type no `AcceptDrop` names is refused there (no
indicator, the not-allowed cursor). `AcceptDrop` draws the indicator (`Box`, or a line above or
below for `LineBefore` and `LineAfter`) while a drag of its type is over the item and returns the
payload on the drop, with `peek` also while it hovers. Disabled and read-only items take nothing.
Both pairs have their scope class (`DragSourceScope`, `DropTargetScope`); an End without its Begin is
reported. Status: ready.
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
The item lands in the widget that returned the result: slot `r.to` of a grid, or the slot widget
itself (`r.to` is -1).

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

A `TextField` with the search icon, the placeholder (`Strings::search` by default) and a clear
button, then the result count (`Strings::resultCount`, `resultCountOne` for one; hidden while
`resultCount` is negative) in Footnote labelSecondary, then the filter chips as a single-line
`ChipGroup`. The field is a text field rather than a `SearchField` because the declaration promises
the text field's Escape: it restores the query from activation and returns true, as in every text
field; the clear button empties the field. The query is written per keystroke. Each chip is a Tab
stop after the field; a click, Enter or Space flips its `active` member. The visible label part is
drawn in front of the field inside the width; the default width fills the available width. The
field takes the width the count and the chips leave, but never less than
`composites.searchFieldMinWidth`; when that does not fit, the chips go on a line of their own under
the field, and chips that do not fit their line move into the chip group's More menu. Chip entries
are keyed by `FilterChip::id` when it is set. After the call the item queries describe the query
edit. `readOnly` and `ImGui::BeginDisabled` write neither the query nor the chips. The sort choice is
not part of the bar yet; see "Planned, not declared yet". Status: ready.

### SettingsSearch

```cpp
void BeginSettingsSearch(const char* id, const SettingsSearchOptions& options = {});
void EndSettingsSearch();
bool SettingsSearchMatch(const char* label, const SettingsSearchMatchOptions& options = {});
int SettingsSearchHitCount(const char* id, const char* section);
class SettingsSearchScope;
```

Decides which of the caller's settings rows are submitted. `SettingsSearchMatch` returns true for a
row when no search is open, when the filter is empty or only spaces, or when the filter matches the
label, the description or the keywords, each on its own: with the fuzzy matcher (every query
character in order, case and Latin diacritics ignored) or, when `filterSettings` is set, with
`PassesFilter` (every query word, as `FilterField` matches). Every row it returns true for counts as
a hit, for its section and for the whole search, so an empty filter counts every row.
`SettingsSearchHitCount` reads the counts of the last completed frame (a frame hook closes them),
keyed by the search name with seed 0 and the section name; an unknown name or section gives 0, and a
search that was not submitted in the last frame counts 0.

Reveal: on the frame the filter changes to a non-empty text, the first row that matches is scrolled
into view (`ImGui::SetScrollHereY`) when it lies outside its window's visible part, and an
`accentSubtle` fill marks it from one frame later (the row's height is measured in the frame it is
submitted): it holds for `motion.feedbackHold` and fades out over `motion.durationSlow`, through the
animation store, so with reduced motion it disappears at once after the hold. The fill is drawn
behind the row's content. Escape belongs to the search field the caller uses: with `SearchField`
the first Escape clears the query (every row shows again) and the next one leaves the field.

Up to four searches can be open inside each other; a deeper one lets every row through. Begin pushes
nothing on the id stack and submits no item. Not done yet: a list of the hits that jumps to a chosen
row (see "Planned, not declared yet"). Status: ready.

### KeybindList

```cpp
bool KeybindList(const char* id, const KeybindListOptions& options = {});
bool KeybindList(const char* id, std::span<KeybindListItem> items, const KeybindListOptions& options = {});
```

A table of actions: a line of column headings (`Strings::action`, `primaryBind`, `alternateBind`),
then per action its label, a `KeybindCapture` for the primary chord and, with `alternate`, one for the
alternate chord, and a reset `IconButton` (`Strings::resetToDefault`) while the row differs from its
default. With `groups`, a heading in Headline starts each run of rows with the same group text. The
first overload lists the shortcut registry (`ShortcutInfo::label` and `group`) and writes with
`SetShortcutChords` and `ResetShortcut`; the second lists the caller's items and writes their `bind`
(the reset copies `defaultBind`, its mode included).

Every capture field is a Tab stop: a click or Enter listens for the next chord, Escape cancels
listening and keeps the chord, Backspace or Delete on a focused field clears it. A chord that another
listed action also uses marks both rows: the warning icon in the `warning` token and
`Strings::conflictWith` with the other action's label in labelSecondary after the label, as far as
the label column has room, and in full as the tooltip of the mark. The registry overload finds
conflicts with `FindChordConflict` (scopes included); the item overload compares the chords of the
other items.

`filter` keeps the rows whose label or chord text matches (fuzzy, or `PassesFilter` with
`filterSettings`); with no row left the list shows `emptyText`, else `Strings::noResults` while a
filter is set and `Strings::empty` otherwise. Loading shows skeleton rows, Error the error state.
Width: the available width by default; the chord columns are `composites.keybindColumnWidth` wide
and a narrow list shrinks the label column first (labels cut with an ellipsis and a tooltip), then
the chord columns. Height: the rows by default; a set `height` scrolls the rows under the headings.
`readOnly` shows the chords and never listens; `ImGui::BeginDisabled` the same with the disabled
colors. Returns true on the frame a binding was written. Status: ready.

### PresetList

```cpp
PresetListResult PresetList(const char* id, int* active, std::span<const PresetItem> presets, const PresetListOptions& options = {});
```

The presets as list rows (the list view block: one row per preset, the list row height of the
regular size) with the active preset on the selection pill and its name in semibold, then a "more"
`IconButton` (`Strings::more`) at the end of the line under the rows that opens the menu. A row
shows the name, cut with an ellipsis and a tooltip, and at its end `Strings::builtIn` and
`Strings::modified` in Footnote labelSecondary for `builtIn` and `modified` presets.

Keys: the rows are one Tab stop and the menu button another. The arrows, Home, End and typeahead
move the keyboard focus without choosing; Enter, a double click or a single click chooses (writes
`*active`, result `Selected`). F2 renames the focused preset in place with `InlineRename`: Enter or
focus loss commits (result `Rename` with `name`), Escape drops the edit. Delete asks with a
destructive `ConfirmDialog` (`Strings::deletePresetTitle`, `deletePresetMessage` with the name,
confirm `Strings::deleteAction`, focus on Cancel): confirming returns `Delete`, Escape or Cancel
returns nothing. Built-in presets are never renamed or deleted. After a rename, a create or a
declined delete the keyboard focus is back on its row.

Menu (on the row that last had the keyboard focus, else the active preset): `Strings::newPreset`
adds an inline field in a new row at the end, prefilled with the default name; Enter returns `Create`
with `name` and `index` -1, Escape adds nothing. Then `Strings::save` (`Save`), `rename`,
`duplicate` (`Duplicate`; the caller names the copy), with `importExport` `importAction` (`Import`,
index -1) and `exportAction` (`Export`), and `deleteAction` (the same confirmation). A name entered
for a new or renamed preset is trimmed; an empty one keeps the old name (a new preset takes
`Strings::newPreset`), and a name another preset already has, ASCII case ignored, gets the first free
number (`"Preset 3 2"`). `name` stays valid until the next call.

Width: the available width by default. Height: the rows by default (at least one row), or
`options.height`. States: Loading shows skeleton lines in the place of the rows, Error the error
state and an empty list the empty text (`emptyText`, else `Strings::empty`); the menu keeps
create and import in each of them. `readOnly` (and a null `active`) shows the list and the active
preset, takes no input that writes and has no menu button; `ImGui::BeginDisabled` the same with the
disabled colors. Not done yet: reordering presets (the result has no move to report). Status: ready.

### ThemeModeControl

```cpp
bool ThemeModeControl(const char* label, ThemeMode* mode, const ThemeModeControlOptions& options = {});
```

A `SegmentedControl` with the segments `Strings::themeLight`, `themeDark` and `themeSystem` (sun,
moon and monitor icons; at a narrow width the segments show only the icons and the names become
tooltips). The visible label part is drawn in front, inside the width; the default width fits the
segments, a set width gives equal segments. One Tab stop; Left, Right, Home and End move the choice
and write at once, as a click does. A click on the chosen segment writes nothing. With `apply` (the
default) the control calls `SetThemeMode` once on the frame it writes `*mode`, and the core blends
to the new colors over `motion.durationStandard` (at once while motion is reduced); every Facet
widget and open popup reads the blended theme each frame, so nothing keeps the old colors. Without
`apply` the caller applies the mode itself. `readOnly`, a null `mode` and `ImGui::BeginDisabled`
show the mode and write nothing; sizes Small, Regular and Large as for the segmented control.
Status: ready.

### AccentPicker

```cpp
bool AccentPicker(const char* label, bool* themeDefault, ImVec4* accent, const AccentPickerOptions& options = {});
```

The visible label part in front, then a `SwatchPalette` with one swatch per preset (each one control
height square) and, with `customColor`, a `ColorWell` for any other color. With a non-null
`themeDefault` the first swatch shows the current theme's own accent and stands for "each theme's
own accent" (its tooltip is `Strings::themeDefaultAccent`); the others show their preset color and
name it on hover. The chosen swatch shows the palette's selection ring and check mark; while a
custom color is the accent the well has the same ring instead, also when that color happens to equal
the first swatch. The palette is one Tab stop: Left, Right, Home and End move between swatches,
Enter or Space chooses; a click chooses. The well opens its color picker in a popover (click, Enter,
Space or Alt+Down); dragging there writes the accent live, and Escape in the popover restores the
color it opened with. Writes: choosing the theme default writes `*themeDefault = true` and leaves
`*accent`; any other choice writes `*accent` (its alpha kept) and `*themeDefault = false`. With
`apply` it then calls `ClearAccent` and `SetThemeMode(GetThemeMode())` for the theme default, or
`SetAccent` for a color, once per written value; `Theme::ApplyAccent` derives the hover, pressed,
on-accent and focus ring colors of every color the same way, preset or custom, so every offered
preset keeps 4.5:1 for text on the accent and 3:1 for the focus ring in both themes (a unit test
checks each preset). The default width fits the label, the swatches and the well; a narrower width
(`ImGui::SetNextItemWidth`) wraps the swatches into rows. At most 32 presets are shown. The preset
names show on hover only, not on keyboard focus (the palette does not report which swatch has the
focus). Status: ready.

### UiScaleControl

```cpp
bool UiScaleControl(const char* label, float* scale, float min, float max, const UiScaleControlOptions& options = {});
```

A `SliderStepped` with a stop every `step` from `min` to `max` (`max` is the last stop when the range
is not a whole number of steps; more than 63 steps use a continuous `Slider` that snaps to `step`),
the value shown through `Strings::percent`, then a zoom out and a zoom in `IconButton`
(`Strings::zoomOut`, `Strings::zoomIn`). The label is the slider's, in front inside the width; the
default width fills the available width. The slider takes the usual slider keys (Left and Right one
stop, Home and End); each button moves one stop and stops at the ends. `zoomInShortcut`,
`zoomOutShortcut` and `zoomResetShortcut` (registered shortcuts, see `core/input.h`) step or reset
to 1 wherever the focus is, and with `wheelZoom` each whole notch of Ctrl+wheel steps once. With
`apply` the control calls `SetUiScale`: a slider drag applies once, when the slider is released,
because applying a scale moves the track under the pointer; keys, buttons, shortcuts and the wheel
apply on the frame they write. `*scale` is written while dragging, as for every slider. `readOnly`
(which also turns the shortcuts and the wheel off), a null `scale`, `min == max` and
`ImGui::BeginDisabled` show the value and write nothing. Status: ready.

### Onboarding

```cpp
[[nodiscard]] bool BeginOnboarding(const char* label, bool* open, int* step, int stepCount, const OnboardingOptions& options = {});
void EndOnboarding();
class OnboardingScope;
```

A `Modal` (the scrim, the open and close motion, the focus trap) with the visible label part as its
title, the caller's content for step `*step`, and a footer: a progress dot per step
(`metrics.pageDotSize`; the current one in the accent, the others in `separatorOpaque`) with
`Strings::stepOf` beside them, so the step is never told by color alone, and at the end the
buttons `Strings::skip` (Plain; while `skippable` and not on the last step), `Strings::back` (from
the second step on) and `Strings::next`, or `Strings::finish` on the last step (Primary). A footer
too narrow for both puts the buttons on a line of their own. Next or Finish takes the keyboard
focus when a step opens, so Enter steps through; Tab and Shift+Tab move between the buttons inside
the panel. Back and Next write `*step`; Finish, Skip, Escape and a click on the scrim write
`*open = false` and leave `*step` as it is. `*step` is clamped to the steps, and `stepCount` below 1
counts as 1. The width is `overlay.modalWidth` unless `width` is set. A null `step` shows the first
step and the buttons write nothing to it. Status: ready.

### ConsoleView

```cpp
bool ConsoleView(const char* id, std::string* command, std::span<const LogEntry> entries, const ConsoleViewOptions& options = {});
bool ConsoleView(const char* id, std::string* command, int entryCount, LogGetter getter, void* user, const ConsoleViewOptions& options = {});
bool ConsoleView(const char* id, char* buffer, std::size_t bufferSize, std::span<const LogEntry> entries, const ConsoleViewOptions& options = {});
bool ConsoleView(const char* id, char* buffer, std::size_t bufferSize, int entryCount, LogGetter getter, void* user, const ConsoleViewOptions& options = {});
```

A `LogView` over the caller's entries (`timestamps`, `state`, `emptyText` and `errorText` go to it)
and a `TextField` for the command under it, with `placeholder` as its placeholder. The default width
fills the available width and the default height the available height; the log takes what the field
leaves. The log is virtualized by `LogView`, so a log of ten thousand lines costs a frame what a
short one does (a unit test counts the getter calls).

Typing writes the command per keystroke. Enter with text submits: the function returns true on that
frame, the text stays for the caller to run, add to its history and clear, and the field gets the
keyboard back in the next frame. Enter on an empty field submits nothing. Up and Down walk
`history` (oldest first): the first Up keeps the text typed so far and shows the newest entry; Up at
the oldest entry and Down at the typed text stay; Down past the newest entry gives the typed text
back. Tab completes the word before the caret from `commands`, ranked with the fuzzy matcher: with
no candidate a line above the field says `Strings::noResults` and the text stays; with one, the word
becomes it and a space follows; with several the word grows to the prefix they share and the line
lists them, and a Tab that cannot grow the word puts them in one per press. Typing hides the line.
Tab completes only while the field holds text, so on an empty field it moves the focus on; while it
holds text, Escape (which restores the text from activation) or Shift+Tab leave the field. Escape
gives the text back as in every text field. `readOnly` keeps the log usable and the field
read-only; `ImGui::BeginDisabled` disables both. Keeps the history position and the completion
under the id. Status: ready.

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

A gradient bar over a row of color stop markers, built on the multi-handle block with free handles
and on `ColorPicker`. The bar draws the gradient as the stored values read: each channel, alpha
included, interpolated linearly between neighbouring stops in gamma-encoded sRGB (the space the
color pickers draw in), the first and last color held out to the ends; with `alpha`, over the
checkerboard. The default width fills the available width; the default height is twice the control
height, of which the markers row takes at least the minimum hit target.

A click on the bar adds a stop with the color shown at that point and gives it the keyboard focus.
A press on a marker grabs it; a press beside the markers moves the nearest one there. A stop
dragged past another passes it: the widget keeps the caller's stops sorted by position and carries
the moved stop's drag, focus and open popover along. A marker dragged more than
`composites.stopRemoveDistance` above or below the editor fades and is removed on release. Each
marker is a Tab stop: the arrows move it (Shift finer, Ctrl coarser), Home and End send it to the
ends, Insert adds a stop half way to its right neighbour (its left one for the last stop), Delete or
Backspace removes it, and Escape during a drag or key press puts every stop back. The last stop is
never removed. A click on a marker that does not move it, or Enter or Space on a focused one, opens
a popover with the stop's `ColorPicker`, its position in percent (`NumberInput`, `Strings::position`)
and a `Strings::remove` button, titled `Strings::colorStop`; the stop it edits keeps an accent
outline. Escape closes it and restores the color from before it opened; a click outside keeps the
edit.

Adding needs room in the caller's storage: a full span takes no new stop, and the vector overload
grows its vector (the only time it allocates). At most 16 stops (the multi-handle block's limit)
are shown and edited; stops beyond that in the caller's storage are left as they are. `readOnly`
shows the stops, lets them take the focus and opens the popover with a read-only picker, and writes
nothing. Disabled draws the gradient and the markers at the pickers' disabled opacity. The label is
the tooltip on hover and keyboard focus. Status: ready.

### PropertyGrid

```cpp
void BeginPropertyGrid(const char* id, const PropertyGridOptions& options = {});
void EndPropertyGrid();
[[nodiscard]] bool BeginPropertyCategory(const char* label, const PropertyCategoryOptions& options = {});
void EndPropertyCategory();
[[nodiscard]] bool BeginPropertyRow(const char* label, const PropertyRowOptions& options = {});
void EndPropertyRow();
```

An inspector of labelled rows in collapsible categories, with a filter; every pair has its scope
class. Each row is a settings row of the layout family (`BeginSettingsRow`): the visible label in
the label column, the modified mark after it, and the caller's control (a `Toggle`, `NumberInput`,
`Slider`, `Dropdown`, `ColorWell`, `TextField`, `VectorInput` or any other widget, with `"##value"`)
in the control column; that control reports its own edits, one change per edit. A row whose label
is cut ends in an ellipsis; `tooltip` shows on hover of the label and while the row's control has
the visible keyboard focus, with the full label above it.

All rows of a grid share one label column, also rows inside nested categories (their column is
narrowed by the indent), so every control starts at one x. The column is `labelWidth`, or the
`labelColumn` token, until the user drags its edge between the labels and the controls; the
dragged column is kept under the grid id and saved in the ini file. A drag never makes it narrower
than `composites.propertyLabelMinWidth` nor so wide that the rows would stack. The edge is the last
Tab stop of the grid: the arrows move it (Shift finer, Ctrl coarser), Home and End go to the
narrowest and widest column, Escape during a drag puts it back, and a double click returns it to
the default. It shows its divider only while hovered, dragged or focused. In a region too narrow
for a label column beside the controls, every row puts its control under the label and there is no
edge.

A category header is a Row disclosure (`BeginDisclosure`): a click, Enter or Space toggles it,
Left closes and Right opens it, and its open state is kept under the label and saved in the ini
file (`defaultOpen` applies the first time). The content opens and closes with an animated height.

The filter matches a row's visible label or its `keywords`, fuzzy, or by the `PassesFilter` rules
when `filterSettings` is set. Rows that do not match are skipped (`BeginPropertyRow` returns
false). While a filter is set, every category returns true so its rows can be matched, and its
header shows, open and not toggled by input, while one of its rows matched in the last frame it
was shown; a category whose rows start to match therefore shows its header one frame after them.
When no row matches, the grid shows `Strings::noResults` as a compact `EmptyState`. Disabled
(`ImGui::BeginDisabled`) reaches the headers, the edge and the caller's controls. The grid keeps
no per-frame allocation once shown. Status: ready.

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
