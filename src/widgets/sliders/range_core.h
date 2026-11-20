#pragma once

// The two-handle track shared by RangeSlider, RangeInput and RangeSliderHistogram: the items
// (through the multi-handle block, src/internal/multi_handle.h), the typed values of a handle,
// the double click reset and the drawing of the track, the span, the knobs and their bubbles.
//
//     ImGui::BeginGroup();
//     ImGui::ItemSize(frame.widget.GetSize());
//     const RangeTrackResult r = RangeTrackBehavior(id, frame, m, low, high, min, max, params);
//     if (r.written[0]) *low = r.values[0];  ... the caller writes, then draws its label and value ...
//     ImGui::EndGroup();
//     FinishWidgetGroup(id);
//
// Values the caller passes in that break the rules (out of range, out of order, closer than the
// minimum gap) are drawn repaired; only the handles an edit moves are reported as written, so an
// out-of-range value that allowOutOfRange let through stays until its own handle is edited.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/sliders.h"
#include "internal/multi_handle.h"
#include "widgets/sliders/slider_core.h"

namespace facet::internal
{

struct RangeTrackParams
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;
    double minGap = 0.0;
    HandleCollision collision = HandleCollision::Clamp;
    bool dragSpan = true;
    bool mirror = false;
    bool fixedSpan = false;
    bool valueBubble = false;
    bool allowOutOfRange = false;
    bool doubleClickReset = false;
    Orientation orientation = Orientation::Horizontal;
    ControlSize size = ControlSize::Inherit;
    bool integer = false;
    bool readOnly = false;   // also a missing value pointer or an empty range
    bool mixed = false;
    bool error = false;
    bool drawTrack = true;   // false: the caller draws the track line and fill itself (histogram)
};

struct RangeTrackResult
{
    double values[2] = {0.0, 0.0};   // after the call: the edited values, or the repaired ones
    bool written[2] = {false, false};
    bool changed = false;            // written[0] || written[1]
    bool typing = false;             // a typed value field covers the control this frame
    bool disabled = false;
    bool readOnly = false;
    bool mixedShown = false;         // the mixed look is drawn (no knobs, no fill)
    ImGuiID handleIds[2] = {0, 0};
    ItemState handles[2];
    MultiHandleResult multi;
    ValueMapping mapping;            // normalized
};

// Runs and draws the two-handle track of `frame` (pixels) for the values `low` and `high` in
// [min, max]. Call it inside the widget's group after its ItemSize; the cursor is left where it
// was before the call.
RangeTrackResult RangeTrackBehavior(ImGuiID id, const SliderFrame& frame, const SliderMetrics& m, double low,
                                    double high, double min, double max, const RangeTrackParams& params);

// The value column of a range: `low`, a short bar and `high`, each value in a slot of
// `valueWidth` pixels, right-aligned in `column`.
float RangeValueColumnWidth(float valueWidth);
void DrawRangeValueText(ImDrawList* drawList, const ImRect& column, float valueWidth, const char* low,
                        const char* high, bool disabled);

// Applies a typed value for handle `index` to `values` by the multi-handle rules (order, gap,
// collision), clamped to the range unless `allowOutOfRange`, whole for integer mappings. Returns
// true when a value changed.
bool ApplyTypedHandleValue(double values[2], int index, double typed, const ValueMapping& mapping,
                           const HandleConstraints& constraints, bool allowOutOfRange);

// The constraints of a two-handle range from its parameters and normalized mapping.
HandleConstraints RangeConstraints(const RangeTrackParams& params, const ValueMapping& mapping);

} // namespace facet::internal
