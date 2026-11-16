#pragma once

#include "pages/pages.h"

namespace facet::demo
{

class AppShell;

struct GalleryState
{
    int selectedPage = 0;  // index into Pages()
};

// io settings shared by all modes. Deterministic runs (capture, selftest) also disable
// the text cursor blink and mouse cursor changes so frames depend only on the input.
void ConfigureImGuiIo(bool deterministic);

// Resets the ImGui style to its dark defaults and applies the UI scale to sizes and fonts.
void ApplyStyle(float scale);

// Opaque clear color behind the UI, taken from the current style.
void ClearColor(float out[4]);

// Draws the whole demo UI: title strip, page navigation and the selected page.
// With a shell, the title strip moves the window and its buttons control it; without one
// (offscreen runs) the strip is drawn the same way and the buttons do nothing.
void DrawGallery(GalleryState& state, PageCtx& ctx, AppShell* shell);

} // namespace facet::demo
