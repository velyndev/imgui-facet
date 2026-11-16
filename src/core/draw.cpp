// Drawing helpers: token-aware rectangles, circular-corner shapes, hairlines, gradients,
// checkerboards, arcs, pointer feedback and DrawGroup.

#include "facet/core/draw.h"

#include <algorithm>
#include <cmath>

#include "core/shape_path.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"

namespace facet
{
namespace
{

constexpr float kPi = 3.14159265359f;
constexpr float kTwoPi = 6.28318530718f;
// Largest distance between a tessellated arc and the true circle, in pixels.
constexpr float kArcMaxError = 0.25f;
constexpr int kMaxArcSegments = 256;

int ArcSegments(float radius, float sweep)
{
    if (radius <= kArcMaxError)
        return 2;
    const float step = 2.0f * std::acos(std::max(-1.0f, 1.0f - kArcMaxError / radius));
    const int segments = static_cast<int>(std::ceil(std::fabs(sweep) / std::max(step, 1e-3f)));
    return std::clamp(segments, 2, kMaxArcSegments);
}

// Appends the arc points; skipFirst leaves out the first one when it repeats the last point
// already on the path (repeated points upset the concave fill).
void ArcPoints(ImDrawList* drawList, ImVec2 center, float radius, float a0, float a1, int segments,
               bool skipFirst = false)
{
    for (int i = skipFirst ? 1 : 0; i <= segments; ++i)
    {
        const float a = a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(segments);
        drawList->_Path.push_back(ImVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius));
    }
}

ImU32 LerpColor(ImU32 a, ImU32 b, float t, float coverage)
{
    ImU32 out = 0;
    for (int shift = 0; shift < 32; shift += 8)
    {
        const float ca = static_cast<float>((a >> shift) & 0xFFu);
        const float cb = static_cast<float>((b >> shift) & 0xFFu);
        float value = ca + (cb - ca) * t;
        if (shift == IM_COL32_A_SHIFT)
            value *= coverage;
        out |= static_cast<ImU32>(std::clamp(value + 0.5f, 0.0f, 255.0f)) << shift;
    }
    return out;
}

bool Overlaps(ImVec2 aMin, ImVec2 aMax, ImVec2 bMin, ImVec2 bMax)
{
    return aMin.x < bMax.x && aMax.x > bMin.x && aMin.y < bMax.y && aMax.y > bMin.y;
}

} // namespace

void FillRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius, ImDrawFlags corners)
{
    if (internal::UsesContinuousCorners(radius))
        FillContinuousRect(drawList, pMin, pMax, color, radius, corners);
    else
        FillRoundRect(drawList, pMin, pMax, color, radius, corners);
}

void StrokeRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius, float thickness,
                ImDrawFlags corners)
{
    if (internal::UsesContinuousCorners(radius))
        StrokeContinuousRect(drawList, pMin, pMax, color, radius, thickness, corners);
    else
        StrokeRoundRect(drawList, pMin, pMax, color, radius, thickness, corners);
}

void FillRoundRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius,
                   ImDrawFlags corners)
{
    if (drawList == nullptr || (color & IM_COL32_A_MASK) == 0)
        return;
    drawList->AddRectFilled(pMin, pMax, color, radius, corners);
}

void StrokeRoundRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius,
                     float thickness, ImDrawFlags corners)
{
    if (drawList == nullptr || (color & IM_COL32_A_MASK) == 0 || thickness <= 0.0f)
        return;
    drawList->AddRect(pMin, pMax, color, radius, thickness, corners | ImDrawFlags_StrokeInside);
}

float HairlineThickness()
{
    return std::max(1.0f, std::round(Px(T().metrics.hairline)));
}

void HairlineH(ImDrawList* drawList, float x0, float x1, float y, ImU32 color)
{
    if (drawList == nullptr || (color & IM_COL32_A_MASK) == 0)
        return;
    const float top = std::floor(y);
    drawList->AddRectFilled(ImVec2(std::round(x0), top), ImVec2(std::round(x1), top + HairlineThickness()), color);
}

void HairlineV(ImDrawList* drawList, float x, float y0, float y1, ImU32 color)
{
    if (drawList == nullptr || (color & IM_COL32_A_MASK) == 0)
        return;
    const float left = std::floor(x);
    drawList->AddRectFilled(ImVec2(left, std::round(y0)), ImVec2(left + HairlineThickness(), std::round(y1)), color);
}

void FillGradientRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 from, ImU32 to,
                      GradientDir direction, float radius, ImDrawFlags corners)
{
    if (drawList == nullptr || pMax.x <= pMin.x || pMax.y <= pMin.y)
        return;
    if (((from | to) & IM_COL32_A_MASK) == 0)
        return;
    if (radius <= 0.0f)
    {
        if (direction == GradientDir::Horizontal)
            drawList->AddRectFilledMultiColor(pMin, pMax, from, to, to, from);
        else
            drawList->AddRectFilledMultiColor(pMin, pMax, from, from, to, to);
        return;
    }
    // Draw the shape in opaque white, then recolor its vertices by position. The vertex
    // alpha that remains is the anti-aliasing coverage of the edge, which scales the result.
    const int start = drawList->VtxBuffer.Size;
    FillRect(drawList, pMin, pMax, IM_COL32_WHITE, radius, corners);
    const bool horizontal = direction == GradientDir::Horizontal;
    const float origin = horizontal ? pMin.x : pMin.y;
    const float length = horizontal ? pMax.x - pMin.x : pMax.y - pMin.y;
    for (int i = start; i < drawList->VtxBuffer.Size; ++i)
    {
        ImDrawVert& vertex = drawList->VtxBuffer[i];
        const float position = horizontal ? vertex.pos.x : vertex.pos.y;
        const float t = std::clamp((position - origin) / length, 0.0f, 1.0f);
        const float coverage = static_cast<float>((vertex.col >> IM_COL32_A_SHIFT) & 0xFFu) / 255.0f;
        vertex.col = LerpColor(from, to, t, coverage);
    }
}

void FillCheckerboard(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 colorA, ImU32 colorB, float cell,
                      float radius, ImDrawFlags corners)
{
    if (drawList == nullptr || pMax.x <= pMin.x || pMax.y <= pMin.y)
        return;
    const float size = std::max(1.0f, std::round(cell));
    FillRect(drawList, pMin, pMax, colorA, radius, corners);
    if ((colorB & IM_COL32_A_MASK) == 0)
        return;

    // The outline, and the boxes at the rounded corners inside which cells need clipping.
    const bool continuous = internal::UsesContinuousCorners(radius);
    const float halfMin = std::min(pMax.x - pMin.x, pMax.y - pMin.y) * 0.5f;
    const float extent =
        std::min(std::max(0.0f, radius) * (continuous ? internal::kContinuousExtent : 1.0f), halfMin);
    const ImDrawFlags rounded = internal::ResolveCorners(corners);
    ImVec2 outline[internal::kMaxOutlinePoints];
    int outlineCount = 0;
    ImVec2 boxMin[4];
    ImVec2 boxMax[4];
    int boxes = 0;
    if (extent >= 0.5f && rounded != 0)
    {
        outlineCount = internal::RoundRectOutline(pMin, pMax, radius, corners, continuous, outline);
        const ImDrawFlags flags[4] = {ImDrawFlags_RoundTopLeft, ImDrawFlags_RoundTopRight,
                                      ImDrawFlags_RoundBottomRight, ImDrawFlags_RoundBottomLeft};
        for (int i = 0; i < 4; ++i)
        {
            if ((rounded & flags[i]) == 0)
                continue;
            const float x = (i == 0 || i == 3) ? pMin.x : pMax.x - extent;
            const float y = (i == 0 || i == 1) ? pMin.y : pMax.y - extent;
            boxMin[boxes] = ImVec2(x, y);
            boxMax[boxes] = ImVec2(x + extent, y + extent);
            ++boxes;
        }
    }

    ImVec2 clipped[internal::kMaxClipPoints];
    ImVec2 scratch[internal::kMaxClipPoints];
    const int columns = static_cast<int>(std::ceil((pMax.x - pMin.x) / size));
    const int rows = static_cast<int>(std::ceil((pMax.y - pMin.y) / size));
    for (int row = 0; row < rows; ++row)
    {
        for (int column = (row + 1) % 2; column < columns; column += 2)
        {
            const ImVec2 a(pMin.x + static_cast<float>(column) * size, pMin.y + static_cast<float>(row) * size);
            const ImVec2 b(std::min(a.x + size, pMax.x), std::min(a.y + size, pMax.y));
            bool atCorner = false;
            for (int i = 0; i < boxes && !atCorner; ++i)
                atCorner = Overlaps(a, b, boxMin[i], boxMax[i]);
            if (!atCorner)
            {
                drawList->AddRectFilled(a, b, colorB);
                continue;
            }
            const ImVec2 square[4] = {a, ImVec2(b.x, a.y), b, ImVec2(a.x, b.y)};
            const int count = internal::ClipToConvex(square, 4, outline, outlineCount, clipped, scratch);
            if (count >= 3)
                drawList->AddConvexPolyFilled(clipped, count, colorB);
        }
    }
}

void StrokeArc(ImDrawList* drawList, ImVec2 center, float radius, float aMin, float aMax, ImU32 color,
               float thickness, bool roundCaps)
{
    if (drawList == nullptr || (color & IM_COL32_A_MASK) == 0 || thickness <= 0.0f || radius <= 0.0f)
        return;
    if (aMax < aMin)
        std::swap(aMin, aMax);
    const float sweep = aMax - aMin;
    thickness = std::min(thickness, radius * 2.0f);
    const float half = thickness * 0.5f;

    if (sweep >= kTwoPi - 1e-4f)
    {
        ArcPoints(drawList, center, radius, 0.0f, kTwoPi, ArcSegments(radius + half, kTwoPi));
        drawList->_Path.Size -= 1;  // the closing point repeats the first
        drawList->PathStroke(color, thickness, ImDrawFlags_Closed | ImDrawFlags_StrokeCenter);
        return;
    }
    if (sweep <= 0.0f)
        return;
    if (!roundCaps)
    {
        ArcPoints(drawList, center, radius, aMin, aMax, ArcSegments(radius + half, sweep));
        drawList->PathStroke(color, thickness, ImDrawFlags_StrokeCenter);
        return;
    }

    // One outline: outer arc forward, the end cap, inner arc backward, the start cap.
    const float outer = radius + half;
    const float inner = std::max(0.0f, radius - half);
    const ImVec2 endCenter(center.x + std::cos(aMax) * radius, center.y + std::sin(aMax) * radius);
    const ImVec2 startCenter(center.x + std::cos(aMin) * radius, center.y + std::sin(aMin) * radius);
    const int capSegments = ArcSegments(half, kPi);
    ArcPoints(drawList, center, outer, aMin, aMax, ArcSegments(outer, sweep));
    ArcPoints(drawList, endCenter, half, aMax, aMax + kPi, capSegments, true);
    if (inner > 0.0f)
        ArcPoints(drawList, center, inner, aMax, aMin, ArcSegments(inner, sweep), true);
    // The start cap ends on the first outer point, which the path already holds.
    ArcPoints(drawList, startCenter, half, aMin + kPi, aMin + kTwoPi, capSegments, true);
    drawList->_Path.Size -= 1;
    drawList->PathFillConcave(color);
}

void ItemCursor()
{
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        return;
    if ((ImGui::GetItemFlags() & ImGuiItemFlags_Disabled) != 0)
        SetNotAllowedCursor();
    else
        SetHandCursor();
}

void SetHandCursor()
{
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
}

void SetNotAllowedCursor()
{
    ImGui::SetMouseCursor(ImGuiMouseCursor_NotAllowed);
}

DrawGroup::DrawGroup(ImDrawList* list, float groupAlpha, float groupScale, ImVec2 groupPivot,
                     ImVec2 groupTranslate)
    : drawList(list),
      vertexStart(list != nullptr ? list->VtxBuffer.Size : 0),
      alpha(groupAlpha),
      scale(groupScale),
      pivot(groupPivot),
      translate(groupTranslate)
{
    if (drawList == nullptr)
        return;
    const bool moves = scale != 1.0f || translate.x != 0.0f || translate.y != 0.0f;
    if (!moves)
        return;
    // Start a command of our own so the clip rects of the group can be moved with it
    // without moving the clipping of what was drawn before.
    if (drawList->CmdBuffer.Size == 0 || drawList->CmdBuffer.back().ElemCount > 0)
        drawList->AddDrawCmd();
    outerClip = drawList->_CmdHeader.ClipRect;
    commandStart = drawList->CmdBuffer.Size - 1;
    indexStart = drawList->IdxBuffer.Size;
}

DrawGroup::~DrawGroup()
{
    if (drawList == nullptr)
        return;
    const bool moves = scale != 1.0f || translate.x != 0.0f || translate.y != 0.0f;
    const bool fades = alpha < 1.0f;
    if (moves || fades)
    {
        const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
        for (int i = vertexStart; i < drawList->VtxBuffer.Size; ++i)
        {
            ImDrawVert& vertex = drawList->VtxBuffer[i];
            if (moves)
            {
                vertex.pos.x = pivot.x + (vertex.pos.x - pivot.x) * scale + translate.x;
                vertex.pos.y = pivot.y + (vertex.pos.y - pivot.y) * scale + translate.y;
            }
            if (fades)
            {
                const float a = static_cast<float>((vertex.col >> IM_COL32_A_SHIFT) & 0xFFu) * clampedAlpha;
                vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (static_cast<ImU32>(a + 0.5f) << IM_COL32_A_SHIFT);
            }
        }
    }

    if (commandStart < 0 || commandStart >= drawList->CmdBuffer.Size)
        return;
    const float s = std::max(0.0f, scale);
    const auto mapX = [&](float x) { return pivot.x + (x - pivot.x) * s + translate.x; };
    const auto mapY = [&](float y) { return pivot.y + (y - pivot.y) * s + translate.y; };
    const int last = drawList->CmdBuffer.Size - 1;
    bool lastTransformed = false;
    for (int i = commandStart; i <= last; ++i)
    {
        ImDrawCmd& command = drawList->CmdBuffer[i];
        // A command that also holds vertices from before the group keeps its clip rect.
        if (static_cast<int>(command.IdxOffset) < indexStart)
            continue;
        // The open command at the end without content keeps the clip rect for what follows.
        if (i == last && command.ElemCount == 0 && command.UserCallback == nullptr)
            continue;
        ImVec4& clip = command.ClipRect;
        clip = ImVec4(std::max(mapX(clip.x), outerClip.x), std::max(mapY(clip.y), outerClip.y),
                      std::min(mapX(clip.z), outerClip.z), std::min(mapY(clip.w), outerClip.w));
        clip.z = std::max(clip.z, clip.x);
        clip.w = std::max(clip.w, clip.y);
        lastTransformed = lastTransformed || i == last;
    }
    // Drawing after the group must not land in a command whose clip rect was moved.
    if (lastTransformed)
        drawList->AddDrawCmd();
}

} // namespace facet
