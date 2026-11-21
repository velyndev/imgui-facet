#pragma once

// Parts the layers of the overlays family share, on the popup host (src/internal/popup_host.h):
// the open-flag protocol of a layer the caller opens, content that scrolls with the scroll core's
// overlay bars once it outgrows the display, the title row of dialogs and drawers, and the display
// area every layer stays inside. Names carry the family ("Overlay"), because every family links into
// one library.
//
//     const OverlayLayerSync sync = OverlaySyncLayer(level, open, anchor, options);  // every frame
//     if (BeginPopupLevel(level))
//     {
//         const OverlayScrollDesc scroll{.width = inner, .maxHeight = room, .surface = surface};
//         if (BeginOverlayScroll(scrollId, scroll))
//         {
//             ... the caller's content ...
//             EndOverlayScroll();
//         }
//         EndPopupLevel();
//     }

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/overlays.h"
#include "internal/popup_host.h"

namespace facet::internal
{

// The main viewport's work area less overlay.viewportMargin on every side, in screen pixels: where
// floating layers stay.
ImRect OverlayWorkArea();

// What OverlaySyncLayer did this frame.
struct OverlayLayerSync
{
    bool open = false;       // the level is open after the call: submit it
    bool opened = false;     // it opened on this frame (the caller wrote true)
    bool dismissed = false;  // Facet wrote *open = false on this frame (Escape, a click outside, a parent closing)
    PopupCloseReason reason = PopupCloseReason::None;  // why the host closed it, when it did on this frame
};

// The rule 8 protocol of a layer the caller opens, for level `level`: *open true opens the level at
// `anchor` with `options` (once; later frames keep it open), *open false closes it, and a level the
// host closed (Escape, a click outside, its parent closing, a frame without submission) writes
// *open = false. A null `open` closes the level and reports it closed. Call every frame, before
// BeginPopupLevel. State under `level`.
OverlayLayerSync OverlaySyncLayer(ImGuiID level, bool* open, const PopupAnchor& anchor, const PopupOptions& options);

// ---- Scrolling content --------------------------------------------------------------------------

// Lengths are pixels of the content box: the area adds the focus ring outset (FocusRingOutset) on
// every side, reaching into the layer's padding, so the content lines up with the layer's title.
struct OverlayScrollDesc
{
    float width = 0.0f;        // pixels; 0 follows the content width (at least minWidth)
    float minWidth = 0.0f;     // pixels; with width 0
    float maxWidth = 0.0f;     // pixels; with width 0; 0 for no limit
    float maxHeight = 0.0f;    // pixels; content taller than this scrolls; 0 for no limit
    bool fill = false;         // the area fills the room left in the level (drawers): no measuring
    ImVec4 surface = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);  // the level's surface, which the edge fades blend into
    bool edgeFades = true;     // false over a blurred surface, where a solid fade would show
};

// The content box (pixels) for this frame: the content measured in the last frame, within the
// limits of `desc`. A layer adds its own padding and passes the result on to its level size
// (SetPopupLevelLayout), so the level is as tall as its content until the content outgrows the
// display, and then the area scrolls.
ImVec2 OverlayScrollSize(ImGuiID id, const OverlayScrollDesc& desc);

// A scroll area core (scroll.h) around a content box of OverlayScrollSize (or the room left, with
// fill) at the cursor, with Facet's overlay bars and
// the level's surface as the fade surface. Returns false when the area is out of sight (then do not
// call EndOverlayScroll). Areas nest up to kOverlayScrollDepth deep.
inline constexpr int kOverlayScrollDepth = 8;
[[nodiscard]] bool BeginOverlayScroll(ImGuiID id, const OverlayScrollDesc& desc);
// Measures the content for the next frame and closes the area.
void EndOverlayScroll();

// ---- Title rows ----------------------------------------------------------------------------------

// The height in pixels of the title row of a dialog or drawer: a Title3 line, at least the height of
// a Small close button.
float OverlayTitleHeight();

// Draws the visible part of `label` as the title (Title3, label color) at `pos` (top-left), `width`
// pixels wide, cut with an ellipsis; the cut title shows in full as a tooltip on hover. Leaves the
// cursor where it was. `id` names the tooltip.
void DrawOverlayTitle(ImGuiID id, const char* label, ImVec2 pos, float width);

// The close button of the dialog or drawer `level`: a Plain, Small icon button named Strings::close
// with its right edge at `right` and centered on `centerY`. Returns true when it was activated.
bool OverlayCloseButton(ImGuiID level, float right, float centerY);

// ---- Where the parts are, for tests and demo selftests ------------------------------------------
//
// Screen rectangles of parts that a caller cannot record itself, from their latest frame. Each
// returns false (and leaves the outputs alone) when the part was not shown in the last two frames.
// The widgets record them under a key of the part's name and its owner (state_storage.h), up to
// three rectangles per key.
ImGuiID OverlayPartKey(const char* part, ImGuiID owner);
void OverlayRecordRects(ImGuiID key, const ImRect* rects, int count);
bool OverlayReadRects(ImGuiID key, ImRect* rects, int count);

// The close button of dialog or drawer `level` (the id of its label in its window).
bool OverlayCloseButtonRect(ImGuiID level, ImRect* rect);
// The Cancel and confirm buttons of confirm dialog `level`.
bool OverlayConfirmButtonRects(ImGuiID level, ImRect* cancel, ImRect* confirm);
// A toast, its action button (empty without one) and its close button.
bool OverlayToastRects(ToastId toast, ImRect* body, ImRect* action, ImRect* close);
// The row of entry `item` in command palette `level`, while it is visible in the list.
bool OverlayPaletteRowRect(ImGuiID level, int item, ImRect* rect);
// The Revert and Apply buttons of unsaved changes bar `id` (the id of its id string in its window).
bool OverlayBarButtonRects(ImGuiID id, ImRect* revert, ImRect* apply);
// The strip of watermark overlay `id`.
bool OverlayWatermarkRect(ImGuiID id, ImRect* rect);

// ---- Frame hooks (overlays_frame.cpp) -----------------------------------------------------------

// The stacks of open Begin / End pairs, emptied at facet::EndFrame: the scope stack has already
// reported a Begin left open, and the next frame starts clean.
void OverlayScrollResetStack();
void OverlayPopoverResetStack();
void OverlayModalResetStack();
void OverlayDrawerResetStack();
void OverlayLoadingResetStack();

// The toast stack (toast.cpp): NewFrame opens the queue for ShowToast, EndFrame draws the stack after
// every page, Shutdown forgets every toast.
void OverlayToastNewFrame();
void OverlayToastEndFrame();
void OverlayToastShutdown();

} // namespace facet::internal
