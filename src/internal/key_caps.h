#pragma once

// Key caps: a chord drawn as one small cap per modifier and one for the key. KeybindCapture
// draws them inside its slots, and the display marks (KeyCap, ShortcutHint) draw them on a line
// of text with their own metrics; both take the cap names and colors from here, so a chord looks
// the same wherever it is shown. Measuring and drawing inside a control are separate, so a control
// can decide before drawing whether the caps fit its width:
//
//     const float capsWidth = MeasureKeyCaps(chord, m.size);
//     if (capsWidth <= room)
//         DrawKeyCaps(drawList, chord, ImVec2(x, rect.GetCenter().y), m.size, item.disabled);
//     else
//         ... draw ChordToText cut with an ellipsis (FitText in control.h) and its tooltip ...
//
// The look: each cap is at least as wide as it is tall, radii.element, fill[2] with the 3:1
// boundary hairline (a held cap fill[0] with a focusRing outline; a disabled one fill[3] with a
// separator hairline), the key name (facet::KeyNameText) in label, or labelTertiary while
// disabled; caps stand spacing.xs apart. Inside a control a cap is the control height less
// spacing.xs above and below, with Footnote text (Caption for ControlSize::Small). A sided
// modifier key (LeftShift) is folded into its modifier, so "Ctrl" and "Shift" read the same
// whichever side was pressed.

#include <imgui.h>

#include "facet/widgets/common.h"

namespace facet::internal
{

// The most caps one chord has: Ctrl, Shift, Alt, Win and the key.
inline constexpr int kKeyCapMax = 5;

// The names of the caps of `chord`, modifiers first (Ctrl, Shift, Alt, Win), then the key; a
// modifier key alone folds into the modifiers and a key without a name is left out. Returns how
// many were written.
int KeyCapNames(ImGuiKeyChord chord, const char* (&names)[kKeyCapMax]);

// The colors of a cap.
struct KeyCapColors
{
    ImU32 fill = 0;
    ImU32 outline = 0;
    ImU32 text = 0;
};
KeyCapColors KeyCapLook(bool pressed, bool disabled);

// Width in pixels of the caps of `chord` at control size `size`; 0 for ImGuiKey_None.
float MeasureKeyCaps(ImGuiKeyChord chord, ControlSize size);

// Height in pixels of one cap at control size `size`.
float KeyCapHeight(ControlSize size);

// Draws the caps with the left edge at `leftCenter.x`, centered on `leftCenter.y`, and returns
// their width. `pressed` draws the held look, for a chord that is held right now.
float DrawKeyCaps(ImDrawList* drawList, ImGuiKeyChord chord, ImVec2 leftCenter, ControlSize size, bool disabled,
                  bool pressed = false);

} // namespace facet::internal
