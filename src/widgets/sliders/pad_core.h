#pragma once

// The parts the two-dimensional widgets of the sliders family share (XYPad, BoxRangePad,
// JoystickPad): the size rule, the frame and the area the handles travel in, and the grid.
//
//     const float width = ResolvePadWidth(options.width, next);
//     const PadFrame pad = LayoutPadFrame(window->DC.CursorPos, width, height, m);
//     ... items over pad.frame, the point on pad.area (value_drag.h, ValueDrag2DBehavior) ...
//     DrawFieldFrame(drawList, pad.frame, look);     // control.h
//     DrawPadGrid(drawList, pad.area, mx, my, yDown, disabled);
//     DrawSliderKnob(drawList, PadKnobRect(point, pad.knob), item, look);   // slider_core.h

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/item.h"
#include "internal/value_drag.h"
#include "widgets/sliders/slider_core.h"

namespace facet::internal
{

// The width of a pad in pixels: options.width by the width rule (units, kFill, minus units),
// then SetNextItemWidth and PushItemWidth; by default the padSize token, at most the available
// width, so a pad fits a narrow column.
float ResolvePadWidth(float optionsWidth, const NextItemData& next);

// The parts of a rectangular pad.
struct PadFrame
{
    ImRect frame;     // the visible field frame; takes the mouse
    ImRect area;      // the rectangle the handle centres travel in: the frame inset by half a knob
    float knob = 0.0f;  // the knob diameter: the slider knob, smaller in a pad too small for it
};

// Lays out a pad of `width` by `height` pixels at `origin`, on whole pixels.
PadFrame LayoutPadFrame(ImVec2 origin, float width, float height, const SliderMetrics& m);

// The square knob rectangle of `knob` pixels centred on `point`, on whole pixels.
ImRect PadKnobRect(ImVec2 point, float knob);

// Grid lines across `area`: at every step of each mapping when there are at most a few dozen,
// otherwise (and without a step) at the quarters. separator hairlines.
void DrawPadGrid(ImDrawList* drawList, const ImRect& area, const ValueMapping& mappingX, const ValueMapping& mappingY,
                 bool yDown);

} // namespace facet::internal
