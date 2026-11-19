#pragma once

// Scroll core: smooth scrolling, overlay scrollbars and edge fades for the Dear ImGui windows
// that scroll (top-level and child windows).
//
// This header holds what the other blocks call: the list core scrolls a row into view, the
// drag and drop core scrolls an area while a drag rests near its edge, the popup host and the
// text field core bring their content into view. The scroll core owns the rest (the overlay
// bars, the edge fades, the smooth wheel) and adds it to this file and to further scroll_*
// files.
//
// The scroll position of a window stays Dear ImGui's own (ImGuiWindow::Scroll): the scroll core
// only moves it over time. So ImGui::SetScrollY, ScrollToItem and keyboard navigation keep
// working next to these calls, and a caller that reads ImGui::GetScrollY sees where the area is
// now, not where it is going. Positions and rectangles are in pixels: scroll offsets as Dear
// ImGui counts them, rectangles in screen space. A window pointer is valid for the current
// frame only; keep window->ID across frames.
//
// Motion: a smooth scroll moves toward its target with the standard spring of the theme and
// jumps there at once while facet::ReduceMotion() is true. A new target replaces the old one;
// a user scroll (wheel, scrollbar drag, keyboard navigation) cancels a running animation.
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

// Scrolls `window` so that `rect` (screen pixels, inside the window's content) lies in its
// visible area, placed by `align` on both axes; a rect larger than the visible area shows its
// start. Uses ScrollAreaTo, so the same motion and clamping apply.
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

// Edge scroll for a drag held near an edge: asks `window` to scroll this frame because a drag
// rests at `point` (screen pixels). Inside the band of the drag.autoScrollZone token along an
// edge, or past it, the area scrolls toward that edge at up to drag.autoScrollSpeed (units per
// second, frame-rate independent), faster the closer the point is to the edge. Call it every
// frame the drag wants it; a request lasts one frame. Returns true when the window scrolled
// this frame.
bool RequestScrollAreaEdgeScroll(ImGuiWindow* window, ImVec2 point);

} // namespace facet::internal
