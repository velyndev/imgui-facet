#pragma once

// UI scale. Every length a widget draws or lays out goes through Px(), which multiplies a
// design unit (a pixel at scale 1) by Scale().
//
// Scale() depends on the mode:
//   FollowDpi   DpiScale() * UiScale()   desktop apps: follows the monitor DPI
//   Manual      UiScale()                overlays: the host decides alone
//   FitDisplay  fit * UiScale(), with fit = min(display width / 1920, display height / 1080)
//               read from ImGui's DisplaySize in facet::NewFrame: menus that grow and shrink
//               with the host's render resolution
// All values are valid before Init. A change of Scale() notifies the library modules
// (shadow and icon atlases are rasterized per scale) and, while a Dear ImGui context exists
// and SetManageFontScale is on, is written into its style: FontScaleDpi = Scale() / UiScale() and FontScaleMain = UiScale(),
// so text pushed at a design size (facet::PushTextStyle) grows with the same factor as every
// Px() length.
//
// Pixel snapping: at fractional scales a Px() length falls between pixels, which blurs thin
// lines and the edges of filled shapes. The Snap helpers round to the pixel grid; positions
// are assumed to be in pixels with the grid at whole numbers (Dear ImGui's convention).

#include <imgui.h>

namespace facet
{

enum class ScaleMode
{
    FollowDpi,
    Manual,
    FitDisplay,
};

// The effective scale factor.
float Scale();

// Design units to pixels.
float Px(float units);
ImVec2 Px(ImVec2 units);

// The user's scale preference, clamped to 0.5 to 4. Default 1.
void SetUiScale(float scale);
float UiScale();

// Monitor DPI / 96, reported by the host (Facet never queries or changes DPI awareness).
// Clamped to 0.5 to 8. Default 1.
void SetDpiScale(float scale);
float DpiScale();

void SetScaleMode(ScaleMode mode);
ScaleMode GetScaleMode();  // default FollowDpi

// Whether Facet writes style.FontScaleDpi and style.FontScaleMain (see above). On by default:
// Facet then writes them on every scale change and in every facet::NewFrame, replacing any
// value the host set. A host that manages the font scale itself, for example an overlay
// inside another application, turns this off; Facet text then grows with the host's font
// scale, which should equal Scale() for text to match the Px() lengths.
void SetManageFontScale(bool enabled);
bool ManagesFontScale();

// Rounds a pixel coordinate or length to the nearest whole pixel.
float Snap(float pixels);
ImVec2 Snap(ImVec2 pixels);

// Px(units) rounded to whole pixels, at least 1 when units > 0. For borders, gaps and
// insets that must stay crisp.
float PxSnapped(float units);

// The center line for a stroke of `thickness` whole pixels that starts at the pixel
// containing `pixels`: lines with an odd thickness sit on a pixel center (x.5), even ones
// on a pixel edge, so the stroke covers whole pixels and is not smeared over two.
float SnapStroke(float pixels, float thickness);

// The rectangle with its edges rounded to whole pixels (min and max separately).
void SnapRect(ImVec2& pMin, ImVec2& pMax);

} // namespace facet
