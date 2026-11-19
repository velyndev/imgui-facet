#pragma once

// The placeholder a declared but not yet implemented widget draws, so every stub says what it
// is in the same way: a box and the text "<Widget>: planned (<visible label>)" in Footnote and
// labelSecondary (labelTertiary inside BeginDisabled).
//
// Every widget group shares the caption: PlannedCaption picks the text that fits a box, and
// DrawPlannedCaption draws it on the first line of the box, level with a row label beside it,
// with the whole text in a tooltip while hovered when it had to be shortened. The controls and
// content stubs draw a fill[3] box with a separator hairline in the control radius around it
// (PlannedWidget, and DrawStub in src/widgets/controls_stub.h); the structure stubs draw a dashed
// frame around the caller's content (src/widgets/structure_stub.cpp).
//
// The item takes the widget's id from `label` (Dear ImGui rules), is not interactive and is not
// a Tab stop, so a page full of stubs never shows something that looks operable. Phase 2
// replaces the stubs.

#include <cstddef>

#include <imgui.h>
#include <imgui_internal.h>

namespace facet::internal
{

inline constexpr int kPlannedCaptionSize = 192;

// Writes the placeholder text for a box `room` pixels wide, measured in the current font. The
// first that fits, in this order: "<widget>: planned (<visible label>)", "<widget>: planned",
// the visible label alone, the widget name alone; else the visible label (or, without one, the
// widget name) shortened with an ellipsis. The label comes before the name because boxes side
// by side in one row differ by their labels, while the row and the box look already say which
// widget is planned. Empty when `room` is 0 or less or not even the ellipsis fits. `out` is
// always zero terminated (outSize > 0). Returns true when anything had to be left out.
bool PlannedCaption(const char* widget, const char* label, float room, char* out, std::size_t outSize);

// Draws the caption of `widget` and `label` into `box` (pixels), inset by spacing.sm: centered in
// a box of one line, on the first line of a taller box. With `hovered` and a shortened caption,
// the whole text shows as a tooltip. Draws into the current window; submits no item.
void DrawPlannedCaption(ImDrawList* drawList, const ImRect& box, const char* widget, const char* label, bool hovered);

// The content and controls placeholder item: a fill[3] box with a separator hairline and the
// caption. `widget` is the public function name ("RangeSlider"); `label` the widget's label or
// id (null for none). `width` in pixels: 0 or less fills the available width. `height` in
// pixels: 0 or less is the regular control height. Draws nothing outside a frame.
void PlannedWidget(const char* widget, const char* label, float width = 0.0f, float height = 0.0f);

} // namespace facet::internal
