#pragma once

// Key caps: a chord drawn as one small cap per modifier and one for the key, inside another
// control (the slots of KeybindCapture, rows that show a binding). Measuring and drawing are
// separate, so a control can decide before drawing whether the caps fit its width:
//
//     const float capsWidth = MeasureKeyCaps(chord, m.size);
//     if (capsWidth <= room)
//         DrawKeyCaps(drawList, chord, ImVec2(x, rect.GetCenter().y), m.size, item.disabled);
//     else
//         ... draw ChordToText cut with an ellipsis (FitText in control.h) and its tooltip ...
//
// The look: each cap is the control height less spacing.xs above and below, at least as wide
// as it is tall, radii.element, the window background bg with the 3:1 boundary hairline, the
// key name (facet::KeyNameText) in Footnote (Caption for ControlSize::Small) in label, or
// labelTertiary while disabled; caps stand spacing.xs apart. A sided modifier key (LeftShift)
// is folded into its modifier, so "Ctrl" and "Shift" read the same whichever side was pressed.

#include <imgui.h>

#include "facet/widgets/common.h"

namespace facet::internal
{

// Width in pixels of the caps of `chord` at control size `size`; 0 for ImGuiKey_None.
float MeasureKeyCaps(ImGuiKeyChord chord, ControlSize size);

// Height in pixels of one cap at control size `size`.
float KeyCapHeight(ControlSize size);

// Draws the caps with the left edge at `leftCenter.x`, centered on `leftCenter.y`, and returns
// their width. `pressed` draws every cap in fill[1], for a chord that is held right now.
float DrawKeyCaps(ImDrawList* drawList, ImGuiKeyChord chord, ImVec2 leftCenter, ControlSize size, bool disabled,
                  bool pressed = false);

} // namespace facet::internal
