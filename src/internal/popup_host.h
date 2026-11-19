#pragma once

// Popup host: every floating layer Facet opens (menus, context menus, dropdown and completion
// lists, popovers, tooltips, modal dialogs, drawers) is a level of one host. The host places a
// level next to its anchor (flipped to the other side when there is no room, then clamped into
// the display), keeps the nested stack (a submenu is the child of its menu), closes levels on
// Escape and on a click outside, plays the open and close motion (the close motion replays the
// level's last draw data while it fades, because Dear ImGui forgets a closed window at once),
// and routes Escape and the context menu keys between layers.
//
// This header holds what the other blocks and the widgets call. The popup host owns the rest
// and adds it to this file and to further popup_* files.
//
// Levels are named by an ImGuiID, normally the id of the widget that owns the level
// (ImGui::GetID of its label). A level is opened once (OpenPopupLevel) and then submitted every
// frame while it is open:
//
//     const ImGuiID id = ImGui::GetID(label);
//     if (fieldClicked)
//         OpenPopupLevel(id, {.rect = fieldRect, .side = PopupSide::Below, .matchWidth = true});
//     if (BeginPopupLevel(id))
//     {
//         ... the list ...
//         EndPopupLevel();
//     }
//     if (PopupLevelClosedReason(id) == PopupCloseReason::Escape) ... // nothing was chosen
//
// Lengths come from the theme: overlay.anchorGap between the level and its anchor,
// overlay.viewportMargin to the display edges; motion.popupOpen, popupClose and popupScaleFrom
// for the motion, which jumps while facet::ReduceMotion() is true. Rectangles are screen pixels.
//
// Escape (docs/api-conventions.md rule 8) acts on one thing per press, in this order:
//   1. an active edit: an item that is active and owns the Escape key (edit sessions take it with
//      ImGui::SetKeyOwner, text fields while they have the keyboard) cancels its edit, and no
//      layer sees the press;
//   2. the topmost open Popup level (the innermost child first);
//   3. the topmost Modal level;
//   4. the topmost shell that set closeOnEscape (EscapeLayer::Shell through ClaimEscape).
// Levels opened through this header take part on their own; layers that are not levels (a
// shell) claim Escape with ClaimEscape. Facet reads Escape through key ownership, so this works
// without ImGuiConfigFlags_NavEnableKeyboard. Tooltip levels are not part of the order.
//
// The context menu keys: a right click on the target, the Menu key or Shift+F10 while the target
// has keyboard focus ask for its context menu (ContextMenuRequested). The keys are read through
// key ownership as well.
//
// Captures: a page forces the open look of a widget with ForcedItemState::Open (item.h). A
// widget that takes that request shows its level with ForcePopupLevelOpen instead of
// OpenPopupLevel: the level shows for that frame without input, takes no focus and ignores
// Escape and clicks outside, so capture images show the open list without changing any value.

#include <imgui.h>
#include <imgui_internal.h>

namespace facet::internal
{

// The side of the anchor a level opens on. After and Before are the trailing and leading side
// (right and left in left-to-right text).
enum class PopupSide
{
    Below,
    Above,
    After,
    Before,
};

// Where the level sits along the anchor edge: Start lines up the level's start with the anchor's
// start (left edges for Below and Above, top edges for After and Before).
enum class PopupAlign
{
    Start,
    Center,
    End,
};

struct PopupAnchor
{
    ImRect rect;                       // screen pixels: the item, field, caret or point (a zero-size rect) the level belongs to
    PopupSide side = PopupSide::Below; // preferred side; the opposite one when only that one has room for the level
    PopupAlign align = PopupAlign::Start;  // along the anchor edge; then moved to stay overlay.viewportMargin inside the display
    bool matchWidth = false;           // Below and Above: at least as wide as the anchor (dropdown lists)
};

// How a level behaves.
enum class PopupKind
{
    Popup,    // menus, lists, popovers, completion lists: closed by Escape and by a click outside; nests
    Modal,    // dialogs and drawers: drawn over a scrim, keeps the keyboard focus inside; a click
              // outside does not close it (the widget decides what a click on the scrim does)
    Tooltip,  // tooltips: no focus, no input, never part of the Escape order
};

struct PopupOptions
{
    PopupKind kind = PopupKind::Popup;
    bool takeFocus = true;             // Popup: the keyboard focus moves into the level when it opens; a
                                       // completion list passes false so typing stays in the field
    bool glass = false;                // backdrop blur behind the level (popovers; DESIGN.md dose caps)
    ImVec2 size = {0.0f, 0.0f};        // pixels per axis; 0 fits the content
    float minWidth = 0.0f;             // pixels; 0 for none (menus pass overlay.menuMinWidth through Px)
};

// Opens level `id` at `anchor`. Opened while another Popup level is being submitted, it becomes
// that level's child and closes with it. Opening a level that is already open moves it to the
// new anchor and keeps its content and focus. Has no effect outside a frame.
void OpenPopupLevel(ImGuiID id, const PopupAnchor& anchor, const PopupOptions& options = {});

// Closes level `id` and its children: they stop taking input at once and fade out. Nothing
// happens when the level is not open.
void ClosePopupLevel(ImGuiID id);

// True while level `id` is open; false while it fades out after closing.
bool IsPopupLevelOpen(ImGuiID id);

// Submits level `id`. Returns true while it is open: submit its content, then call
// EndPopupLevel. Returns false while it is closed or closing (a closing level is drawn by the
// host from its last draw data); call EndPopupLevel only after true. Places the window from the
// anchor, applies Escape and outside clicks by its kind, and keeps ids under the level.
[[nodiscard]] bool BeginPopupLevel(ImGuiID id);
void EndPopupLevel();

// Why a level closed.
enum class PopupCloseReason
{
    None,          // it did not close on this frame
    Escape,        // Escape closed it (rule 8)
    ClickOutside,  // a click outside closed it
    Closed,        // ClosePopupLevel, or its parent closed
};

// Why level `id` closed on this frame, so a widget can tell a dismissal from a choice.
PopupCloseReason PopupLevelClosedReason(ImGuiID id);

// The innermost open level, or 0 when none is open.
ImGuiID TopPopupLevel();

// Shows level `id` at `anchor` for this frame only, for a forced Open state in a capture run
// (see the top of this file). Submit it with BeginPopupLevel as usual. Without a call in the
// next frame the level is gone, with no close motion.
void ForcePopupLevelOpen(ImGuiID id, const PopupAnchor& anchor, const PopupOptions& options = {});

// Layers in the Escape order that are not popup levels.
enum class EscapeLayer
{
    Shell,  // a shell that closes on Escape (ShellOptions::closeOnEscape)
};

// Claims Escape for `owner` (the layer's id) on this frame; call it every frame the layer is
// shown. Returns true on the frame an Escape press belongs to this layer: nothing before it in
// the order took the press, and among claims of one layer the front-most window wins.
bool ClaimEscape(ImGuiID owner, EscapeLayer layer);

// What a context menu opens for.
enum class ContextRequestTarget
{
    LastItem,  // the item submitted last
    Window,    // empty space of the current window
};

// True on the frame the user asks for a context menu over `target`: the right mouse button
// released over it, or the Menu key or Shift+F10 pressed while it has keyboard focus (for Window:
// while the focus is in the window and on no item that has its own menu). *at (may be null)
// receives where the menu opens: the pointer for the mouse, the bottom-left corner of the
// focused item (or the window's content start) for the keys.
bool ContextMenuRequested(ContextRequestTarget target, ImVec2* at);

} // namespace facet::internal
