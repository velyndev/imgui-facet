// Continuous-corner rectangles and the outline builder shared with the other helpers.

#include <algorithm>
#include <array>
#include <cmath>

#include "core/shape_path.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"

namespace facet
{
namespace internal
{
namespace
{

constexpr float kHalfPi = 1.57079632679f;
constexpr int kMinCornerSegments = 4;

struct CornerTables
{
    // tables[n] holds n + 1 points; entries below kMinCornerSegments stay unused.
    std::array<std::array<ImVec2, kMaxCornerSegments + 1>, kMaxCornerSegments + 1> tables{};

    CornerTables()
    {
        const float power = 2.0f / kContinuousExponent;
        for (int n = kMinCornerSegments; n <= kMaxCornerSegments; ++n)
        {
            for (int k = 0; k <= n; ++k)
            {
                const float angle = kHalfPi * static_cast<float>(k) / static_cast<float>(n);
                const float c = std::max(0.0f, std::cos(angle));
                const float s = std::max(0.0f, std::sin(angle));
                tables[n][k] = ImVec2(std::pow(c, power), std::pow(s, power));
            }
            // Exact end points, so neighbouring edges meet without a gap.
            tables[n][0] = ImVec2(1.0f, 0.0f);
            tables[n][n] = ImVec2(0.0f, 1.0f);
        }
    }
};

// Corner centers and the quarter turn of each corner, clockwise from the top-left.
struct CornerFrame
{
    ImDrawFlags flag;
    int quarter;  // rotation in quarter turns: 0 bottom-right, 1 bottom-left, 2 top-left, 3 top-right
};

constexpr CornerFrame kCorners[4] = {
    {ImDrawFlags_RoundTopLeft, 2},
    {ImDrawFlags_RoundTopRight, 3},
    {ImDrawFlags_RoundBottomRight, 0},
    {ImDrawFlags_RoundBottomLeft, 1},
};

// A unit point (x, y) of the first quadrant rotated by `quarter` quarter turns.
ImVec2 Rotate(ImVec2 p, int quarter)
{
    switch (quarter)
    {
    case 0:
        return p;
    case 1:
        return ImVec2(-p.y, p.x);
    case 2:
        return ImVec2(-p.x, -p.y);
    default:
        return ImVec2(p.y, -p.x);
    }
}

ImVec2 CornerPoint(ImVec2 pMin, ImVec2 pMax, int index)
{
    switch (index)
    {
    case 0:
        return pMin;
    case 1:
        return ImVec2(pMax.x, pMin.y);
    case 2:
        return pMax;
    default:
        return ImVec2(pMin.x, pMax.y);
    }
}

ImVec2 CornerCenter(ImVec2 pMin, ImVec2 pMax, int index, float extent)
{
    switch (index)
    {
    case 0:
        return ImVec2(pMin.x + extent, pMin.y + extent);
    case 1:
        return ImVec2(pMax.x - extent, pMin.y + extent);
    case 2:
        return ImVec2(pMax.x - extent, pMax.y - extent);
    default:
        return ImVec2(pMin.x + extent, pMax.y - extent);
    }
}

} // namespace

int CornerSegments(float extent)
{
    // About one segment per 2.5 px of corner, which keeps the chord error well under a pixel.
    const int segments = static_cast<int>(std::ceil(extent / 2.5f)) + 2;
    return std::clamp(segments, kMinCornerSegments, kMaxCornerSegments);
}

const ImVec2* ContinuousCornerTable(int segments)
{
    static const CornerTables cache;
    segments = std::clamp(segments, kMinCornerSegments, kMaxCornerSegments);
    return cache.tables[segments].data();
}

ImDrawFlags ResolveCorners(ImDrawFlags flags)
{
    const ImDrawFlags corners = flags & ImDrawFlags_RoundMask_;
    if (corners == 0)
        return ImDrawFlags_RoundAll;
    if (corners == ImDrawFlags_RoundNone)
        return 0;
    return corners;
}

int RoundRectOutline(ImVec2 pMin, ImVec2 pMax, float radius, ImDrawFlags flags, bool continuous, ImVec2* out)
{
    const float halfMin = std::max(0.0f, std::min(pMax.x - pMin.x, pMax.y - pMin.y) * 0.5f);
    const ImDrawFlags corners = ResolveCorners(flags);
    const float extent = std::min(std::max(0.0f, radius) * (continuous ? kContinuousExtent : 1.0f), halfMin);
    int count = 0;
    if (extent < 0.5f || corners == 0)
    {
        for (int i = 0; i < 4; ++i)
            out[count++] = CornerPoint(pMin, pMax, i);
        return count;
    }

    const int segments = CornerSegments(extent);
    const ImVec2* table = continuous ? ContinuousCornerTable(segments) : nullptr;
    for (int i = 0; i < 4; ++i)
    {
        const CornerFrame& frame = kCorners[i];
        if ((corners & frame.flag) == 0)
        {
            out[count++] = CornerPoint(pMin, pMax, i);
            continue;
        }
        const ImVec2 center = CornerCenter(pMin, pMax, i, extent);
        for (int k = 0; k <= segments; ++k)
        {
            ImVec2 unit;
            if (table != nullptr)
            {
                unit = table[k];
            }
            else
            {
                const float angle = kHalfPi * static_cast<float>(k) / static_cast<float>(segments);
                unit = ImVec2(std::cos(angle), std::sin(angle));
            }
            const ImVec2 offset = Rotate(unit, frame.quarter);
            out[count++] = ImVec2(center.x + offset.x * extent, center.y + offset.y * extent);
        }
    }
    return count;
}

// Sutherland-Hodgman: clips the convex polygon `in` against the convex clockwise polygon
// `clip`. Returns the number of points written to `out`.
int ClipToConvex(const ImVec2* in, int inCount, const ImVec2* clip, int clipCount, ImVec2* out, ImVec2* scratch)
{
    ImVec2* source = scratch;
    ImVec2* target = out;
    int count = inCount;
    for (int i = 0; i < count; ++i)
        source[i] = in[i];
    for (int e = 0; e < clipCount && count > 0; ++e)
    {
        const ImVec2 a = clip[e];
        const ImVec2 b = clip[(e + 1) % clipCount];
        const ImVec2 d(b.x - a.x, b.y - a.y);
        if (d.x == 0.0f && d.y == 0.0f)
            continue;
        const auto side = [&](ImVec2 p) { return d.x * (p.y - a.y) - d.y * (p.x - a.x); };
        int written = 0;
        for (int i = 0; i < count && written < kMaxClipPoints - 1; ++i)
        {
            const ImVec2 p = source[i];
            const ImVec2 q = source[(i + 1) % count];
            const float sp = side(p);
            const float sq = side(q);
            if (sp >= 0.0f)
                target[written++] = p;
            if ((sp >= 0.0f) != (sq >= 0.0f))
            {
                const float t = sp / (sp - sq);
                target[written++] = ImVec2(p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t);
            }
        }
        count = written;
        std::swap(source, target);
    }
    if (source != out)
    {
        for (int i = 0; i < count; ++i)
            out[i] = source[i];
    }
    return count;
}

bool UsesContinuousCorners(float radius)
{
    const Radii& radii = T().radii;
    return radii.continuousCorners && radius > 0.0f && radius >= Px(radii.continuousFrom);
}

} // namespace internal

void PathContinuousRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, ImDrawFlags corners)
{
    if (drawList == nullptr)
        return;
    ImVec2 points[internal::kMaxOutlinePoints];
    const int count = internal::RoundRectOutline(pMin, pMax, radius, corners, true, points);
    drawList->_Path.reserve(drawList->_Path.Size + count);
    for (int i = 0; i < count; ++i)
        drawList->_Path.push_back(points[i]);
}

void FillContinuousRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius,
                        ImDrawFlags corners)
{
    if (drawList == nullptr || (color & IM_COL32_A_MASK) == 0 || pMax.x <= pMin.x || pMax.y <= pMin.y)
        return;
    PathContinuousRect(drawList, pMin, pMax, radius, corners);
    drawList->PathFillConvex(color);
}

void StrokeContinuousRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius,
                          float thickness, ImDrawFlags corners)
{
    if (drawList == nullptr || (color & IM_COL32_A_MASK) == 0 || thickness <= 0.0f || pMax.x <= pMin.x ||
        pMax.y <= pMin.y)
        return;
    const float halfMin = std::min(pMax.x - pMin.x, pMax.y - pMin.y) * 0.5f;
    PathContinuousRect(drawList, pMin, pMax, radius, corners);
    // The outline is clockwise, so the inside stroke lies within the rectangle, like AddRect.
    drawList->_AddPolyline(drawList->_Path.Data, drawList->_Path.Size, color, std::min(thickness, halfMin),
                           ImDrawFlags_Closed | ImDrawFlags_StrokeInside, halfMin);
    drawList->_Path.Size = 0;
}

} // namespace facet
