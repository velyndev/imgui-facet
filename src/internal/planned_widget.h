#pragma once

// The placeholder a declared but not yet implemented widget draws, so every stub looks the
// same and says what it is: a box with a hairline outline and the text "<Widget>: planned"
// (plus the visible part of the label), one item of the given height and width.
//
// Used by the stubs of the three widget API packages until Phase 2 replaces them. The item
// takes the widget's id from `label` (Dear ImGui rules), is not interactive and is not a Tab
// stop, so a page full of stubs never shows something that looks operable.

namespace facet::internal
{

// `widget` is the public function name ("RangeSlider"); `label` the widget's label or id
// (null for none). `width` in pixels: 0 or less fills the available width. `height` in
// pixels: 0 or less is the regular control height. Draws nothing outside a frame.
void PlannedWidget(const char* widget, const char* label, float width = 0.0f, float height = 0.0f);

} // namespace facet::internal
