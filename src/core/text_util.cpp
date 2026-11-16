// Text helpers: truncation, wrapping, match highlighting (see src/internal/text_util.h).

#include "internal/text_util.h"

#include <algorithm>
#include <cfloat>
#include <cstring>

#include <imgui_internal.h>

namespace facet::internal
{
namespace
{

bool IsContinuation(unsigned char c)
{
    return (c & 0xC0u) == 0x80u;
}

// Moves back to the start of the UTF-8 character that contains `p`.
const char* CharStart(const char* begin, const char* p)
{
    while (p > begin && IsContinuation(static_cast<unsigned char>(*p)))
        --p;
    return p;
}

float Width(ImFont* font, float size, const char* begin, const char* end)
{
    if (begin >= end)
        return 0.0f;
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, begin, end).x;
}

// The start of the character after the one at `p` (never past `limit`).
const char* NextChar(const char* p, const char* limit)
{
    ++p;
    while (p < limit && IsContinuation(static_cast<unsigned char>(*p)))
        ++p;
    return p;
}

// The longest prefix of [begin, end) that is at most `maxWidth` wide; returns its end.
// Binary search over character boundaries: `low` always fits, `high` never does.
const char* FitPrefix(ImFont* font, float size, const char* begin, const char* end, float maxWidth)
{
    if (Width(font, size, begin, end) <= maxWidth)
        return end;
    const char* low = begin;
    const char* high = end;
    for (;;)
    {
        const char* mid = CharStart(begin, low + (high - low) / 2);
        if (mid <= low)
            mid = NextChar(low, high);
        if (mid >= high)
            return low;
        if (Width(font, size, begin, mid) <= maxWidth)
            low = mid;
        else
            high = mid;
    }
}

// The earliest start in [begin, end] whose suffix up to `end` is at most `maxWidth` wide.
// `high` always fits (the empty suffix does), `low` never does.
const char* FitSuffix(ImFont* font, float size, const char* begin, const char* end, float maxWidth)
{
    if (Width(font, size, begin, end) <= maxWidth)
        return begin;
    const char* low = begin;
    const char* high = end;
    for (;;)
    {
        const char* mid = CharStart(begin, low + (high - low) / 2);
        if (mid <= low)
            mid = NextChar(low, high);
        if (mid >= high)
            return high;
        if (Width(font, size, mid, end) <= maxWidth)
            high = mid;
        else
            low = mid;
    }
}

int EncodeUtf8(unsigned int c, char* out)
{
    if (c < 0x80)
    {
        out[0] = static_cast<char>(c);
        return 1;
    }
    if (c < 0x800)
    {
        out[0] = static_cast<char>(0xC0 | (c >> 6));
        out[1] = static_cast<char>(0x80 | (c & 0x3F));
        return 2;
    }
    out[0] = static_cast<char>(0xE0 | (c >> 12));
    out[1] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
    out[2] = static_cast<char>(0x80 | (c & 0x3F));
    return 3;
}

// Appends [begin, end) to out at `length`, cut at a character boundary when out is full.
void Append(char* out, std::size_t outSize, int& length, const char* begin, const char* end)
{
    const std::size_t room = outSize - 1 - static_cast<std::size_t>(length);
    std::size_t count = static_cast<std::size_t>(end - begin);
    if (count > room)
    {
        count = room;
        while (count > 0 && IsContinuation(static_cast<unsigned char>(begin[count])))
            --count;
    }
    std::memcpy(out + length, begin, count);
    length += static_cast<int>(count);
    out[length] = '\0';
}

char ToLowerAscii(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

} // namespace

TruncateResult TruncateText(ImFont* font, float size, const char* text, const char* textEnd, float maxWidth,
                            TruncateMode mode, char* out, std::size_t outSize)
{
    TruncateResult result;
    if (out == nullptr || outSize == 0)
        return result;
    out[0] = '\0';
    if (font == nullptr || text == nullptr)
        return result;
    if (textEnd == nullptr)
        textEnd = text + std::strlen(text);

    const float full = Width(font, size, text, textEnd);
    if (full <= maxWidth)
    {
        Append(out, outSize, result.length, text, textEnd);
        result.width = Width(font, size, out, out + result.length);
        result.truncated = result.length < static_cast<int>(textEnd - text);
        return result;
    }

    result.truncated = true;
    char ellipsis[4] = {};
    const ImWchar ellipsisChar = font->EllipsisChar != 0 ? font->EllipsisChar : static_cast<ImWchar>('.');
    int ellipsisLength = EncodeUtf8(ellipsisChar, ellipsis);
    if (font->EllipsisChar == 0)
    {
        std::memcpy(ellipsis, "...", 3);
        ellipsisLength = 3;
    }
    const float ellipsisWidth = Width(font, size, ellipsis, ellipsis + ellipsisLength);
    const float room = maxWidth - ellipsisWidth;
    if (room < 0.0f)
        return result;

    if (mode == TruncateMode::End)
    {
        const char* cut = FitPrefix(font, size, text, textEnd, room);
        // Do not leave a space dangling before the ellipsis.
        while (cut > text && cut[-1] == ' ')
            --cut;
        Append(out, outSize, result.length, text, cut);
        Append(out, outSize, result.length, ellipsis, ellipsis + ellipsisLength);
    }
    else
    {
        const char* headEnd = FitPrefix(font, size, text, textEnd, room * 0.5f);
        const float headWidth = Width(font, size, text, headEnd);
        const char* tailBegin = FitSuffix(font, size, headEnd, textEnd, room - headWidth);
        Append(out, outSize, result.length, text, headEnd);
        Append(out, outSize, result.length, ellipsis, ellipsis + ellipsisLength);
        Append(out, outSize, result.length, tailBegin, textEnd);
    }
    result.width = Width(font, size, out, out + result.length);
    return result;
}

int WrapText(ImFont* font, float size, const char* text, const char* textEnd, float wrapWidth,
             std::span<WrapLine> lines)
{
    if (font == nullptr || text == nullptr)
        return 0;
    if (textEnd == nullptr)
        textEnd = text + std::strlen(text);
    int count = 0;
    const char* cursor = text;
    while (cursor < textEnd || count == 0)
    {
        const char* lineEnd = textEnd;
        const char* newline = static_cast<const char*>(std::memchr(cursor, '\n', static_cast<size_t>(textEnd - cursor)));
        const char* paragraphEnd = newline != nullptr ? newline : textEnd;
        const char* next = paragraphEnd;
        if (wrapWidth > 0.0f && cursor < paragraphEnd)
        {
            const char* wrap = font->CalcWordWrapPosition(size, cursor, paragraphEnd, wrapWidth);
            if (wrap <= cursor)
            {
                // Not even one character fits: take one anyway so the text always advances.
                wrap = cursor + 1;
                while (wrap < paragraphEnd && IsContinuation(static_cast<unsigned char>(*wrap)))
                    ++wrap;
            }
            lineEnd = std::min(wrap, paragraphEnd);
            next = lineEnd;
        }
        else
        {
            lineEnd = paragraphEnd;
        }
        const char* trimmed = lineEnd;
        while (trimmed > cursor && (trimmed[-1] == ' ' || trimmed[-1] == '\r'))
            --trimmed;
        if (count < static_cast<int>(lines.size()))
            lines[static_cast<size_t>(count)] = {cursor, trimmed, Width(font, size, cursor, trimmed)};
        ++count;

        if (next == paragraphEnd && newline != nullptr)
            next = newline + 1;
        else
            while (next < paragraphEnd && *next == ' ')
                ++next;  // the break swallows the spaces it happened at
        if (next >= textEnd)
        {
            // A trailing line break starts one more, empty line.
            if (newline != nullptr && newline + 1 == textEnd)
            {
                if (count < static_cast<int>(lines.size()))
                    lines[static_cast<size_t>(count)] = {textEnd, textEnd, 0.0f};
                ++count;
            }
            break;
        }
        cursor = next;
    }
    return count;
}

float DrawHighlightedText(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, const char* text,
                          const char* textEnd, std::span<const TextRange> ranges, const HighlightStyle& style)
{
    if (font == nullptr || text == nullptr)
        return 0.0f;
    if (textEnd == nullptr)
        textEnd = text + std::strlen(text);
    const int length = static_cast<int>(textEnd - text);
    float x = pos.x;
    int cursor = 0;
    const float lineHeight = size;
    for (const TextRange& range : ranges)
    {
        const int begin = std::clamp(range.begin, cursor, length);
        const int end = std::clamp(range.end, begin, length);
        if (begin > cursor)
        {
            if (drawList != nullptr)
                drawList->AddText(font, size, ImVec2(x, pos.y), style.text, text + cursor, text + begin);
            x += Width(font, size, text + cursor, text + begin);
        }
        if (end > begin)
        {
            const float width = Width(font, size, text + begin, text + end);
            if (drawList != nullptr)
            {
                drawList->AddRectFilled(ImVec2(x - style.padding, pos.y),
                                        ImVec2(x + width + style.padding, pos.y + lineHeight), style.matchBackground,
                                        style.rounding);
                drawList->AddText(font, size, ImVec2(x, pos.y), style.matchText, text + begin, text + end);
            }
            x += width;
        }
        cursor = end;
    }
    if (cursor < length)
    {
        if (drawList != nullptr)
            drawList->AddText(font, size, ImVec2(x, pos.y), style.text, text + cursor, textEnd);
        x += Width(font, size, text + cursor, textEnd);
    }
    return x - pos.x;
}

int FindMatches(const char* text, const char* textEnd, const char* query, std::span<TextRange> ranges)
{
    if (text == nullptr || query == nullptr || query[0] == '\0')
        return 0;
    if (textEnd == nullptr)
        textEnd = text + std::strlen(text);
    const std::size_t queryLength = std::strlen(query);
    const std::size_t length = static_cast<std::size_t>(textEnd - text);
    int count = 0;
    std::size_t i = 0;
    while (i + queryLength <= length)
    {
        bool match = true;
        for (std::size_t k = 0; k < queryLength && match; ++k)
            match = ToLowerAscii(text[i + k]) == ToLowerAscii(query[k]);
        if (match)
        {
            if (count < static_cast<int>(ranges.size()))
                ranges[static_cast<size_t>(count)] = {static_cast<int>(i), static_cast<int>(i + queryLength)};
            ++count;
            i += queryLength;
        }
        else
        {
            ++i;
        }
    }
    return count;
}

} // namespace facet::internal
