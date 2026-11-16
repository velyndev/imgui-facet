#pragma once

// Text styles and fonts.
//
// Facet keeps one ImFont per weight (regular, semibold, bold) plus optional display faces
// for the large title styles, and draws a TextStyle by pushing the font of its weight at
// the style's size from T().type. Sizes are passed to ImGui unscaled; ImGui applies the
// UI scale through the style's font scale, so text follows Scale() like everything else.
//
// By default the fonts come from the system: Segoe UI Variable (its named instances for
// each weight) where installed, otherwise the static Segoe UI files. An app supplies its
// own fonts with SetFontSet.

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
// next NewFrame. Passing an empty FontSet returns to the system fonts.
void SetFontSet(const FontSet& fonts);

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
