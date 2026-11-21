# Navigation

The sidebar, tab bars, menu bars, menus and menu items, breadcrumbs, pagination, step indicators,
wizards and the shortcut sheet, declared in `facet/widgets/navigation.h`. Every declaration follows
[the API conventions](../api-conventions.md); the header comments are the full reference. See
[the widget index](../widgets.md) for the other families. Shortcuts are registered and routed by the
registry in `facet/core/input.h`; menu items, the shortcut sheet and the command palette show them.

Each entry below ends with the widget's status. A **ready** widget works as its header describes.

The bars of this family (tab bar, sidebar, menu bar, breadcrumbs, pagination, step indicator) are
one Tab stop each: Tab and Shift+Tab reach the entry that is selected or was focused last, and the
arrow keys, Home and End move inside the bar. The focus ring shows after keyboard input only. Every
widget leaves Dear ImGui's id, group, clip rect, item flag and window stacks as it found them, and
the item queries after a call describe the whole widget.

## Widgets

### BeginSidebar, SidebarItem, BeginSidebarSection

```cpp
[[nodiscard]] bool BeginSidebar(const char* id, const SidebarOptions& options = {});
void EndSidebar();
bool SidebarItem(const char* label, Icon icon, const SidebarItemOptions& options = {});
[[nodiscard]] bool BeginSidebarSection(const char* label, const SidebarSectionOptions& options = {});
void EndSidebarSection();
class SidebarScope;
class SidebarSectionScope;
```

A navigation column on the `bgSecondary` surface with the card radius.

- **Column.** A scroll area of the scroll core (overlay bars, edge fades on `bgSecondary`), `width`
  wide (0: `containers.sidebarWidth`) and `height` tall (0: the available height). In the rail mode
  it is `containers.railWidth` wide. The width springs (`motion.standard`) when the mode changes;
  with reduced motion it jumps.
- **Modes.** `Auto` takes the expanded form in an Expanded width class of the available width and
  the rail in a Regular or Compact one. `Hidden` draws nothing and returns false.
- **Items.** One list row tall (`list.rowHeight`): the icon, the label (cut with an ellipsis and shown
  in full on hover and keyboard focus) and the count `badge` at the end, the grey wash for
  `Tone::Neutral` and the tone's solid fill otherwise. In the rail the icon is centered, the badge
  sits on the icon's top-right corner and the label is the tooltip on hover and keyboard focus. The
  item the caller marks `selected` sits on the selection pill (`accentSubtle`, label in semibold),
  which slides to the next selected item with the spring of `SegmentedControl`, across sections.
  `SidebarItem` returns true on a click, Enter or Space; the caller then selects it.
- **Sections.** A header in Footnote and `labelSecondary`. With `collapsible` the header is a row
  of its own with a chevron, in the roving order with the items: a click, Enter or Space toggles it,
  Left closes and Right opens it; the open state starts at `defaultOpen` and is kept under the
  label. In the rail a section is a hairline and its items always show, because the header cannot be
  operated there. Sections push their label on the id stack, so items in two sections may share a
  label.
- **Keys.** The sidebar is one Tab stop (the focused item, else the selected one, else the first);
  Up and Down move the focus, Home and End go to the ends.
- **Disabled.** `ImGui::BeginDisabled` around an item gives its disabled look; it is skipped by the
  arrows and not a Tab stop.

Status: ready.

### TabBar

```cpp
bool TabBar(const char* id, int* index, std::span<const ChoiceItem> tabs, const TabBarOptions& options = {});
bool TabBar(const char* id, int* index, std::span<const char* const> tabs, const TabBarOptions& options = {});
int TabBarClosedTab();
ReorderMove TabBarMovedTab();
```

A strip of tabs at one control height (`size`), the available width by default; the caller draws
the content of the selected tab.

- **Tabs.** Each tab is its icon, its label and the item `detail` as a badge, plus `paddingX` on
  both sides, between `tabs.minWidth` and `tabs.maxWidth`; a longer label is cut with an ellipsis and
  shown in full on hover and keyboard focus. Tabs start at the left edge, `spacing.xs` apart. A tab
  whose `ChoiceItem::disabled` is set cannot be chosen, closed or moved.
- **Selection.** `TabStyle::Pill` puts the selection pill (`accentSubtle`) behind the selected tab;
  `TabStyle::Underline` draws a `tabs.underlineHeight` line in the focus ring color under it, over a
  hairline under the whole strip. Both slide with the spring of `SegmentedControl`, rest on whole
  pixels, follow a change of width or scale with the spring and jump with reduced motion. The
  selected label is semibold.
- **Keys.** One Tab stop (the focused tab, else the selected one); Left and Right choose the
  neighbouring enabled tab and move the focus with it, Home and End the first and last.
  Ctrl+Page Down and Ctrl+Page Up choose the next and previous tab, wrapping, while the keyboard
  focus is anywhere in the window: the tab bar that held the focus last takes the keys, else the
  first tab bar of the window. Ctrl+Tab is not used: Dear ImGui always claims it for its window
  switcher.
- **Closing.** With `closable` every tab has a close button (named by `Strings::close` in its
  tooltip, a mouse target that is not a Tab stop); the close button, a middle click on the tab, and
  Delete or Backspace on the focused tab report it through `TabBarClosedTab`, and the caller removes
  it. `*index` is written for the list without that tab: the selected tab keeps its place, and when
  it is the one closed, its next enabled neighbour (the following tab, else the previous one) is
  selected and takes the keyboard focus if the closed tab had it. Closing the last tab writes -1.
- **Reordering.** With `reorderable` a tab is dragged to a new place, the other tabs moving aside;
  or the focused tab is picked up with Ctrl+Space (Space still chooses), moved with Left and Right,
  dropped with Space or Enter and put back with Escape. `TabBarMovedTab` reports the move for
  `ApplyReorderMove`, and `*index` already follows the selected tab to its new place. A tab can be
  dropped only on its own bar.
- **Overflow.** Tabs that do not fit move into a "more" menu after the last visible tab, the last
  ones first. The selected tab always stays: when even it does not fit beside the button, it gives up
  width and cuts its label. The menu lists the hidden tabs with a check mark on the selected one;
  a row chooses its tab, which shows in the strip from then on. The button carries the pill's still
  look while the selected tab is in the menu. The arrow keys reach the hidden tabs too.
- **Results.** Returns true when another tab was chosen or `*index` was written. A null `index`
  draws the bar read-only.

Status: ready.

### BeginMenuBar

```cpp
[[nodiscard]] bool BeginMenuBar(const char* id, const MenuBarOptions& options = {});
void EndMenuBar();
class MenuBarScope;
```

A row of menu titles at the regular control height, the available width by default, with a
hairline under it; the titles are `BeginMenu` calls between the two.

- **Opening.** A title opens its menu on the press, Enter, Space or Down. Once a menu of the bar is
  open, the pointer moving onto another title (or onto the "more" button) opens that one instead.
  In an open menu Left and Right open the previous and next title's menu, wrapping, unless Right
  opens the submenu of the focused row.
- **Keys.** The bar is one Tab stop; Left and Right move between the titles. A press and release of
  Alt alone, while the bar's window has the focus and no popup is open, moves the focus to the first
  enabled title with the focus ring; with two bars in one window the first takes it.
- **Overflow.** Titles that do not fit move, from the end, into a "more" menu after the last visible
  title. A row there opens that menu in place of the "more" menu, so a hidden menu keeps all its rows
  and submenus.
- **Out of sight.** A bar scrolled out of view with no menu open returns false and lays out one item
  of its size.

Status: ready.

### BeginMenu

```cpp
[[nodiscard]] bool BeginMenu(const char* label, const MenuOptions& options = {});
void EndMenu();
class MenuScope;
```

In a menu bar a title (an `icon` replaces the label, which becomes the tooltip); inside an open
menu, a context menu or a popover a submenu row with a chevron (the `icon` before the label);
anywhere else a title on its own that opens its menu below it.

- **Menus.** A popup level of the popup host below its title, at least `overlay.menuMinWidth`
  wide, on the elevated surface; it takes the keyboard focus when it opens and gives it back to the
  title when Escape closes it. Up and Down move between the rows through Dear ImGui's navigation
  (`ImGuiConfigFlags_NavEnableKeyboard`).
- **Submenus.** A submenu row opens its submenu to the side with room after the pointer rests on it
  for `overlay.submenuDelay`, and at once on a click, Enter, Space or Right (these move the focus
  into it). The submenu stays open while the pointer travels toward it across other rows (the aim
  test of the popup host); resting on another row of the menu closes it. Left inside a submenu closes
  it and the focus goes back to its row; Escape closes one level per press.
- **Typeahead.** Typing while a menu has the focus moves it to the next enabled row whose label
  starts with the typed text (ASCII case-insensitive, a pause of `list.typeaheadTimeout` starts a new
  text); the same letter again cycles through the rows with that start. Disabled rows are skipped
  by the arrows and by typeahead.

Status: ready.

### MenuItem, MenuCheckItem, MenuRadioItem

```cpp
bool MenuItem(const char* label, const MenuItemOptions& options = {});
bool MenuCheckItem(const char* label, bool* checked, const MenuItemOptions& options = {});
bool MenuRadioItem(const char* label, int* selection, int value, const MenuItemOptions& options = {});
```

Rows of the menu level being submitted, the level's full width at the regular control height: the
leading slot (the `icon` in `labelSecondary`, or the mark), the label, the shortcut right-aligned in
`labelSecondary` and the hover fill. A row activates on a click, Enter or Space and closes the whole
menu chain.

- **Marks.** `MenuCheckItem` shows a check mark while `*checked` and flips it; `MenuRadioItem` shows a
  round dot while `*selection == value` and writes `value`. Both keep the leading slot when off, so
  the rows of a menu line up. The marks take `ChoiceCheckColor` (`accentText`). A null value pointer
  draws the row disabled.
- **Shortcuts.** `shortcut` is shown as text (`ChordToText`) and routed by the caller. With `command`
  the registered chord of that shortcut is shown instead, activation calls `TriggerShortcut`, so the
  shortcut's `ShortcutPressed` handler runs the action in the next frame exactly as when its chord
  is pressed with the menu closed, and a disabled shortcut draws the row disabled.
- **Tone.** `destructive` draws the label and the icon in the destructive color.
- **Outside a menu** the row draws and activates in the current window, so menu rows can also be
  laid out inline; its activation then closes nothing.
- **Disabled** rows (`ImGui::BeginDisabled`) draw in `labelTertiary` and do nothing.

Status: ready.

### Breadcrumbs

```cpp
int Breadcrumbs(const char* id, std::span<const ChoiceItem> path, const BreadcrumbsOptions& options = {});
int Breadcrumbs(const char* id, std::span<const char* const> path, const BreadcrumbsOptions& options = {});
```

A path trail at one control height (`size`), the available width by default. Segments are their
icon and label with chevrons between them; earlier segments are links in `labelSecondary` (label
with the hover fill), the last one is the current place in `label` and not a link.

- **Activation.** A click, Enter or Space on a segment returns its index; the caller navigates.
  Disabled segments (`ChoiceItem::disabled`) do nothing.
- **Keys.** One Tab stop; Left and Right, Home and End move between the links and the "more" button.
- **Overflow.** Middle segments that do not fit collapse, from the second one on, into a "more"
  button in their place whose menu lists them; a row there returns its index. The first and the last
  segment stay longest, and the last cuts its label (with a tooltip) when even it does not fit.

Status: ready.

### Pagination

```cpp
bool Pagination(const char* id, int* page, int pageCount, const PaginationOptions& options = {});
```

Previous and next buttons (chevrons, named `Strings::previous` and `Strings::next` in tooltips)
around page buttons, at one control height (`size`), its content width.

- **Pages.** Up to seven pages show all of them; more pages always take seven slots: the first and
  last page, the current page with one neighbour on each side, and a gap for each run of pages left
  out. The current page sits on the selection pill, which slides with the spring of
  `SegmentedControl` when the page changes.
- **Gaps.** A gap is a button (named `Strings::goToPage`) that opens a small popup with a number
  field; Enter there writes the typed page (clamped to the range) and closes it, Escape closes it.
- **Compact.** `compact`, or a row that does not fit the available width, shows previous,
  `Strings::pageOf` and next.
- **Keys.** One Tab stop (the current page); Left and Right, Home and End move the focus between the
  buttons, skipping previous and next while they cannot move; Enter or Space goes to the page.
  Holding previous or next repeats.
- **Values.** `*page` is 0-based, shown clamped to `pageCount`, and written at once on a choice; the
  result is true then. With `pageCount` 0 only the disabled buttons and "Page 0 of 0" show.

Status: ready.

### StepIndicator

```cpp
void StepIndicator(const char* id, int current, std::span<const ChoiceItem> steps,
                   const StepIndicatorOptions& options = {});
void StepIndicator(const char* id, int current, std::span<const char* const> steps,
                   const StepIndicatorOptions& options = {});
bool StepIndicator(const char* id, int* current, std::span<const ChoiceItem> steps,
                   const StepIndicatorOptions& options = {});
bool StepIndicator(const char* id, int* current, std::span<const char* const> steps,
                   const StepIndicatorOptions& options = {});
```

Numbered markers (`metrics.stepMarkerSize`, scaled with `size`) joined by connectors
(`metrics.stepConnectorWidth`), the available width by default.

- **Look.** Done steps (before `current`) are an accent disc with a check mark; the current step is
  a focus-ring colored ring around an `accentSubtle` disc with its number and label in semibold;
  open steps are a boundary ring with the number in `labelSecondary`. A disabled step
  (`ChoiceItem::disabled`) takes the disabled look wherever it is. Connectors between done steps
  take the focus ring color, the others the 3:1 control boundary; the check mark keeps the progress
  readable without color. Labels and the item `detail` (a second line) are cut with an ellipsis and
  shown in full on hover.
- **Layout.** Horizontal: one column per step, the label under the marker. Vertical, and horizontal
  in a Compact width class: one row per step, the label beside the marker.
- **Interactive.** The pointer overload makes every done step that is not disabled a target: a
  click, Enter or Space writes `*current` and returns true. They are one Tab stop with Left and Right
  (Up and Down when vertical), Home and End. The current and later steps cannot be chosen, so a flow
  never skips ahead past a step that is not done.

Status: ready.

### BeginWizard, WizardFooter

```cpp
void BeginWizard(const char* id, int* step, std::span<const ChoiceItem> steps, const WizardOptions& options = {});
void BeginWizard(const char* id, int* step, std::span<const char* const> steps, const WizardOptions& options = {});
void EndWizard();
WizardAction WizardFooter(const WizardFooterOptions& options = {});
class WizardScope;
```

A multi-step flow, the available width by default.

- **Steps.** With `stepIndicator` the interactive step indicator sits above the content, so a done
  step can be chosen to go back. The caller's content of `*step` follows; when the step changes it
  slides in from the side it comes from (`spacing.xxxl`, `motion.durationStandard`) and stands still
  with reduced motion.
- **Footer.** `WizardFooter` puts Back and Next (Finish on the last enabled step, or `finishLabel`)
  at the trailing end, Next as the primary button, and with `cancelButton` a Cancel button at the
  leading end. Back and Next write `*step` and return `Back` and `Next`; Finish returns `Finish`
  and leaves the step; Cancel returns `Cancel`. The buttons are Tab stops.
- **Validation.** Next and Finish are disabled while `canAdvance` is false: a step that fails its
  validation holds the flow, and the caller shows why (an error under the field). Back is disabled on
  the first enabled step.
- **Disabled steps.** Steps whose `ChoiceItem::disabled` is set are skipped by Back and Next and
  shown dimmed in the indicator.
- **Read-only.** A null `step` draws the steps and a disabled footer.

Status: ready.

### ShortcutSheet

```cpp
void ShortcutSheet(const char* label, bool* open, const ShortcutSheetOptions& options = {});
```

A modal level of the popup host (`overlay.modalWidth` wide, centered over the scrim) listing the
registered shortcuts, shown while `*open` is true.

- **Content.** The visible label part as the title in Title3 with a close button (`Strings::close`)
  at its end; with `searchable` a search field that takes the keyboard when the sheet opens and
  filters by fuzzy match on the label, the group and the chord text; then the shortcuts grouped under
  their group names (in the order of their first registration, ungrouped ones last), each with its
  label and its primary and alternate chords as key caps (`Strings::unbound` for none). Disabled
  shortcuts draw in `labelTertiary` with disabled caps. `group` lists only that group.
- **States.** No shortcut in the group: `Strings::empty`; none matching the query:
  `Strings::noResults`. The list scrolls once it is taller than half the display's work area.
- **Closing.** Escape (after it cleared a query and left the field), the close button and a click
  on the scrim write `*open = false`; the focus returns to the item that had it.
- The rows are not interactive: the sheet shows the shortcuts and does not edit them
  (`KeybindList` in the composites does).

Status: ready.

## Usage examples

A sidebar that drives the app's page. The app owns the selection and tells each item whether it is
the selected one; the selection pill follows it. With the default Auto mode the same code gives the
expanded sidebar in a wide window and the icon rail, with labels as tooltips, in a narrow one:

```cpp
static int page = 0;
if (facet::BeginSidebar("nav"))
{
    if (facet::BeginSidebarSection("Library"))
    {
        if (facet::SidebarItem("Files", facet::Icon("folder"), {.selected = page == 0}))
            page = 0;
        if (facet::SidebarItem("Downloads", facet::Icon("download"), {.selected = page == 1, .badge = pending}))
            page = 1;
        facet::EndSidebarSection();
    }
    facet::EndSidebar();
}
```

Tabs that can be closed and moved. The tab bar reports the closed tab and the move; the app applies
them to its list, and `current` already points at the right tab:

```cpp
if (facet::TabBar("documents", &current, tabs, {.closable = true, .reorderable = true}))
    ShowDocument(current);
if (const int closed = facet::TabBarClosedTab(); closed >= 0)
    tabs.erase(tabs.begin() + closed);
facet::ApplyReorderMove(tabs, facet::TabBarMovedTab());
```

A menu bar whose items run registered shortcuts, so the menu, the key chord and the command
palette share one handler:

```cpp
static const facet::ShortcutId save = facet::RegisterShortcut("file.save", ImGuiMod_Ctrl | ImGuiKey_S,
                                                              {.label = "Save", .group = "File"});
if (facet::BeginMenuBar("main"))
{
    if (facet::BeginMenu("File"))
    {
        facet::MenuItem("Save", {.icon = facet::Icon("download"), .command = save});
        facet::MenuCheckItem("Show hidden files", &showHidden);
        facet::MenuRadioItem("Sort by name", &sortMode, 0);
        facet::MenuRadioItem("Sort by date", &sortMode, 1);
        facet::EndMenu();
    }
    facet::EndMenuBar();
}
if (facet::ShortcutPressed(save))   // the chord, the menu item and the palette all end here
    Save();
```

Breadcrumbs return the activated segment:

```cpp
if (const int level = facet::Breadcrumbs("path", pathItems); level >= 0)
    NavigateUp(level);
```

A wizard. The footer moves `step` itself; the app only says whether the current step is complete
and handles the finish:

```cpp
static int step = 0;
facet::BeginWizard("setup", &step, steps);
DrawStep(step);
switch (facet::WizardFooter({.canAdvance = StepComplete(step), .cancelButton = true}))
{
case facet::WizardAction::Finish: ApplySetup(); break;
case facet::WizardAction::Cancel: CloseSetup(); break;
default: break;
}
facet::EndWizard();
```

## Later

No widget of this family is in the later tier.
