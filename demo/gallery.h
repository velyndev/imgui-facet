#pragma once

#include <cstdint>

#include "pages/pages.h"

namespace facet::demo
{

class AppShell;

struct GalleryState
{
    int selectedPage = 0;      // index into Pages()
    bool dark = true;          // theme shown while followSystem is off
    bool followSystem = false; // show the Windows app mode instead of `dark`
    float uiScale = 1.0f;      // user scale; the title strip control changes it
    float baseScale = 1.0f;    // set by the caller every frame: monitor DPI / 96 in the window,
                               // --scale offscreen

    // Theme mode and total scale last applied to Facet. DrawGallery applies them again
    // whenever they differ from the values above. -1: nothing applied yet; the first frame
    // also forgets an accent left over from an earlier session.
    int appliedMode = -1;
    float appliedScale = 0.0f;
};

// io settings shared by all modes. Deterministic runs (capture, selftest) also disable
// the text cursor blink and mouse cursor changes so frames depend only on the input.
void ConfigureImGuiIo(bool deterministic);

// Sets the DPI part of the Facet scale and writes the current theme and scale into the
// ImGui style (facet::ApplyImGuiStyle). Called once before Facet starts.
void ApplyStyle(float scale);

// Opaque clear color behind the UI: the theme's background.
void ClearColor(float out[4]);

// Draws the whole demo UI: title strip, page navigation and the selected page.
// First applies the theme mode and scale of `state` when they changed (facet::SetThemeMode,
// facet::SetDpiScale, facet::SetUiScale, facet::ApplyImGuiStyle; the style then follows
// every theme transition through facet::SyncImGuiStyle), and sets ctx.dark and ctx.scale.
// Theme switches animate in the window; capture and selftest runs (ctx.capture or
// ctx.probes set) switch at once and ignore the Windows animation setting, so their frames
// do not depend on timing or on the machine. Must run before any ImGui window is begun.
// With a shell, the title strip moves the window and its buttons control it; without one
// (offscreen runs) the strip is drawn the same way and the window buttons do nothing.
void DrawGallery(GalleryState& state, PageCtx& ctx, AppShell* shell);

// Building blocks for the demo pages, drawn with the text style and color tokens.

// The page title (Title1) and a one-paragraph summary under it.
void PageHeader(const char* title, const char* summary);
// A section heading (Title3) with the section spacing above it.
void SectionHeader(const char* title);
// Wrapped body text in labelSecondary.
void Paragraph(const char* text);
// One line of footnote text in labelSecondary, printf formatted.
void Note(const char* fmt, ...) IM_FMTARGS(1);

} // namespace facet::demo
