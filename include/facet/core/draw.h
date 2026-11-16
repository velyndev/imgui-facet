#pragma once

// Drawing helpers on top of ImDrawList.
//
// Every function here works in screen pixels, like ImDrawList itself: convert design
// units with Px() before passing lengths. `corners` takes the ImDrawFlags_RoundXXX flags;
// 0 rounds every corner, as in ImDrawList. Strokes lie inside the rectangle, so a stroked
// and a filled rect with the same bounds line up.

#include <imgui.h>

namespace facet
{

enum class GradientDir
{
    Horizontal,  // `from` on the left, `to` on the right
    Vertical,    // `from` at the top, `to` at the bottom
};

// Rectangles with corners chosen by the theme: continuous corners when
// T().radii.continuousCorners is on and the radius is at least Px(T().radii.continuousFrom),
// circular arcs otherwise. Use these unless a shape needs one specific corner type.
void FillRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius = 0.0f,
              ImDrawFlags corners = 0);
void StrokeRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius = 0.0f,
                float thickness = 1.0f, ImDrawFlags corners = 0);

// Rectangles with circular-arc corners.
void FillRoundRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius = 0.0f,
                   ImDrawFlags corners = 0);
void StrokeRoundRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius = 0.0f,
                     float thickness = 1.0f, ImDrawFlags corners = 0);

// Rectangles with continuous corners: the curvature grows smoothly from the straight edge
// into the corner instead of jumping to a circle, which reads softer on large surfaces.
// The corner extends somewhat further along the edges than a circular arc of the same radius.
void FillContinuousRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius,
                        ImDrawFlags corners = 0);
void StrokeContinuousRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 color, float radius,
                          float thickness = 1.0f, ImDrawFlags corners = 0);
// Appends the outline of a continuous-corner rectangle to the current path, clockwise.
// Finish with PathFillConvex or PathStroke(..., ImDrawFlags_Closed).
void PathContinuousRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, ImDrawFlags corners = 0);

// Hairlines: Px(T().metrics.hairline) rounded to whole pixels, at least one pixel, snapped
// to the pixel grid so they stay crisp at every scale. The line covers the pixels starting
// at the given coordinate (rounded down) and growing right or down.
float HairlineThickness();
void HairlineH(ImDrawList* drawList, float x0, float x1, float y, ImU32 color);
void HairlineV(ImDrawList* drawList, float x, float y0, float y1, ImU32 color);

// Linear two-color gradient. Corners follow FillRect.
void FillGradientRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 from, ImU32 to,
                      GradientDir direction = GradientDir::Vertical, float radius = 0.0f,
                      ImDrawFlags corners = 0);

// Transforms everything drawn into one draw list during its lifetime: when the scope ends,
// the vertices added since construction are scaled by `scale` around `pivot`, moved by
// `translate`, and their alpha is multiplied by `alpha`. Used for fade, zoom and slide
// transitions of a group of shapes and text.
//
//     {
//         facet::DrawGroup group(drawList, fade, 0.96f + 0.04f * fade, center);
//         ... draw the popover ...
//     }
//
// Clip rectangles are not transformed. Groups nest: the inner group applies first.
struct DrawGroup
{
    DrawGroup(ImDrawList* drawList, float alpha = 1.0f, float scale = 1.0f, ImVec2 pivot = ImVec2(0.0f, 0.0f),
              ImVec2 translate = ImVec2(0.0f, 0.0f));
    ~DrawGroup();
    DrawGroup(const DrawGroup&) = delete;
    DrawGroup& operator=(const DrawGroup&) = delete;

    ImDrawList* drawList = nullptr;
    int vertexStart = 0;  // VtxBuffer.Size at construction
    float alpha = 1.0f;
    float scale = 1.0f;
    ImVec2 pivot;
    ImVec2 translate;
};

} // namespace facet
