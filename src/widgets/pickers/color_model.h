#pragma once

// The color math and the shared state of the color pickers (pickers.h): HSV and HSL conversions in
// double precision, the hue memory that keeps a grey's hue and a black's saturation, color text
// (hex and the forms ColorInputs and ColorWell paste), and the helpers every picker part uses to
// pick its handle colors.
//
//     ColorHsv hsv = ColorTrackHsv(id, *color);            // the hue survives a trip through grey
//     hsv.s = newSaturation;
//     const ImVec4 next = ColorFromHsv(hsv, color->w);
//     ColorRememberHsv(id, next, hsv);                     // what was meant, for the next frame
//
// Colors are ImVec4 in gamma-encoded sRGB with straight alpha (core/color.h). HSV and HSL are the
// usual cylinder models over those gamma-encoded channels; the gradients of the pickers are drawn in
// the same space, so the color under a handle is the color written.

#include <cstddef>

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/control.h"

namespace facet::internal
{

// Hue in turns (0 to 1, 1 excluded), saturation and value from 0 to 1. Value may be above 1 for an
// HDR color (its largest channel).
struct ColorHsv
{
    double h = 0.0;
    double s = 0.0;
    double v = 0.0;
};

// Hue in turns, saturation and lightness from 0 to 1.
struct ColorHsl
{
    double h = 0.0;
    double s = 0.0;
    double l = 0.0;
};

// The conversions. A grey gives hue 0 and saturation 0; ColorTrackHsv keeps the hue instead.
ColorHsv ColorToHsv(const ImVec4& color);
ImVec4 ColorFromHsv(const ColorHsv& hsv, float alpha);
ColorHsl ColorToHsl(const ImVec4& color);
ImVec4 ColorFromHsl(const ColorHsl& hsl, float alpha);
// The fully saturated color of a hue (turns), opaque.
ImVec4 ColorHueColor(double hue);

// The HSV of `color` for the picker with `id`, with the hue memory: while the color is the one the
// picker saw or wrote last, the remembered HSV comes back unchanged (so a saturation dragged to 0
// keeps its hue and a value dragged to 0 keeps its saturation and hue); a color changed from
// outside is converted afresh, still keeping the remembered hue while it is grey (saturation or
// value 0) and the remembered saturation while it is black. The same for the HSL saturation.
ColorHsv ColorTrackHsv(ImGuiID id, const ImVec4& color);
ColorHsl ColorTrackHsl(ImGuiID id, const ImVec4& color);
// Records that the picker wrote `color` meaning `hsv` (or `hsl`).
void ColorRememberHsv(ImGuiID id, const ImVec4& color, const ColorHsv& hsv);
void ColorRememberHsl(ImGuiID id, const ImVec4& color, const ColorHsl& hsl);

// True when the channels the pickers compare are equal: red, green and blue, and alpha too when
// `alpha` is set. Exact float comparison: a color that round trips is the same color.
bool ColorSame(const ImVec4& a, const ImVec4& b, bool alpha);

// The color as a picker writes it: alpha kept from `previous` unless `alpha`, channels clamped to 0
// to 1 unless `hdr` (then only below 0).
ImVec4 ColorForWrite(const ImVec4& color, const ImVec4& previous, bool alpha, bool hdr);

// ---- Text ----------------------------------------------------------------------------------

// The 8-bit channel of a value (clamped, rounded).
int ColorChannelByte(float channel);

// Writes "#RRGGBB" or "#RRGGBBAA" (with `alpha`) into `out`. Returns the length.
int ColorFormatHex(char* out, std::size_t size, const ImVec4& color, bool alpha);

// Reads color text: "#RGB", "#RGBA", "#RRGGBB", "#RRGGBBAA" (the # optional), "rgb(51, 102, 255)",
// "rgba(51, 102, 255, 0.5)", "hsv(210, 80%, 100%)", "hsl(210, 100%, 60%)", and three or four
// bare numbers separated by commas or spaces ("0.2, 0.4, 1" as 0 to 1 when every number is at most
// 1, "51 102 255" as 0 to 255 otherwise). Returns false and leaves *out alone for anything else,
// also for a single number, so a number typed into a field is never read as a color. *hasAlpha
// (may be null) says whether the text gave an alpha.
bool ColorParseText(const char* text, ImVec4* out, bool* hasAlpha);

// ---- Look ----------------------------------------------------------------------------------

// The color drawn for a picked color on screen: `color` over the first checker cell of the
// theme when it is translucent, clamped to 0 to 1, opaque.
ImVec4 ColorOnChecker(const ImVec4& color);

// The two rings of a handle that must read on any color under it: the inner ring in white or
// black, whichever reads better on `under`, the outer hairline in the other one. Computed from
// the color, so they are the same in both themes.
struct ColorHandleRings
{
    ImU32 inner = 0;
    ImU32 outer = 0;
};
ColorHandleRings ColorHandleRingColors(const ImVec4& under);

// A round handle centered on `center` with `radius` pixels: filled with `fill` (opaque), the two
// rings of ColorHandleRingColors(fill) (labelTertiary for both when disabled), grown a little while
// held (pressAmount), and the round focus ring at focusAmount.
void DrawColorHandle(ImDrawList* drawList, ImVec2 center, float radius, const ImVec4& fill, float pressAmount,
                     float focusAmount, bool disabled);

// Fills `rect` with the swatch of `color`: the checkerboard of the theme under a translucent color
// (the left half shows the color opaque when `split`, so both the hue and the alpha read), then the
// color. `radius` pixels, corners as FillRect.
void DrawColorSwatch(ImDrawList* drawList, const ImRect& rect, const ImVec4& color, float radius, bool split,
                     ImDrawFlags corners = 0);

// The striped look of a mixed value: diagonal stripes of fill[0] over fill[2], clipped to `rect`.
void DrawColorMixedSwatch(ImDrawList* drawList, const ImRect& rect, float radius);

// The share of their normal opacity that the gradients and swatches of a disabled picker keep: the
// disabled picture opacity every family shares (control.h).
inline constexpr float kColorDisabledOpacity = kDisabledPictureOpacity;

} // namespace facet::internal
