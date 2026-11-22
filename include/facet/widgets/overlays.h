#pragma once

// Overlays: modals and confirm dialogs, popovers, context menus, tooltips, toasts, drawers,
// loading overlays, the command palette, watermark overlays and the unsaved changes bar.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families, and docs/widgets/overlays.md has usage examples. Every widget of the
// family is implemented; its entry there describes what it does and ends with its status.
//
// How layers open and how results come back (rule 8):
//   - Layers the caller opens (BeginModal, ConfirmDialog, BeginPopover, BeginDrawer,
//     CommandPalette) take `bool* open`. The caller writes true to open; Facet writes false when
//     the layer is dismissed. The call is made every frame, open or not. A null `open` shows
//     nothing and returns the closed result (false or None).
//   - Layers opened by input (BeginContextMenu, tooltips) open themselves over the last item.
//   - Results are read from return values (DialogResult, a written index) or from query
//     functions on the same or the next frame; there are no callbacks.
//   - Escape closes the topmost layer only, after any active edit was canceled.
// When End is called: a Begin that returns void always lays out its content and its End is
// always called; a Begin that returns [[nodiscard]] bool is ended only after true. Every pair
// has an XScope class. Null pointers, null labels and out-of-range values follow rule 14.

#include <cstdint>
#include <span>

#include <imgui.h>

#include "facet/core/input.h"
#include "facet/widgets/common.h"

namespace facet
{

// --- Modal and confirm dialog --------------------------------------------------------------------

struct ModalOptions
{
    bool dismissible = true;           // Escape and a click on the scrim close it
    bool closeButton = true;           // a close button at the trailing end of the title row
    float width = 0.0f;                // units; 0 for the token default
};

// A modal dialog over a scrim, shown while *open is true; the visible label part is the title.
// It scales in when opened, traps the keyboard focus inside and gives it back to the item that
// had it when it closes; modals opened from inside a modal stack above it. Writes *open = false
// when dismissed. Returns false while closed and when `open` is null; call EndModal only after
// true. Its content keeps ids under the dialog window.
[[nodiscard]] bool BeginModal(const char* label, bool* open, const ModalOptions& options = {});
void EndModal();

class [[nodiscard]] ModalScope
{
public:
    ModalScope(const char* label, bool* open, const ModalOptions& options = {}) : open_(BeginModal(label, open, options)) {}
    ~ModalScope() { if (open_) EndModal(); }
    ModalScope(const ModalScope&) = delete;
    ModalScope& operator=(const ModalScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// How a dialog ended.
enum class DialogResult
{
    None,       // still open, or closed
    Confirmed,
    Canceled,
};

struct ConfirmDialogOptions
{
    const char* confirmLabel = nullptr; // null uses Strings::confirm
    const char* cancelLabel = nullptr;  // null uses Strings::cancel
    bool destructive = false;           // destructive confirm button; focus starts on Cancel
    bool holdToConfirm = false;         // the confirm button must be held for motion.durationSlow
};

// A compact modal with `message` (read during the call) and two buttons; the visible label part
// is the title. Returns Confirmed or Canceled on the deciding frame and writes *open = false;
// None otherwise, and always when `open` is null (nothing is shown then). Escape and a click on
// the scrim cancel; Enter confirms unless destructive.
DialogResult ConfirmDialog(const char* label, bool* open, const char* message,
                           const ConfirmDialogOptions& options = {});

// --- Popover -------------------------------------------------------------------------------------

// The side of the anchor a popover opens on. Auto takes the side with the most room; every
// placement flips to the opposite side and clamps to the display when it does not fit.
enum class PopoverPlacement
{
    Auto,
    Below,
    Above,
    Start,  // to the leading side
    End,    // to the trailing side
};

// What a popover is anchored to.
enum class PopoverAnchor
{
    LastItem,  // the item submitted right before BeginPopover
    Mouse,     // the pointer position when it opened
};

struct PopoverOptions
{
    PopoverPlacement placement = PopoverPlacement::Auto;  // the preferred side; Auto takes the side with the most room
    PopoverAnchor anchor = PopoverAnchor::LastItem;       // what it points at; the anchor's rect is taken when it opens
    bool arrow = true;                 // a pointer toward the anchor
    bool openOnHover = false;          // Facet writes *open = true while the anchor is hovered, false when the pointer leaves both
    bool dismissible = true;           // Escape and a click outside both the popover and its anchor item close it
    float width = 0.0f;                // units; 0 fits the content
};

// A floating card anchored to the last item, with a shadow and backdrop blur, shown while *open
// is true. Writes *open = false when dismissed. A click on the anchor item (LastItem) does not
// count as outside, so an anchor button may toggle the flag itself (`open = !open`) and a second
// click closes the popover instead of reopening it. Returns false while closed and when `open`
// is null; call EndPopover only after true. Keyboard focus moves into it when it opens and
// returns to the anchor when it closes. Its content keeps ids under the popover window.
[[nodiscard]] bool BeginPopover(const char* id, bool* open, const PopoverOptions& options = {});
void EndPopover();

class [[nodiscard]] PopoverScope
{
public:
    PopoverScope(const char* id, bool* open, const PopoverOptions& options = {})
        : open_(BeginPopover(id, open, options)) {}
    ~PopoverScope() { if (open_) EndPopover(); }
    PopoverScope(const PopoverScope&) = delete;
    PopoverScope& operator=(const PopoverScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Context menu --------------------------------------------------------------------------------

enum class ContextTarget
{
    LastItem,  // the item submitted right before BeginContextMenu
    Window,    // empty space of the current window
};

struct ContextMenuOptions
{
    ContextTarget target = ContextTarget::LastItem;  // where a right click opens the menu
};

// A menu that opens on right click, the Menu key or Shift+F10 over the target, at the pointer
// (or at the focused item for the keys). Facet reads the Menu key and Shift+F10 on the focused
// item itself through key ownership, like Escape (rule 8), so the keys work without
// ImGuiConfigFlags_NavEnableKeyboard. `id` names the popup, so it also works after widgets
// made of several items. Returns true while open: submit menu items (facet::MenuItem,
// BeginMenu), then call EndContextMenu. Returns false while closed; call EndContextMenu only
// after true.
[[nodiscard]] bool BeginContextMenu(const char* id, const ContextMenuOptions& options = {});
void EndContextMenu();

class [[nodiscard]] ContextMenuScope
{
public:
    explicit ContextMenuScope(const char* id, const ContextMenuOptions& options = {})
        : open_(BeginContextMenu(id, options)) {}
    ~ContextMenuScope() { if (open_) EndContextMenu(); }
    ContextMenuScope(const ContextMenuScope&) = delete;
    ContextMenuScope& operator=(const ContextMenuScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Tooltips ------------------------------------------------------------------------------------

struct TooltipOptions
{
    float maxWidth = 0.0f;             // units; text wraps here; 0 for the token default
    bool interactive = false;          // the tooltip stays while the pointer moves into it, so its content can be clicked
};

// A themed tooltip for the last item, printf formatted, shown after the hover delay and while
// the item has keyboard focus; once one tooltip shows, neighbours show theirs without delay.
void ItemTooltip(const char* fmt, ...) IM_FMTARGS(1);
// Rich content for the last item's tooltip (images, key caps, charts). Returns true while the
// tooltip shows: submit the content, then call EndRichTooltip; call it only after true. Not
// named BeginItemTooltip because Dear ImGui's function of that name pairs with ImGui::EndTooltip.
[[nodiscard]] bool BeginRichTooltip(const TooltipOptions& options = {});
void EndRichTooltip();

class [[nodiscard]] RichTooltipScope
{
public:
    explicit RichTooltipScope(const TooltipOptions& options = {}) : open_(BeginRichTooltip(options)) {}
    ~RichTooltipScope() { if (open_) EndRichTooltip(); }
    RichTooltipScope(const RichTooltipScope&) = delete;
    RichTooltipScope& operator=(const RichTooltipScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Toasts --------------------------------------------------------------------------------------

// Handle of a queued toast; 0 is invalid.
struct ToastId
{
    std::uint32_t value = 0;
    constexpr bool IsValid() const { return value != 0; }
    friend constexpr bool operator==(const ToastId&, const ToastId&) = default;
};

struct ToastOptions
{
    Tone tone = Tone::Neutral;         // colors the icon; Neutral has no status color
    Icon icon;                         // none: the tone's icon
    const char* description = nullptr; // wrapped line under the text; null for none
    const char* actionLabel = nullptr; // one action button; null for none
    float duration = 0.0f;             // seconds; 0 for the token default; < 0 until dismissed
    bool progressBar = false;          // a progress bar under the text
    float progress = kIndeterminate;   // 0 to 1, or kIndeterminate; shown with progressBar
};

// Queues a toast. The texts are copied into a fixed queue; when it is full the oldest toast is
// dropped. facet::EndFrame draws the stack in the toast corner; a toast stays while hovered and
// can be swiped away or closed with its close button. Returns its id, or an invalid id outside
// a frame.
ToastId ShowToast(const char* text, const ToastOptions& options = {});
// Replaces the text and options of a visible toast (for progress); its timer restarts.
void UpdateToast(ToastId toast, const char* text, const ToastOptions& options = {});
// True once, on the frame after the toast's action button was activated.
bool ToastActionPressed(ToastId toast);
bool IsToastVisible(ToastId toast);
void DismissToast(ToastId toast);

// ScreenCorner is in common.h: shells take it too.

// Where the toast stack sits; BottomRight until changed. Applies from the next frame.
void SetToastCorner(ScreenCorner corner);

// --- Drawer --------------------------------------------------------------------------------------

// The display or region edge a drawer slides in from.
enum class DrawerSide
{
    End,     // trailing edge
    Start,   // leading edge
    Bottom,  // a bottom sheet
    Top,
};

struct DrawerOptions
{
    DrawerSide side = DrawerSide::End; // the edge it slides in from
    float extent = 0.0f;               // units; width for End and Start, height for Bottom and Top; 0 for the token default
    bool modal = true;                 // a scrim behind it, and focus stays inside
    bool resizable = false;            // the inner edge drags to resize
    bool dismissible = true;           // Escape, a click on the scrim and a flick toward the edge close it
};

// A panel that slides in from an edge of the display, shown while *open is true; the visible
// label part is its title. Writes *open = false when dismissed. Returns false while closed and
// when `open` is null; call EndDrawer only after true. Its content keeps ids under the drawer
// window.
[[nodiscard]] bool BeginDrawer(const char* label, bool* open, const DrawerOptions& options = {});
void EndDrawer();

class [[nodiscard]] DrawerScope
{
public:
    DrawerScope(const char* label, bool* open, const DrawerOptions& options = {})
        : open_(BeginDrawer(label, open, options)) {}
    ~DrawerScope() { if (open_) EndDrawer(); }
    DrawerScope(const DrawerScope&) = delete;
    DrawerScope& operator=(const DrawerScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

// --- Loading overlay -----------------------------------------------------------------------------

struct LoadingOverlayOptions
{
    const char* message = nullptr;     // under the spinner; null uses Strings::loading
    float progress = kIndeterminate;   // 0 to 1 shows a bar; kIndeterminate shows the spinner
    bool loading = false;              // dims and blocks the region while true
};

// Wraps a region: while `loading` is set, the content submitted until EndLoadingOverlay is
// dimmed, takes no input and shows a spinner (or a progress bar) with the message over it. The
// content is always laid out, so the region keeps its size; EndLoadingOverlay is always called.
void BeginLoadingOverlay(const char* id, const LoadingOverlayOptions& options = {});
void EndLoadingOverlay();

class [[nodiscard]] LoadingOverlayScope
{
public:
    explicit LoadingOverlayScope(const char* id, const LoadingOverlayOptions& options = {})
    {
        BeginLoadingOverlay(id, options);
    }
    ~LoadingOverlayScope() { EndLoadingOverlay(); }
    LoadingOverlayScope(const LoadingOverlayScope&) = delete;
    LoadingOverlayScope& operator=(const LoadingOverlayScope&) = delete;
};

// --- Command palette -----------------------------------------------------------------------------

struct CommandPaletteOptions
{
    const char* placeholder = nullptr; // placeholder of the search field; null uses Strings::search
    ShortcutId openShortcut = {};      // a registered shortcut (core/input.h) whose press writes *open = true; invalid for none
    int maxVisibleRows = 0;            // list height in rows at most; fewer entries shrink it; 0 for the token default
    bool recentFirst = true;           // recently picked entries come first while the field is empty
    const char* emptyText = nullptr;   // null uses Strings::noResults
};

// A floating launcher at the top center of the display with a search field and the entries
// fuzzy matched as one types; ChoiceItem::group groups them, detail shows a hint at the end.
// Shown while *open is true; the visible label part is a small heading above the search field
// ("##name" for none). Up and Down move,
// Enter picks, Escape closes. On a pick it writes *picked and *open = false and returns true.
// A null `open` shows nothing and returns false. Recent picks are kept under the id.
bool CommandPalette(const char* label, bool* open, int* picked, std::span<const ChoiceItem> commands,
                    const CommandPaletteOptions& options = {});
bool CommandPalette(const char* label, bool* open, int* picked, std::span<const char* const> commands,
                    const CommandPaletteOptions& options = {});
// The same over the enabled shortcuts of the registry (core/input.h), with their chords as key
// caps. On a pick it writes *picked, calls TriggerShortcut for it and returns true. Run the
// action in the ShortcutPressed handler only; the result reports the pick, for example to
// close other UI.
bool CommandPalette(const char* label, bool* open, ShortcutId* picked, const CommandPaletteOptions& options = {});

// --- Watermark overlay ---------------------------------------------------------------------------

// One field of a watermark overlay: a name and a value.
struct WatermarkField
{
    const char* label = nullptr;       // required
    const char* value = nullptr;       // null for a label alone
    Icon icon;
};

struct WatermarkOptions
{
    ScreenCorner corner = ScreenCorner::TopLeft;  // the display corner of the strip, until moved
    bool movable = false;              // dragged while the overlay menu is visible, snapping to the corners
};

// A strip of fields in a corner of the display, drawn over everything and letting input through.
// Fields are read during the call. With movable, the chosen corner is kept under the id and
// saved in the ini file.
void WatermarkOverlay(const char* id, std::span<const WatermarkField> fields, const WatermarkOptions& options = {});

// --- Unsaved changes bar -------------------------------------------------------------------------

// What the unsaved changes bar did this frame.
enum class UnsavedChangesAction
{
    None,
    Apply,
    Revert,
};

struct UnsavedChangesBarOptions
{
    const char* message = nullptr;     // text before the buttons; null fills Strings::unsavedChange
                                       // (one change) or Strings::unsavedChanges
    const char* applyLabel = nullptr;  // null uses Strings::apply
    const char* revertLabel = nullptr; // null uses Strings::revert
    ShortcutId applyShortcut = {};     // a registered shortcut (for example Ctrl+S) that applies; invalid for none
};

// A bar that slides in at the bottom of the current window while changeCount > 0, with Apply
// and Revert buttons; Escape reverts while it has focus. Returns the action of this frame; the
// caller applies or reverts its settings, which brings changeCount to 0. changeCount replaces
// "{count}" in the Strings template.
UnsavedChangesAction UnsavedChangesBar(const char* id, int changeCount, const UnsavedChangesBarOptions& options = {});

} // namespace facet
