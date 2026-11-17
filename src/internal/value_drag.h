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
//    pointer has to come back to the end before the value moves back from it.
//    Keyboard, while the control has keyboard focus: the arrows along the track (Left and Right
//    for horizontal, Down and Up for vertical) step, Shift finer, Ctrl coarser; Page Up and Page
//    Down move a tenth of the range; Home and End go to the ends. Held keys repeat at the
//    Dear ImGui key repeat rate, counted from the elapsed time, so the result does not depend
//    on the frame rate. The arrows along the track are taken from keyboard navigation while
//    the control is focused; the other arrows keep moving the focus.
//    Wheel: one step per notch when the pointer is over the control and the control is
//    focused or Alt is held. Otherwise the wheel scrolls the window as usual, so a page full of
//    sliders still scrolls.
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
// Precision: a drag maps the pointer travel since the press onto a fraction of the track and
// converts that fraction to a value each frame; nothing is accumulated in value units, so no
// rounding drift builds up. Fraction 0 is exactly `min` and 1 exactly `max`; stepped and
// integer values land exactly on min + n * step, and `max` is always reachable even when the
// range is not a whole number of steps. Keyboard steps of continuous values move on a grid of
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
    ValueScale scale = ValueScale::Linear;  // Logarithmic needs min > 0; otherwise Linear is used
    double step = 0.0;                      // > 0: values snap to min + n * step; 0: continuous
    bool integer = false;                   // whole numbers; the step is at least 1
    std::span<const double> stops = {};     // values a drag snaps to when the knob comes near
};

// The mapping with its invalid parts repaired, as every function below sees it: min > max
// swapped, a NaN bound replaced by the other one (or 0), a negative or NaN step treated as 0,
// an integer step rounded to a whole number and at least 1, Logarithmic with min <= 0 turned
// into Linear.
ValueMapping NormalizeMapping(const ValueMapping& mapping);

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
// first moves to the next grid point in the direction of travel. Clamped to the range.
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
    bool keyboard = true;   // arrows, Page Up and Down, Home and End while focused
    bool wheel = true;      // the mouse wheel as described at the top
    bool readOnly = false;  // shown and focusable, never written (also ImGuiItemFlags_ReadOnly)
    // Limits inside the range that an edit never passes, for a handle between neighbours: the
    // next value is clamped to them before it is written, so an edit that cannot move the value
    // writes nothing and its session ends without an edit. The defaults do not limit.
    double clampLow = -std::numeric_limits<double>::infinity();
    double clampHigh = std::numeric_limits<double>::infinity();
};

struct ValueDragResult
{
    bool changed = false;   // *value was written this frame
    bool canceled = false;  // Escape restored the value from the start of the edit (changed too)
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

// Ends an edit session of `id` by Escape on this frame: the query facet::IsItemEditCanceled
// reports it for that item. Used by ValueDragBehavior and by other blocks that revert edits.
void MarkEditCanceled(ImGuiID id);
// True when Escape ended the edit session of `id` on this frame.
bool WasEditCanceled(ImGuiID id);

// Forgets the edit session in progress and the last canceled edit (facet::Init and Shutdown,
// through the focus subsystem), so nothing carries over to another Dear ImGui context.
void ResetValueDragState();

} // namespace facet::internal
