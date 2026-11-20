#pragma once

// The parts every widget of the sliders family shares, so a Slider, a RangeSlider, a stepped
// slider and a histogram range measure, place and draw their track, knobs, value text and value
// bubble the same way, on whole pixels.
//
//     const SliderMetrics m = GetSliderMetrics(options.size);
//     const SliderFrame frame = LayoutSliderFrame(origin, label, width, valueWidth, m, orientation, length);
//     ... items over frame.control, the knob centres on frame.track ...
//     DrawSliderTrack(drawList, frame.track, orientation, m, fillFrom, fillTo, look);
//     DrawSliderKnob(drawList, SliderKnobRect(frame.track, orientation, m, fraction), item, look);
//     DrawSliderBubble(drawList, knobRect, text, orientation, amount);
//
// Typed values (Ctrl+click or Enter on a knob) go through SliderTypeField: while a part of a
// widget is being typed, a number field from the number core covers the control area, and the
// widget gets the typed value back when Enter, Tab or focus loss commits it.

#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/common.h"
#include "facet/widgets/sliders.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/text_field_number.h"
#include "internal/value_drag.h"

namespace facet::internal
{

// A float value as the double of its shortest decimal form (0.8f gives 0.8, not 0.800000011920929),
// so a value the caller typed as a decimal lies exactly on the keyboard grid and on the step grid
// of the value drag core, and a step from it never rounds back to the same float. NaN and the
// infinities pass through.
double SliderFloatToDouble(float value);

// The caller's value as the double the family edits.
inline double SliderValueIn(float value) { return SliderFloatToDouble(value); }
inline double SliderValueIn(int value) { return static_cast<double>(value); }

// Lengths of a slider of one control size, in whole pixels at the current scale.
struct SliderMetrics
{
    ControlMetrics control;
    float knob = 0.0f;       // knob diameter: sliderKnobSize scaled with the control height, even
    float track = 0.0f;      // track thickness: sliderTrackHeight
    float tick = 0.0f;       // step and stop marks: sliderTickSize
    float valueGap = 0.0f;   // between the control area and the value text: spacing.sm
};

SliderMetrics GetSliderMetrics(ControlSize size);

// How the track and the knobs look besides their normal state.
enum class SliderLook
{
    Normal,
    ReadOnly,  // fill in labelSecondary: shown, focusable, not editable
    Disabled,  // everything in the disabled tokens
};

SliderLook PickSliderLook(bool disabled, bool readOnly);

// The parts of a straight slider row: the label in front, the control area that takes the mouse,
// the line the knob centres travel along, and the value column at the end.
struct SliderFrame
{
    ImRect widget;    // the whole row (or the vertical fader), without the message line
    FrontLabel label; // drawn at labelPos; empty for vertical sliders
    ImVec2 labelPos;
    ImRect control;   // takes the mouse
    ImRect track;     // zero thickness line of the knob centres, inset by half a knob
    ImRect value;     // the value text column; zero width when there is none or no room
    float knob = 0.0f;  // the knob diameter used: the metrics' one, smaller in a very short control
};

// Lays out a slider at `origin` (pixels). Horizontal: `width` pixels wide and one control height
// high, the label in front (cut at kFrontLabelMaxShare), then the control, then a value column of
// `valueWidth` pixels (0 for none; dropped when the control would get shorter than two knobs).
// Vertical: one control height wide and `length` pixels high, no label and no value column.
SliderFrame LayoutSliderFrame(ImVec2 origin, const char* label, float width, float valueWidth, const SliderMetrics& m,
                              Orientation orientation, float length);

// The knob rectangle of a fraction on the track, a square of `knob` pixels on whole pixels.
ImRect SliderKnobRect(const ImRect& track, Orientation orientation, float knob, double fraction);
// The position along the track of a fraction, rounded to whole pixels.
float SliderTrackPos(const ImRect& track, Orientation orientation, double fraction);

// The track line with its fill between two fractions; `error` draws the unfilled part in the
// destructive color. `fillColor` 0 uses the accent (labelSecondary read-only, labelTertiary
// disabled).
void DrawSliderTrack(ImDrawList* drawList, const ImRect& track, Orientation orientation, const SliderMetrics& m,
                     double fillFrom, double fillTo, SliderLook look, bool error, ImU32 fillColor = 0);
// A filled stretch of the track between two fractions in `color` (segments of a multi slider).
void DrawSliderTrackStretch(ImDrawList* drawList, const ImRect& track, Orientation orientation, const SliderMetrics& m,
                            double from, double to, ImU32 color);
// A tick mark at each fraction, beside the track.
void DrawSliderTicks(ImDrawList* drawList, const ImRect& track, Orientation orientation, const SliderMetrics& m,
                     std::span<const double> fractions, SliderLook look);
// The fractions of the step grid of `mapping` when it has at most `maxTicks` steps; returns the
// count written into `out`.
int SliderStepTicks(const ValueMapping& mapping, std::span<double> out, int maxTicks);

// A knob: the background color with a 3:1 outline that darkens on hover and grows a little while
// held, and the round focus ring.
void DrawSliderKnob(ImDrawList* drawList, const ImRect& rect, const ItemState& item, SliderLook look);

// The value bubble above a horizontal knob (beside a vertical one) at `amount` (0 to 1).
void DrawSliderBubble(ImDrawList* drawList, const ImRect& knob, const char* text, Orientation orientation, float amount);

// The value text right-aligned in its column, in ControlSecondaryColor.
void DrawSliderValueText(ImDrawList* drawList, const ImRect& column, const char* text, bool disabled);

// The widest of the formatted values at the ends of the range and `value`, in the current font,
// rounded up to whole pixels and never shrinking for one widget id while it is shown, so the
// value column does not jitter while the value changes.
float SliderValueWidth(ImGuiID id, double min, double max, double value, const char* format, FormatFn formatter,
                       void* user, bool integer);

// The animation amount of a value bubble: shown while the knob is held or has visible focus.
float SliderBubbleAmount(ImGuiID id, const ItemState& item);

// ---- The Slider body -----------------------------------------------------------------------

// How a widget built around a Slider (SliderWithInput) runs it.
struct SliderBodyParams
{
    bool valueColumn = true;            // false: no value text at the end
    float width = 0.0f;                 // pixels; 0: the width rule of the options
    bool errorLook = false;             // the error look on the track, with the message drawn by the caller
    const NextItemData* next = nullptr; // the next-item data the caller took; null: taken here
    ImRect* controlRect = nullptr;      // receives the control area
};

// facet::Slider on a double (src/widgets/sliders/slider.cpp): writes *value (null: read-only) and
// returns true when it did. Integer values are written whole.
bool SliderBody(const char* label, double* value, double min, double max, bool integer, const SliderOptions& options,
                const SliderBodyParams& params);

// ---- Typed values --------------------------------------------------------------------------

// True when the press or key that `item` reports this frame asks to type a value: Ctrl+click, or
// Enter (keypad Enter too) on the focused knob.
bool SliderWantsTyping(const ItemState& item);

// Starts typing part `part` of widget `widgetId` on the next SliderTypeField call.
void SliderBeginTyping(ImGuiID widgetId, int part);
// The part being typed, -1 none.
int SliderTypingPart(ImGuiID widgetId);

struct SliderTypeResult
{
    bool typing = false;    // the field was submitted this frame
    bool changed = false;   // a typed value was committed: `value` holds it
    double value = 0.0;
    bool ended = false;     // typing ended this frame (commit, Escape or focus loss)
    ImGuiID fieldId = 0;
};

// While part `part` of `widgetId` is being typed: submits a number field over `rect` (pixels, at
// the cursor of a group the widget opened) starting from `value`, with `params` for its format
// and range. When typing ends and the focus has not moved elsewhere, it goes back to `returnFocus`.
SliderTypeResult SliderTypeField(ImGuiID widgetId, int part, const ImRect& rect, double value,
                                 const NumberFieldParams& params, ImGuiID returnFocus);

// Forgets every typing session (facet::Shutdown through the family subsystem).
void ResetSliderTyping();

// Forgets the insert and removal MultiSliderAddedHandle and MultiSliderRemovedHandle report
// (src/widgets/sliders/multi_slider.cpp; facet::Init and Shutdown through the family subsystem).
void ResetMultiSliderEvents();

} // namespace facet::internal
