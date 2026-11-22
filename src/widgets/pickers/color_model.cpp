// The color math and shared looks of the color pickers (color_model.h).

#include "widgets/pickers/color_model.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/state_storage.h"

namespace facet::internal
{
namespace
{

// How much a held handle grows, as a share of its radius: the same growth as a slider knob, so a
// grab reads the same on every track.
constexpr float kColorHandlePressGrowth = 0.15f;
// The white rim of a handle around the picked color, as a share of its radius: wide enough to read
// as the knob of the sliders, narrow enough to leave most of the handle to the color.
constexpr float kColorHandleRimShare = 0.3f;
// The contrast the edge of a handle keeps against the color under it: the 3:1 of a state mark,
// because the handle's position is the value of the control.
constexpr float kColorHandleEdgeContrast = 3.0f;
// Stripes of the mixed swatch: their spacing in hairlines, and their width.
constexpr float kColorMixedStripeGap = 4.0f;
constexpr float kColorMixedStripeWidth = 1.5f;
// The stripes stay inside the rounded corners: the share of the radius a corner arc reaches in
// from the bounding box along the diagonal (1 - 1/sqrt(2)).
constexpr float kColorCornerInset = 0.2929f;

struct ColorHueMemory
{
    ImVec4 seen = {};
    ColorHsv hsv;
    bool valid = false;
    ImVec4 seenHsl = {};
    ColorHsl hsl;
    bool validHsl = false;
};

double Wrap01(double value)
{
    if (!std::isfinite(value))
        return 0.0;
    value -= std::floor(value);
    return value >= 1.0 ? 0.0 : value;
}

// Hue in turns of red, green and blue with max, min and their difference.
double HueOf(double r, double g, double b, double max, double delta)
{
    if (delta <= 0.0)
        return 0.0;
    double h = 0.0;
    if (max == r)
        h = (g - b) / delta;
    else if (max == g)
        h = (b - r) / delta + 2.0;
    else
        h = (r - g) / delta + 4.0;
    return Wrap01(h / 6.0);
}

bool IsSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

int HexDigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

char Lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// [begin, end) without the spaces around it.
void Trim(const char*& begin, const char*& end)
{
    while (begin < end && IsSpace(*begin))
        ++begin;
    while (end > begin && IsSpace(end[-1]))
        --end;
}

bool StartsWith(const char* begin, const char* end, const char* prefix)
{
    const std::size_t length = std::strlen(prefix);
    if (static_cast<std::size_t>(end - begin) < length)
        return false;
    for (std::size_t i = 0; i < length; ++i)
    {
        if (Lower(begin[i]) != prefix[i])
            return false;
    }
    return true;
}

bool ParseHex(const char* begin, const char* end, bool hashed, ImVec4* out, bool* hasAlpha)
{
    const int count = static_cast<int>(end - begin);
    bool letter = false;
    for (const char* p = begin; p < end; ++p)
    {
        const int digit = HexDigit(*p);
        if (digit < 0)
            return false;
        letter = letter || digit >= 10;
    }
    // Without the #, only six or eight digits with a letter among them: "123456" stays a number.
    if (!hashed && (!letter || (count != 6 && count != 8)))
        return false;
    int channels[4] = {0, 0, 0, 255};
    if (count == 3 || count == 4)
    {
        for (int i = 0; i < count; ++i)
            channels[i] = HexDigit(begin[i]) * 17;
    }
    else if (count == 6 || count == 8)
    {
        for (int i = 0; i < count / 2; ++i)
            channels[i] = HexDigit(begin[i * 2]) * 16 + HexDigit(begin[i * 2 + 1]);
    }
    else
    {
        return false;
    }
    *out = ImVec4(static_cast<float>(channels[0]) / 255.0f, static_cast<float>(channels[1]) / 255.0f,
                  static_cast<float>(channels[2]) / 255.0f, static_cast<float>(channels[3]) / 255.0f);
    if (hasAlpha != nullptr)
        *hasAlpha = count == 4 || count == 8;
    return true;
}

struct ParsedNumber
{
    double value = 0.0;
    bool percent = false;
};

// Three or four numbers separated by commas, spaces or slashes, each with an optional % (or "deg"
// after the first). Returns the count, 0 when the text holds anything else.
int ParseNumbers(const char* begin, const char* end, ParsedNumber* numbers)
{
    int count = 0;
    const char* p = begin;
    while (p < end)
    {
        while (p < end && (IsSpace(*p) || *p == ',' || *p == '/'))
            ++p;
        if (p >= end)
            break;
        if (count == 4)
            return 0;
        const char* start = p;
        if (*p == '+')
            ++p;
        double value = 0.0;
        const std::from_chars_result read = std::from_chars(p, end, value);
        if (read.ec != std::errc() || read.ptr == start || !std::isfinite(value))
            return 0;
        p = read.ptr;
        ParsedNumber& number = numbers[count++];
        number.value = value;
        if (p < end && *p == '%')
        {
            number.percent = true;
            ++p;
        }
        else if (end - p >= 3 && StartsWith(p, end, "deg"))
        {
            p += 3;
        }
        if (p < end && !IsSpace(*p) && *p != ',' && *p != '/')
            return 0;
    }
    return count;
}

// A 0 to 1 share from a number that is a percentage or a fraction (or, above 1, a percentage).
double Share(const ParsedNumber& number)
{
    if (number.percent || number.value > 1.0)
        return std::clamp(number.value / 100.0, 0.0, 1.0);
    return std::clamp(number.value, 0.0, 1.0);
}

} // namespace

ColorHsv ColorToHsv(const ImVec4& color)
{
    const double r = color.x;
    const double g = color.y;
    const double b = color.z;
    const double max = std::max({r, g, b});
    const double min = std::min({r, g, b});
    const double delta = max - min;
    ColorHsv hsv;
    hsv.v = std::max(0.0, max);
    hsv.s = max > 0.0 ? std::clamp(delta / max, 0.0, 1.0) : 0.0;
    hsv.h = HueOf(r, g, b, max, delta);
    return hsv;
}

ImVec4 ColorFromHsv(const ColorHsv& hsv, float alpha)
{
    const double h = Wrap01(hsv.h) * 6.0;
    const double s = std::clamp(std::isfinite(hsv.s) ? hsv.s : 0.0, 0.0, 1.0);
    const double v = std::max(0.0, std::isfinite(hsv.v) ? hsv.v : 0.0);
    const int sector = std::min(5, static_cast<int>(std::floor(h)));
    const double f = h - sector;
    const double p = v * (1.0 - s);
    const double q = v * (1.0 - s * f);
    const double t = v * (1.0 - s * (1.0 - f));
    double r = v, g = v, b = v;
    switch (sector)
    {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }
    return ImVec4(static_cast<float>(r), static_cast<float>(g), static_cast<float>(b), alpha);
}

ColorHsl ColorToHsl(const ImVec4& color)
{
    const double r = color.x;
    const double g = color.y;
    const double b = color.z;
    const double max = std::max({r, g, b});
    const double min = std::min({r, g, b});
    const double delta = max - min;
    ColorHsl hsl;
    hsl.l = (max + min) * 0.5;
    const double denominator = 1.0 - std::fabs(2.0 * hsl.l - 1.0);
    hsl.s = delta > 0.0 && denominator > 0.0 ? std::clamp(delta / denominator, 0.0, 1.0) : 0.0;
    hsl.h = HueOf(r, g, b, max, delta);
    return hsl;
}

ImVec4 ColorFromHsl(const ColorHsl& hsl, float alpha)
{
    const double h = Wrap01(hsl.h) * 6.0;
    const double s = std::clamp(std::isfinite(hsl.s) ? hsl.s : 0.0, 0.0, 1.0);
    const double l = std::max(0.0, std::isfinite(hsl.l) ? hsl.l : 0.0);
    const double chroma = (1.0 - std::fabs(2.0 * std::min(l, 1.0) - 1.0)) * s;
    const double x = chroma * (1.0 - std::fabs(std::fmod(h, 2.0) - 1.0));
    const double m = l - chroma * 0.5;
    const int sector = std::min(5, static_cast<int>(std::floor(h)));
    double r = 0.0, g = 0.0, b = 0.0;
    switch (sector)
    {
    case 0: r = chroma; g = x; break;
    case 1: r = x; g = chroma; break;
    case 2: g = chroma; b = x; break;
    case 3: g = x; b = chroma; break;
    case 4: r = x; b = chroma; break;
    default: r = chroma; b = x; break;
    }
    return ImVec4(static_cast<float>(r + m), static_cast<float>(g + m), static_cast<float>(b + m), alpha);
}

ImVec4 ColorHueColor(double hue)
{
    return ColorFromHsv({hue, 1.0, 1.0}, 1.0f);
}

ColorHsv ColorTrackHsv(ImGuiID id, const ImVec4& color)
{
    ColorHueMemory& memory = GetState<ColorHueMemory>(id);
    if (memory.valid && ColorSame(memory.seen, color, false))
        return memory.hsv;
    ColorHsv fresh = ColorToHsv(color);
    if (memory.valid)
    {
        if (fresh.v <= 0.0)
        {
            fresh.h = memory.hsv.h;
            fresh.s = memory.hsv.s;
        }
        else if (fresh.s <= 0.0)
        {
            fresh.h = memory.hsv.h;
        }
    }
    memory.seen = color;
    memory.hsv = fresh;
    memory.valid = true;
    return fresh;
}

ColorHsl ColorTrackHsl(ImGuiID id, const ImVec4& color)
{
    ColorHueMemory& memory = GetState<ColorHueMemory>(id);
    if (memory.validHsl && ColorSame(memory.seenHsl, color, false))
        return memory.hsl;
    ColorHsl fresh = ColorToHsl(color);
    // The hue is shared with the HSV memory, so switching the model keeps it.
    const bool knownHue = memory.validHsl || memory.valid;
    const double hue = memory.validHsl ? memory.hsl.h : memory.hsv.h;
    if (knownHue && fresh.s <= 0.0)
    {
        fresh.h = hue;
        if (memory.validHsl && (fresh.l <= 0.0 || fresh.l >= 1.0))
            fresh.s = memory.hsl.s;
    }
    memory.seenHsl = color;
    memory.hsl = fresh;
    memory.validHsl = true;
    return fresh;
}

void ColorRememberHsv(ImGuiID id, const ImVec4& color, const ColorHsv& hsv)
{
    ColorHueMemory& memory = GetState<ColorHueMemory>(id);
    memory.seen = color;
    memory.hsv = hsv;
    memory.hsv.h = Wrap01(hsv.h);
    memory.valid = true;
}

void ColorRememberHsl(ImGuiID id, const ImVec4& color, const ColorHsl& hsl)
{
    ColorHueMemory& memory = GetState<ColorHueMemory>(id);
    memory.seenHsl = color;
    memory.hsl = hsl;
    memory.hsl.h = Wrap01(hsl.h);
    memory.validHsl = true;
}

bool ColorSame(const ImVec4& a, const ImVec4& b, bool alpha)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && (!alpha || a.w == b.w);
}

ImVec4 ColorForWrite(const ImVec4& color, const ImVec4& previous, bool alpha, bool hdr)
{
    const auto channel = [hdr](float value) {
        if (!std::isfinite(value))
            return 0.0f;
        return hdr ? std::max(0.0f, value) : std::clamp(value, 0.0f, 1.0f);
    };
    ImVec4 out(channel(color.x), channel(color.y), channel(color.z), previous.w);
    if (alpha)
        out.w = std::isfinite(color.w) ? std::clamp(color.w, 0.0f, 1.0f) : previous.w;
    return out;
}

int ColorChannelByte(float channel)
{
    if (!std::isfinite(channel))
        return 0;
    return static_cast<int>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f));
}

int ColorFormatHex(char* out, std::size_t size, const ImVec4& color, bool alpha)
{
    if (out == nullptr || size == 0)
        return 0;
    int written = 0;
    if (alpha)
        written = std::snprintf(out, size, "#%02X%02X%02X%02X", ColorChannelByte(color.x), ColorChannelByte(color.y),
                                ColorChannelByte(color.z), ColorChannelByte(color.w));
    else
        written = std::snprintf(out, size, "#%02X%02X%02X", ColorChannelByte(color.x), ColorChannelByte(color.y),
                                ColorChannelByte(color.z));
    return std::clamp(written, 0, static_cast<int>(size) - 1);
}

bool ColorParseText(const char* text, ImVec4* out, bool* hasAlpha)
{
    if (text == nullptr || out == nullptr)
        return false;
    const char* begin = text;
    const char* end = text + std::strlen(text);
    Trim(begin, end);
    if (begin >= end)
        return false;
    if (*begin == '#')
        return ParseHex(begin + 1, end, true, out, hasAlpha);

    enum class Form { Bare, Rgb, Hsv, Hsl };
    Form form = Form::Bare;
    const char* body = begin;
    const char* bodyEnd = end;
    for (const auto& [prefix, kind] : {std::pair{"rgba(", Form::Rgb}, std::pair{"rgb(", Form::Rgb},
                                       std::pair{"hsva(", Form::Hsv}, std::pair{"hsv(", Form::Hsv},
                                       std::pair{"hsla(", Form::Hsl}, std::pair{"hsl(", Form::Hsl}})
    {
        if (StartsWith(begin, end, prefix))
        {
            if (end[-1] != ')')
                return false;
            form = kind;
            body = begin + std::strlen(prefix);
            bodyEnd = end - 1;
            break;
        }
    }
    if (form == Form::Bare && ParseHex(begin, end, false, out, hasAlpha))
        return true;

    ParsedNumber numbers[4];
    const int count = ParseNumbers(body, bodyEnd, numbers);
    if (count < 3)
        return false;
    const bool withAlpha = count == 4;
    float alpha = 1.0f;
    ImVec4 color;
    if (form == Form::Hsv || form == Form::Hsl)
    {
        const double hue = numbers[0].value / 360.0;
        if (withAlpha)
            alpha = static_cast<float>(Share(numbers[3]));
        if (form == Form::Hsv)
            color = ColorFromHsv({hue, Share(numbers[1]), Share(numbers[2])}, alpha);
        else
            color = ColorFromHsl({hue, Share(numbers[1]), Share(numbers[2])}, alpha);
    }
    else
    {
        // rgb() and bare numbers: 0 to 255, unless every color number is a fraction of at most 1
        // in bare form; percentages are shares either way.
        bool fractions = form == Form::Bare;
        for (int i = 0; i < 3; ++i)
            fractions = fractions && !numbers[i].percent && numbers[i].value <= 1.0;
        float channels[3];
        for (int i = 0; i < 3; ++i)
        {
            const ParsedNumber& n = numbers[i];
            double share = n.percent ? n.value / 100.0 : (fractions ? n.value : n.value / 255.0);
            channels[i] = static_cast<float>(std::clamp(share, 0.0, 1.0));
        }
        if (withAlpha)
        {
            const ParsedNumber& a = numbers[3];
            double share = a.percent ? a.value / 100.0 : (a.value <= 1.0 ? a.value : a.value / 255.0);
            alpha = static_cast<float>(std::clamp(share, 0.0, 1.0));
        }
        color = ImVec4(channels[0], channels[1], channels[2], alpha);
    }
    *out = color;
    if (hasAlpha != nullptr)
        *hasAlpha = withAlpha;
    return true;
}

ImVec4 ColorOnChecker(const ImVec4& color)
{
    ImVec4 clamped(std::clamp(color.x, 0.0f, 1.0f), std::clamp(color.y, 0.0f, 1.0f), std::clamp(color.z, 0.0f, 1.0f),
                   std::clamp(color.w, 0.0f, 1.0f));
    if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z))
        clamped = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    if (!std::isfinite(color.w))
        clamped.w = 1.0f;
    ImVec4 out = clamped.w >= 1.0f ? clamped : Over(clamped, T().colors.checker[0]);
    out.w = 1.0f;
    return out;
}

ImVec4 ColorHandleEdge(const ImVec4& under, float hoverAmount, bool disabled)
{
    const Colors& c = T().colors;
    ImVec4 plane = under;
    plane.w = 1.0f;
    // The white rim already stands out from a darker color; on a light one it would vanish, so
    // the hairline takes black or white, whichever reads on that color.
    if (ContrastRatio(c.knob, plane) < kColorHandleEdgeContrast)
        return ReadableOn(plane);
    OutlineLook outline;
    outline.hoverAmount = disabled ? 0.0f : hoverAmount;
    outline.disabled = disabled;
    return ControlOutlineColor(outline);
}

void DrawColorHandle(ImDrawList* drawList, ImVec2 center, float radius, const ImVec4& fill, float hoverAmount,
                     float pressAmount, float focusAmount, bool disabled)
{
    const Colors& c = T().colors;
    const float hairline = HairlineThickness();
    const float r = std::max(hairline * 4.0f, std::round(radius * (1.0f + kColorHandlePressGrowth * pressAmount)));
    const ImRect knob(center.x - r, center.y - r, center.x + r, center.y + r);
    // The solid knob of the sliders: the knob token with the level 1 shadow and one hairline
    // (ColorHandleEdge). The edge reaches 3:1 against the color under it, by the rim on a dark color
    // and by the hairline on a light one; the picked color fills the middle.
    if (!disabled)
        DrawElevation(drawList, knob.Min, knob.Max, r, 1);
    FillRect(drawList, knob.Min, knob.Max, ToU32(WithAlpha(c.knob, c.knob.w * (disabled ? kDisabledPictureOpacity : 1.0f))), r);
    const float rim = std::max(hairline * 2.0f, std::round(r * kColorHandleRimShare));
    const float inner = std::max(hairline, r - rim);
    ImVec4 shown = fill;
    shown.w = disabled ? kColorDisabledOpacity : 1.0f;
    FillRect(drawList, ImVec2(center.x - inner, center.y - inner), ImVec2(center.x + inner, center.y + inner), ToU32(shown),
             inner);
    DrawOutline(drawList, knob, r, ColorHandleEdge(fill, hoverAmount, disabled));
    DrawFocusRing(drawList, knob, r, focusAmount, 0, RingShape::Round);
}

void DrawColorSwatch(ImDrawList* drawList, const ImRect& rect, const ImVec4& color, float radius, bool split,
                     ImDrawFlags corners)
{
    const Theme& t = T();
    const ImVec4 safe = ColorOnChecker(ImVec4(color.x, color.y, color.z, 1.0f));
    const float alpha = std::isfinite(color.w) ? std::clamp(color.w, 0.0f, 1.0f) : 1.0f;
    if (alpha >= 1.0f)
    {
        FillRect(drawList, rect.Min, rect.Max, ToU32(safe), radius, corners);
        return;
    }
    FillCheckerboard(drawList, rect.Min, rect.Max, ToU32(t.colors.checker[0]), ToU32(t.colors.checker[1]),
                     Px(t.metrics.checkerCell), radius, corners);
    FillRect(drawList, rect.Min, rect.Max, ToU32(WithAlpha(safe, alpha)), radius, corners);
    if (split)
    {
        const float mid = std::round(rect.GetCenter().x);
        const ImDrawFlags left = corners == 0 ? ImDrawFlags_RoundLeft : (corners & ImDrawFlags_RoundLeft);
        FillRect(drawList, rect.Min, ImVec2(mid, rect.Max.y), ToU32(safe), radius,
                 left != 0 ? left : ImDrawFlags_RoundNone);
    }
}

void DrawColorMixedSwatch(ImDrawList* drawList, const ImRect& rect, float radius)
{
    const Colors& c = T().colors;
    FillRect(drawList, rect.Min, rect.Max, ToU32(c.fill[2]), radius);
    const float inset = std::ceil(radius * kColorCornerInset);
    const ImRect clip(rect.Min.x + inset, rect.Min.y + inset, rect.Max.x - inset, rect.Max.y - inset);
    if (clip.GetWidth() <= 0.0f || clip.GetHeight() <= 0.0f)
        return;
    const float hairline = HairlineThickness();
    const float gap = hairline * kColorMixedStripeGap;
    const ImU32 color = ToU32(c.labelTertiary);
    drawList->PushClipRect(clip.Min, clip.Max, true);
    const float height = rect.GetHeight();
    for (float x = rect.Min.x - height; x < rect.Max.x; x += gap)
        drawList->AddLine(ImVec2(x, rect.Max.y), ImVec2(x + height, rect.Min.y), color, hairline * kColorMixedStripeWidth);
    drawList->PopClipRect();
}

} // namespace facet::internal
