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
// without ImGuiConfigFlags_NavEnableKeyboard. Tooltip levels are not part of the order; a Panel
// level with closeOnEscape takes its place right above the Popup and Modal levels that were open
// when it opened (the newest panel first).
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
    // Panels: a floating layer outside Dear ImGui's popup stack that takes input and focus like a
    // window and blocks nothing around it: the page under and beside it stays hoverable and
    // clickable (popovers, non-modal drawers). With closeOnEscape, Escape closes it before the
    // popup levels that were open when it opened, and after the popup levels opened since (a list
    // opened from its content). With closeOnClickOutside, a press of any mouse button outside it,
    // outside its anchor rect and outside the popup levels opened since closes it, and the press
    // still reaches what is under the pointer. When it closes by Escape or ClosePopupLevel while the
    // keyboard focus is inside it, the focus goes back to the window (and its item) that had it when
    // the panel opened. It stays in front of the window it was opened from, also after a click there,
    // and behind the popups that are open.
    Panel,
};

struct PopupOptions
{
    PopupKind kind = PopupKind::Popup;
    bool takeFocus = true;             // Popup and Panel: the keyboard focus moves into the level when it
                                       // opens; a completion list passes false so typing stays in the field
    bool glass = false;                // backdrop blur behind the level (popovers; DESIGN.md dose caps)
    ImVec2 size = {0.0f, 0.0f};        // pixels per axis; 0 fits the content
    float minWidth = 0.0f;             // pixels; 0 for none (menus pass overlay.menuMinWidth through Px)
    bool closeOnEscape = true;         // Popup and Modal: Escape closes it; false: the press stops
                                       // at the level and closes nothing (a modal that must be
                                       // answered)
    // Members below were added by the overlays family.
    ImVec2 padding = {-1.0f, -1.0f};   // pixels between the surface and the content; < 0 for the kind's default
    ImRect placeRect;                  // screen pixels: the level sits exactly here (no flip or clamp), for
                                       // drawers at a display edge; empty (the default) places it at the anchor
    bool slideIn = false;              // the open and close motion slides the whole level in from `slideEdge`
                                       // instead of growing from the anchor side (drawers)
    PopupSide slideEdge = PopupSide::After;  // the display edge a sliding level comes from
    ImDrawFlags corners = 0;           // which surface corners are rounded (ImDrawFlags_Round*); 0 for all
    bool interactive = false;          // Tooltip: takes the mouse, so its content can be clicked (no focus, no keys)
    float radius = -1.0f;              // pixels of the surface corners; < 0 for the kind's default
    int elevation = 0;                 // shadow and surface level 1 to 3; 0 for the kind's default
    bool closeOnClickOutside = false;  // Panel: a press outside closes it (see PopupKind::Panel)
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

// ---- Beyond the shared calls ------------------------------------------------------------------
//
// How the host behaves in detail (what a widget author can rely on):
//
// Levels on Dear ImGui. Popup and Modal levels are Dear ImGui popups (OpenPopupEx, BeginPopupEx),
// so they share Dear ImGui's popup stack with the host's own popups and follow its rules:
//   - OpenPopupLevel and BeginPopupLevel of one level are called at the same popup depth: a
//     level opened inside another level's BeginPopupLevel / EndPopupLevel is submitted there too.
//   - One level per depth: opening a level where a sibling is open closes the sibling and its
//     children ("opening a sibling closes the other branch"). Two independent popups at the root
//     therefore close each other, exactly like two of the host's own popups. Tooltip levels and
//     the forced levels of captures are plain windows outside the stack and never close a popup.
//   - A level that is open but not submitted in a frame (its widget is gone, the page changed) is
//     closed at facet::EndFrame of the next frame, without motion and without giving the focus
//     back to an item that no longer exists.
// Clicks outside. Dear ImGui closes the levels above the window that was clicked. The host then
// keeps the left mouse button until it is released, so the click that dismissed a level never
// presses the control under it; a click on the anchor of an open level therefore closes it, and
// the anchor sees nothing (the next click opens it again). A right click outside closes the levels
// too and still asks for a context menu where it lands (ContextMenuRequested). A Popup level opened
// without takeFocus is closed by the host itself, before the frame's items run, when a click lands
// outside it and outside the levels above it, also on an item of another window (Dear ImGui would
// leave it open, because that item takes the click); the button is held in the same way, except
// for a click on the level's own anchor rect, which the anchor still receives (a text field placing
// its caret; a dropdown field reads PopupLevelClosedReason and does not reopen).
// Focus. A Popup level with takeFocus moves the keyboard focus to its first item (or the item
// that called ImGui::SetItemDefaultFocus) when it opens; Dear ImGui's focus ring shows only after
// keyboard input. When a level closes by Escape or by ClosePopupLevel, the focus returns to the
// item that had it when the level opened. If that item is not submitted in the frame after, the
// focus is cleared instead (it never rests on a destroyed item). After a click outside, the focus
// stays where the click went.
// Escape. The host owns ImGuiKey_Escape (ImGui::SetKeyOwner) in every frame after one in which a
// Popup or Modal level was open or a layer claimed Escape, unless an item is active or another
// owner took the key in that frame. Dear ImGui's own navigation therefore never sees the press
// as well, with or without ImGuiConfigFlags_NavEnableKeyboard. A press the host acts on is
// locked for the rest of its frame (ImGuiInputFlags_LockThisFrame), so code that reads the key
// with ImGui::IsKeyPressed(ImGuiKey_Escape) and no owner does not see it either; other presses it
// sees as before. When Escape closes a level, Dear ImGui's navigation cursor is made visible,
// so Enter and Space act on the item the focus went back to. When the topmost entry of Dear
// ImGui's popup stack is a popup of the host application, the host leaves Escape to it.
// Motion. A level fades in over motion.popupOpen while it grows from motion.popupScaleFrom of its
// size at the edge that faces the anchor and slides overlay.anchorGap toward it; it fades out over
// motion.popupClose the same way. The close motion replays the level's last frame (popup_replay.h)
// in a window that takes no input, so input reaches what is under the level from the frame it
// closes. Under facet::ReduceMotion() a level appears and disappears at once. A device reset, a
// scale or theme change and facet::Shutdown end every close motion at once.
// Surface. The host draws each level's surface behind its content: the popover radius (the window
// radius for a Modal, the control radius for a Tooltip), a hairline, the elevation shadow (2 for
// Popup, 3 for Modal, 1 for Tooltip) and either the elevated surface color or, with glass, the
// blurred backdrop (the opaque tint where blur is not available). A Modal also draws colors.scrim
// over everything below it. Content padding: spacing.xs for Popup (rows run almost edge to edge),
// spacing.xl for Modal, spacing.sm by spacing.xs for Tooltip.
// Placement. Every frame, from the anchor and the content size of the previous frame
// (popup_place.h), inside the main viewport's work area minus overlay.viewportMargin. While the
// anchor stays where it is, a level keeps the side it took as long as it fits there, so content
// that grows and shrinks does not make it jump; when the anchor moves it is placed afresh. A
// Modal whose anchor rect is empty (the default) is centered in the work area. A Popup level
// opened from a window of the page (not from another level) passes that window as the region of
// popup_place.h, so its Start or End alignment turns to the other end when only that keeps the
// level over the window it belongs to.

// Moves an open level to a new anchor; call it every frame while the anchor can move (it
// scrolls, the window resizes). Unlike OpenPopupLevel it never opens a closed level, so calling
// it every frame cannot bring back a level that a click outside closed.
void SetPopupLevelAnchor(ImGuiID id, const PopupAnchor& anchor);

// Changes the size (pixels per axis, 0 fits the content) and the fixed rect (empty for none) of an
// open level from this frame on, for content measured by its widget (a dialog whose content
// scrolls) and drawers resized by their edge. Does nothing for a level that is not open.
void SetPopupLevelLayout(ImGuiID id, ImVec2 size, const ImRect& placeRect);

// Closes the chain `id` belongs to: from the outermost Popup level below it that is not under a
// Modal (or the root) up, so a menu row that ran its action closes the whole menu. Inside a Modal
// it closes the levels above the Modal only.
void ClosePopupLevelChain(ImGuiID id);

// The level being submitted (between BeginPopupLevel and EndPopupLevel), or 0.
ImGuiID CurrentPopupLevel();

// Where level `id` was placed in its last BeginPopupLevel (screen pixels) and the side it took;
// an empty rect and the anchor's side when it is not shown.
ImRect PopupLevelRect(ImGuiID id);
PopupSide PopupLevelSide(ImGuiID id);

// The surface color behind the content of the level whose window is `window` (a root window),
// shown in this frame: the elevated surface of the level's elevation, or bgElevated for a glass
// level, whose tint over the blurred backdrop has no single color. False when `window` is no level
// of the host. Forced levels and panels are plain windows, not Dear ImGui popups, so the scroll
// core asks here which surface its edge fades blend into.
bool PopupLevelSurface(const ImGuiWindow* window, ImVec4* surface);

// True on the frame a mouse button was clicked over the scrim of Modal level `id`: outside its
// window while it is the topmost level. The level stays open; the widget decides. A left press on
// the scrim is held until its release, so even when the widget closes the dialog on it, it presses
// nothing under the scrim and leaves the focus given back where it is.
bool PopupLevelScrimClicked(ImGuiID id);

// True while level `id` plays its close motion.
bool IsPopupLevelClosing(ImGuiID id);

// True while the pointer moves toward `target` (screen pixels): it moved in this frame and its
// position lies in the triangle from where it was to the near edge of `target`. Menus keep an
// open submenu while the pointer crosses other rows on its way there.
bool PopupPointerAimsAt(const ImRect& target);

// ---- Tooltips ------------------------------------------------------------------------------------
//
// A tooltip belongs to an item. Call right after the item, every frame, with its state:
//
//     const ItemState item = ItemBehavior(id, size);
//     ... draw the item ...
//     if (BeginPopupTooltip(item.id, item.rect, item.hovered, item.focusVisible))
//     {
//         facet::Text(TextStyle::Footnote, "Copies the selection");
//         EndPopupTooltip();
//     }
//
// The rules (times from the overlay tokens, multiplied by motion.scale):
//   - The pointer rests on the item for overlay.tooltipDelay, then the tooltip shows below and
//     after the pointer, so the cursor never covers it, flipped where there is no room.
//   - Within overlay.tooltipGrace after a tooltip was last visible, the next item shows its
//     tooltip at once, so moving along a toolbar needs no new wait.
//   - A mouse button press hides it until the pointer leaves the item; a key press hides it until
//     the pointer moves again.
//   - An item with visible keyboard focus (focusVisible) shows its tooltip below the item after
//     the same delay; so does an item reported hovered while the pointer is not over it (a forced
//     capture state).
//   - An item outside its window's clip rectangle shows none.
//   - One tooltip at a time; it never takes focus or input.
// maxWidth: pixels where its text wraps; 0 for overlay.tooltipMaxWidth.
[[nodiscard]] bool BeginPopupTooltip(ImGuiID itemId, const ImRect& itemRect, bool hovered, bool focusVisible,
                                     float maxWidth = 0.0f);
void EndPopupTooltip();  // only after BeginPopupTooltip returned true

// The same with more choices (added by the overlays family).
struct PopupTooltipDesc
{
    float maxWidth = 0.0f;     // pixels where the text wraps; 0 for overlay.tooltipMaxWidth
    // The tooltip takes the mouse and stays while the pointer is over it or on its way there from
    // the item (inside the box from where the pointer was when it showed to the tooltip, or aiming
    // at it with PopupPointerAimsAt), so its content can be hovered and clicked. A press inside it
    // does not hide it. It still never takes the keyboard focus.
    bool interactive = false;
};
[[nodiscard]] bool BeginPopupTooltip(ImGuiID itemId, const ImRect& itemRect, bool hovered, bool focusVisible,
                                     const PopupTooltipDesc& desc);

// The level id of the tooltip of item `itemId` (for IsPopupLevelOpen and tests).
ImGuiID PopupTooltipLevel(ImGuiID itemId);

// ---- For tests -------------------------------------------------------------------------------------

struct PopupHostStats
{
    int trackedLevels = 0;     // level records in use (open, closing or remembered)
    int closingLevels = 0;     // levels playing their close motion
    int replayVertices = 0;    // vertex capacity of all snapshots together
    int replayViews = 0;       // shader resource views the snapshots hold a reference on
    bool ownsEscape = false;   // the host is the owner of ImGuiKey_Escape for the current frame
};
PopupHostStats GetPopupHostStats();

} // namespace facet::internal
