#pragma once

// The two-axis input of the color pickers: the saturation and value square, the triangle of the
// wheel and the hue ring. The value drag core (internal/value_drag.h) moves one value along one
// track; a plane moves two fractions at once, so this block runs its own session with the same
// rules and the core's step math (KeyboardStep, the keyboard grid, Shift fine and Ctrl coarse):
//
//     ItemState item = PartBehavior(id, squareRect, {.press = PressMode::Click});
//     double x = hsv.s, y = hsv.v;
//     const ColorPlaneResult edit = ColorPlaneBehavior(item, &x, &y, {.map = SquareMap, .user = &squareRect});
//     if (edit.changed) ... write the color from x and y ...
//
// Mouse: a press puts the handle under the pointer, and the drag follows the pointer (Shift moves
// ten times finer from there on); leaving the area keeps the drag, clamped to the area. Keyboard,
// while the part has keyboard focus: Left and Right step x, Down and Up step y (Shift a tenth of
// a step, Ctrl ten steps), Page Up and Page Down step y by a tenth of the range, Home and End put
// x at 0 and 1. A part without a second axis (the hue ring) steps x with every arrow. Held keys
// repeat at Dear ImGui's key repeat rate, counted from the elapsed time. Escape during a drag or a
// held key restores both fractions from the start of the edit, writes them and ends the edit
// (facet::IsItemEditCanceled reports it). One edit session per drag and per key press, as the
// value drag core keeps them, so the item queries report one edit each.

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/item.h"

namespace facet::internal
{

struct ColorPlaneOptions
{
    // The fractions under a point (screen pixels), unclamped: x from 0 to 1 across, y from 0 to 1
    // upward. Required.
    ImVec2 (*map)(const void* user, ImVec2 point) = nullptr;
    // Moves fractions into the area the part covers (the triangle of the wheel); null clamps each
    // to 0 to 1.
    void (*clamp)(const void* user, double* x, double* y) = nullptr;
    const void* user = nullptr;
    bool wrapX = false;    // x is a circle (the hue ring): steps and drags go on past either end
    bool twoAxes = true;   // false: there is no y, and Up and Down step x as well
    bool readOnly = false; // shown and focusable, never written (also ImGuiItemFlags_ReadOnly)
};

struct ColorPlaneResult
{
    bool changed = false;   // *x or *y was written this frame
    bool canceled = false;  // Escape ended the edit
    bool dragging = false;  // a mouse drag is under way
    bool keyboard = false;  // a key edit is under way
};

// Runs the input of the part `item` (from PartBehavior this frame) on the fractions *x and *y. A
// disabled or read-only part never writes, and an item showing a forced state takes no input.
ColorPlaneResult ColorPlaneBehavior(const ItemState& item, double* x, double* y, const ColorPlaneOptions& options);

// Forgets the session in progress (facet::Init and Shutdown, through the color pickers subsystem).
void ResetColorPlaneState();

} // namespace facet::internal
