#pragma once

// Soft shadows, glow and blurred translucent panels.
//
// Like the draw helpers, these functions take screen pixels (see draw.h). Shadows and glow
// batch with normal drawing; DrawGlass adds a draw callback that blurs what was rendered
// before it.

#include <imgui.h>

namespace facet
{

// A soft drop shadow of a rounded rectangle. Lengths in pixels.
struct ShadowSpec
{
    ImVec2 offset = ImVec2(0.0f, 0.0f);
    float blur = 0.0f;    // Gaussian blur radius; 0 gives a hard edge
    float spread = 0.0f;  // grows (positive) or shrinks the shape before blurring
    ImU32 color = 0;      // including the shadow's opacity
};

// The shadow of an elevation level (1 to 3, clamped) from T().elevation and
// T().colors.shadow, converted to pixels with Px().
ShadowSpec ElevationShadow(int level);

// Draws the shadow of the rect pMin..pMax with corner `radius`. Draw it before the shape.
void DrawShadow(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const ShadowSpec& spec);

// DrawShadow with ElevationShadow(level). Level 0 draws nothing.
void DrawElevation(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, int level);

// A soft glow of `size` pixels around the rect (focus rings, the accent glow), drawn
// outside the shape so it can go before or after it.
void DrawGlow(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, ImU32 color, float size);

// Blurred panel settings. Negative values and a zero tint alpha take the theme's Glass
// tokens and colors.glassTint.
struct GlassSpec
{
    float blurRadius = -1.0f;  // pixels
    float tintOpacity = -1.0f;
    float saturation = -1.0f;
    ImVec4 tint = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    bool hairline = true;  // draws a separator-colored hairline border
};

// Draws a panel that blurs the content rendered behind it, tinted and with corner
// `radius`. Where blurring is not possible (GlassAvailable() is false), the panel is an
// opaque tint instead, so content on it stays readable.
void DrawGlass(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const GlassSpec& spec = {});

// True when DrawGlass can blur on the current device and render target.
bool GlassAvailable();

} // namespace facet
