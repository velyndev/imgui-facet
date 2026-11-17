#pragma once

// Containers: the floating shell, the app frame and its title bar, the overlay controller,
// tool and status bars, accordions, disclosures, cards, group boxes, scroll areas, virtual
// lists, mini windows and the inspector pane.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families, and docs/widgets/containers.md has usage examples. The family is
// planned: its widgets are declared here and draw a placeholder until Phase 2 implements them
// (docs/widgets.md).
//
// When End is called (rule 8): a Begin that returns void always lays out its content, and its
// End is always called. A Begin that returns [[nodiscard]] bool may skip its content (closed,
// collapsed, hidden, out of sight); its End is called only after true. Every pair has an
// XScope class that follows the same rule. facet::EndFrame reports a Begin whose End was not
// called. Null pointers, null labels and out-of-range values follow rule 14.

#include <span>

#include <imgui.h>

#include "facet/core/input.h"
#include "facet/widgets/common.h"

namespace facet
{

// --- Shell -----------------------------------------------------------------------------------

// Where a shell first appears in the display, until a saved position exists.
enum class ShellPlacement
{
    Center,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
};

struct ShellOptions
{
    Icon icon;                         // before the title; none by default
    ImVec2 initialSize = {0.0f, 0.0f}; // units; used until a saved size exists; 0 for the token default
    ShellPlacement placement = ShellPlacement::Center;
    ImVec2 minSize = {0.0f, 0.0f};     // units; 0 keeps the title bar and one control row
    ImVec2 maxSize = {0.0f, 0.0f};     // units; 0 for the display size
    bool resizable = true;             // every edge and corner resizes
    bool movable = true;               // the title bar is the drag region
    bool snapToEdges = true;           // snaps to display edges and corners within spacing.lg while dragged
    bool keepInDisplay = true;         // re-clamped into the display after a display size or scale change
    bool collapsible = false;          // a double click on the title bar collapses the shell to it
    bool closeOnEscape = false;        // Escape closes it once nothing above it takes Escape (rule 8)
    bool glass = true;                 // backdrop blur, opaque tint where blur is unavailable
    bool persist = true;               // position and size saved in units in the Dear ImGui ini file
};

// A floating, resizable menu window drawn by Facet: a title bar with the icon, the visible
// label part as title and a close button (when `open` is not null), the window surface with
// its shadow, and Facet's own resize grips on every edge and corner. Dragging the title bar
// moves it; it snaps to display edges and corners, never grows past maxSize or the display,
// and comes back into the display when the display shrinks or the scale changes. Position and
// size are kept and saved in design units, so a shell reopens at the same place and size at
// any scale. `open` (may be null: always open, no close button) is written false when the
// close button or Escape (with closeOnEscape) closes it; the caller writes true to open it.
// Returns false while closed or collapsed; call EndShell only after true. Call it outside any
// other window, like ImGui::Begin. The content keeps ids under the shell's window.
[[nodiscard]] bool BeginShell(const char* label, bool* open, const ShellOptions& options = {});
void EndShell();

class [[nodiscard]] ShellScope
{
public:
    ShellScope(const char* label, bool* open, const ShellOptions& options = {})
        : open_(BeginShell(label, open, options)) {}
    ~ShellScope() { if (open_) EndShell(); }
    ShellScope(const ShellScope&) = delete;
    ShellScope& operator=(const ShellScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- App frame -----------------------------------------------------------------------------------

struct AppFrameOptions
{
    float resizeBorder = 0.0f;         // units; band along the edges that AppFrameHitTest reports as resize area; 0 for the token default
    bool hostMaximized = false;        // the host window is maximized: no resize band, square corners
    ImVec2 minSize = {0.0f, 0.0f};     // units; reported by AppFrameMinSize; 0 for no minimum
    ImVec2 maxSize = {0.0f, 0.0f};     // units; reported by AppFrameMaxSize; 0 for no maximum
};

// The root surface of a desktop app: an undecorated window that fills the main viewport of the
// host, for a borderless host window that Facet draws completely (title bar, sidebar, content,
// status bar). The host keeps its window resizable and snappable by answering its hit test
// with AppFrameHitTest and its size limits with AppFrameMinSize and AppFrameMaxSize.
// Returns false while the viewport has no area (the host window is minimized); call
// EndAppFrame only after true. Call it outside any other window, once per frame.
[[nodiscard]] bool BeginAppFrame(const char* id, const AppFrameOptions& options = {});
void EndAppFrame();

class [[nodiscard]] AppFrameScope
{
public:
    explicit AppFrameScope(const char* id, const AppFrameOptions& options = {})
        : open_(BeginAppFrame(id, options)) {}
    ~AppFrameScope() { if (open_) EndAppFrame(); }
    AppFrameScope(const AppFrameScope&) = delete;
    AppFrameScope& operator=(const AppFrameScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// What a point of the app frame is, for the host's non-client hit test (on Win32, the answer
// to WM_NCHITTEST: Caption for HTCAPTION, the edges for HTLEFT and so on, MaximizeButton for
// HTMAXBUTTON so the system shows its snap layouts over it).
enum class FrameHit
{
    Client,          // Facet content: the host passes the input to Dear ImGui
    Caption,         // free title bar area: drag to move, double click to maximize
    Left,
    Right,
    Top,
    Bottom,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
    MinimizeButton,
    MaximizeButton,
    CloseButton,
};

// The region under `point` (pixels, main viewport coordinates) in the app frame of the last
// frame: title bars report their free area as Caption and their window buttons as buttons,
// the resize band reports the edges and corners, everything else is Client. Client before
// the first BeginAppFrame.
FrameHit AppFrameHitTest(ImVec2 point);
// The minimum and maximum size of the last BeginAppFrame in pixels at the current scale, for
// the host's size limits; {0, 0} where none was set.
ImVec2 AppFrameMinSize();
ImVec2 AppFrameMaxSize();

// The host window's placement in design units (screen pixels divided by Scale() at the time
// it was saved), so a window comes back at the same size on a monitor with another scale.
struct FramePlacement
{
    ImVec2 position = {0.0f, 0.0f};    // units; top-left corner of the restored window
    ImVec2 size = {0.0f, 0.0f};        // units; size of the restored window
    bool maximized = false;
};

// Stores the placement under `id` (hashed alone, not on the id stack) in Facet's part of the
// Dear ImGui ini file. The host calls it when its window moves or resizes.
void SaveAppFramePlacement(const char* id, const FramePlacement& placement);
// Reads the placement saved under `id`. Returns false and leaves *placement unchanged when
// nothing was saved. The host clamps the result to its monitors before using it.
bool LoadAppFramePlacement(const char* id, FramePlacement* placement);

// --- Title bar -----------------------------------------------------------------------------------

// A window button of a title bar.
enum class WindowButton
{
    None,
    Minimize,
    Maximize,  // also restore, while TitleBarOptions::maximized is set
    Close,
};

struct TitleBarOptions
{
    Icon icon;                         // before the title; none by default
    const char* subtitle = nullptr;    // second text after the title in labelSecondary; null for none
    bool windowButtons = false;        // minimize, maximize or restore and close at the trailing end
    bool maximized = false;            // the maximize button shows restore
    float width = 0.0f;                // units; 0 fills the available width
    float height = 0.0f;               // units; 0 for the token default
};

// A title row: icon, the visible label part as title (truncated with an ellipsis when narrow),
// subtitle, then the caller's items, then the window buttons. Items submitted between
// BeginTitleBar and EndTitleBar sit at the trailing end, right-aligned with their widths from
// the last frame. The free area of the bar is the drag region (Caption in AppFrameHitTest).
// EndTitleBar is always called. A window button press is read with TitleBarPressedButton.
void BeginTitleBar(const char* label, const TitleBarOptions& options = {});
void EndTitleBar();

// The window button activated in the last EndTitleBar of this frame, or None.
WindowButton TitleBarPressedButton();

class [[nodiscard]] TitleBarScope
{
public:
    explicit TitleBarScope(const char* label, const TitleBarOptions& options = {}) { BeginTitleBar(label, options); }
    ~TitleBarScope() { EndTitleBar(); }
    TitleBarScope(const TitleBarScope&) = delete;
    TitleBarScope& operator=(const TitleBarScope&) = delete;
};

// --- Overlay controller --------------------------------------------------------------------------

struct OverlayOptions
{
    Keybind toggleKey;                 // a press of either chord flips *visible; the mode is not used; unbound by default
    bool scrim = false;                // dims the host's scene behind the menu while it is visible
};

// Wraps the menu of an overlay (a menu drawn over a game or another app). Shells and other
// windows submitted between BeginOverlay and EndOverlay form the menu: they fade in when
// *visible becomes true and fade out when it becomes false (motion.durationStandard; at once
// with reduced motion), and take no input while fading out. Mini windows outside the pair
// stay visible while pinned and let input through while the menu is hidden. `visible` is the
// caller's: write it to show or hide the menu; Facet writes it when toggleKey is pressed.
// Returns false once the menu is fully hidden, and when `visible` is null; call EndOverlay only
// after true. Call it outside any window, once per frame: the menu's windows are top-level.
// Where input goes is read with OverlayWantsMouse, OverlayWantsKeyboard, OverlayHitTest and
// GetOverlayInputRegions.
[[nodiscard]] bool BeginOverlay(const char* id, bool* visible, const OverlayOptions& options = {});
void EndOverlay();

class [[nodiscard]] OverlayScope
{
public:
    OverlayScope(const char* id, bool* visible, const OverlayOptions& options = {})
        : open_(BeginOverlay(id, visible, options)) {}
    ~OverlayScope() { if (open_) EndOverlay(); }
    OverlayScope(const OverlayScope&) = delete;
    OverlayScope& operator=(const OverlayScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// A screen rectangle in pixels, main viewport coordinates.
struct ScreenRegion
{
    ImVec2 min = {0.0f, 0.0f};
    ImVec2 max = {0.0f, 0.0f};
};

// Whether the overlay takes the mouse or the keyboard this frame: like io.WantCaptureMouse and
// io.WantCaptureKeyboard, but false over click-through windows and while the menu is hidden.
// The host passes input to the game or app when false.
bool OverlayWantsMouse();
bool OverlayWantsKeyboard();
// Whether a point (pixels) lies on a Facet window that takes input in the last frame.
bool OverlayHitTest(ImVec2 point);
// Writes the screen rectangles that take input in the last frame (visible menu windows and
// popups; not click-through windows) into `regions` and returns how many exist, which may be
// more than regions.size(). For hosts that shape their window or input region.
int GetOverlayInputRegions(std::span<ScreenRegion> regions);

// --- Mini window ---------------------------------------------------------------------------------

struct MiniWindowOptions
{
    Icon icon;                         // before the title; none by default
    bool clickThrough = true;          // lets input through while the overlay menu is hidden
    bool persist = true;               // position saved in units in the Dear ImGui ini file
    float width = 0.0f;                // units; 0 fits the content
};

// A compact, auto-sized panel with a title and a pin button. It shows while the overlay menu is
// visible, and while *pinned is true also when the menu is hidden, then without its title bar
// and (with clickThrough) without taking input. The pin button writes *pinned. Dragged by its
// title like a shell; position kept in units. Returns false while hidden; call EndMiniWindow
// only after true. Call it outside any window and outside BeginOverlay.
[[nodiscard]] bool BeginMiniWindow(const char* label, bool* pinned, const MiniWindowOptions& options = {});
void EndMiniWindow();

class [[nodiscard]] MiniWindowScope
{
public:
    MiniWindowScope(const char* label, bool* pinned, const MiniWindowOptions& options = {})
        : open_(BeginMiniWindow(label, pinned, options)) {}
    ~MiniWindowScope() { if (open_) EndMiniWindow(); }
    MiniWindowScope(const MiniWindowScope&) = delete;
    MiniWindowScope& operator=(const MiniWindowScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Toolbar and status bar ----------------------------------------------------------------------

struct ToolbarOptions
{
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fills the available width
};

// A row of tool buttons. Submit ToolbarButton, ToolbarSeparator and ToolbarSpacer between
// BeginToolbar and EndToolbar; other widgets may be submitted too but never overflow. When the
// buttons do not fit, the ones with the lowest priority move into a "more" menu at the end.
// EndToolbar is always called. Arrow keys move between the buttons. The widths of the last
// frame are kept under the id.
void BeginToolbar(const char* id, const ToolbarOptions& options = {});
void EndToolbar();

struct ToolbarButtonOptions
{
    bool selected = false;             // shows the on look; the caller flips its state on activation
    bool showLabel = false;            // the visible label part beside the icon, not only as tooltip
    int priority = 0;                  // higher stays visible longer when the toolbar is narrow
};

// A button of a toolbar. The visible label part is required: it is the tooltip and the name in
// the overflow menu. Returns true when it is activated, also from the overflow menu.
bool ToolbarButton(const char* label, Icon icon, const ToolbarButtonOptions& options = {});
// A hairline between groups of buttons.
void ToolbarSeparator();
// Flexible space: the items after the last spacer sit at the trailing end.
void ToolbarSpacer();

class [[nodiscard]] ToolbarScope
{
public:
    explicit ToolbarScope(const char* id, const ToolbarOptions& options = {}) { BeginToolbar(id, options); }
    ~ToolbarScope() { EndToolbar(); }
    ToolbarScope(const ToolbarScope&) = delete;
    ToolbarScope& operator=(const ToolbarScope&) = delete;
};

struct StatusBarOptions
{
    float width = 0.0f;                // units; 0 fills the available width
};

// A bottom strip of status segments, submitted with StatusItem between BeginStatusBar and
// EndStatusBar; other widgets may be submitted too. Segments that do not fit are hidden, lowest
// priority first. EndStatusBar is always called. Not interactive itself.
void BeginStatusBar(const char* id, const StatusBarOptions& options = {});
void EndStatusBar();

struct StatusItemOptions
{
    Icon icon;
    Tone tone = Tone::Neutral;         // colors the icon and a leading status dot
    int priority = 0;                  // higher stays visible longer
};

// One status segment with `text` (read during the call), laid out after the previous one from
// the leading edge.
void StatusItem(const char* text, const StatusItemOptions& options = {});

class [[nodiscard]] StatusBarScope
{
public:
    explicit StatusBarScope(const char* id, const StatusBarOptions& options = {}) { BeginStatusBar(id, options); }
    ~StatusBarScope() { EndStatusBar(); }
    StatusBarScope(const StatusBarScope&) = delete;
    StatusBarScope& operator=(const StatusBarScope&) = delete;
};

// --- Accordion and disclosure --------------------------------------------------------------------

struct AccordionOptions
{
    bool singleOpen = false;           // opening a section closes the others
    float width = 0.0f;                // units; 0 fills the available width
};

// A stack of sections that open and close with an animated height. Submit sections with
// BeginAccordionSection; EndAccordion is always called. Up and Down move between the section
// headers. The accordion owns the open states (kept under the section ids) because it closes
// the other sections in single-open mode.
void BeginAccordion(const char* id, const AccordionOptions& options = {});
void EndAccordion();

struct AccordionSectionOptions
{
    Icon icon;
    const char* detail = nullptr;      // secondary text at the end of the header; null for none
    bool defaultOpen = false;          // open state the first time the section appears
};

// A section header with a chevron. Click, Enter or Space toggles it. Returns true while it is
// open or animating closed: submit its content, then call EndAccordionSection. Returns false
// while closed; call EndAccordionSection only after true. ImGui::SetNextItemOpen before the
// call opens or closes it.
[[nodiscard]] bool BeginAccordionSection(const char* label, const AccordionSectionOptions& options = {});
void EndAccordionSection();

class [[nodiscard]] AccordionScope
{
public:
    explicit AccordionScope(const char* id, const AccordionOptions& options = {}) { BeginAccordion(id, options); }
    ~AccordionScope() { EndAccordion(); }
    AccordionScope(const AccordionScope&) = delete;
    AccordionScope& operator=(const AccordionScope&) = delete;
};

class [[nodiscard]] AccordionSectionScope
{
public:
    explicit AccordionSectionScope(const char* label, const AccordionSectionOptions& options = {})
        : open_(BeginAccordionSection(label, options)) {}
    ~AccordionSectionScope() { if (open_) EndAccordionSection(); }
    AccordionSectionScope(const AccordionSectionScope&) = delete;
    AccordionSectionScope& operator=(const AccordionSectionScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

enum class DisclosureStyle
{
    Row,   // a full-width row with a chevron
    Link,  // inline accent text with a chevron ("Show more")
};

struct DisclosureOptions
{
    DisclosureStyle style = DisclosureStyle::Row;
    const char* summary = nullptr;     // shown after the label while closed; null for none
    bool defaultOpen = false;          // used when `open` is null, the first time it appears
};

// An inline toggle that reveals content below it with an animated height. `open` is the
// caller's open state (written on toggle); null keeps the state under the id. Click, Enter or
// Space toggles. Returns true while open or animating closed: submit the content, then call
// EndDisclosure. Returns false while closed; call EndDisclosure only after true.
[[nodiscard]] bool BeginDisclosure(const char* label, bool* open, const DisclosureOptions& options = {});
void EndDisclosure();

class [[nodiscard]] DisclosureScope
{
public:
    DisclosureScope(const char* label, bool* open, const DisclosureOptions& options = {})
        : open_(BeginDisclosure(label, open, options)) {}
    ~DisclosureScope() { if (open_) EndDisclosure(); }
    DisclosureScope(const DisclosureScope&) = delete;
    DisclosureScope& operator=(const DisclosureScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Card and group box --------------------------------------------------------------------------

struct CardOptions
{
    const char* title = nullptr;       // header line; null for none
    Icon icon;                         // before the title
    float width = 0.0f;                // units; 0 fills the available width
    float height = 0.0f;               // units; 0 fits the content
};

// A grouped surface (bgSecondary, card radius), not interactive. Its content is always
// submitted; EndCard is always called. The card is the last item after EndCard. Ids are not
// scoped by the card.
void BeginCard(const char* id, const CardOptions& options = {});
void EndCard();

class [[nodiscard]] CardScope
{
public:
    explicit CardScope(const char* id, const CardOptions& options = {}) { BeginCard(id, options); }
    ~CardScope() { EndCard(); }
    CardScope(const CardScope&) = delete;
    CardScope& operator=(const CardScope&) = delete;
};

struct GroupBoxOptions
{
    Icon icon;                         // before the title
    const char* description = nullptr; // line under the title in labelSecondary; null for none
    bool collapsible = false;          // the title row toggles the content
    bool defaultOpen = true;           // open state the first time it appears, when collapsible
    float width = 0.0f;                // units; 0 fills the available width
};

// A titled region with a hairline border that groups controls; the visible label part is the
// title. To disable every control in it, wrap it in ImGui::BeginDisabled. Returns false while
// collapsed (only with collapsible); call EndGroupBox only after true. The open state is kept
// under the id.
[[nodiscard]] bool BeginGroupBox(const char* label, const GroupBoxOptions& options = {});
void EndGroupBox();

class [[nodiscard]] GroupBoxScope
{
public:
    explicit GroupBoxScope(const char* label, const GroupBoxOptions& options = {})
        : open_(BeginGroupBox(label, options)) {}
    ~GroupBoxScope() { if (open_) EndGroupBox(); }
    GroupBoxScope(const GroupBoxScope&) = delete;
    GroupBoxScope& operator=(const GroupBoxScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Scroll area and virtual list ----------------------------------------------------------------

struct ScrollAreaOptions
{
    bool horizontal = false;           // also scrolls sideways
    bool edgeFades = true;             // content fades out at an edge that has more content behind it
    float width = 0.0f;                // units; 0 fills the available width
    float height = 0.0f;               // units; 0 fills the available height
};

// A scrolling region (a Dear ImGui child window) with an overlay scrollbar that appears while
// scrolling or hovered, smooth wheel scrolling and edge fades. The keyboard scrolls it when a
// child has focus (Page Up and Down, Home and End). Inside, ImGui::SetScrollHereY and the other
// stock scroll calls work. Returns false while it is out of sight (Facet then closes its child
// window itself); call EndScrollArea only after true. The content keeps ids under the area.
[[nodiscard]] bool BeginScrollArea(const char* id, const ScrollAreaOptions& options = {});
void EndScrollArea();

class [[nodiscard]] ScrollAreaScope
{
public:
    explicit ScrollAreaScope(const char* id, const ScrollAreaOptions& options = {})
        : open_(BeginScrollArea(id, options)) {}
    ~ScrollAreaScope() { if (open_) EndScrollArea(); }
    ScrollAreaScope(const ScrollAreaScope&) = delete;
    ScrollAreaScope& operator=(const ScrollAreaScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

struct VirtualListOptions
{
    float rowHeight = 0.0f;            // units; 0 for the control height
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;   // null uses Strings::empty
    const char* errorText = nullptr;   // null uses Strings::loadFailed
    float height = 0.0f;               // units; 0 fills the available height
};

// A scrolling list of rowCount rows of equal height that submits only the visible rows. With
// `selection` null the rows are not selectable; otherwise click, Ctrl, Shift and box select
// write it through ImGui::BeginMultiSelect. Returns false while hidden, not Ready, or empty
// (the state is drawn instead); call EndVirtualList only after true. Inside, loop
// NextVisibleRows and start each row with VirtualListRow. The scroll position is kept under
// the id.
[[nodiscard]] bool BeginVirtualList(const char* id, int rowCount, ImGuiSelectionBasicStorage* selection,
                                    const VirtualListOptions& options = {});
// Starts row `index`: a full-width row item (selection, the selection pill, activation)
// followed on the same line by the caller's content. Returns true when the row was activated
// (double click, Enter).
bool VirtualListRow(int index);
void EndVirtualList();

// Scrolls the next BeginVirtualList so that `row` is in view, once.
void SetNextVirtualListScrollTo(int row);

class [[nodiscard]] VirtualListScope
{
public:
    VirtualListScope(const char* id, int rowCount, ImGuiSelectionBasicStorage* selection,
                     const VirtualListOptions& options = {})
        : open_(BeginVirtualList(id, rowCount, selection, options)) {}
    ~VirtualListScope() { if (open_) EndVirtualList(); }
    VirtualListScope(const VirtualListScope&) = delete;
    VirtualListScope& operator=(const VirtualListScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Inspector pane ------------------------------------------------------------------------------

struct InspectorPaneOptions
{
    float paneWidth = 0.0f;            // units; first width of the pane; 0 for the token default
    float minPaneWidth = 0.0f;         // units; 0 for the token default
    float maxPaneWidth = 0.0f;         // units; 0 for half the region
    bool resizable = true;             // the pane's leading edge drags to resize
    float height = 0.0f;               // units; 0 fills the available height
};

// A region with a details pane at its trailing edge. The main content follows BeginInspectorPane;
// InspectorPaneNext switches to the pane and returns whether its content is shown (the pane is
// open or sliding), false while *visible is false. EndInspectorPane is always called. The
// visible label part is the pane's title; its close button writes *visible = false, and the
// caller writes true to open it. A null `visible` keeps the pane open without a close button. In a
// Compact width the pane floats over the main content as a drawer. The pane width is kept in
// units under the id and saved in the ini file.
void BeginInspectorPane(const char* label, bool* visible, const InspectorPaneOptions& options = {});
[[nodiscard]] bool InspectorPaneNext();
void EndInspectorPane();

class [[nodiscard]] InspectorPaneScope
{
public:
    InspectorPaneScope(const char* label, bool* visible, const InspectorPaneOptions& options = {})
    {
        BeginInspectorPane(label, visible, options);
    }
    ~InspectorPaneScope() { EndInspectorPane(); }
    InspectorPaneScope(const InspectorPaneScope&) = delete;
    InspectorPaneScope& operator=(const InspectorPaneScope&) = delete;
};

} // namespace facet
