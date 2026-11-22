#pragma once

// The backdrop of overlay mode: a busy scene drawn in code that stands in for the game or app an
// overlay menu floats over, so the shell's backdrop blur has detail and motion to work on.

#include <imgui.h>

namespace facet::demo
{

// Draws the backdrop into `drawList` over the rectangle min to max (pixels): the window
// background, slanted bands, circles on slow orbits and rows of text, in the chart palette and the
// label color of the theme. `time` is seconds of motion; the same time draws the same picture,
// so a capture is the same at any frame rate.
void DrawOverlayBackdrop(ImDrawList* drawList, ImVec2 min, ImVec2 max, double time);

} // namespace facet::demo
