// The shared parts of the curve and easing editors (curve_parts.h).

#include "widgets/composites/curve_parts.h"

#include <algorithm>
#include <cmath>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"

namespace facet::internal
{
namespace
{

// The grid aims at a line every this many spacing.xxxl along each axis: dense enough to read a
// value off, loose enough not to turn the canvas into a texture.
constexpr float kCurveGridSpacingInXxxl = 1.0f;

// The pixel positions of the major ticks of `scale` inside its domain, without the ends (the frame
// is there). Returns the count.
int CurveGridPixels(const Scale& scale, float length, float* out, int capacity)
{
    const int target = TickTargetCount(length, Px(T().spacing.xxxl) * kCurveGridSpacingInXxxl, 0.0f);
    Tick ticks[kMaxTicks];
    const double lo = std::min(scale.domainMin, scale.domainMax);
    const double hi = std::max(scale.domainMin, scale.domainMax);
    const TickSet set = LinearTicks(lo, hi, target, ticks);
    if (set.step <= 0.0)
        return 0;
    int count = 0;
    const int n = std::min(set.count, kMaxTicks);
    for (int i = 0; i < n && count < capacity; ++i)
    {
        const double span = hi - lo;
        if (std::fabs(ticks[i].value - lo) <= span * 1e-9 || std::fabs(ticks[i].value - hi) <= span * 1e-9)
            continue;
        out[count++] = scale.ToPixel(ticks[i].value);
    }
    return count;
}

} // namespace

CurveCanvas LayoutCurveCanvas(ImVec2 origin, float width, float height)
{
    const Theme& t = T();
    CurveCanvas c;
    origin = ImVec2(std::round(origin.x), std::round(origin.y));
    width = std::max(1.0f, std::round(width));
    height = std::max(1.0f, std::round(height));
    c.frame = ImRect(origin, ImVec2(origin.x + width, origin.y + height));
    c.handle = std::max(2.0f, std::round(Px(t.composites.editorHandleSize)));
    const float inset = std::round(c.handle * 0.5f + Px(t.spacing.xs));
    c.area = ImRect(c.frame.Min.x + inset, c.frame.Min.y + inset, std::max(c.frame.Min.x + inset + 1.0f, c.frame.Max.x - inset),
                    std::max(c.frame.Min.y + inset + 1.0f, c.frame.Max.y - inset));
    return c;
}

void DrawCurveCanvas(ImDrawList* drawList, const CurveCanvas& canvas, const Scale& x, const Scale& y, bool grid,
                     const ItemState& item, bool readOnly)
{
    FieldFrameLook look;
    look.hoverAmount = item.hoverAmount;
    look.disabled = item.disabled;
    look.readOnly = readOnly && !item.disabled;
    DrawFieldFrame(drawList, canvas.frame, look);
    if (!grid)
        return;
    float xs[kMaxTicks];
    float ys[kMaxTicks];
    const int columns = CurveGridPixels(x, canvas.area.GetWidth(), xs, kMaxTicks);
    const int rows = CurveGridPixels(y, canvas.area.GetHeight(), ys, kMaxTicks);
    DrawGridLines(drawList, canvas.area, std::span<const float>(xs, static_cast<std::size_t>(columns)),
                  std::span<const float>(ys, static_cast<std::size_t>(rows)), ToU32(T().colors.separator));
}

ImU32 CurveLineColor(bool disabled)
{
    const Colors& c = T().colors;
    return ToU32(disabled ? c.labelTertiary : SeriesColor(c, 0));
}

CurvePolyline::CurvePolyline(ImDrawList* drawList, ImU32 color, const ImRect& clip)
    : drawList_(drawList), color_(color), clip_(clip), thickness_(std::max(HairlineThickness(), Px(T().charts.lineWidth)))
{
}

void CurvePolyline::Add(ImVec2 point)
{
    if (count_ == kChunk)
    {
        DrawPolyline(drawList_, std::span<const ImVec2>(points_, static_cast<std::size_t>(count_)), color_, thickness_, clip_);
        // The next chunk starts where this one ended, so the line has no gap.
        points_[0] = points_[count_ - 1];
        count_ = 1;
    }
    points_[count_++] = point;
}

void CurvePolyline::Finish()
{
    if (count_ >= 2)
        DrawPolyline(drawList_, std::span<const ImVec2>(points_, static_cast<std::size_t>(count_)), color_, thickness_, clip_);
    count_ = 0;
}

ImRect CurvePointRect(ImVec2 center, float size)
{
    const float half = std::round(size * 0.5f);
    const float x = std::round(center.x);
    const float y = std::round(center.y);
    return ImRect(x - half, y - half, x - half + size, y - half + size);
}

void DrawCurvePoint(ImDrawList* drawList, ImVec2 center, float size, const ItemState& item, bool readOnly)
{
    DrawSliderKnob(drawList, CurvePointRect(center, size), item, PickSliderLook(item.disabled, readOnly));
}

} // namespace facet::internal
