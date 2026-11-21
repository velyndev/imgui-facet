#pragma once

// Helpers shared by the widgets of the layout family (layout.h). Every name carries "Layout" or
// the widget's name, because every family links into one library.
//
// The divider of SplitView and Panels, one item on the band between two panes:
//
//     const LayoutDividerInput input = LayoutDivider(id, band, ImGuiAxis_X, firstSize, origin, options);
//     if (input.dragging)           firstSize = input.dragTarget;      // pixels from `origin`
//     else if (input.steps != 0)    firstSize += input.steps * input.stepPixels;
//     if (input.home) ...  if (input.end) ...  if (input.toggle) ...  if (input.reset) ...
//     if (input.canceled) ...                                           // Escape during a drag
//     DrawLayoutDivider(input.item, bandWhereItEndsUp, ImGuiAxis_X);
//
// The work rect narrowing that stacks, grids and flows use for their content, so items that fill
// the available width stop at the container's edge:
//
//     const LayoutWorkRect saved = NarrowLayoutWorkRect(window, right);
//     ... content ...
//     RestoreLayoutWorkRect(saved);
//
// Lengths are pixels.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/layout.h"
#include "internal/item.h"
#include "internal/row_layout.h"

namespace facet::internal
{

// Pixels of a gap token.
float LayoutGapPixels(Gap gap);

// ---- Work rect ----------------------------------------------------------------------------------

struct LayoutWorkRect
{
    ImGuiWindow* window = nullptr;
    float workMaxX = 0.0f;
    float contentMaxX = 0.0f;
};

// Narrows the work rect and the content region of `window` to end at `right` (never widens them)
// and returns what to put back.
LayoutWorkRect NarrowLayoutWorkRect(ImGuiWindow* window, float right);
// Puts back what NarrowLayoutWorkRect changed. Does nothing for an empty record.
void RestoreLayoutWorkRect(const LayoutWorkRect& saved);

// ---- Divider --------------------------------------------------------------------------------------

// Multiplier of the plain arrow step (spacing.lg) for Ctrl: a coarse step of four times
// spacing.xxxl, so a divider crosses a wide region in a few presses.
inline constexpr float kLayoutDividerCoarseSteps = 4.0f;

struct LayoutDividerInput
{
    ItemState item;            // the divider item (PartBehavior on the band)
    bool dragging = false;     // held by the mouse: dragTarget is valid
    float dragTarget = 0.0f;   // pixels from `origin` where the pointer puts the divider this frame
    bool started = false;      // a mouse drag began this frame
    bool canceled = false;     // Escape ended a mouse drag: put the value from its start back
    int steps = 0;             // arrow presses this frame toward the end (+) or the start (-)
    float stepPixels = 0.0f;   // pixels per step: spacing.lg, spacing.xs with Shift, coarse with Ctrl
    bool home = false;         // Home: the divider to its start
    bool end = false;          // End: the divider to its end
    bool toggle = false;       // Enter or Space on the focused divider
    bool reset = false;        // a double click on the divider
};

// Runs the divider `id` on `band` (screen pixels; the space is already laid out). `axis` is the
// axis it moves along (ImGuiAxis_X for panes side by side), `split` the divider's current offset
// from `origin` (the first pane's start on that axis). Shows the resize cursor while hovered or
// dragged. While focused it owns the arrow keys of its axis, Home and End; while dragged it owns
// Escape. `options` go to the item behaviour (forced states, disabled). Draws nothing: the caller
// applies the input and then draws the divider where it ends up with DrawLayoutDivider.
LayoutDividerInput LayoutDivider(ImGuiID id, const ImRect& band, ImGuiAxis axis, float split, float origin,
                                 const ItemOptions& options = {});

// Draws a divider on `band`: the hairline across it in the separator color, a grip in the middle
// (scrollThumb at rest, scrollThumbHover while hovered, dragged or focused; labelTertiary while
// disabled), and the focus ring of `item` around the band.
void DrawLayoutDivider(const ItemState& item, const ImRect& band, ImGuiAxis axis);

// ---- Split view -----------------------------------------------------------------------------------

// The divider of the split view ended last (an empty record before any), for tests and probes.
struct LayoutDividerRecord
{
    ImGuiID id = 0;
    ImRect band;     // screen pixels, where it was drawn
    bool vertical = false;  // the panes sit one above the other
};
const LayoutDividerRecord& LastSplitViewDivider();

// ---- Settings row ---------------------------------------------------------------------------------

// The layout of the settings row ended last (valid until the next EndSettingsRow), for tests.
const RowLayout& LastSettingsRowLayout();

// ---- Frame end ------------------------------------------------------------------------------------

// facet::EndFrame (layout_frame.cpp): every widget of the family forgets the Begins whose End was
// not called (the scope stack reports them) and puts back what it changed in a window.
void ClearSettingsRowStack();
void ClearStackStack();
void ClearGridStack();
void ClearFlowStack();
void ClearSplitViewStack();
void ClearDependentGroupStack();
void ClearListDetailStack();
void ClearPanelsStack();
void ClearItemGridStack();

} // namespace facet::internal
