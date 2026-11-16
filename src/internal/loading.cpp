// Loading indicators: shimmer clock, skeleton blocks and the spinner.

#include "internal/loading.h"

#include <algorithm>
#include <cmath>

#include "core/core_internal.h"
#include "core/shape_path.h"
#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/theme.h"

namespace facet::internal
{
namespace
{

constexpr float kTwoPi = 6.28318530718f;
// Width of the shimmer band as a fraction of the skeleton width.
constexpr float kBandFraction = 0.45f;
// Arc of the spinner as a fraction of a full turn.
constexpr float kSpinnerSweep = 0.75f;
// Lowest opacity of the reduced-motion spinner pulse.
constexpr float kPulseMinimum = 0.4f;

float Period(float tokenSeconds)
{
    return std::max(1e-3f, tokenSeconds * std::max(T().motion.scale, 1e-3f));
}

float Cycle(float period)
{
    const double t = MotionClock() / static_cast<double>(period);
    return static_cast<float>(t - std::floor(t));
}

// Fills the part of the convex polygon `clip` that lies between x0 and x1 with a horizontal
// gradient from `from` at x0 to `to` at x1.
void FillBandSlice(ImDrawList* drawList, const ImVec2* clip, int clipCount, float x0, float x1, float top,
                   float bottom, ImU32 from, ImU32 to)
{
    if (x1 <= x0)
        return;
    const ImVec2 slice[4] = {ImVec2(x0, top), ImVec2(x1, top), ImVec2(x1, bottom), ImVec2(x0, bottom)};
    ImVec2 out[kMaxClipPoints];
    ImVec2 scratch[kMaxClipPoints];
    const int count = ClipToConvex(slice, 4, clip, clipCount, out, scratch);
    if (count < 3)
        return;
    const int start = drawList->VtxBuffer.Size;
    drawList->AddConvexPolyFilled(out, count, IM_COL32_WHITE);
    const ImVec4 a = FromU32(from);
    const ImVec4 b = FromU32(to);
    for (int i = start; i < drawList->VtxBuffer.Size; ++i)
    {
        ImDrawVert& vertex = drawList->VtxBuffer[i];
        const float t = std::clamp((vertex.pos.x - x0) / (x1 - x0), 0.0f, 1.0f);
        const float coverage = static_cast<float>((vertex.col >> IM_COL32_A_SHIFT) & 0xFFu) / 255.0f;
        const ImVec4 c(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                       (a.w + (b.w - a.w) * t) * coverage);
        vertex.col = ToU32(c);
    }
}

} // namespace

float ShimmerPhase()
{
    if (ReduceMotion())
        return -1.0f;
    return Cycle(Period(T().motion.shimmerPeriod));
}

float SpinnerAngle()
{
    if (ReduceMotion())
        return 0.0f;
    return Cycle(Period(T().motion.spinnerPeriod)) * kTwoPi;
}

float SpinnerOpacity()
{
    if (!ReduceMotion())
        return 1.0f;
    const float wave = 0.5f + 0.5f * std::cos(Cycle(Period(T().motion.spinnerPeriod * 2.0f)) * kTwoPi);
    return kPulseMinimum + (1.0f - kPulseMinimum) * wave;
}

void DrawSkeleton(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius)
{
    if (drawList == nullptr || pMax.x <= pMin.x || pMax.y <= pMin.y)
        return;
    const Colors& c = T().colors;
    FillRect(drawList, pMin, pMax, ToU32(c.fill[2]), radius);

    const float phase = ShimmerPhase();
    if (phase < 0.0f)
        return;
    ImVec2 outline[kMaxOutlinePoints];
    const int outlineCount = RoundRectOutline(pMin, pMax, radius, 0, UsesContinuousCorners(radius), outline);
    // The band enters fully from the left and leaves fully on the right.
    const float width = pMax.x - pMin.x;
    const float band = width * kBandFraction;
    const float left = pMin.x - band + (width + band) * phase;
    const float middle = left + band * 0.5f;
    const ImU32 clear = ToU32(WithAlpha(c.fill[3], 0.0f));
    const ImU32 peak = ToU32(c.fill[3]);
    FillBandSlice(drawList, outline, outlineCount, std::max(left, pMin.x), std::min(middle, pMax.x), pMin.y, pMax.y,
                  clear, peak);
    FillBandSlice(drawList, outline, outlineCount, std::max(middle, pMin.x), std::min(left + band, pMax.x), pMin.y,
                  pMax.y, peak, clear);
}

void DrawSpinner(ImDrawList* drawList, ImVec2 center, float radius, ImU32 color, float thickness)
{
    if (drawList == nullptr || radius <= 0.0f)
        return;
    StrokeArc(drawList, center, radius, 0.0f, kTwoPi, ToU32(T().colors.fill[1]), thickness, false);
    const float start = SpinnerAngle();
    StrokeArc(drawList, center, radius, start, start + kSpinnerSweep * kTwoPi, ScaleAlpha(color, SpinnerOpacity()),
              thickness, true);
}

} // namespace facet::internal
