# Containers

The floating shell, the app frame and its title bar, the overlay controller, mini windows, tool and
status bars, accordions, disclosures, cards, group boxes, scroll areas, virtual lists and the
inspector pane, declared in `facet/widgets/containers.h`. Every declaration follows
[the API conventions](../api-conventions.md); the header comments are the full reference. See
[the widget index](../widgets.md) for the other families.

Each entry below ends with the widget's status. A **planned** widget is declared and callable: it
draws a dashed placeholder with its name, returns the neutral result and never writes a value.
Containers whose content sits in the page show that content inside the placeholder; layers (the
shell, the mini window) open nothing and show one placeholder line while the caller asks for them.
A **ready** widget works as its header describes.

When End is called: a Begin that returns `void` always lays out its content and its End is always
called. A Begin that returns `[[nodiscard]] bool` may skip its content, and its End is called only
after true. Every pair has a scope class with the same rule. Every ready container leaves Dear
ImGui's id, group, clip rect, item flag and window stacks as it found them on every path (open,
closed, collapsed, clipped away, disabled, nested in itself, called outside a frame), and an End
without its Begin is reported by name and changes nothing.

## Widgets

### BeginShell

```cpp
[[nodiscard]] bool BeginShell(const char* label, bool* open, const ShellOptions& options = {});
void EndShell();
class ShellScope;
```

A floating, resizable menu window that Facet draws itself. Like `ImGui::Begin` it makes a top-level
window (the label is its Dear ImGui window name, with the usual `##` and `###` rules), also when it
is called while another window is being submitted, so a page may open one from its own code.

- **Look.** The surface is glass with its shadow (`glass`, the default), or the opaque elevated
  surface with the elevation shadow and a hairline where blur is off or unavailable. The title bar
  (`containers.titleBarHeight`) shows the optional `icon`, the visible label part in Headline (cut
  with an ellipsis and shown in full as a tooltip on hover) and, when `open` is not null, a close
  button named by `Strings::close`. A hairline divides it from the content.
- **Moving.** Dragging the free part of the title bar moves the shell (`movable`). While dragged it
  snaps to the display edges and corners within `spacing.lg` (`snapToEdges`).
- **Resizing.** Bands of `containers.resizeBorder` along every edge and a square of twice that in
  every corner resize it (`resizable`), with the resize cursors; the opposite edges stay where they
  are. Facet's grip marks the bottom-right corner and brightens while a band is hovered or held. The
  bands lie inside the window, so the pointer over them is over the shell and never reaches what is
  behind it. The size stays within `minSize` (0: the title bar and one control row inside the
  padding, at least `overlay.menuMinWidth` wide) and `maxSize` (0: the display).
- **Place and size.** The first place is the display center (`centered`) or the display corner of
  `corner`, at `initialSize` (0: `overlay.modalWidth` wide and a quarter taller, until a size token
  exists). Position and size are kept in design units, the position relative to the display's work
  area, under the window name, and with `persist` saved in Dear ImGui's ini file: the shell reopens
  at the same place and apparent size at any scale and after a restart. With `keepInDisplay` it is
  clamped into the display every frame, so a smaller display or a larger scale brings it back.
- **Collapsing.** With `collapsible`, a double click on the title bar collapses the shell to its
  title bar and back; `BeginShell` returns false while it is collapsed. The state is saved with the
  place.
- **Closing.** The close button writes `*open = false`; so does Escape with `closeOnEscape`, once
  nothing above the shell takes Escape (rule 8 of the conventions). The caller writes true to open it.
- **Content.** Below the title bar the content is a scroll area of the scroll core (overlay
  scrollbar, smooth wheel, edge fades on the opaque surface) inset by the resize bands, with
  `spacing.lg` around it. Keyboard navigation reaches every item inside; content ids are under the
  shell's window.

`BeginShell` returns false while closed, collapsed or skipped by Dear ImGui (it then ends its window
itself); call `EndShell` only after true. Moving and resizing take the mouse only; the keyboard
reaches the close button and the content. Status: ready.

### BeginAppFrame

```cpp
[[nodiscard]] bool BeginAppFrame(const char* id, const AppFrameOptions& options = {});
void EndAppFrame();
class AppFrameScope;
FrameHit AppFrameHitTest(ImVec2 point);
ImVec2 AppFrameMinSize();
ImVec2 AppFrameMaxSize();
void SaveAppFramePlacement(const char* id, const FramePlacement& placement);
bool LoadAppFramePlacement(const char* id, FramePlacement* placement);
```

The root surface of a borderless desktop app that fills the host viewport, with the hit test, size
limits and placement storage the host window needs to stay resizable and snappable. The host calls
it outside every window, so the widget gallery shows it only through the example below.
Status: planned.

### BeginTitleBar

```cpp
void BeginTitleBar(const char* label, const TitleBarOptions& options = {});
void EndTitleBar();
WindowButton TitleBarButtonPressed();
class TitleBarScope;
```

A title row of `containers.titleBarHeight` (or `height`), filling the available width unless `width`
is set.

- **Leading.** The optional `icon`, the visible label part as the title in Headline and the optional
  `detail` in Footnote and labelSecondary. Where the items begin the title ends in an ellipsis and
  shows in full as a tooltip on hover; the detail gives way first.
- **Trailing.** Items submitted between `BeginTitleBar` and `EndTitleBar` form a group that is
  vertically centered on the bar and ends before the window buttons, placed with its width of the
  last frame (kept under the id); use `ImGui::SameLine` between items as in any row. With
  `windowButtons`, three square plain buttons follow: minimize, maximize (restore while `maximized`)
  and close, named by `Strings::minimize`, `Strings::maximize`, `Strings::restore` and
  `Strings::close` in their tooltips. The close button takes the destructive fill on hover and press,
  because it closes the window. The buttons are Tab stops and activate with a click, Enter or Space;
  `TitleBarButtonPressed` returns the one activated in the last `EndTitleBar` of the frame, or `None`.
- **Caption.** The free area of the bar (everything but the items and the buttons) is what
  `AppFrameHitTest` reports as `Caption`. The bar itself does not move a window.
- **States.** Inside `ImGui::BeginDisabled` the title draws in labelTertiary and the buttons do
  nothing.

`EndTitleBar` is always called; after it the bar is one item of its whole size. Status: ready.

### BeginOverlay (overlay controller)

```cpp
[[nodiscard]] bool BeginOverlay(const char* id, bool* visible, const OverlayOptions& options = {});
void EndOverlay();
class OverlayScope;
bool OverlayWantsMouse();
bool OverlayWantsKeyboard();
bool OverlayHitTest(ImVec2 point);
int GetOverlayInputRegions(std::span<ScreenRegion> regions);
```

Shows and hides an overlay menu with a fade and a toggle key, and tells the host which screen
regions take input. Called by the host outside every window, like the mini window below.
Status: planned.

### BeginMiniWindow

```cpp
[[nodiscard]] bool BeginMiniWindow(const char* label, bool* pinned, const MiniWindowOptions& options = {});
void EndMiniWindow();
class MiniWindowScope;
```

A compact panel that stays visible while pinned and lets input through while the overlay menu is
hidden. Status: planned.

### BeginToolbar

```cpp
void BeginToolbar(const char* id, const ToolbarOptions& options = {});
void EndToolbar();
bool ToolbarButton(const char* label, Icon icon, const ToolbarButtonOptions& options = {});
void ToolbarSeparator();
void ToolbarSpacer();
class ToolbarScope;
```

A row of plain tool buttons at one control height (`size`), filling the available width unless
`width` is set.

- **Buttons.** `ToolbarButton` returns true when it is activated: a click, Space or Enter while it
  has keyboard focus, or its row in the more menu. An icon-only button (the default) is square and
  shows its visible label as a tooltip on hover and keyboard focus; with `iconOnly` off it shows the
  icon and the label. `selected` shows the on look (the accent's subtle fill under accent text, so it
  does not rely on the fill alone); the caller flips its state on activation. Outside a toolbar a
  tool button is a plain button of its own.
- **Groups.** `ToolbarSeparator` draws a hairline between groups. `ToolbarSpacer` is flexible space:
  the items after the last spacer sit at the trailing end.
- **Overflow.** When the buttons do not fit, the ones with the lowest `priority` move into a menu
  behind a more button at the trailing end; among equal priorities the button furthest to the end
  goes first. A separator shows only with a visible item on each side, so a row never starts or ends
  with one. The menu lists the hidden buttons in toolbar order with their icons (a check mark for a
  selected one), and a row activates the same action: the button's call returns true on the frame
  after the row was chosen. A hidden button comes back once the row fits it with `spacing.lg` to
  spare, so a slowly resized window does not make it blink. The widths are those of the last frame,
  kept under the id; in the first frame a button shows while it fits.
- **Keys.** Each button and the more button are Tab stops; Left and Right move along the row
  (Dear ImGui keyboard navigation). Enter, Space or a click opens the menu, whose rows take Up,
  Down, Enter and Escape; closing it returns the focus to the more button.
- **Other widgets** submitted between `BeginToolbar` and `EndToolbar` sit on the row after the
  previous item and never overflow; their width is taken off the room the buttons have.
- **States.** A button inside `ImGui::BeginDisabled` draws in labelTertiary, is not a Tab stop and
  never returns true, also from the menu, where its row is disabled.

`EndToolbar` is always called; after it the toolbar is one item of its full width. Up to 48 buttons
and separators per toolbar take part in the overflow; later ones show while they fit and never move
into the menu. Status: ready.

### BeginStatusBar

```cpp
void BeginStatusBar(const char* id, const StatusBarOptions& options = {});
void EndStatusBar();
void StatusItem(const char* text, const StatusItemOptions& options = {});
class StatusBarScope;
```

A bottom strip of toned status segments that hide by priority when narrow. Status: planned.

### BeginAccordion

```cpp
void BeginAccordion(const char* id, const AccordionOptions& options = {});
void EndAccordion();
[[nodiscard]] bool BeginAccordionSection(const char* label, const AccordionSectionOptions& options = {});
void EndAccordionSection();
class AccordionScope;
class AccordionSectionScope;
```

A stack of sections. Each section is a header row (a chevron, the optional `icon`, the title in
Headline and the optional `detail` at the end in labelSecondary) over content that opens and closes
with an animated height; a hairline separates the sections.

- **Opening.** A click, Enter or Space on a header toggles its section. `defaultOpen` sets the state
  the first time a section appears; `ImGui::SetNextItemOpen` before `BeginAccordionSection` opens or
  closes it (`ImGuiCond_Always` every time, the other conditions only while nothing is known yet).
  With `singleOpen`, opening a section closes the others. The open states are kept under the
  section ids, which are scoped by the accordion id, and saved in the Dear ImGui ini file.
- **Motion.** The height eases over `motion.durationStandard`. While it moves, the content is clipped
  at the animated height and a closing section takes no keyboard focus; once a section is open and
  settled nothing is clipped, so focus rings show whole. With reduced motion a section opens and
  closes at once. The scroll position of the window around it is not touched.
- **Keys.** Tab reaches every header and the content of open sections. On a focused header, Up and
  Down move to the previous or next header and Home and End to the first and last, skipping the
  content in between.
- **Layout.** The accordion fills the available width unless `width` is set; the content of a
  section is indented to line up with its title. A title that does not fit ends in an ellipsis and
  shows in full as a tooltip on hover and keyboard focus; the detail text gives way first.
- **States.** Inside `ImGui::BeginDisabled` the headers draw in labelTertiary and do not toggle.

`BeginAccordionSection` returns true while its section is open or animating closed; call
`EndAccordionSection` only then. A section outside an accordion works on its own (no hairline, no
header keys). Status: ready.

### BeginDisclosure

```cpp
[[nodiscard]] bool BeginDisclosure(const char* label, bool* open, const DisclosureOptions& options = {});
void EndDisclosure();
class DisclosureScope;
```

An inline toggle that reveals the content below it with the same animated height as an accordion
section (clipped only while it moves, no focus inside while it closes, at once with reduced motion).

- **Styles.** `Row` (the default) is a full-width row with a chevron and the label; its content is
  indented to line up with the label. `Link` is accent text with a chevron, as wide as its content,
  underlined on hover; its content starts at the link's edge.
- **Summary.** `summary` is drawn after the label in labelSecondary while the disclosure is closed.
- **State.** `open` is the caller's flag and is written on every toggle; with a null `open` the
  state is kept under the id (`defaultOpen` the first time) and saved in the ini file.
- **Keys.** A click, Enter or Space toggles it; it is a Tab stop with the focus ring after keyboard
  input. A cut label shows in full as a tooltip on hover and keyboard focus.
- **States.** Inside `ImGui::BeginDisabled` it draws in labelTertiary and does not toggle.

Status: ready.

### BeginCard

```cpp
void BeginCard(const char* label, const CardOptions& options = {});
void EndCard();
class CardScope;
```

A grouped surface in bgSecondary with the card radius, not interactive. The visible label part is a
header line in Headline with the optional `icon` before it (`"##id"` for a card without one); a
title that does not fit ends in an ellipsis and shows in full as a tooltip on hover. The content sits
inside `spacing.lg` padding, and items that fill the available width stop at the inner edge. The
card fills the available width unless `width` is set; `height` is a minimum, the card grows with its
content. Its content is always submitted and `EndCard` is always called; after it the card is the
last item. Ids are not scoped by the card. Scroll areas inside a card fade into its surface color.
Inside `ImGui::BeginDisabled` the title draws in labelTertiary. Status: ready.

### BeginGroupBox

```cpp
[[nodiscard]] bool BeginGroupBox(const char* label, const GroupBoxOptions& options = {});
void EndGroupBox();
class GroupBoxScope;
```

A titled region that groups controls: the title in Headline with the optional `icon`, the optional
`description` under it in Footnote and labelSecondary (wrapped), then the controls inside a separator
hairline with the card radius and `spacing.md` padding. It fills the available width unless `width`
is set; items that fill the available width stop at the inner edge.

With `collapsible` the title row becomes a toggle (chevron, Enter, Space or a click; a Tab stop with
the focus ring) that opens and closes the bordered region with the animated height of the accordion,
and the open state is kept under the id (`defaultOpen` the first time) and saved in the ini file.
`BeginGroupBox` returns false while it is collapsed; call `EndGroupBox` only after true. To disable
every control in the box, wrap it in `ImGui::BeginDisabled`: the title and description then draw in
labelTertiary. After `EndGroupBox` the whole box is the last item. Status: ready.

### BeginScrollArea

```cpp
[[nodiscard]] bool BeginScrollArea(const char* id, const ScrollAreaOptions& options = {});
void EndScrollArea();
class ScrollAreaScope;
```

A scrolling region on the scroll core: a Dear ImGui child window whose overlay scrollbar is thin at
rest, wider while the pointer is over it, and fades out a moment after the last scroll; the wheel
scrolls smoothly, and with `edgeFades` the content fades into the surface behind the area (bg, or
the card's color inside a card) where more of it continues past an edge. `horizontal` adds sideways
scrolling. The width fills the available width unless `width` is set; `height` 0 fills the available
height, a negative value leaves that many units free below the area.

Keyboard navigation crosses into the area and scrolls it to the focused item; with the focus inside,
Page Up and Page Down move it by a page and Home and End to the ends. `ImGui::SetScrollHereY` and
the other stock scroll calls work inside. The bars are mouse affordances and take no focus.
`BeginScrollArea` returns false while the area is out of sight (from its second frame on, because a
new child window lays out its items once to measure them), and then closes its child window itself;
call `EndScrollArea` only after true. The content keeps ids under the area. Status: ready.

### BeginVirtualList

```cpp
[[nodiscard]] bool BeginVirtualList(const char* id, int rowCount, ImGuiSelectionBasicStorage* selection,
                                    const VirtualListOptions& options = {});
bool VirtualListRow(int index);
void EndVirtualList();
void SetNextVirtualListScrollTo(int row);
class VirtualListScope;
```

A list of many equal rows with arbitrary content that submits only the visible rows. Status: planned.

### BeginInspectorPane

```cpp
void BeginInspectorPane(const char* label, bool* visible, const InspectorPaneOptions& options = {});
[[nodiscard]] bool InspectorPaneNext();
void EndInspectorPane();
class InspectorPaneScope;
```

A details pane at the trailing edge of a region that becomes a drawer in a Compact width.
Status: planned.

## Usage examples

An overlay menu inside a game or another host. The controller fades the menu, the shell keeps its
place and size, the mini window stays on screen while pinned, and the host routes input with the
queries:

```cpp
static bool menuVisible = true, statsPinned = false, settingsOpen = true;
facet::NewFrame();
static const facet::ShortcutId toggleMenu = facet::RegisterShortcut("app.menu", ImGuiKey_Insert, {.label = "Show menu"});
if (facet::BeginOverlay("overlay", &menuVisible, {.toggleShortcut = toggleMenu}))
{
    if (facet::BeginShell("Settings###settings", &settingsOpen,
                          {.minSize = {320.0f, 240.0f}, .closeOnEscape = true}))
    {
        DrawSettings();
        facet::EndShell();
    }
    facet::EndOverlay();
}
if (facet::BeginMiniWindow("Stats", &statsPinned))
{
    DrawStats();
    facet::EndMiniWindow();
}
facet::EndFrame();
ForwardInputToHost(!facet::OverlayWantsMouse(), !facet::OverlayWantsKeyboard());
```

A borderless desktop app. Facet draws the whole window and answers the host's hit test, so the
window keeps native resizing, snapping and snap layouts:

```cpp
// Frame code
const bool maximized = IsZoomed(hwnd) != 0;
if (facet::BeginAppFrame("app", {.hostMaximized = maximized, .minSize = {640.0f, 400.0f}}))
{
    facet::BeginTitleBar("Editor", {.icon = facet::Icon("monitor"), .windowButtons = true, .maximized = maximized});
    DrawTitleBarItems();                                       // the app's trailing items
    facet::EndTitleBar();
    switch (facet::TitleBarButtonPressed())
    {
    case facet::WindowButton::Minimize: ShowWindow(hwnd, SW_MINIMIZE); break;
    case facet::WindowButton::Maximize: ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE); break;
    case facet::WindowButton::Close: PostMessage(hwnd, WM_CLOSE, 0, 0); break;
    default: break;
    }
    DrawContent();
    facet::EndAppFrame();
}

// Window procedure of the host
case WM_NCHITTEST:
    switch (facet::AppFrameHitTest(ClientPoint(hwnd, lParam)))
    {
    case facet::FrameHit::Caption: return HTCAPTION;
    case facet::FrameHit::MaximizeButton: return HTMAXBUTTON;
    case facet::FrameHit::Left: return HTLEFT;
    // ... the other edges and corners ...
    default: return HTCLIENT;
    }
// Over HTMAXBUTTON the system sends non-client button messages. The Win32 backend of Dear ImGui
// already turns WM_NCMOUSEMOVE into a mouse position; the host forwards the button itself and
// keeps the message from DefWindowProc, so the drawn button shows hover and press:
case WM_NCLBUTTONDOWN:
case WM_NCLBUTTONUP:
    if (wParam == HTMAXBUTTON)
    {
        ImGui::GetIO().AddMouseButtonEvent(0, msg == WM_NCLBUTTONDOWN);
        return 0;
    }
    break;
```

The window placement keeps its position in physical pixels of the virtual screen and its size in
design units, so the window reopens at the same place and at the same apparent size on a monitor
with another scale. Dear ImGui reads the ini file in the first `ImGui::NewFrame`, so a host that
wants the placement before its window shows reads the ini file itself after `facet::Init`:

```cpp
HWND hwnd = CreateHostWindowHidden();               // not visible yet
CreateDevice(hwnd);
ImGui::CreateContext();
facet::Init(desc);
ImGui::LoadIniSettingsFromDisk(ImGui::GetIO().IniFilename);
facet::FramePlacement placement;
if (facet::LoadAppFramePlacement("main", &placement))
    ApplyPlacement(hwnd, ClampToMonitors(placement));  // size in pixels: restoredSize * the monitor's scale
ShowWindow(hwnd, placement.maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
// WM_EXITSIZEMOVE and WM_CLOSE:
facet::SaveAppFramePlacement("main", CurrentPlacement(hwnd));   // restored rect, Scale() and IsZoomed
```

A virtual list with ten thousand rows and multiple selection:

```cpp
static ImGuiSelectionBasicStorage selection;
if (facet::BeginVirtualList("log", rowCount, &selection, {.state = loadState}))
{
    facet::RowRange rows;
    while (facet::NextVisibleRows(&rows))
        for (int row = rows.first; row < rows.last; ++row)
        {
            if (facet::VirtualListRow(row))
                OpenEntry(row);
            facet::Text(facet::TextStyle::Body, "%s", EntryText(row));
        }
    facet::EndVirtualList();
}
```

An inspector pane next to the main content:

```cpp
facet::BeginInspectorPane("Details", &showDetails);
DrawDocument();
if (facet::InspectorPaneNext())
    DrawDetails(selection);
facet::EndInspectorPane();
```

## Later

No widget of this family is in the later tier.
