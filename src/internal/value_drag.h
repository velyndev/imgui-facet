#pragma once

// Value drag core: the mapping between a value and a position on a track, and the input that
// edits a value by dragging, by keyboard and by the mouse wheel. Sliders, knobs, drag fields,
// number inputs, color channel sliders and the curve editors are built on it.
//
// Two layers:
//
// 1. Pure math (no Dear ImGui context needed, unit tested directly): ValueMapping describes the
//    range, ValueToFraction / FractionToValue convert both ways, SnapValue applies the step,
//    KeyboardStep and PageStep compute the next value for a key press, DragTracker accumulates
//    pointer movement.
//
// 2. ValueDragBehavior, run after the item behaviour (item.h) of the control that owns the
//    value, every frame:
//
//        ItemState item = ItemBehavior(id, size, {.press = PressMode::Click});
//        double value = *userValue;
//        const ValueDragResult edit = ValueDragBehavior(item, &value, trackRect,
//                                                       {.mapping = {.min = 0.0, .max = 1.0}});
//        if (edit.changed) *userValue = static_cast<float>(value);
//        ... draw the knob at ValueToFraction(mapping, value) ...
//
//    Mouse: a press on the knob grabs it without moving the value (relative drag, the knob
//    keeps its offset to the pointer); a press elsewhere on the track first moves the value
//    under the pointer. While dragging, Shift moves ten times finer and Ctrl snaps to the
//    coarse grid (a tenth of the range, or ten steps). Leaving the track keeps the drag; the
//    pointer has to come back to the end before the value moves back from it (a wrapping
//    mapping goes on from the other end instead). While the pointer position is unknown the
//    value stays, and the drag goes on from where the pointer comes back.
//    Delta mode (ValueDragOptions::speed, drag fields and number inputs): no track; the value
//    follows the pointer travel since the press times the speed, from the value at the press,
//    over any range including an unbounded one.
//    Keyboard, while the control has keyboard focus: the arrows along the track (Left and Right
//    for horizontal, Down and Up for vertical) step, Shift finer, Ctrl coarser; Page Up and Page
//    Down move a tenth of the range; Home and End go to the ends. Held keys repeat at the
//    Dear ImGui key repeat rate, counted from the elapsed time, so the result does not depend
//    on the frame rate. The arrows along the track are taken from keyboard navigation while
//    the control is focused; the other arrows keep moving the focus.
//    Wheel: one step per notch when the pointer is over the control and the control is
//    focused or Alt is held. Otherwise the wheel scrolls the window as usual, so a page full of
//    sliders still scrolls. While the window under the pointer is still scrolling from earlier
//    wheel turns (scroll.h), the wheel stays with that window, so a focused control that slides
//    under a resting pointer does not take over in the middle of a scroll. Fractions of a notch
//    (touchpads) add up while the pointer stays on the control and the wheel keeps its direction.
//    Escape during a drag or a held key restores the value from the start of the edit, writes
//    it and ends the edit.
//
// Edit sessions and the item queries (docs/api-conventions.md, rule 3): a mouse drag is the
// item's active period (press to release). A key press that changes the value makes the item
// active as well, until no step key is held any more, so ImGui::IsItemActivated,
// IsItemDeactivated and IsItemDeactivatedAfterEdit report one session per drag and per key
// press (a held key with its repeats is one session). The value is marked edited on every
// write. facet::IsItemEditCommitted is true on the frame such a session ends with an edit,
// facet::IsItemEditCanceled on the frame Escape ended it.
//
// Precision: a drag maps the pointer travel since the press onto a fraction of the track (in
// delta mode, onto an offset from the value at the press) and converts it to a value each
// frame; nothing is accumulated in value units, so no rounding drift builds up. Fraction 0 is
// exactly `min` and 1 exactly `max`; stepped and integer values land exactly on
// min + n * step, and `max` is always reachable even when the range is not a whole number of
// steps. Keyboard steps of continuous values move on a grid of
// hundredths of the track, so a hundred presses from min end exactly on max.

#include <limits>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/common.h"
#include "internal/item.h"

namespace facet::internal
{

// ---- Mapping -------------------------------------------------------------------------------

struct ValueMapping
{
    double min = 0.0;
    double max = 1.0;
    // Logarithmic over a range above zero is ln(value). A range that reaches zero or below is
    // Linear (the fallback docs/api-conventions.md and common.h promise), unless logZero is set:
    // then it is logarithmic on both sides of zero, mirrored, with a linear zone from -logZero to
    // logZero joining them (continuous, with a continuous slope), and zero exactly reachable.
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;                      // > 0: values snap to min + n * step; 0: continuous
    bool integer = false;                   // whole numbers; the step is at least 1
    std::span<const double> stops = {};     // values a drag snaps to when the knob comes near
    double logZero = 0.0;                   // > 0: the half width of the linear zone (see scale)
    // The range is a circle (angles, hues): steps, wheel turns and drags past one end go on from
    // the other end, min and max are the same point. Needs finite bounds.
    bool wrap = false;
};

// The mapping with its invalid parts repaired, as every function below sees it: min > max
// swapped, a NaN bound replaced by the other one (or 0), a negative or NaN step treated as 0,
// an integer step rounded to a whole number and at least 1, Logarithmic with min <= 0 and no
// logZero (or with an infinite bound) turned into Linear, wrap without a finite range turned off.
// Infinite bounds are kept: only the delta mode (ValueDragOptions::speed) and OffsetValue
// support them, the fraction functions need finite ones.
ValueMapping NormalizeMapping(const ValueMapping& mapping);

// For a wrapping mapping, the value brought into the range by whole turns (a value at max stays
// max); otherwise unchanged.
double WrapValue(const ValueMapping& mapping, double value);

// `value + offset`, wrapped (wrap) or clamped to the range, on the step grid (from min, or from 0
// when min is infinite) and whole for integer mappings. The math of the delta mode.
double OffsetValue(const ValueMapping& mapping, double value, double offset);

// The value as a fraction of the track, 0 at min and 1 at max, clamped. NaN gives 0.
double ValueToFraction(const ValueMapping& mapping, double value);

// The value at a fraction of the track (clamped to 0 to 1), snapped by SnapValue. 0 returns
// exactly min and 1 exactly max.
double FractionToValue(const ValueMapping& mapping, double fraction);

// Clamps to the range, then rounds to the step grid (min + n * step, with max as the last
// stop) and to whole numbers for integer mappings. NaN gives min.
double SnapValue(const ValueMapping& mapping, double value);

// How large a keyboard step is.
enum class StepSize
{
    Normal,  // arrows
    Fine,    // Shift + arrows: a tenth of a step on continuous values; one step when stepped
    Coarse,  // Ctrl + arrows: ten steps, or a tenth of the track on continuous values
};

// Number of keyboard steps across the track of a continuous mapping, per StepSize.
inline constexpr double kKeyboardGrid = 100.0;
inline constexpr double kKeyboardGridFine = 1000.0;
inline constexpr double kKeyboardGridCoarse = 10.0;

// The value after `count` arrow presses in `direction` (+1 toward max, -1 toward min). With a
// step, it moves by whole steps from the grid point at or next to `value`. Without one, it moves
// on a grid of 1/kKeyboardGrid of the track (1/kKeyboardGridFine fine, 1/kKeyboardGridCoarse
// coarse) in fraction space, so logarithmic tracks step evenly too. A value between grid points
// first moves to the next grid point in the direction of travel. A value within the rounding of a
// float of a grid point (0.8f is 0.800000011920929 as a double) counts as on that point, so a
// caller that keeps its value in a float can pass it as it is. Clamped to the range.
double KeyboardStep(const ValueMapping& mapping, double value, int direction, StepSize size, int count = 1);

// The value after `count` Page Up (direction +1) or Page Down (-1) presses: a tenth of the
// track, and at least one step when stepped.
double PageStep(const ValueMapping& mapping, double value, int direction, int count = 1);

// The snap stop within `radiusFraction` (a fraction of the track) of `fraction`, as a value; or
// `value` unchanged when no stop is that close. The nearest stop wins.
double ApplySnapStops(const ValueMapping& mapping, double value, double fraction, double radiusFraction);

// ---- Drag accumulation ---------------------------------------------------------------------

// Pointer travel of one drag, converted to a fraction of the track. Every Move adds its delta
// times the speed factor, so a modifier pressed mid-drag changes only the movement after it;
// the fraction itself is never rounded, so it cannot drift.
class DragTracker
{
public:
    // Starts at `fraction` of the track, `trackLength` pixels long (at least 1).
    void Begin(double fraction, double trackLength);
    // Adds pointer movement along the track in pixels (positive toward max) times `factor`.
    void Move(double deltaPixels, double factor = 1.0);
    // The fraction reached, unclamped: the pointer can go beyond either end.
    double Fraction() const { return start + travel / length; }
    double StartFraction() const { return start; }

private:
    double start = 0.0;
    double travel = 0.0;
    double length = 1.0;
};

// Speed factor of a drag with Shift held (finer).
inline constexpr double kFineDragFactor = 0.1;

// ---- Behaviour -----------------------------------------------------------------------------

struct ValueDragOptions
{
    ValueMapping mapping;
    Orientation orientation = Orientation::Horizontal;
    // Pixels from the knob centre within which a press grabs the knob without moving the value;
    // a press further away first moves the value under the pointer. A negative radius makes every
    // press a grab (drag fields without a track).
    float grabRadius = 0.0f;
    // Pixels of pointer travel per whole range for drag fields; 0 uses the track length.
    float dragLength = 0.0f;
    // Delta mode, for drag fields and number inputs over any range, also an unbounded one: with
    // speed > 0 (value units per pixel) a drag sets the value at the press plus the pointer
    // travel since the press times speed (Shift a tenth, Ctrl ten times), through OffsetValue;
    // there is no track and every press grabs. The keys and the wheel step by keyStep (Shift a
    // tenth, Ctrl, Page Up and Page Down ten times); Home and End go to finite ends only.
    double speed = 0.0;
    double keyStep = 0.0;  // value units; 0: the mapping's step, else speed
    bool keyboard = true;   // arrows, Page Up and Down, Home and End while focused
    bool wheel = true;      // the mouse wheel as described at the top
    bool readOnly = false;  // shown and focusable, never written (also ImGuiItemFlags_ReadOnly)
    // Limits inside the range that an edit never passes, for a handle between neighbours: the
    // next value is clamped to them before it is written, so an edit that cannot move the value
    // writes nothing and its session ends without an edit. The defaults do not limit.
    double clampLow = -std::numeric_limits<double>::infinity();
    double clampHigh = std::numeric_limits<double>::infinity();
    // The arrows across the track step too, Right and Up toward max, Left and Down toward min,
    // and are taken from keyboard navigation while focused: for a control with no direction of
    // its own on screen, such as a knob. Off: those arrows keep moving the focus.
    bool crossArrows = false;
};

// Speed factor of a delta mode drag or step with Ctrl held (coarser), and of Page Up and Down.
inline constexpr double kCoarseDeltaFactor = 10.0;

struct ValueDragResult
{
    bool changed = false;   // *value was written this frame
    bool canceled = false;  // Escape ended the edit; changed too when the value had moved and was restored
    bool dragging = false;  // a mouse drag is under way
    bool keyboard = false;  // a key edit is under way
};

// Runs the drag, keyboard and wheel input of the control `item` (from ItemBehavior or
// PartBehavior this frame) on *value. `track` is the rectangle the knob centre travels along:
// fraction 0 at its left (horizontal) or bottom (vertical) edge, 1 at the other end. Returns
// what happened. A disabled or read-only item never writes and leaves the arrow keys to
// keyboard navigation; an item showing a forced state (captures) takes no input at all. A value
// outside the range is left as it is until an edit writes a new one.
ValueDragResult ValueDragBehavior(const ItemState& item, double* value, const ImRect& track,
                                  const ValueDragOptions& options);

// The position along `track` (x for horizontal, y for vertical) of a fraction, and back.
float FractionToTrack(const ImRect& track, Orientation orientation, double fraction);
double TrackToFraction(const ImRect& track, Orientation orientation, ImVec2 point);

// ---- Two axes ------------------------------------------------------------------------------
//
// ValueDrag2DBehavior: the same input for a point on a plane, for the pads (XYPad, JoystickPad)
// and the edges and corners of a box (BoxRangePad). x runs along the width of `area`, y along
// its height, each with a mapping of its own:
//
//     ItemState item = ItemBehavior(id, size, {.press = PressMode::Click});
//     double x = value->x, y = value->y;
//     const ValueDrag2DResult edit = ValueDrag2DBehavior(item, &x, &y, area,
//                                                        {.mappingX = mx, .mappingY = my});
//     if (edit.changed) *value = ImVec2(static_cast<float>(x), static_cast<float>(y));
//     ... draw the point at PadPoint(area, mx, my, false, x, y) ...
//
// Mouse: as ValueDragBehavior on both axes at once. A press within grabRadius of the point grabs
// it without moving it, a press further away first moves the point under the pointer. The
// pointer travel since the press maps onto fractions of the area, so a drag out of the window and
// back returns exactly to the values it passed. Shift during a drag keeps the axis the drag goes
// along when Shift is pressed (the one with the larger travel since the press) until Shift is let
// go; the other axis goes back to its value at the press. Ctrl snaps both axes to the coarse
// grid. There is no fine drag: Shift is the axis lock.
// Keyboard, while the item has keyboard focus, in PadKeys::Step: Left and Right step x, Up and
// Down move the point up and down on screen (toward max.y, or toward min.y with yDown), by the
// steps of KeyboardStep with Shift finer and Ctrl coarser; Page Up and Page Down move it up and
// down by a tenth of the y range; Home and End send x to min and max (y, for an item that moves
// y only). In PadKeys::Hold the arrows hold the point at the edge of the range in their direction
// while they are down (two arrows: the diagonal; Shift: half way), for a stick that springs back.
// The arrows of an axis the item moves are taken from keyboard navigation; the others, Tab and
// the axes the item does not move keep moving the focus. The wheel is left to the window.
// Escape during a drag or a key session restores both values from its start.
// Edit sessions: as ValueDragBehavior, one per drag and per key press with its repeats (in Hold,
// while any arrow is down), so the item queries and facet::IsItemEditCommitted report each once.

// What the arrow keys do on a two-axis item.
enum class PadKeys
{
    Step,  // step the point like a slider's arrows
    Hold,  // hold the point at the edge while down (a stick); the caller decides what release does
};

struct ValueDrag2DOptions
{
    ValueMapping mappingX;
    ValueMapping mappingY;
    bool yDown = false;      // fraction 0 of y at the top of the area; by default at the bottom
    // Pixels from the point within which a press grabs it; a press further away first moves the
    // point under the pointer. Negative: every press grabs (a box body).
    float grabRadius = 0.0f;
    bool moveX = true;       // the axes the input moves; the other keeps its value
    bool moveY = true;
    // The point stays inside the ellipse inscribed in the two ranges: a drag or key that would
    // leave it ends on its edge in the same direction from the middle (a round stick).
    bool disc = false;
    bool axisLock = true;    // Shift during a drag keeps one axis (see above)
    bool keyboard = true;
    PadKeys keys = PadKeys::Step;
    bool readOnly = false;   // shown and focusable, never written (also ImGuiItemFlags_ReadOnly)
    // Limits inside the ranges that an edit never passes, per axis, as ValueDragOptions has.
    double clampLowX = -std::numeric_limits<double>::infinity();
    double clampHighX = std::numeric_limits<double>::infinity();
    double clampLowY = -std::numeric_limits<double>::infinity();
    double clampHighY = std::numeric_limits<double>::infinity();
};

struct ValueDrag2DResult
{
    bool changed = false;   // *x or *y was written this frame
    bool canceled = false;  // Escape ended the edit; changed too when the point had moved and was restored
    bool dragging = false;  // a mouse drag is under way
    bool keyboard = false;  // a key session is under way
    bool ended = false;     // a drag or key session of this item ended this frame (release, keys up, Escape, focus loss)
};

// Runs the drag and keyboard input of `item` (from ItemBehavior or PartBehavior this frame) on
// the point (*x, *y). `area` is the rectangle the point travels in: fraction 0 of x at its left
// edge, fraction 0 of y at its bottom edge (top with yDown). A disabled or read-only item never
// writes and leaves every arrow to keyboard navigation; an item showing a forced state takes no
// input. A value outside its range is left as it is until an edit writes a new one.
ValueDrag2DResult ValueDrag2DBehavior(const ItemState& item, double* x, double* y, const ImRect& area,
                                      const ValueDrag2DOptions& options);

// The screen position in `area` of the point (x, y), clamped into the area.
ImVec2 PadPoint(const ImRect& area, const ValueMapping& mappingX, const ValueMapping& mappingY, bool yDown, double x,
                double y);
// The fractions of a screen position in `area`, not clamped (a pointer can be outside).
void PadFractions(const ImRect& area, bool yDown, ImVec2 point, double* fx, double* fy);
// Moves fractions (fx, fy) that lie outside the ellipse inscribed in the unit square onto its
// edge, toward the middle (0.5, 0.5); fractions inside stay.
void ClampToDisc(double* fx, double* fy);

// Ends an edit session of `id` by Escape on this frame: the query facet::IsItemEditCanceled
// reports it for that item. Used by ValueDragBehavior and by other blocks that revert edits.
void MarkEditCanceled(ImGuiID id);
// True when Escape ended the edit session of `id` on this frame.
bool WasEditCanceled(ImGuiID id);

// Forgets the edit sessions in progress (one and two axes) and the last canceled edit
// (facet::Init and Shutdown, through the focus subsystem), so nothing carries over to another
// Dear ImGui context.
void ResetValueDragState();

} // namespace facet::internal
