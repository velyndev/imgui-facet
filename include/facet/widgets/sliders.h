#pragma once

// Sliders: sliders, stepped sliders, sliders with a field, range sliders, multi-handle sliders,
// knobs and two-dimensional pads.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families. The family is being implemented: the entry of each widget in
// docs/widgets/sliders.md ends with its status. A widget whose status is planned draws a
// placeholder of the widget's default size, returns false and writes nothing; its queries report
// that nothing happened (-1).
//
// Common to every widget here:
//   - The bool result is true on the frame a value was written. Drags, clicks and keys write at
//     once; typed values (Ctrl+click or Enter on a handle) write on Enter, Tab or focus loss.
//     Escape during an edit restores every value from activation, writes it and returns true.
//     facet::IsItemEditCommitted() after the call reports one finished drag or typed value.
//   - Keys on a focused handle: Left and Right (Down and Up when vertical) move one step, or
//     one hundredth of the range without a step; Shift moves a tenth of that, Ctrl ten times
//     that; Page Up and Page Down move ten steps; Home and End move to the nearest limit the
//     handle can reach.
//   - Values outside [min, max], NaN and handles out of order are drawn clamped and sorted and
//     written only by an edit. min > max is swapped; min == max shows the values read-only.
//   - A null value pointer draws the widget read-only and returns false.
//   - Disabled: ImGui::BeginDisabled. readOnly shows and focuses the values but never writes.
//   - Lengths are design units (multiplied by Px()); value parameters are in value units.
//   - Formats are printf formats in the Dear ImGui dialect; nullptr uses "%.3g" for float and
//     "%d" for int. A formatter, when set, is used instead and gets the value as a double.
//   - Horizontal sliders, RangeInput, RangeSliderHistogram and AllocationBar draw the visible
//     label part in front of the track, inside the widget width. Knob and ArcRangeSlider draw it
//     under the dial. Vertical sliders, XYPad, JoystickPad and BoxRangePad do not draw it; for
//     them it is the accessible name and the tooltip.
//
// The multi-handle model, shared by RangeSlider, RangeInput, RangeSliderHistogram, MultiSlider,
// ArcRangeSlider, BoxRangePad and AllocationBar, so a caller who knows RangeSlider can guess the
// others:
//   - Positional parameters: the label, the values the widget writes (two pointers, a span or a
//     vector), the range bounds, then item data (histogram bins, segment items).
//   - Each handle is its own Dear ImGui item under the widget id, with its own Tab stop and focus
//     ring; the handle index is its part number (ctx.ForceNextItemState in the demo). While a
//     handle is active or was just deactivated, the last item id is that handle's, so
//     IsItemActive and IsItemDeactivatedAfterEdit work; otherwise it is the widget's.
//   - Handles keep their order and at least `minGap` (value units) between neighbours.
//     `collision` decides what a handle does when it reaches its neighbour: Clamp stops it, Push
//     moves the neighbour along until that one reaches the end of the range.
//   - `dragSpan`: dragging the filled part between two handles moves both and keeps their
//     distance. A click on the empty track moves the nearest handle there.
//   - The option members have the same names and order wherever they apply: format, formatter,
//     formatterUser, scale, step, minGap, collision, dragSpan, the widget's own members,
//     orientation, then the common block (size, width, height, readOnly, mixed, error,
//     helpText). The pads use ImVec2 for step and minGap, one value per axis. Every widget of the
//     model has readOnly, mixed, error and helpText; `mixed` hides the handles until the first
//     edit writes every value. The two-handle linear widgets (RangeSlider, RangeInput,
//     RangeSliderHistogram) also share allowOutOfRange and doubleClickReset.

#include <cstddef>
#include <span>
#include <vector>

#include <imgui.h>

#include "facet/widgets/common.h"

namespace facet
{

// What a handle does when it reaches its neighbour (or the minimum gap to it).
enum class HandleCollision
{
    Clamp,  // the handle stops
    Push,   // the neighbour moves along, until it reaches the end of the range
};

// --- Slider -----------------------------------------------------------------------------------

struct SliderOptions
{
    const char* format = nullptr;       // printf; null: "%.3g" (float) or "%d" (int)
    FormatFn formatter = nullptr;       // used instead of format when set
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;                  // value units; > 0 snaps to min + n * step; 0 continuous (int: 1)
    bool fillFromCenter = false;        // fill from the middle of the range, for bipolar values
    bool valueBubble = false;           // value shown above the knob while dragging or focused
    Orientation orientation = Orientation::Horizontal;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units, horizontal; 0 fills the available width
    float height = 0.0f;                // units, vertical; 0 for the default fader length
    bool readOnly = false;
    bool mixed = false;                 // no knob until the first edit
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A track with a fill and a knob (sliderTrackHeight and sliderKnobSize tokens), the visible
// label in front and the value at the end. Drag the knob or click the track; keys as above.
// Writes while dragging. Returns true when *value was written. Default width: the available
// width. Keeps the knob animation under its id.
bool Slider(const char* label, float* value, float min, float max, const SliderOptions& options = {});
bool Slider(const char* label, int* value, int min, int max, const SliderOptions& options = {});

// --- SliderStepped ----------------------------------------------------------------------------

struct SliderSteppedOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    std::span<const char* const> stopLabels = {};  // read during the call; one per stop, thinned when crowded
    bool magnetic = false;              // moves freely and snaps only near a stop; otherwise always on a stop
    Orientation orientation = Orientation::Horizontal;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    float height = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A slider over non-uniform stops (ascending values, read during the call) with a tick mark per
// stop. Arrow keys move to the next stop; Home and End to the first and last. *value is written
// with a stop value (or, with magnetic, any value in [first, last]). A value between stops is
// drawn at its place and written only by an edit. Returns true when *value was written. Fewer
// than two stops show the value read-only.
bool SliderStepped(const char* label, float* value, std::span<const float> stops,
                   const SliderSteppedOptions& options = {});

// --- SliderWithInput --------------------------------------------------------------------------

struct SliderWithInputOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;
    bool allowOutOfRange = false;       // the field accepts values outside [min, max]; the slider shows them clamped
    float fieldWidth = 0.0f;            // units; 0 for the default number field width
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fills; when narrow the field drops below the track
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A Slider and a number field bound to the same value. The field writes typed values on Enter,
// Tab or focus loss (Dear ImGui's default for scalars); the slider writes while dragging.
// Returns true when *value was written by either part. Two Tab stops.
bool SliderWithInput(const char* label, float* value, float min, float max,
                     const SliderWithInputOptions& options = {});
bool SliderWithInput(const char* label, int* value, int min, int max, const SliderWithInputOptions& options = {});

// --- RangeSlider ------------------------------------------------------------------------------

struct RangeSliderOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;
    double minGap = 0.0;                // value units; smallest allowed high - low, clamped to the range
    HandleCollision collision = HandleCollision::Clamp;
    bool dragSpan = true;               // dragging the filled span moves both handles
    bool mirror = false;                // moving one handle moves the other the same distance the other way around the range center
    bool fixedSpan = false;             // high - low never changes: every drag and key moves both handles
    bool valueBubble = false;           // each handle shows its value while dragged or focused
    bool allowOutOfRange = false;       // a typed value outside [min, max] is written; the track shows it clamped
    bool doubleClickReset = false;      // a double click on a handle moves it to its end of the range (low to min, high to max)
    Orientation orientation = Orientation::Horizontal;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units, horizontal; 0 fills the available width
    float height = 0.0f;                // units, vertical; 0 for the default fader length
    bool readOnly = false;
    bool mixed = false;                 // no handles and no fill until the first edit writes both values
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Two handles selecting [*low, *high] inside [min, max], following the multi-handle model
// above: handle 0 is *low, handle 1 is *high, each its own Tab stop. Returns true when *low or
// *high was written. Default width: the available width. Keeps the handle animations under its
// id.
bool RangeSlider(const char* label, float* low, float* high, float min, float max,
                 const RangeSliderOptions& options = {});
bool RangeSlider(const char* label, int* low, int* high, int min, int max, const RangeSliderOptions& options = {});

// --- RangeInput -------------------------------------------------------------------------------

struct RangeInputOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;
    double minGap = 0.0;
    HandleCollision collision = HandleCollision::Clamp;
    bool dragSpan = true;
    bool slider = true;                 // the track between the two fields; false shows the fields only
    float fieldWidth = 0.0f;            // units; 0 for the default number field width
    bool allowOutOfRange = false;       // as RangeSliderOptions::allowOutOfRange, for the fields and typed handles
    bool doubleClickReset = false;      // as RangeSliderOptions::doubleClickReset
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fills; when narrow the fields drop below the track
    bool readOnly = false;
    bool mixed = false;                 // both fields empty and no handles until the first edit writes both values
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A RangeSlider between a low and a high number field. A typed value obeys the same rules as a
// dragged handle: it keeps minGap and the order, and `collision` decides whether it stops at
// the other value or pushes it. Typed values write on Enter, Tab or focus loss. Four Tab stops:
// low field, low handle, high handle, high field. Returns true when *low or *high was written.
bool RangeInput(const char* label, float* low, float* high, float min, float max,
                const RangeInputOptions& options = {});
bool RangeInput(const char* label, int* low, int* high, int min, int max, const RangeInputOptions& options = {});

// --- RangeSliderHistogram ---------------------------------------------------------------------

struct RangeSliderHistogramOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;
    double minGap = 0.0;
    HandleCollision collision = HandleCollision::Clamp;
    bool dragSpan = true;
    bool allowOutOfRange = false;       // as RangeSliderOptions::allowOutOfRange
    bool doubleClickReset = false;      // as RangeSliderOptions::doubleClickReset
    float histogramHeight = 0.0f;       // units; 0 for the default bar area height
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;                 // as RangeSliderOptions::mixed; the bars are drawn undimmed
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A horizontal RangeSlider under a bar chart of `bins` (read during the call). The bins divide
// [min, max] evenly; each is a count or weight >= 0, drawn relative to the largest. Bars outside
// [*low, *high] are dimmed, and hovering a bar shows its range and value in a tooltip. Empty
// bins draw the track alone. Handles and result as RangeSlider.
bool RangeSliderHistogram(const char* label, float* low, float* high, float min, float max,
                          std::span<const float> bins, const RangeSliderHistogramOptions& options = {});
bool RangeSliderHistogram(const char* label, int* low, int* high, int min, int max, std::span<const float> bins,
                          const RangeSliderHistogramOptions& options = {});

// --- MultiSlider ------------------------------------------------------------------------------

struct MultiSliderOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;
    double minGap = 0.0;                // value units; smallest distance between neighbouring handles
    HandleCollision collision = HandleCollision::Clamp;
    bool dragSpan = false;              // dragging a segment moves the two handles around it
    bool allowAdd = false;              // growable overloads: a double click on the track adds a handle
    bool allowRemove = false;           // growable overloads: Delete on a focused handle, or dragging it off the track, removes it
    int minHandles = 0;                 // growable overloads: removing stops at this count
    int maxHandles = 0;                 // growable overloads: adding stops at this count; 0: the storage
                                        // capacity (span overload) or unlimited (vector); the smaller wins
    std::span<const ImU32> segmentColors = {};  // read during the call; data colors of the segments, repeated; a 0 entry
                                        // takes the chart palette color of its index; empty: one accent fill,
                                        // because a slider is a control and the fill shows its value
    Orientation orientation = Orientation::Horizontal;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    float height = 0.0f;
    bool readOnly = false;
    bool mixed = false;                 // the bare track and no handles until the first edit writes every value
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// N handles on one track, in ascending order, following the multi-handle model above; handle i
// is values[i]. The handles divide the track into segments, filled with segmentColors. Returns
// true when a value was written (or, for the growable overloads, a handle was added or removed).
// An empty span or vector draws the bare track. These two overloads keep the handle count:
// allowAdd and allowRemove need a growable overload, and set here they are reported as a usage
// error (IM_ASSERT_USER_ERROR) and ignored, so a std::vector passed by value cannot silently
// lose its add and remove.
bool MultiSlider(const char* label, std::span<float> values, float min, float max,
                 const MultiSliderOptions& options = {});
bool MultiSlider(const char* label, std::span<int> values, int min, int max, const MultiSliderOptions& options = {});
// Growable, in the caller's storage (rule 5): `storage` is the capacity and the first *count
// values are the handles. allowAdd and allowRemove write *count. A new handle is inserted where
// it keeps the order; read MultiSliderAddedHandle or MultiSliderRemovedHandle to update data
// kept per handle or per segment. Never allocates.
bool MultiSlider(const char* label, std::span<float> storage, int* count, float min, float max,
                 const MultiSliderOptions& options = {});
// Growable, in a vector the widget resizes. Allocates only when a handle is added.
bool MultiSlider(const char* label, std::vector<float>* values, float min, float max,
                 const MultiSliderOptions& options = {});

// The index at which the last MultiSlider call this frame inserted a handle, or -1.
int MultiSliderAddedHandle();
// The index the handle removed by the last MultiSlider call this frame had, or -1.
int MultiSliderRemovedHandle();

// --- Knob -------------------------------------------------------------------------------------

enum class KnobVariant
{
    Arc,    // a value arc around the knob; the default
    Dot,    // a dot on the knob face marks the value
    Wiper,  // a line from the center marks the value
};

struct KnobOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;
    KnobVariant variant = KnobVariant::Arc;
    bool fillFromCenter = false;        // the arc starts at the middle of the range, for bipolar values
    bool circularDrag = false;          // the value follows the pointer angle; otherwise a vertical drag
    float sweep = 0.0f;                 // degrees of the arc; 0 for the default, which leaves a gap at the bottom
    float diameter = 0.0f;              // units; 0 derives it from the control height of `size`
    ControlSize size = ControlSize::Inherit;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A rotary control with the visible label under it and the value inside or below. Drag up or
// right to increase (or around, with circularDrag), the wheel while hovered and the arrow keys
// step; Ctrl+click or Enter types a value; a double click returns to the value from
// activation. Writes while dragging. Returns true when *value was written.
bool Knob(const char* label, float* value, float min, float max, const KnobOptions& options = {});
bool Knob(const char* label, int* value, int min, int max, const KnobOptions& options = {});

// --- XYPad ------------------------------------------------------------------------------------

struct XYPadOptions
{
    const char* format = nullptr;       // used for both axes
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ImVec2 step = {0.0f, 0.0f};         // value units per axis; 0 continuous
    bool grid = false;                  // grid lines at every step (or at quarters without one)
    bool crosshair = true;              // guide lines through the point and axis readouts
    bool yDown = false;                 // min.y at the top; by default min.y is at the bottom
    float width = 0.0f;                 // units; 0: default side, at most the available width; kFill fills
    float height = 0.0f;                // units; 0 makes the pad square (as high as it is wide)
    bool readOnly = false;
    bool mixed = false;                 // no point until the first edit
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A two-dimensional area with one draggable point; *value.x and *value.y lie in [min, max] per
// axis. Dragging moves the point (Shift keeps the axis the drag started along); arrow keys move
// it by a step. Writes while dragging. Returns true when *value was written.
bool XYPad(const char* label, ImVec2* value, ImVec2 min, ImVec2 max, const XYPadOptions& options = {});

// --- JoystickPad ------------------------------------------------------------------------------

struct JoystickPadOptions
{
    float deadzone = 0.0f;              // 0 to 1 of the radius; inside it the value is 0
    bool springReturn = true;           // the stick returns to the center on release
    bool circular = true;               // the value stays inside the unit circle; otherwise inside the square
    bool showDeadzone = false;          // draws the deadzone ring
    float diameter = 0.0f;              // units; 0: default size, at most the available width
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A round pad with a stick; *value is in [-1, 1] per axis, x to the right and y up. Dragging
// moves the stick; arrow keys hold it toward the edge while down. With springReturn the
// release writes {0, 0}. Writes while dragging. Returns true when *value was written.
bool JoystickPad(const char* label, ImVec2* value, const JoystickPadOptions& options = {});

// --- ArcRangeSlider ---------------------------------------------------------------------------

struct ArcRangeSliderOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;
    double minGap = 0.0;
    HandleCollision collision = HandleCollision::Clamp;
    bool dragSpan = true;               // dragging the band rotates both handles
    bool wrap = false;                  // cyclic range: *low may exceed *high, the band then runs through max to min
    float sweep = 0.0f;                 // degrees of the arc; 0 for the knob default; 360 a full circle (needed by wrap)
    float diameter = 0.0f;              // units; 0 derives it from the control height of `size`
    ControlSize size = ControlSize::Inherit;
    bool readOnly = false;
    bool mixed = false;                 // as RangeSliderOptions::mixed
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A RangeSlider bent into an arc, for angular sectors (min 0, max 360, wrap) or bands on a
// dial. Handles, keys and result as RangeSlider; with wrap, handles pass min and max instead of
// stopping there and order is not enforced.
bool ArcRangeSlider(const char* label, float* low, float* high, float min, float max,
                    const ArcRangeSliderOptions& options = {});

// --- BoxRangePad ------------------------------------------------------------------------------

struct BoxRangePadOptions
{
    const char* format = nullptr;       // used for both axes
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ImVec2 step = {0.0f, 0.0f};         // value units per axis; 0 continuous
    ImVec2 minGap = {0.0f, 0.0f};       // value units; smallest high - low per axis
    HandleCollision collision = HandleCollision::Clamp;
    bool dragSpan = true;               // dragging the box body moves the whole box
    bool yDown = false;                 // min.y at the top; by default min.y is at the bottom
    float width = 0.0f;                 // units; 0: default side, at most the available width; kFill fills
    float height = 0.0f;                // units; 0 makes the pad square (as high as it is wide)
    bool readOnly = false;
    bool mixed = false;                 // no box and no handles until the first edit writes both corners
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A two-dimensional range: the box from *low to *high inside [min, max] per axis. Eight handles,
// each a Tab stop: parts 0 to 3 are the left, top, right and bottom edges, parts 4 to 7 the
// corners from the top left clockwise. Edges drag one axis, corners two; arrow keys nudge the
// focused handle. The multi-handle rules apply per axis. Returns true when *low or *high was
// written.
bool BoxRangePad(const char* label, ImVec2* low, ImVec2* high, ImVec2 min, ImVec2 max,
                 const BoxRangePadOptions& options = {});

// --- AllocationBar ----------------------------------------------------------------------------

struct AllocationBarOptions
{
    const char* format = nullptr;       // gets the share in percent (0 to 100); null: "%.0f%%"
    FormatFn formatter = nullptr;       // used instead of format; gets the share in percent (0 to 100) too
    void* formatterUser = nullptr;
    double step = 0.0;                  // share units (0 to 1); 0 continuous
    double minGap = 0.0;                // smallest share of one segment, clamped to 1 / segment count
    HandleCollision collision = HandleCollision::Clamp;  // Push lets a divider move the dividers beyond it
    std::span<const ImU32> segmentColors = {};  // read during the call; data colors, repeated; empty or a 0 entry:
                                        // the chart palette color of the segment index, as in DonutChart
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fills the available width
    bool readOnly = false;
    bool mixed = false;                 // equal segments without dividers until the first edit writes every share
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// One bar split into segments whose shares always add up to 1, one segment per item and one
// share per segment (shares.size() should equal items.size(); the smaller count is used). The
// dividers are the handles of the multi-handle model: dragging divider i moves share only
// between segments i and i + 1. Enter on a divider, or a click on a segment's percentage,
// types the share of the segment before it. Shares that do not add up to 1 are drawn
// normalized and written only by an edit, which writes all of them. Returns true when a share
// was written. Fewer than two segments show the bar read-only.
bool AllocationBar(const char* label, std::span<float> shares, std::span<const ChoiceItem> items,
                   const AllocationBarOptions& options = {});
bool AllocationBar(const char* label, std::span<float> shares, std::span<const char* const> items,
                   const AllocationBarOptions& options = {});

} // namespace facet
