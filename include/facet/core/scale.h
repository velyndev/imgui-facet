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
// (shadow and icon atlases are rasterized per scale) and, while a Dear ImGui context exists,
// is written into its style: FontScaleDpi = Scale() / UiScale() and FontScaleMain = UiScale(),
// so text pushed at a design size (facet::PushTextStyle) grows with the same factor as every
// Px() length.

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

} // namespace facet
