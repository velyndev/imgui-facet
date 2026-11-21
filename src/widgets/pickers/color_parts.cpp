// The parts the color pickers are made of (color_parts.h).

#include "widgets/pickers/color_parts.h"

#include <algorithm>
#include <cmath>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/focus.h"
#include "internal/value_drag.h"
#include "widgets/pickers/color_plane.h"

namespace facet::internal
{
namespace
{

// The hue circle in six stretches: between the primaries and secondaries the gamma-encoded
// channels change linearly, so each stretch is one exact two-color gradient.
constexpr int kColorHueStretches = 6;
// A hovered handle grows by this share of its pressed growth, so hover and press read apart.
constexpr float kColorHandleHoverShare = 0.35f;

ImU32 Dimmed(const ImVec4& color, bool disabled)
{
    return disabled ? ToU32(WithAlpha(color, color.w * kColorDisabledOpacity)) : ToU32(color);
}

double Clamp01(double value)
{
    return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
}

// One stretch of a bar from fraction `a` to `b` with the colors at both ends, along the bar.
void BarStretch(ImDrawList* drawList, const ImRect& bar, Orientation orientation, double a, double b, ImU32 from,
                ImU32 to, float radius, ImDrawFlags corners)
{
    if (orientation == Orientation::Vertical)
    {
        // Fraction 0 at the bottom: the stretch runs upward, the gradient top is `to`.
        const float y0 = std::round(bar.Max.y - static_cast<float>(b) * bar.GetHeight());
        const float y1 = std::round(bar.Max.y - static_cast<float>(a) * bar.GetHeight());
        if (y1 > y0)
            FillGradientRect(drawList, ImVec2(bar.Min.x, y0), ImVec2(bar.Max.x, y1), to, from, GradientDir::Vertical,
                             radius, corners);
        return;
    }
    const float x0 = std::round(bar.Min.x + static_cast<float>(a) * bar.GetWidth());
    const float x1 = std::round(bar.Min.x + static_cast<float>(b) * bar.GetWidth());
    if (x1 > x0)
        FillGradientRect(drawList, ImVec2(x0, bar.Min.y), ImVec2(x1, bar.Max.y), from, to, GradientDir::Horizontal, radius,
                         corners);
}

ImVec2 SquareMap(const void* user, ImVec2 point)
{
    const ImRect& area = *static_cast<const ImRect*>(user);
    const float w = std::max(1.0f, area.GetWidth());
    const float h = std::max(1.0f, area.GetHeight());
    return ImVec2((point.x - area.Min.x) / w, (area.Max.y - point.y) / h);
}

float HandleGrowth(const ItemState& item)
{
    return std::min(1.0f, item.pressAmount + item.hoverAmount * kColorHandleHoverShare);
}

} // namespace

double ColorChannelFraction(ColorChannel channel, const ImVec4& color, const ColorHsv& hsv)
{
    switch (channel)
    {
    case ColorChannel::Hue: return Clamp01(hsv.h);
    case ColorChannel::Saturation: return Clamp01(hsv.s);
    case ColorChannel::Value: return Clamp01(hsv.v);
    case ColorChannel::Red: return Clamp01(color.x);
    case ColorChannel::Green: return Clamp01(color.y);
    case ColorChannel::Blue: return Clamp01(color.z);
    case ColorChannel::Alpha: return Clamp01(color.w);
    }
    return 0.0;
}

ImVec4 ColorWithChannel(ColorChannel channel, const ImVec4& color, ColorHsv* hsv, double fraction)
{
    fraction = Clamp01(fraction);
    ImVec4 out = color;
    const float f = static_cast<float>(fraction);
    switch (channel)
    {
    case ColorChannel::Hue:
    case ColorChannel::Saturation:
    case ColorChannel::Value:
    {
        ColorHsv next = *hsv;
        if (channel == ColorChannel::Hue)
            next.h = fraction >= 1.0 ? 0.0 : fraction;
        else if (channel == ColorChannel::Saturation)
            next.s = fraction;
        else
            next.v = fraction;
        out = ColorFromHsv(next, color.w);
        *hsv = next;
        return out;
    }
    case ColorChannel::Red: out.x = f; break;
    case ColorChannel::Green: out.y = f; break;
    case ColorChannel::Blue: out.z = f; break;
    case ColorChannel::Alpha: out.w = f; return out;
    }
    // A channel edit changes the HSV; the hue of a grey stays the one the picker had.
    ColorHsv fresh = ColorToHsv(out);
    if (fresh.v <= 0.0)
    {
        fresh.h = hsv->h;
        fresh.s = hsv->s;
    }
    else if (fresh.s <= 0.0)
    {
        fresh.h = hsv->h;
    }
    *hsv = fresh;
    return out;
}

ImVec4 ColorChannelSample(ColorChannel channel, const ImVec4& color, const ColorHsv& hsv, double fraction)
{
    fraction = Clamp01(fraction);
    const float f = static_cast<float>(fraction);
    const ImVec4 clamped(std::clamp(color.x, 0.0f, 1.0f), std::clamp(color.y, 0.0f, 1.0f),
                         std::clamp(color.z, 0.0f, 1.0f), 1.0f);
    switch (channel)
    {
    case ColorChannel::Hue: return ColorFromHsv({fraction, Clamp01(hsv.s), Clamp01(hsv.v)}, 1.0f);
    case ColorChannel::Saturation: return ColorFromHsv({hsv.h, fraction, Clamp01(hsv.v)}, 1.0f);
    case ColorChannel::Value: return ColorFromHsv({hsv.h, Clamp01(hsv.s), fraction}, 1.0f);
    case ColorChannel::Red: return ImVec4(f, clamped.y, clamped.z, 1.0f);
    case ColorChannel::Green: return ImVec4(clamped.x, f, clamped.z, 1.0f);
    case ColorChannel::Blue: return ImVec4(clamped.x, clamped.y, f, 1.0f);
    case ColorChannel::Alpha: return ImVec4(clamped.x, clamped.y, clamped.z, f);
    }
    return clamped;
}

const char* ColorChannelName(ColorChannel channel)
{
    const Strings& s = GetStrings();
    switch (channel)
    {
    case ColorChannel::Hue: return s.colorHue;
    case ColorChannel::Saturation: return s.colorSaturation;
    case ColorChannel::Value: return s.colorValue;
    case ColorChannel::Red: return s.colorRed;
    case ColorChannel::Green: return s.colorGreen;
    case ColorChannel::Blue: return s.colorBlue;
    case ColorChannel::Alpha: return s.colorAlpha;
    }
    return "";
}

void DrawColorChannelBar(ImDrawList* drawList, const ImRect& bar, Orientation orientation, ColorChannel channel,
                         const ImVec4& color, const ColorHsv& hsv, bool disabled)
{
    if (bar.GetWidth() <= 0.0f || bar.GetHeight() <= 0.0f)
        return;
    const Theme& t = T();
    const bool vertical = orientation == Orientation::Vertical;
    const float radius = std::floor((vertical ? bar.GetWidth() : bar.GetHeight()) * 0.5f);
    if (channel == ColorChannel::Alpha)
    {
        const ImVec4& a = t.colors.checker[0];
        const ImVec4& b = t.colors.checker[1];
        FillCheckerboard(drawList, bar.Min, bar.Max, Dimmed(a, disabled), Dimmed(b, disabled), Px(t.metrics.checkerCell),
                         radius);
    }
    // The rounded ends are caps of the end colors, half a thickness long; the gradient spans the
    // part between them, which is the knob travel, so the knob centre always sits on its color.
    ImRect travel = bar;
    ImRect startCap = bar;
    ImRect endCap = bar;
    if (vertical)
    {
        travel.Min.y += radius;
        travel.Max.y -= radius;
        startCap.Min.y = travel.Max.y;
        endCap.Max.y = travel.Min.y;
    }
    else
    {
        travel.Min.x += radius;
        travel.Max.x -= radius;
        startCap.Max.x = travel.Min.x;
        endCap.Min.x = travel.Max.x;
    }
    const ImDrawFlags startCorners = vertical ? ImDrawFlags_RoundBottom : ImDrawFlags_RoundLeft;
    const ImDrawFlags endCorners = vertical ? ImDrawFlags_RoundTop : ImDrawFlags_RoundRight;
    FillRect(drawList, startCap.Min, startCap.Max, Dimmed(ColorChannelSample(channel, color, hsv, 0.0), disabled), radius,
             startCorners);
    FillRect(drawList, endCap.Min, endCap.Max, Dimmed(ColorChannelSample(channel, color, hsv, 1.0), disabled), radius,
             endCorners);
    if (travel.GetWidth() <= 0.0f || travel.GetHeight() <= 0.0f)
        return;
    const int stretches = channel == ColorChannel::Hue ? kColorHueStretches : 1;
    for (int i = 0; i < stretches; ++i)
    {
        const double a = static_cast<double>(i) / stretches;
        const double b = static_cast<double>(i + 1) / stretches;
        BarStretch(drawList, travel, orientation, a, b, Dimmed(ColorChannelSample(channel, color, hsv, a), disabled),
                   Dimmed(ColorChannelSample(channel, color, hsv, b), disabled), 0.0f, ImDrawFlags_RoundNone);
    }
}

void DrawColorSquare(ImDrawList* drawList, const ImRect& area, double hue, float radius, bool disabled)
{
    if (area.GetWidth() <= 0.0f || area.GetHeight() <= 0.0f)
        return;
    // HSV over gamma-encoded channels is v * ((1 - s) * white + s * hue): a horizontal gradient
    // from white to the hue, darkened by a vertical gradient of black from transparent at the top
    // to opaque at the bottom. Straight alpha blending is linear in the channels, so both layers
    // together give the exact color of every point.
    const ImVec4 white = ColorFromHsv({hue, 0.0, 1.0}, 1.0f);
    const ImVec4 pure = ColorHueColor(hue);
    const ImVec4 black = ColorFromHsv({hue, 0.0, 0.0}, 1.0f);
    FillGradientRect(drawList, area.Min, area.Max, Dimmed(white, disabled), Dimmed(pure, disabled),
                     GradientDir::Horizontal, radius);
    const ImVec4 clear = WithAlpha(black, 0.0f);
    FillGradientRect(drawList, area.Min, area.Max, ToU32(clear), Dimmed(black, disabled), GradientDir::Vertical, radius);
}

float ColorBarThickness(float controlHeight)
{
    const float thickness = std::max(2.0f, std::round(Px(T().picker.colorBarHeight) * 0.5f) * 2.0f);
    return std::min(thickness, std::max(2.0f, controlHeight));
}

ColorPartResult ColorBarPart(ImGuiID id, const ImRect& control, const ImRect& track, float knob, Orientation orientation,
                             ColorChannel channel, const ImVec4& color, ColorHsv* hsv, const ColorPartParams& params)
{
    ColorPartResult result;
    result.color = color;
    ItemOptions itemOptions;
    itemOptions.press = PressMode::Click;
    itemOptions.readOnly = params.readOnly;
    itemOptions.mixed = params.mixed;
    itemOptions.forced = params.forced;
    itemOptions.part = params.part;
    itemOptions.handCursor = !params.readOnly;
    const ItemState item = PartBehavior(id, control, itemOptions);
    result.item = item;
    const bool readOnly = params.readOnly || item.readOnly;

    double fraction = ColorChannelFraction(channel, color, *hsv);
    ValueDragOptions drag;
    drag.mapping.min = 0.0;
    drag.mapping.max = 1.0;
    drag.mapping.wrap = channel == ColorChannel::Hue;
    drag.orientation = orientation;
    drag.grabRadius = knob * 0.5f;
    drag.readOnly = readOnly;
    const ValueDragResult edit = ValueDragBehavior(item, &fraction, track, drag);
    if (edit.changed && !readOnly)
    {
        result.color = ColorWithChannel(channel, color, hsv, fraction);
        result.changed = true;
    }

    if (!item.visible)
        return result;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const bool vertical = orientation == Orientation::Vertical;
    const float thickness = ColorBarThickness(vertical ? control.GetWidth() : control.GetHeight());
    const float half = thickness * 0.5f;
    // The bar spans the knob travel: at either end the knob centre sits on the end color.
    ImRect bar;
    if (vertical)
    {
        const float x = std::round(track.Min.x - half);
        bar = ImRect(x, std::round(track.Min.y), x + thickness, std::round(track.Max.y));
    }
    else
    {
        const float y = std::round(track.Min.y - half);
        bar = ImRect(std::round(track.Min.x), y, std::round(track.Max.x), y + thickness);
    }
    // The rounded ends reach half a thickness past the travel, so the end colors cover the knob.
    if (vertical)
    {
        bar.Min.y -= half;
        bar.Max.y += half;
    }
    else
    {
        bar.Min.x -= half;
        bar.Max.x += half;
    }
    const ColorHsv shownHsv = *hsv;
    const ImVec4 shown = result.color;
    DrawColorChannelBar(drawList, bar, orientation, channel, shown, shownHsv, item.disabled);

    const double at = ColorChannelFraction(channel, shown, shownHsv);
    const float along = std::round(FractionToTrack(track, orientation, at));
    const ImVec2 center = vertical ? ImVec2(std::round(track.Min.x), along) : ImVec2(along, std::round(track.Min.y));
    const bool mixed = params.mixed && !result.changed;
    if (mixed)
    {
        const float r = knob * 0.5f;
        DrawFocusRing(drawList, ImRect(center.x - r, center.y - r, center.x + r, center.y + r), r, item.focusAmount, 0,
                      RingShape::Round);
    }
    else
    {
        ImVec4 fill = ColorChannelSample(channel, shown, shownHsv, at);
        fill = ColorOnChecker(fill);
        DrawColorHandle(drawList, center, knob * 0.5f, fill, readOnly ? 0.0f : HandleGrowth(item), item.focusAmount,
                        item.disabled);
    }
    return result;
}

ImRect ColorSquareArea(const ImRect& rect, float knob)
{
    const float inset = std::round(knob * 0.5f);
    ImRect area(rect.Min.x + inset, rect.Min.y + inset, rect.Max.x - inset, rect.Max.y - inset);
    if (area.Max.x < area.Min.x)
        area.Max.x = area.Min.x;
    if (area.Max.y < area.Min.y)
        area.Max.y = area.Min.y;
    return area;
}

ColorPartResult ColorSquarePart(ImGuiID id, const ImRect& rect, float knob, const ImVec4& color, ColorHsv* hsv,
                                const ColorPartParams& params)
{
    ColorPartResult result;
    result.color = color;
    ItemOptions itemOptions;
    itemOptions.press = PressMode::Click;
    itemOptions.readOnly = params.readOnly;
    itemOptions.mixed = params.mixed;
    itemOptions.forced = params.forced;
    itemOptions.part = params.part;
    itemOptions.handCursor = !params.readOnly;
    const ItemState item = PartBehavior(id, rect, itemOptions);
    result.item = item;
    const bool readOnly = params.readOnly || item.readOnly;
    const ImRect area = ColorSquareArea(rect, knob);

    double s = Clamp01(hsv->s);
    double v = Clamp01(hsv->v);
    ColorPlaneOptions plane;
    plane.map = SquareMap;
    plane.user = &area;
    plane.readOnly = readOnly;
    const ColorPlaneResult edit = ColorPlaneBehavior(item, &s, &v, plane);
    if (edit.changed && !readOnly)
    {
        ColorHsv next = *hsv;
        next.s = s;
        next.v = v;
        result.color = ColorFromHsv(next, color.w);
        *hsv = next;
        result.changed = true;
    }

    if (!item.visible)
        return result;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float radius = std::min(Px(T().radii.element), area.GetWidth() * 0.5f);
    DrawColorSquare(drawList, area, hsv->h, radius, item.disabled);
    const ImVec2 center(std::round(area.Min.x + static_cast<float>(Clamp01(hsv->s)) * area.GetWidth()),
                        std::round(area.Max.y - static_cast<float>(Clamp01(hsv->v)) * area.GetHeight()));
    if (params.mixed && !result.changed)
    {
        const float r = knob * 0.5f;
        DrawFocusRing(drawList, ImRect(center.x - r, center.y - r, center.x + r, center.y + r), r, item.focusAmount, 0,
                      RingShape::Round);
    }
    else
    {
        const ImVec4 fill = ColorFromHsv({hsv->h, Clamp01(hsv->s), Clamp01(hsv->v)}, 1.0f);
        DrawColorHandle(drawList, center, knob * 0.5f, fill, readOnly ? 0.0f : HandleGrowth(item), item.focusAmount,
                        item.disabled);
    }
    return result;
}

} // namespace facet::internal
