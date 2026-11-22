#pragma once

// Text helpers shared by widgets: truncation with an ellipsis, line wrapping and drawing text
// with highlighted ranges (search matches). Implemented in src/core/text_util.cpp.
//
// Every function measures with an explicit font and size; pass ImGui::GetFont() and
// ImGui::GetFontSize() for the current text style (after facet::PushTextStyle). Text is
// UTF-8; cuts happen only at character boundaries. Nothing here allocates memory: results go
// into caller buffers.

#include <cstddef>
#include <span>

#include <imgui.h>

namespace facet::internal
{

enum class TruncateMode
{
    End,     // "A long file na..."
    Middle,  // "A long f...ame.txt": keeps the end, for paths and file names
    Start,   // "...file name.txt": keeps only the end
};

struct TruncateResult
{
    bool truncated = false;  // the text did not fit and was shortened
    float width = 0.0f;      // width of the written text in pixels
    int length = 0;          // bytes written to the buffer, without the terminator
};

// Writes `text` (to `textEnd`, or zero terminated when null) into `out` as it fits into
// `maxWidth` pixels: unchanged when it fits, otherwise shortened at the end or in the middle
// with the font's ellipsis character. When not even the ellipsis fits, `out` is empty.
// `out` is always zero terminated; text that does not fit the buffer is cut there as well.
TruncateResult TruncateText(ImFont* font, float size, const char* text, const char* textEnd, float maxWidth,
                            TruncateMode mode, char* out, std::size_t outSize);

// One wrapped line: [begin, end) without the trailing spaces or line break.
struct WrapLine
{
    const char* begin = nullptr;
    const char* end = nullptr;
    float width = 0.0f;
};

// Breaks text into lines no wider than `wrapWidth` (at spaces, or inside a word that is longer
// than a line), honouring explicit line breaks. Writes up to lines.size() lines and returns the
// total number of lines, which may be larger: call again with more room to get them all.
int WrapText(ImFont* font, float size, const char* text, const char* textEnd, float wrapWidth,
             std::span<WrapLine> lines);

// A byte range [begin, end) in a text, for highlighted search matches.
struct TextRange
{
    int begin = 0;
    int end = 0;
};

// Colors of DrawHighlightedText.
struct HighlightStyle
{
    ImU32 text = 0;           // text outside the ranges
    ImU32 matchText = 0;      // text inside the ranges
    ImU32 matchBackground = 0;
    float rounding = 0.0f;    // corner radius of the match backgrounds in pixels
    float padding = 0.0f;     // horizontal padding of the match backgrounds in pixels
};

// Draws one line of text at `pos` with `ranges` (sorted, not overlapping, clamped to the
// text) drawn on a background. Returns the width of the text.
float DrawHighlightedText(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, const char* text,
                          const char* textEnd, std::span<const TextRange> ranges, const HighlightStyle& style);

// Finds every case-insensitive (ASCII) occurrence of `query` in `text`, writes up to
// ranges.size() of them and returns how many exist. An empty query matches nothing.
int FindMatches(const char* text, const char* textEnd, const char* query, std::span<TextRange> ranges);

// A number or a text for a template of the Strings table: "{name}" in the template becomes
// `value`, or `text` when that is set (a preset or action name: {"name", 0, label}).
struct TextToken
{
    const char* name = nullptr;  // without braces, for example "count"
    long long value = 0;
    const char* text = nullptr;  // UTF-8, zero terminated; null writes `value` instead
};

// Writes `format` (a Strings template, which may come from a translation) into `out` with each
// "{name}" of `tokens` replaced by its text or its decimal value. A token's text is copied as it
// is, never read as a template again. Everything else is copied as written:
// '%', unknown "{...}" and lone braces included, so no input can cause undefined behaviour.
// `out` is always zero terminated and cut at a whole UTF-8 character when it is too small.
// Returns the bytes written without the terminator. A null format writes "".
int FormatTokens(char* out, std::size_t outSize, const char* format, std::span<const TextToken> tokens);

} // namespace facet::internal
