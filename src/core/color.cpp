// Color math: sRGB transfer curve, OKLab and OKLCH conversions, WCAG contrast.

#include "facet/core/color.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace facet
{
namespace
{

constexpr float kDegreesPerRadian = 180.0f / std::numbers::pi_v<float>;

float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

bool InGamut(const ImVec4& linear)
{
    // A little slack for float rounding at the gamut edge.
    constexpr float kEpsilon = 1e-4f;
    return linear.x >= -kEpsilon && linear.x <= 1.0f + kEpsilon && linear.y >= -kEpsilon &&
           linear.y <= 1.0f + kEpsilon && linear.z >= -kEpsilon && linear.z <= 1.0f + kEpsilon;
}

Oklab LchToLab(const Oklch& lch)
{
    const float radians = lch.h / kDegreesPerRadian;
    return {lch.L, lch.C * std::cos(radians), lch.C * std::sin(radians)};
}

ImVec4 ClampColor(const ImVec4& color)
{
    return ImVec4(Clamp01(color.x), Clamp01(color.y), Clamp01(color.z), Clamp01(color.w));
}

} // namespace

ImU32 ToU32(const ImVec4& color)
{
    return ImGui::ColorConvertFloat4ToU32(ClampColor(color));
}

ImVec4 FromU32(ImU32 color)
{
    return ImGui::ColorConvertU32ToFloat4(color);
}

float SrgbToLinear(float channel)
{
    if (channel <= 0.04045f)
        return channel / 12.92f;
    return std::pow((channel + 0.055f) / 1.055f, 2.4f);
}

float LinearToSrgb(float channel)
{
    if (channel <= 0.0031308f)
        return channel * 12.92f;
    return 1.055f * std::pow(channel, 1.0f / 2.4f) - 0.055f;
}

ImVec4 SrgbToLinear(const ImVec4& color)
{
    return ImVec4(SrgbToLinear(color.x), SrgbToLinear(color.y), SrgbToLinear(color.z), color.w);
}

ImVec4 LinearToSrgb(const ImVec4& color)
{
    return ImVec4(LinearToSrgb(color.x), LinearToSrgb(color.y), LinearToSrgb(color.z), color.w);
}

// Matrices of the published OKLab definition (linear sRGB, D65).
Oklab LinearToOklab(const ImVec4& linear)
{
    const float l = 0.4122214708f * linear.x + 0.5363325363f * linear.y + 0.0514459929f * linear.z;
    const float m = 0.2119034982f * linear.x + 0.6806995451f * linear.y + 0.1073969566f * linear.z;
    const float s = 0.0883024619f * linear.x + 0.2817188376f * linear.y + 0.6299787005f * linear.z;
    const float l3 = std::cbrt(l);
    const float m3 = std::cbrt(m);
    const float s3 = std::cbrt(s);
    return {0.2104542553f * l3 + 0.7936177850f * m3 - 0.0040720468f * s3,
            1.9779984951f * l3 - 2.4285922050f * m3 + 0.4505937099f * s3,
            0.0259040371f * l3 + 0.7827717662f * m3 - 0.8086757660f * s3};
}

ImVec4 OklabToLinear(const Oklab& lab, float alpha)
{
    const float l3 = lab.L + 0.3963377774f * lab.a + 0.2158037573f * lab.b;
    const float m3 = lab.L - 0.1055613458f * lab.a - 0.0638541728f * lab.b;
    const float s3 = lab.L - 0.0894841775f * lab.a - 1.2914855480f * lab.b;
    const float l = l3 * l3 * l3;
    const float m = m3 * m3 * m3;
    const float s = s3 * s3 * s3;
    return ImVec4(4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s,
                  -1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s,
                  -0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s, alpha);
}

Oklab ToOklab(const ImVec4& color)
{
    return LinearToOklab(SrgbToLinear(color));
}

ImVec4 FromOklab(const Oklab& lab, float alpha)
{
    return ClampColor(LinearToSrgb(ClampColor(OklabToLinear(lab, alpha))));
}

Oklch ToOklch(const ImVec4& color)
{
    const Oklab lab = ToOklab(color);
    const float chroma = std::sqrt(lab.a * lab.a + lab.b * lab.b);
    float hue = std::atan2(lab.b, lab.a) * kDegreesPerRadian;
    if (hue < 0.0f)
        hue += 360.0f;
    if (chroma < 1e-6f)
        hue = 0.0f;
    return {lab.L, chroma, hue};
}

ImVec4 FromOklch(const Oklch& lch, float alpha)
{
    const ImVec4 direct = OklabToLinear(LchToLab(lch), alpha);
    if (InGamut(direct))
        return ClampColor(LinearToSrgb(ClampColor(direct)));

    // Binary search for the largest chroma that stays inside the gamut.
    float low = 0.0f;
    float high = lch.C;
    for (int i = 0; i < 24; ++i)
    {
        const float mid = (low + high) * 0.5f;
        if (InGamut(OklabToLinear(LchToLab({lch.L, mid, lch.h}), alpha)))
            low = mid;
        else
            high = mid;
    }
    return ClampColor(LinearToSrgb(ClampColor(OklabToLinear(LchToLab({lch.L, low, lch.h}), alpha))));
}

ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t)
{
    const Oklab la = ToOklab(a);
    const Oklab lb = ToOklab(b);
    const Oklab mixed = {la.L + (lb.L - la.L) * t, la.a + (lb.a - la.a) * t, la.b + (lb.b - la.b) * t};
    return FromOklab(mixed, a.w + (b.w - a.w) * t);
}

ImVec4 WithAlpha(const ImVec4& color, float alpha)
{
    return ImVec4(color.x, color.y, color.z, alpha);
}

ImU32 ScaleAlpha(ImU32 color, float factor)
{
    const float alpha = static_cast<float>((color >> IM_COL32_A_SHIFT) & 0xFFu);
    const auto scaled = static_cast<ImU32>(std::lround(std::clamp(alpha * factor, 0.0f, 255.0f)));
    return (color & ~IM_COL32_A_MASK) | (scaled << IM_COL32_A_SHIFT);
}

ImVec4 Over(const ImVec4& top, const ImVec4& bottom)
{
    const float alpha = top.w + bottom.w * (1.0f - top.w);
    if (alpha <= 0.0f)
        return ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    auto channel = [&](float t, float b) { return (t * top.w + b * bottom.w * (1.0f - top.w)) / alpha; };
    return ImVec4(channel(top.x, bottom.x), channel(top.y, bottom.y), channel(top.z, bottom.z), alpha);
}

float RelativeLuminance(const ImVec4& color)
{
    const ImVec4 linear = SrgbToLinear(ClampColor(color));
    return 0.2126f * linear.x + 0.7152f * linear.y + 0.0722f * linear.z;
}

float ContrastRatio(const ImVec4& foreground, const ImVec4& background)
{
    const ImVec4 opaqueBackground = WithAlpha(background, 1.0f);
    const ImVec4 composed = Over(foreground, opaqueBackground);
    const float a = RelativeLuminance(composed);
    const float b = RelativeLuminance(opaqueBackground);
    return (std::max(a, b) + 0.05f) / (std::min(a, b) + 0.05f);
}

ImVec4 ReadableOn(const ImVec4& background)
{
    const ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 black(0.0f, 0.0f, 0.0f, 1.0f);
    return ContrastRatio(white, background) >= ContrastRatio(black, background) ? white : black;
}

} // namespace facet
