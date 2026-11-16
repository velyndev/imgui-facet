// Drawing helpers: token-aware rectangles, circular-corner shapes, hairlines, gradients and
// DrawGroup.
//
// FillGradientRect ignores the radius for now and draws square corners.

#include "facet/core/draw.h"

#include <algorithm>
#include <cmath>

#include "facet/core/scale.h"
#include "facet/core/theme.h"

namespace facet
{
namespace
{

bool UseContinuous(float radius)
{
    const Radii& radii = T().radii;
    return radii.continuousCorners && radius > 0.0f && radius >= Px(radii.continuousFrom);
}

} // namespace

void FillRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius, ImDrawFlags corners)
{
    if (UseContinuous(radius))
        FillContinuousRect(drawList, pMin, pMax, color, radius, corners);
    else
        FillRoundRect(drawList, pMin, pMax, color, radius, corners);
}

void StrokeRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius, float thickness,
                ImDrawFlags corners)
{
    if (UseContinuous(radius))
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
                      GradientDir direction, float, ImDrawFlags)
{
    if (drawList == nullptr)
        return;
    if (direction == GradientDir::Horizontal)
        drawList->AddRectFilledMultiColor(pMin, pMax, from, to, to, from);
    else
        drawList->AddRectFilledMultiColor(pMin, pMax, from, from, to, to);
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
}

DrawGroup::~DrawGroup()
{
    if (drawList == nullptr)
        return;
    const bool moves = scale != 1.0f || translate.x != 0.0f || translate.y != 0.0f;
    const bool fades = alpha < 1.0f;
    if (!moves && !fades)
        return;
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

} // namespace facet
