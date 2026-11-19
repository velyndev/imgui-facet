#pragma once

// The placeholder a declared but not yet implemented widget draws, so every stub says what it
// is in the same way: a dashed separator hairline with square corners and the text
// "<Widget>: planned (<visible label>)" in Footnote and labelSecondary (labelTertiary inside
// BeginDisabled). Real controls are solid and rounded, so no placeholder can be taken for one.
//
// Every widget group (the helpers in src/widgets/controls_stub.h, structure_stub.h and
// content_stub.h) draws through this file, so all stubs share one look, one item and one width
// rule:
//   - PlannedWidget submits the placeholder item; DrawPlannedBox and DrawPlannedCaption draw the
//     look into a rectangle that is not an item (a message line, a frame around the caller's
//     content, drawn at its End);
//   - PlannedCaption picks the text that fits a box; the caption sits on the first line of the
//     box, level with a row label beside it, with the whole text in a tooltip while hovered when
//     it had to be shortened;
//   - PlannedWidth resolves the width by docs/api-conventions.md rule 6, kFill included;
//   - PlannedCanDraw says when a stub may draw at all.
// Stubs never write through a value pointer, so options.readOnly changes nothing about them:
// every stub honours it the same way, by writing nothing in any case.
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

// True inside a frame (between ImGui::NewFrame and Render) with a current window that is not
// Dear ImGui's fallback window: the only time a stub draws. Outside it, stubs draw nothing and
// bool Begins return false (docs/api-conventions.md rule 14).
bool PlannedCanDraw();

// The widget width in pixels by docs/api-conventions.md rule 6: `optionWidth` from the options
// (> 0 that many units, kFill the available width, other values < 0 the available width minus
// that many units), then ImGui::SetNextItemWidth, then a pushed ImGui::PushItemWidth (both only
// with `itemWidth`; layers that are not items of the caller's layout pass false), then
// `fallback` pixels, where 0 or less is the available width. Never below one pixel; 1 when
// nothing can be drawn.
float PlannedWidth(float optionWidth, float fallback, bool itemWidth = true);

// The placeholder outline around `box` (pixels): a dashed separator hairline with square
// corners, dashes and gaps spacing.xs long. Draws into `drawList`; submits no item.
void DrawPlannedBox(ImDrawList* drawList, const ImRect& box);

// Writes the placeholder text for a box `room` pixels wide, measured in the current font. With
// a visible label: "<widget>: planned (<visible label>)" when it fits, else the visible label
// alone, shortened with an ellipsis when needed. Without one: "<widget>: planned" when it fits,
// else the widget name, shortened when needed. A labelled box never falls back to the name,
// because boxes side by side in one row differ by their labels, while the row and the box look
// already say which widget is planned. Empty when `room` is 0 or less or not even the ellipsis
// fits. `out` is always zero terminated (outSize > 0). Returns true when anything had to be
// left out.
bool PlannedCaption(const char* widget, const char* label, float room, char* out, std::size_t outSize);

// Draws the caption of `widget` and `label` into `box` (pixels), inset by spacing.sm: centered in
// a box of one line, on the first line of a taller box. With `hovered` and a shortened caption,
// the whole text shows as a tooltip. Draws into the current window; submits no item.
void DrawPlannedCaption(ImDrawList* drawList, const ImRect& box, const char* widget, const char* label, bool hovered);

// The placeholder item of every widget group: DrawPlannedBox and the caption. `widget` is the
// public function name ("RangeSlider"); `label` the widget's label or id, whose hash is the item
// id (null: the item has no id, as for a separator; "" hashes like any label). `width` in
// pixels: 0 or less fills the available width. `height` in pixels: 0 or less is the regular
// control height. Draws nothing unless PlannedCanDraw.
void PlannedWidget(const char* widget, const char* label, float width = 0.0f, float height = 0.0f);

} // namespace facet::internal
