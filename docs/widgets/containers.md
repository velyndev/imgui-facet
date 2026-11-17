# Containers

The floating shell, the app frame and its title bar, the overlay controller, mini windows, tool and
status bars, accordions, disclosures, cards, group boxes, scroll areas, virtual lists and the
inspector pane, declared in `facet/widgets/containers.h`. Every declaration follows
[the API conventions](../api-conventions.md); the header comments are the full reference. See
[the widget index](../widgets.md) for the other families.

A widget whose status is planned is declared and callable: it draws a dashed placeholder with its
name, returns the neutral result and never writes a value. Containers whose content sits in the page
show that content inside the placeholder; layers (the shell, the mini window) open nothing and show
one placeholder line while the caller asks for them. Phase 2 implements each widget and changes its
status to ready.

When End is called: a Begin that returns `void` always lays out its content and its End is always
called. A Begin that returns `[[nodiscard]] bool` may skip its content, and its End is called only
after true. Every pair has a scope class with the same rule.

## Widgets

### BeginShell

Status: planned

```cpp
[[nodiscard]] bool BeginShell(const char* label, bool* open, const ShellOptions& options = {});
void EndShell();
class ShellScope;
```

A floating, resizable menu window: title bar as drag region, edge and corner resize, minimum and
maximum size, snapping to display edges, size and position kept in design units and clamped back
into the display after a resolution or scale change. Like `ImGui::Begin` it makes a top-level
window, so a page may open one from its own code.

### BeginAppFrame

Status: planned

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

### BeginTitleBar

Status: planned

```cpp
void BeginTitleBar(const char* label, const TitleBarOptions& options = {});
void EndTitleBar();
WindowButton TitleBarPressedButton();
class TitleBarScope;
```

A title row with icon, subtitle, the caller's trailing items and optional window buttons; its free
area is the drag region.

### BeginOverlay (overlay controller)

Status: planned

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

### BeginMiniWindow

Status: planned

```cpp
[[nodiscard]] bool BeginMiniWindow(const char* label, bool* pinned, const MiniWindowOptions& options = {});
void EndMiniWindow();
class MiniWindowScope;
```

A compact panel that stays visible while pinned and lets input through while the overlay menu is
hidden.

### BeginToolbar

Status: planned

```cpp
void BeginToolbar(const char* id, const ToolbarOptions& options = {});
void EndToolbar();
bool ToolbarButton(const char* label, Icon icon, const ToolbarButtonOptions& options = {});
void ToolbarSeparator();
void ToolbarSpacer();
class ToolbarScope;
```

A row of tool buttons whose lowest-priority buttons move into a "more" menu when it is narrow.

### BeginStatusBar

Status: planned

```cpp
void BeginStatusBar(const char* id, const StatusBarOptions& options = {});
void EndStatusBar();
void StatusItem(const char* text, const StatusItemOptions& options = {});
class StatusBarScope;
```

A bottom strip of toned status segments that hide by priority when narrow.

### BeginAccordion

Status: planned

```cpp
void BeginAccordion(const char* id, const AccordionOptions& options = {});
void EndAccordion();
[[nodiscard]] bool BeginAccordionSection(const char* label, const AccordionSectionOptions& options = {});
void EndAccordionSection();
class AccordionScope;
class AccordionSectionScope;
```

Sections that open and close with an animated height, one or several at a time.

### BeginDisclosure

Status: planned

```cpp
[[nodiscard]] bool BeginDisclosure(const char* label, bool* open, const DisclosureOptions& options = {});
void EndDisclosure();
class DisclosureScope;
```

An inline toggle that reveals content below it, as a row or as a link.

### BeginCard

Status: planned

```cpp
void BeginCard(const char* id, const CardOptions& options = {});
void EndCard();
class CardScope;
```

A grouped surface with an optional title, not interactive.

### BeginGroupBox

Status: planned

```cpp
[[nodiscard]] bool BeginGroupBox(const char* label, const GroupBoxOptions& options = {});
void EndGroupBox();
class GroupBoxScope;
```

A titled, bordered region that groups controls and can collapse.

### BeginScrollArea

Status: planned

```cpp
[[nodiscard]] bool BeginScrollArea(const char* id, const ScrollAreaOptions& options = {});
void EndScrollArea();
class ScrollAreaScope;
```

A scrolling region with an overlay scrollbar, smooth wheel scrolling and edge fades.

### BeginVirtualList

Status: planned

```cpp
[[nodiscard]] bool BeginVirtualList(const char* id, int rowCount, ImGuiSelectionBasicStorage* selection,
                                    const VirtualListOptions& options = {});
bool VirtualListRow(int index);
void EndVirtualList();
void SetNextVirtualListScrollTo(int row);
class VirtualListScope;
```

A list of many equal rows with arbitrary content that submits only the visible rows.

### BeginInspectorPane

Status: planned

```cpp
void BeginInspectorPane(const char* label, bool* visible, const InspectorPaneOptions& options = {});
[[nodiscard]] bool InspectorPaneNext();
void EndInspectorPane();
class InspectorPaneScope;
```

A details pane at the trailing edge of a region that becomes a drawer in a Compact width.

## Usage examples

An overlay menu inside a game or another host. The controller fades the menu, the shell keeps its
place and size, the mini window stays on screen while pinned, and the host routes input with the
queries:

```cpp
static bool menuVisible = true, statsPinned = false, settingsOpen = true;
facet::NewFrame();
if (facet::BeginOverlay("overlay", &menuVisible, {.toggleKey = {.primary = ImGuiKey_Insert}}))
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
    switch (facet::TitleBarPressedButton())
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
    ApplyPlacement(hwnd, ClampToMonitors(placement));  // size in pixels: size * the monitor's scale
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
