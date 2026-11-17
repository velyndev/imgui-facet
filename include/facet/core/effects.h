#pragma once

// Soft shadows, glow and blurred translucent panels.
//
// Like the draw helpers, these functions take screen pixels (see draw.h). Shadows and glow
// are textured quads from a baked atlas and batch with normal drawing; DrawGlass adds a draw
// callback that blurs what was rendered before it.
//
// Where each effect belongs (the dose rules of DESIGN.md):
//   - Shadows only under floating layers: popovers, menus, dialogs, the floating menu
//     shell, dragged items. Content that sits on a surface has no shadow.
//   - Glass only for the menu shell and popovers. Panels inside a page are opaque surfaces.
//   - Glow only as the focus ring halo, plus at most one opt-in accent glow per screen.
// Effects drawn everywhere stop meaning anything; drawn sparingly they mark what floats
// above the rest and where the keyboard focus is.

#include <imgui.h>

namespace facet
{

// A soft drop shadow of a rounded rectangle. Lengths in pixels.
struct ShadowSpec
{
    ImVec2 offset = ImVec2(0.0f, 0.0f);
    float blur = 0.0f;    // blur radius: the shadow fades out about this far beyond the
                          // shape (two standard deviations of the Gaussian); 0 gives a hard edge
    float spread = 0.0f;  // grows (positive) or shrinks the shape before blurring
    ImU32 color = 0;      // including the shadow's opacity
};

// The shadow of an elevation level (1 to 3, clamped) from T().elevation and
// T().colors.shadow, converted to pixels with Px().
ShadowSpec ElevationShadow(int level);

// Draws the shadow of the rect pMin..pMax with corner `radius`. Draw it before the shape;
// the shadow also covers the area under the shape, so the shape should be opaque. A glass
// panel is not opaque: pass its shadow in GlassSpec::shadow instead, because a shadow drawn
// before DrawGlass is blurred into the panel's backdrop and darkens it.
// Blur is limited to 128 pixels and the radius to 128 pixels; the shape's radius and the
// blur are rounded to whole and half pixels so similar shadows share one baked tile.
// A shadow wholly outside the draw list's clip rect records nothing. A tile is baked on the
// CPU while recording, and until its texels are on the atlas page every shadow that uses it
// gets a small upload callback right in front of its geometry. The first of those callbacks
// the renderer runs does the upload, so the tile is in place before its first draw even
// when the draw list's channels are split and merged in another order (tables, or any
// ImDrawListSplitter); the device context is only used while the draw data is rendered.
// Once uploaded, a tile adds no callback.
void DrawShadow(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const ShadowSpec& spec);

// DrawShadow with ElevationShadow(level). Level 0 draws nothing.
void DrawElevation(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, int level);

// A soft glow of `size` pixels around the rect (focus rings, the accent glow): a shadow
// without offset in `color`, drawn outside the shape only, so it can go before or after it.
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
    // The panel's shadow, for example ElevationShadow(2) for a popover; a zero color draws
    // none. It is drawn after the backdrop is captured, so it shows around the panel but is
    // never part of the blurred backdrop behind it.
    ShadowSpec shadow = {};
};

// Draws a panel that blurs the content rendered behind it, tinted and with corner
// `radius`. Where blurring is not possible (GlassAvailable() is false), the panel is an
// opaque tint instead, so content on it stays readable.
//
// The blur runs at most once per frame and render target: the first glass panel of a frame
// blurs everything drawn before it, and later panels in the same frame reuse that image
// (with the first panel's blur radius and saturation). Draw the backdrop of every glass
// panel before the first one. The render target is copied, never bound as an input, so it
// works in a swap chain owned by a host application, with multisampled, sRGB and typeless
// formats. Must be drawn into a draw list of the main viewport. The blurred image is one
// shared texture of the main viewport's size, so all glass panels of a frame must come from
// one Dear ImGui context with one viewport size: a panel recorded for another size samples a
// texture the blur no longer writes. A panel wholly outside the
// draw list's clip rect records no capture and no panel, only its shadow when that reaches
// into view.
void DrawGlass(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const GlassSpec& spec = {});

// The panel DrawGlass draws when it cannot blur: spec.shadow, the tint at full opacity and
// the hairline.
void DrawGlassFallback(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const GlassSpec& spec = {});

// True when DrawGlass can blur on the current device and render target. It turns false
// when a capture fails while rendering (no render target, a format that cannot be sampled,
// a texture that cannot be created); DrawGlass then tries again every 2 seconds.
bool GlassAvailable();

} // namespace facet
