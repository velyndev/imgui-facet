#pragma once

// Scroll core: smooth scrolling, overlay scrollbars and edge fades for the Dear ImGui windows
// that scroll (top-level and child windows), and the scroll area that Facet widgets scroll in.
//
// Two layers:
//   - Calls on any window (ScrollAreaTo, ScrollAreaRevealRect, ScrollAreaAt, edge scroll): the
//     list core scrolls a row into view, the drag and drop core scrolls an area while a drag
//     rests near its edge, the popup host and the text field core bring content into view.
//   - The scroll area (BeginScrollAreaCore / EndScrollAreaCore): a child window whose stock
//     scrollbar is hidden (through window flags; the host's style is never touched) and whose
//     overlay bars, edge fades and wheel handling are Facet's. ScrollArea, VirtualList, the
//     list widgets, LogView and the popup lists are built on it.
//
//     ScrollAreaDesc desc{.size = ImVec2(0.0f, Px(240.0f))};
//     if (BeginScrollAreaCore("##log", desc))
//     {
//         ... content; ImGui::SetScrollHereY and keyboard navigation keep working ...
//         EndScrollAreaCore();                        // only after true
//     }
//
// The scroll position of a window stays Dear ImGui's own (ImGuiWindow::Scroll): the scroll core
// only moves it over time. So ImGui::SetScrollY, ScrollToItem and keyboard navigation keep
// working next to these calls, and a caller that reads ImGui::GetScrollY sees where the area is
// now, not where it is going. Positions and rectangles are in pixels: scroll offsets as Dear
// ImGui counts them, rectangles in screen space. A window pointer is valid for the current
// frame only; keep window->ID across frames. Dear ImGui keeps the scroll offset of a child
// window for as long as the context lives, so an area that disappears (another page, a closed
// panel) and comes back under the same id is where it was.
//
// Motion: a smooth scroll moves toward its target on a critically damped spring that settles in
// about motion.durationStandard (times motion.scale), solved in closed form for the frame time,
// so it covers the same distance in the same time at 30 and at 240 frames per second. It jumps
// there at once while facet::ReduceMotion() is true. A new target replaces the old one and keeps
// the speed; any other scroll of the window (a stock SetScroll call, keyboard navigation, a
// scrollbar drag) cancels the motion on that axis, so the core never fights it.
//
// The wheel: while the pointer is over a scroll area, the scroll core takes the mouse wheel from
// Dear ImGui (key ownership of ImGuiKey_MouseWheelX and Y, renewed every frame in
// facet::NewFrame) and hands each turn to the innermost window under the pointer that can still
// scroll in that direction, measured at its target: an inner area at its end passes the rest of
// the gesture to the area or stock window around it at once. Stock windows in that chain scroll
// smoothly too; windows with ImGuiWindowFlags_NoScrollWithMouse or NoMouseInputs are passed over.
// One notch moves T().scroll.wheelDistance units; fractions of a notch (touchpads) move the same
// fraction. Shift turns the vertical wheel into a horizontal one and Ctrl leaves the wheel alone,
// as in Dear ImGui. A gesture stays with the window it scrolls while that window can still scroll
// and the pointer rests (it does not move by more than io.MouseDragThreshold), so an area sliding
// under a resting pointer does not take over; a mouse press, a turn the other way or
// scroll.barHideDelay without a turn ends the gesture. A control that claimed the wheel for itself in the previous frame (a focused slider
// under the pointer) keeps it, and keyboard focus alone never routes the wheel anywhere. The
// first frame the pointer is over an area, Dear ImGui still routes the wheel itself.
//
//     // List core: bring the focused row into view.
//     ScrollAreaRevealRect(ImGui::GetCurrentWindow(), rowRect, ScrollAlign::Nearest);
//
//     // Drag and drop core: every frame a drag is over an area.
//     if (ImGuiWindow* area = ScrollAreaAt(pointer))
//         RequestScrollAreaEdgeScroll(area, pointer);

#include <imgui.h>
#include <imgui_internal.h>

namespace facet::internal
{

// Where a rectangle lands when it is scrolled into view, per axis.
enum class ScrollAlign
{
    Nearest,  // the least movement that shows it: nothing when it is already visible
    Start,    // at the top (or left) edge of the visible area
    Center,   // in the middle of the visible area
    End,      // at the bottom (or right) edge of the visible area
};

// Scrolls `window` to the scroll offset `target` (pixels per axis, clamped to the window's
// scroll range). A component below 0 leaves that axis alone. With `animate` the move is smooth
// (see the top of this file), otherwise it happens at once. Does nothing for a null window.
void ScrollAreaTo(ImGuiWindow* window, ImVec2 target, bool animate = true);

// Scrolls `window` so that `rect` (screen pixels, inside the window's content, at the current
// scroll offset) lies in its visible area less the window padding, placed by `align` on both
// axes; a rect larger than the visible area shows its start. Nearest compares with where the
// window is going (ScrollAreaTarget), so reveals during a smooth scroll stay stable. Uses
// ScrollAreaTo, so the same motion and clamping apply.
void ScrollAreaRevealRect(ImGuiWindow* window, const ImRect& rect, ScrollAlign align = ScrollAlign::Nearest,
                          bool animate = true);

// The scroll offset `window` is moving to: the target of a running smooth scroll, else its
// current offset. Zero for a null window.
ImVec2 ScrollAreaTarget(ImGuiWindow* window);

// True while `window` is being scrolled: a smooth scroll runs, the wheel turned over it this
// frame, its scrollbar is dragged, or an edge scroll moved it this frame. Hover effects and
// tooltips stay quiet while this is true, so content passing under a resting pointer does not
// flicker.
bool IsScrollAreaScrolling(ImGuiWindow* window);

// The innermost window under `point` (screen pixels) that can still scroll on some axis, or
// null. Child windows count before their parents; popups and other floating layers before the
// windows below them.
ImGuiWindow* ScrollAreaAt(ImVec2 point);

// True while the area under the pointer (ScrollAreaAt(io.MousePos)) is still scrolling
// (IsScrollAreaScrolling). Every control that reads the wheel itself (a focused slider, a number
// field stepping while typing) checks it first and leaves the wheel alone then, so a control that
// slides under a resting pointer in the middle of a scroll gesture does not take the wheel over.
bool ScrollWheelBelongsToScroll();

// Edge scroll for a drag held near an edge: asks `window` to scroll this frame because a drag
// rests at `point` (screen pixels). Inside the band of the drag.autoScrollZone token along an
// edge, or past it, the area scrolls toward that edge at up to drag.autoScrollSpeed (units per
// second, frame-rate independent), faster the closer the point is to the edge. Call it every
// frame the drag wants it; a request lasts one frame. Returns true when the window scrolled
// this frame.
bool RequestScrollAreaEdgeScroll(ImGuiWindow* window, ImVec2 point);

// ---- Scroll areas -------------------------------------------------------------------------------

struct ScrollAreaDesc
{
    ImVec2 size = ImVec2(0.0f, 0.0f);  // pixels; 0 fills the available width or height, < 0 leaves that much free
    bool horizontal = false;           // also scrolls sideways; otherwise only vertically
    bool fitHeight = false;            // the height follows the content (one frame behind), so it never scrolls vertically
    bool edgeFades = true;             // a fade of the surface where content continues past an edge
    // The surface under the area, the color its edge fades blend into. Alpha 0 (the default)
    // takes the innermost PushScrollFadeSurface pushed in the same root window (a popup does not
    // take a surface the page pushed), else bgElevated inside a popup or tooltip and bg
    // everywhere else.
    ImVec4 fadeSurface = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    // Pixels between the area's edge and its content on every side; < 0 takes FocusRingOutset()
    // (focus.h), so the focus ring of content flush with the padding shows whole.
    float padding = -1.0f;
};

// Opens a scroll area under `id` (hashed on the current id stack; the content gets ids under the
// area) at the cursor, laid out like one item of the area's size. The area is a Dear ImGui child
// window with ImGuiChildFlags_NavFlattened, so keyboard navigation crosses into it and scrolls it
// to the focused item; Page Up and Down, Home and End move the focus by a page or to the ends.
// It clips its content at its own rectangle. Its overlay bars are thin while the pointer is
// elsewhere, wider while it is over a bar or drags one, and fade out scroll.barHideDelay after
// the last scroll while the area is not hovered; dragging a thumb scrolls with it and pressing
// the track pages toward the pointer, repeating while held. The bars are mouse affordances: they
// take no keyboard focus. A capture can force the bar's look: ForceNextItemState (item.h) right
// before this call shows the vertical bar (part 0, or -1) or the horizontal one (part 1) hovered,
// pressed or dragging.
// Returns false while the area is out of sight; the child window is then already closed and
// EndScrollAreaCore must not be called. Areas nest up to kScrollAreaDepth deep.
inline constexpr int kScrollAreaDepth = 16;
[[nodiscard]] bool BeginScrollAreaCore(ImGuiID id, const ScrollAreaDesc& desc = {});
[[nodiscard]] bool BeginScrollAreaCore(const char* id, const ScrollAreaDesc& desc = {});
// Draws the edge fades and the bars over the content, child windows of the area included, and
// closes the area.
void EndScrollAreaCore();

// The overlay bars and the wheel handling of a scroll area for a child window that another call
// begins (a Dear ImGui table's inner window, which BeginTable creates itself). Ask for the window
// without room for stock bars (ImGuiStyleVar_ScrollbarSize 0 while it begins; NoScrollbar would also
// keep its content from being wider than the window) and without Dear ImGui's own wheel handling
// (ImGui::SetNextWindowFlags with ImGuiWindowFlags_NoScrollWithMouse right before that call), then
// call BeginScrollAreaBars while it is the current window, before its content, and
// EndScrollAreaBars once its content is done; the window may have ended by then (EndTable), the
// bars are drawn on top of what it drew. The bars, the wheel, their motion and the forced capture
// states are those of BeginScrollAreaCore; there is no padding, no clip rect and no edge fade.
// The pairs share the nesting of the scroll areas (kScrollAreaDepth).
struct ScrollAreaBarsDesc
{
    bool horizontal = false;  // also the horizontal bar and the sideways wheel
    float insetTop = 0.0f;    // pixels at the top the vertical bar leaves free (a header that stays in view)
    float insetLeft = 0.0f;   // pixels at the left the horizontal bar leaves free (columns that stay in view)
};
void BeginScrollAreaBars(const ScrollAreaBarsDesc& desc = {});
void EndScrollAreaBars();

// The bar of `axis` of a scroll area as drawn in the latest frame: the track it runs in and its
// thumb (screen pixels). False when the area has no bar on that axis (its content fits) or
// `window` is not a scroll area. For probes in tests and selftests.
bool ScrollAreaBarRects(ImGuiWindow* window, ImGuiAxis axis, ImRect* track, ImRect* thumb);

// The surface the edge fades of the scroll areas opened until the matching PopScrollFadeSurface
// blend into: a container that draws its own surface (a card, a panel, a popover) pushes its
// color around its content. A push belongs to the root window it was made in: scroll areas of
// other root windows opened inside it (popup levels) ignore it. Pushes nest up to
// kScrollFadeSurfaceDepth deep; deeper pushes are
// reported and ignored (their pops still balance), a pop without a push is reported, and
// facet::EndFrame reports surfaces still pushed and clears them.
inline constexpr int kScrollFadeSurfaceDepth = 8;
void PushScrollFadeSurface(const ImVec4& surface);
void PopScrollFadeSurface();

} // namespace facet::internal
