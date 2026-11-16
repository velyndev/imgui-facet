// Continuous-corner rectangles.
//
// The curvature-matched corner path is not implemented yet: these functions draw
// circular-arc corners with the same radius, so callers get a correct rounded shape.

#include "facet/core/draw.h"

namespace facet
{

void FillContinuousRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius,
                        ImDrawFlags corners)
{
    FillRoundRect(drawList, pMin, pMax, color, radius, corners);
}

void StrokeContinuousRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius,
                          float thickness, ImDrawFlags corners)
{
    StrokeRoundRect(drawList, pMin, pMax, color, radius, thickness, corners);
}

void PathContinuousRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, ImDrawFlags corners)
{
    if (drawList != nullptr)
        drawList->PathRect(pMin, pMax, radius, corners);
}

} // namespace facet
