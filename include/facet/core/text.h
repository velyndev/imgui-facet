#pragma once

// Text styles and fonts.
//
// Facet keeps one ImFont per weight (regular, semibold, bold) plus optional display faces
// for the large title styles, and draws a TextStyle by pushing the font of its weight at
// the style's size from T().type. Sizes are passed to ImGui unscaled; ImGui applies the
// UI scale through the style's font scale, so text follows Scale() like everything else.
//
// By default the fonts come from the system, loaded in facet::Init into the font atlas of the
// current Dear ImGui context:
//   1. Segoe UI Variable (SegUIVar.ttf): the named instances Text Regular, Text Semibold,
//      Text Bold, Display Semibold and Display Bold. Used only when Dear ImGui rasterizes
//      with FreeType and the instances really differ (the bold one measures wider than the
//      regular one); otherwise the next step is taken.
//   2. The static Segoe UI files segoeui.ttf, seguisb.ttf and segoeuib.ttf (Windows 10).
//   3. No Facet fonts: every style draws with the atlas's default font at the style's size,
//      and a warning is written to the debugger output and stderr.
// Display faces draw the LargeTitle and Title1 styles; a missing display face falls back to
// the text face of the same weight. The regular face becomes io.FontDefault unless the app
// set one, so stock Dear ImGui widgets use it too. An app supplies its own fonts with
// SetFontSet. Shutdown removes the fonts Facet added.

#include <cstdarg>
#include <cstddef>

#include <imgui.h>

#include "facet/core/theme.h"

namespace facet
{

// One font face, from a file or from memory. `data` is not copied: it must stay valid
// until Shutdown. `faceIndex` is passed to the rasterizer as the face index (for a named
// instance of a variable font: (instanceIndex + 1) << 16).
struct FontSource
{
    const char* path = nullptr;  // UTF-8 file path; used when data is null
    const void* data = nullptr;
    std::size_t size = 0;
    int faceIndex = 0;

    bool IsSet() const { return path != nullptr || data != nullptr; }
};

// The faces Facet uses. Regular, semibold and bold are required for a custom set; unset
// display faces fall back to the text faces of the same weight.
struct FontSet
{
    FontSource regular;
    FontSource semibold;
    FontSource bold;
    FontSource displayRegular;
    FontSource displaySemibold;
    FontSource displayBold;
};

// Replaces the system fonts. Call before Init; a call after Init reloads the fonts at the
// next NewFrame. Passing an empty FontSet returns to the system fonts. File paths are
// copied; memory data is not. A set whose regular, semibold or bold face cannot be loaded
// is not used: the system fonts load instead, with a warning.
void SetFontSet(const FontSet& fonts);

// Where the fonts in use come from.
enum class FontOrigin
{
    None,       // not loaded (before Init, or every source failed: the default font draws)
    Custom,     // the FontSet passed to SetFontSet
    Variable,   // Segoe UI Variable named instances
    Static,     // the static Segoe UI files
};
FontOrigin ActiveFontOrigin();
// A short English name of the origin for diagnostics, never null.
const char* FontOriginName(FontOrigin origin);

// The font of a weight, or nullptr while fonts are not loaded (ImGui's PushFont then keeps
// the current font). `display` selects the display face where one exists.
ImFont* WeightFont(FontWeight weight, bool display = false);

// Pushes the font and size of a style; pair with PopTextStyle.
void PushTextStyle(TextStyle style);
void PopTextStyle();

// Text in a style, with printf formatting. Uses the current text color.
void Text(TextStyle style, const char* fmt, ...) IM_FMTARGS(2);
void TextV(TextStyle style, const char* fmt, va_list args) IM_FMTLIST(2);

// Size of `text` drawn in a style, in pixels at the current scale. `textEnd` null means
// zero terminated; wrapWidth <= 0 means no wrapping.
ImVec2 MeasureText(TextStyle style, const char* text, const char* textEnd = nullptr, float wrapWidth = 0.0f);

} // namespace facet
