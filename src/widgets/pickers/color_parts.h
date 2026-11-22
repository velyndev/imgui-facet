#pragma once

// The parts the color pickers are made of, shared by SatValSquare, ColorChannelSlider and
// ColorPicker so the square and the bars look and behave the same wherever they appear:
//
//     ColorHsv hsv = ColorTrackHsv(widgetId, *color);              // color_model.h
//     const ColorPartResult square = ColorSquarePart(squareId, rect, knob, *color, &hsv, params);
//     const ColorPartResult hue = ColorBarPart(hueId, frame, ColorChannel::Hue, square.color, &hsv, params);
//     if (square.changed || hue.changed) { *color = ...; ColorRememberHsv(widgetId, *color, hsv); }
//
// Each part is one item (PartBehavior) and one Tab stop, inside space the widget laid out. The
// square runs the two-axis input of color_plane.h, a bar the value drag core
// (internal/value_drag.h) with the keys of a Slider. Gradients are drawn in gamma-encoded sRGB, the
// space HSV is defined in, so the color under the centre of a handle is the color the handle
// stands for. Handles are the solid knobs of the sliders with that color inside their rim
// (DrawColorHandle), so they read on any color in both themes.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/pickers.h"
#include "internal/item.h"
#include "widgets/pickers/color_model.h"

namespace facet::internal
{

// How a part runs inside its widget.
struct ColorPartParams
{
    bool readOnly = false;  // shown and focusable, never written
    bool mixed = false;     // no handle until the first edit
    const ForcedItemRequest* forced = nullptr;  // the widget's forced capture state
    int part = -1;          // this part's index for the forced state
};

struct ColorPartResult
{
    ItemState item;
    bool changed = false;   // the part edited the color: `color` and *hsv hold the new value
    ImVec4 color = {};      // the color with the part's edit (unchanged when there was none)
};

// ---- Channels --------------------------------------------------------------------------------

// The position of `channel` of the color on its bar, 0 to 1 (value clamped for HDR colors).
double ColorChannelFraction(ColorChannel channel, const ImVec4& color, const ColorHsv& hsv);
// The color with `channel` at `fraction`, the other channels held; updates *hsv for the HSV
// channels and keeps it consistent for the others.
ImVec4 ColorWithChannel(ColorChannel channel, const ImVec4& color, ColorHsv* hsv, double fraction);
// The color at `fraction` of the bar of `channel`, for drawing (alpha 1 except on the alpha bar).
ImVec4 ColorChannelSample(ColorChannel channel, const ImVec4& color, const ColorHsv& hsv, double fraction);
// The visible name of a channel (Strings::colorRed and the others).
const char* ColorChannelName(ColorChannel channel);

// ---- Drawing -----------------------------------------------------------------------------------

// The bar of `channel` filling `bar` (pixels): rounded ends of half its thickness in the end colors,
// and the gradient between them, fraction 0 at the left (horizontal) or bottom (vertical) end; a
// checkerboard under the alpha bar. Disabled: at kColorDisabledOpacity.
void DrawColorChannelBar(ImDrawList* drawList, const ImRect& bar, Orientation orientation, ColorChannel channel,
                         const ImVec4& color, const ColorHsv& hsv, bool disabled);

// The saturation and value square of `hue` filling `area`: saturation from left to right, value
// from bottom to top, as two exact linear gradients. `radius` pixels.
void DrawColorSquare(ImDrawList* drawList, const ImRect& area, double hue, float radius, bool disabled);

// ---- Parts ---------------------------------------------------------------------------------------

// The thickness of a color bar in pixels (picker.colorBarHeight), even, at most `controlHeight`.
float ColorBarThickness(float controlHeight);

// A bar part: the item covers `control`, the knob of `knob` pixels travels along `track` (both
// from LayoutSliderFrame of the sliders family). Writes on drags and keys like a Slider; the hue
// wraps at the ends.
ColorPartResult ColorBarPart(ImGuiID id, const ImRect& control, const ImRect& track, float knob, Orientation orientation,
                             ColorChannel channel, const ImVec4& color, ColorHsv* hsv, const ColorPartParams& params);

// The square part: the item covers `rect`; the gradient fills it inset by half the knob, so the
// handle stays inside `rect` at every corner. Writes saturation and value; hue and alpha stay.
ColorPartResult ColorSquarePart(ImGuiID id, const ImRect& rect, float knob, const ImVec4& color, ColorHsv* hsv,
                                const ColorPartParams& params);

// The gradient area of a square part in `rect`.
ImRect ColorSquareArea(const ImRect& rect, float knob);

} // namespace facet::internal
