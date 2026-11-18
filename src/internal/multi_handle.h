#pragma once

// Multi-handle model: several handles on one track, the base of the range slider (two handles),
// the multi slider (N handles), range inputs, histograms with a range, the allocation bar, the
// chart range brush, image compare and, per axis, the box range pad.
//
// Two layers:
//
// 1. Pure logic (no Dear ImGui context needed, unit tested directly). Handle values are kept in
//    ascending order in one span (in any order with CollisionMode::Free). HandleConstraints
//    holds the range, the minimum gap between neighbours, the collision mode and the optional
//    mirror. MoveHandle moves one handle and
//    resolves the constraints, MoveSpan moves a run of handles together, ConstrainHandles
//    repairs values that came in invalid, InsertHandle and RemoveHandle change the count.
//    PickHandle (along a track) and PickHandle2D (on a plane) choose the handle a press takes,
//    HandleDrawOrder the order to draw them so the one a press takes is on top. The same
//    functions serve one dimension, two dimensions (one HandleConstraints per axis; a corner
//    handle moves one handle on each axis) and an arc (values are angles or fractions of the
//    sweep).
//
// 2. MultiHandleBehavior: the items of a widget with handles along a straight track, every
//    frame between ImGui::BeginGroup and EndGroup:
//
//        double values[2] = {*low, *high};
//        ItemState handles[2];
//        ImGui::BeginGroup();
//        const MultiHandleResult r = MultiHandleBehavior(id, widgetRect, track, values, handles, options);
//        ImGui::EndGroup();
//        FinishWidgetGroup(id);   // item.h: the item queries describe the whole widget
//        if (r.changed) { *low = values[0]; *high = values[1]; }
//        ... draw the track, the span, then the handles in HandleDrawOrder, each with
//            DrawFocusRing around handles[i].rect at handles[i].focusAmount ...
//
//    The items: one track item over the whole widget that takes every mouse press (never
//    focused by the keyboard), one item per handle (each its own id under the widget id, a Tab
//    stop in index order, moved by the arrow keys through the value drag core), and one span
//    item for dragging the run between the first and the last handle (never focused by the
//    keyboard). A press on the track picks a handle with PickHandle: within half a handle of
//    one it grabs that handle without moving it, between the first and the last handle (when
//    span dragging is on) it grabs the span, anywhere else it moves the nearest handle under
//    the pointer. A press on handles that sit at one position waits for the first movement
//    along the track: toward max the last of them takes the drag, toward min the first, so
//    stacked handles can always be pulled apart (a press released without moving focuses the
//    one PickHandle chose). The grabbed part then becomes Dear ImGui's active item and keeps
//    the drag until release; Escape restores every handle to where the drag or key press began.

#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/item.h"
#include "internal/value_drag.h"

namespace facet::internal
{

inline constexpr int kMaxHandles = 16;

// What a moving handle does when it meets its neighbour.
enum class CollisionMode
{
    Clamp,  // it stops one minimum gap before the neighbour
    Push,   // it pushes the neighbour (and the ones beyond) along until the range ends
    // Handles pass each other: the values keep no order (gradient stops, markers). No minimum
    // gap, no mirror and no span; picking and the draw order sort the handles by position.
    Free,
};

struct HandleConstraints
{
    double min = 0.0;
    double max = 1.0;
    double minGap = 0.0;  // smallest distance between neighbours; clamped to fit the range
    CollisionMode collision = CollisionMode::Clamp;
    // Two handles only: moving one moves the other to the mirrored value around `center`, so
    // the pair stays symmetric. Collisions then cannot happen; the gap and the range still hold.
    bool mirror = false;
    double center = 0.5;
};

// The minimum gap the constraints really use for `count` handles: minGap clamped to 0 to
// (max - min) / (count - 1). NaN counts as 0.
double EffectiveGap(const HandleConstraints& constraints, int count);

// Repairs values that break the constraints (out of range, out of order, closer than the gap)
// with the least movement in index order. For values that came from the caller, before drawing;
// the caller's variables are written only after an edit.
void ConstrainHandles(std::span<double> values, const HandleConstraints& constraints);

// Moves handle `index` to `target` and resolves the constraints. Returns true when any value
// changed. An index outside the span or a NaN target changes nothing.
bool MoveHandle(std::span<double> values, int index, double target, const HandleConstraints& constraints);

// Moves handles first to last (inclusive) by `delta`, keeping their distances. At the range ends
// and at the neighbours outside the run (one minimum gap away) the run stops as a whole, so its
// distances never change. Returns true when the values changed.
bool MoveSpan(std::span<double> values, int first, int last, double delta, const HandleConstraints& constraints);

// Inserts a handle at `value` into the first `count` entries of `storage` (ascending), keeping
// the order; free handles go last. Returns its index, or -1 when storage is full or the value is
// closer than the minimum gap to a neighbour. A caller with handle keys inserts a new key at
// the same index.
int InsertHandle(std::span<double> storage, int count, double value, const HandleConstraints& constraints);

// Removes handle `index` from the first `count` values. Returns the new count; a count at or
// below `minCount` or an index outside the range removes nothing.
int RemoveHandle(std::span<double> values, int count, int index, int minCount = 1);

// The handle a press at `pointer` takes, from the handles' positions along a track (ascending,
// in pixels): the nearest one. Handles at the same position are ordered so the one that can
// still move toward the pointer wins: the last of them when the pointer is beyond them, the
// first when it is before them; with the pointer right on them, the first at the track's end
// (trackEnd) and the last at its start (trackStart), elsewhere `preferred` when it is one of
// them, else the last. -1 when there are no handles.
int PickHandle(std::span<const float> positions, float pointer, float trackStart, float trackEnd, int preferred = -1);

// The handle nearest to `pointer` on a plane within `maxDistance` pixels, -1 when none is that
// close. Among handles at the same distance the later one wins (it is drawn on top).
int PickHandle2D(std::span<const ImVec2> positions, ImVec2 pointer, float maxDistance);

// The order to draw the handles in, bottom first, so that wherever handles overlap the one a
// press would take is drawn last: at the range end the first handle of an overlapping run is on
// top, at the range start the last, and `top` (the active or focused handle, or -1) above all.
// `order` receives values.size() indices.
void HandleDrawOrder(std::span<const double> values, const HandleConstraints& constraints, int top,
                     std::span<int> order);

// ---- Behaviour -----------------------------------------------------------------------------

struct MultiHandleOptions
{
    HandleConstraints constraints;  // min and max should equal the mapping's
    ValueDragOptions drag;          // mapping, orientation, keyboard, wheel, readOnly per handle
    float handleSize = 0.0f;        // pixels, the square of a handle; 0: the slider knob token
    bool spanDrag = true;           // with two handles or more, the span between first and last drags
    bool removeKeys = false;        // Delete and Backspace on a focused handle ask to remove it
    // One key per handle (same count as the values, unique within the widget): the handle's
    // identity. Its item id, keyboard focus, drag and animations follow the key, so they stay on
    // the same handle when handles are inserted or removed before it. Empty: the index is the
    // identity, which suits a fixed number of handles.
    std::span<const ImGuiID> keys = {};
};

struct MultiHandleResult
{
    bool changed = false;    // a value was written this frame
    bool canceled = false;   // Escape restored the values from the start of the edit
    int activeHandle = -1;   // the handle being dragged or stepped, -1 none
    int hoveredHandle = -1;  // the handle a press would take, while the pointer is over the widget
    int focusedHandle = -1;  // the handle with keyboard focus
    bool spanActive = false; // the span is being dragged
    bool spanHovered = false;// a press would grab the span
    int removeRequested = -1;// with removeKeys: the focused handle whose removal was asked for
    ItemState track;         // the track item (hover, disabled, cursor)
    ItemState span;          // the span item (meaningful with two handles or more)
};

// Runs the items of a multi-handle widget. `widgetRect` is the area that takes the mouse (the
// track with the handles), `track` the line the handle centres travel along (fraction 0 at its
// left or bottom end). `values` holds the handle values in ascending order (any order for free
// handles; at most kMaxHandles); they are repaired with ConstrainHandles first, then edited.
// `handles` receives the item state of each handle (same count as values). The part index of
// handle i for forced states is i, the span's is values.size(). The parts' ids are derived from
// `id` and the handle's key (or ImGui::GetID(i) under `id` without keys), and "##span" under
// `id`.
MultiHandleResult MultiHandleBehavior(ImGuiID id, const ImRect& widgetRect, const ImRect& track,
                                      std::span<double> values, std::span<ItemState> handles,
                                      const MultiHandleOptions& options);

// Forgets the drag in progress and the values a cancel would restore (facet::Init and
// Shutdown, through the focus subsystem).
void ResetMultiHandleState();

// The rectangle of handle `value` on `track` for a handle of `size` pixels: centered on the
// track's cross axis.
ImRect HandleRect(const ImRect& track, Orientation orientation, const ValueMapping& mapping, double value, float size);

} // namespace facet::internal
