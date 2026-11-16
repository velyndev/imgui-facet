#pragma once

#include "pages/pages.h"

namespace facet
{
struct Theme;
}

namespace facet::demo
{

class AppShell;

struct GalleryState
{
    int selectedPage = 0;     // index into Pages()
    bool dark = true;         // theme shown; the title strip switch changes it
    float uiScale = 1.0f;     // user scale; the title strip control changes it
    float baseScale = 1.0f;   // set by the caller every frame: monitor DPI / 96 in the window,
                              // --scale offscreen

    // Theme and total scale last applied to Facet and the ImGui style. DrawGallery applies
    // them again whenever they differ from the values above.
    bool appliedDark = false;
    float appliedScale = 0.0f;
};

// io settings shared by all modes. Deterministic runs (capture, selftest) also disable
// the text cursor blink and mouse cursor changes so frames depend only on the input.
void ConfigureImGuiIo(bool deterministic);

// Resets the ImGui style to its dark defaults and applies the UI scale to sizes and fonts.
void ApplyStyle(float scale);

// Maps the theme's color tokens onto the ImGuiStyle colors, so stock Dear ImGui widgets
// follow the Facet theme. Starts from ImGui's own light or dark colors for the entries
// without a matching token.
void ApplyThemeToImGuiStyle(const facet::Theme& theme);

// Opaque clear color behind the UI, taken from the current style.
void ClearColor(float out[4]);

// Draws the whole demo UI: title strip, page navigation and the selected page.
// First applies the theme and scale of `state` (facet::SetTheme, facet::SetDpiScale,
// facet::SetUiScale, ApplyStyle, ApplyThemeToImGuiStyle) when they changed, and sets
// ctx.dark and ctx.scale. Must run before any ImGui window is begun in the frame.
// With a shell, the title strip moves the window and its buttons control it; without one
// (offscreen runs) the strip is drawn the same way and the window buttons do nothing.
void DrawGallery(GalleryState& state, PageCtx& ctx, AppShell* shell);

} // namespace facet::demo
