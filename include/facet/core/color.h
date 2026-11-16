#pragma once

// Color math. Colors are ImVec4 in gamma-encoded sRGB with straight alpha (x = r, y = g,
// z = b, w = alpha), the same convention as ImGuiStyle::Colors, unless a function says
// it takes or returns linear values.

#include <cstdint>

#include <imgui.h>

namespace facet
{

// OKLab: perceptual lightness L (0 to 1) and the opponent axes a (green to red) and
// b (blue to yellow).
struct Oklab
{
    float L = 0.0f;
    float a = 0.0f;
    float b = 0.0f;
};

// OKLCH: the polar form of OKLab. C is the chroma (0 for greys), h the hue in degrees
// (0 to 360).
struct Oklch
{
    float L = 0.0f;
    float C = 0.0f;
    float h = 0.0f;
};

// 0xRRGGBB plus an alpha from 0 to 1, for token tables: Rgb(0x1C1C1E).
constexpr ImVec4 Rgb(std::uint32_t rgb, float alpha = 1.0f)
{
    return ImVec4(static_cast<float>((rgb >> 16) & 0xFFu) / 255.0f,
                  static_cast<float>((rgb >> 8) & 0xFFu) / 255.0f,
                  static_cast<float>(rgb & 0xFFu) / 255.0f, alpha);
}

// Packs a color for ImDrawList calls (clamped, rounded to 8 bits per channel).
ImU32 ToU32(const ImVec4& color);
// Unpacks an ImU32 color.
ImVec4 FromU32(ImU32 color);

// One channel between gamma-encoded sRGB and linear light (IEC 61966-2-1 curve).
float SrgbToLinear(float channel);
float LinearToSrgb(float channel);
// All three channels; alpha passes through unchanged.
ImVec4 SrgbToLinear(const ImVec4& color);
ImVec4 LinearToSrgb(const ImVec4& color);

// Between linear sRGB and OKLab.
Oklab LinearToOklab(const ImVec4& linear);
ImVec4 OklabToLinear(const Oklab& lab, float alpha = 1.0f);

// Between gamma-encoded sRGB and OKLab or OKLCH. The From* functions return colors inside
// the sRGB gamut: an out-of-gamut OKLCH color keeps its lightness and hue and loses chroma
// until it fits; the remaining rounding error is clamped.
Oklab ToOklab(const ImVec4& color);
ImVec4 FromOklab(const Oklab& lab, float alpha = 1.0f);
Oklch ToOklch(const ImVec4& color);
ImVec4 FromOklch(const Oklch& lch, float alpha = 1.0f);

// Interpolates in OKLab, so midpoints keep their perceived lightness; alpha is
// interpolated linearly. t = 0 gives a, t = 1 gives b.
ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t);

// The color with its alpha replaced.
ImVec4 WithAlpha(const ImVec4& color, float alpha);
// The color with its alpha multiplied by `factor`.
ImU32 WithAlpha(ImU32 color, float factor);

// `top` drawn over `bottom` with straight alpha blending in sRGB, the way Dear ImGui
// renders it. The result has the combined coverage as alpha.
ImVec4 Over(const ImVec4& top, const ImVec4& bottom);

// WCAG 2 relative luminance (0 black, 1 white) of the opaque color; alpha is ignored.
float RelativeLuminance(const ImVec4& color);

// WCAG 2 contrast ratio from 1 to 21 between a foreground and a background. A translucent
// foreground is first drawn over the background; the background is treated as opaque.
float ContrastRatio(const ImVec4& foreground, const ImVec4& background);

// White or black, whichever has the higher contrast on the background. Either one reaches
// at least 4.5:1 on any opaque color.
ImVec4 ReadableOn(const ImVec4& background);

} // namespace facet
