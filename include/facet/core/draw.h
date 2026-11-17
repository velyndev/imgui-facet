#pragma once

// Drawing helpers on top of ImDrawList.
//
// Every function here works in screen pixels, like ImDrawList itself: convert design
// units with Px() before passing lengths. `corners` takes the ImDrawFlags_RoundXXX flags;
// 0 rounds every corner, as in ImDrawList. Strokes lie inside the rectangle, so a stroked
// and a filled rect with the same bounds line up.
//
// Drawing a shape under a texture of your own: Dear ImGui takes some shapes from its font
// atlas through their UVs. Fills sample the atlas's white pixel, anti-aliased strokes sample
// baked line textures (ImDrawFlags_AALines), and rounded rectangles with a radius of up to
// IM_DRAWLIST_TEX_CORNERS_ROUNDING_MAX pixels use baked corner textures whenever
// ImDrawFlags_UseTexForRoundCorners is on together with ImDrawFlags_AAFill or AALines (the
// default unless the atlas has ImFontAtlasFlags_NoBakedRoundCorners). Under
// ImDrawList::PushTexture those UVs index the other texture instead. A helper that fills a
// rounded shape with its own texture therefore wraps it in
// ImDrawList::PushDrawFlag(ImDrawFlags_UseTexForRoundCorners, false) and PopDrawFlag(), so
// FillRect builds the corners as geometry, then sets every UV it added itself (for example
// with ImGui::ShadeVertsLinearUV, as ImDrawList::AddImageRounded does). Strokes and
// untextured shapes go before PushTexture or after PopTexture.

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
// Each corner is a superellipse quadrant (exponent 3) whose curvature is zero where it meets
// the edges. It extends about 1.42 times the radius along each edge, so that its middle
// point lies exactly where a circular arc of the same radius would put it; when the
// rectangle is too small for that, the extent shrinks to half the shorter side. The corner
// points come from a unit table cached per segment count, chosen from the radius in pixels.
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

// Linear two-color gradient. Corners follow FillRect. The colors are interpolated per
// channel in sRGB with straight alpha, across the whole rectangle (the rounded corners cut
// the gradient, they do not compress it).
void FillGradientRect(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 from, ImU32 to,
                      GradientDir direction = GradientDir::Vertical, float radius = 0.0f,
                      ImDrawFlags corners = 0);

// A two-color checkerboard of square cells `cell` pixels wide (rounded to whole pixels, at
// least 1), starting with `colorA` at the top-left cell. Drawn behind translucent colors so
// their alpha shows. Corners follow FillRect; cells at a rounded corner are clipped to it.
void FillCheckerboard(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, ImU32 colorA, ImU32 colorB, float cell,
                      float radius = 0.0f, ImDrawFlags corners = 0);

// A circular arc stroke from angle aMin to aMax in radians (0 points right, angles grow
// clockwise on screen because y points down). With `roundCaps`, each end gets a half-disc
// cap of the stroke width, drawn as part of one outline so translucent colors do not
// double up. A sweep of a full turn or more draws a closed ring without caps. For progress
// rings, gauges and knobs.
void StrokeArc(ImDrawList* drawList, ImVec2 center, float radius, float aMin, float aMax, ImU32 color,
               float thickness, bool roundCaps = true);

// Pointer feedback for the last submitted item: the hand cursor while it is hovered and
// enabled, the not-allowed cursor while it is hovered and disabled (inside BeginDisabled).
// Call right after the item.
void ItemCursor();
// The same for any custom hit test: call while the pointer is over an interactive area.
void SetHandCursor();
void SetNotAllowedCursor();

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
// When the group moves or scales (decided by the values passed to the constructor), it
// starts a new draw command, and at the end the clip rectangles of the commands it added are
// transformed the same way and intersected with the clip rectangle that was current at
// construction. So content clipped inside the group (a scrolled list in a popover) stays
// clipped to its moved frame, and nothing is drawn outside the frame around the group.
// Commands added after the group keep their own clip rectangle. Only the draw list passed in
// is affected: child windows have draw lists of their own. Groups nest: the inner group
// applies first.
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
    int commandStart = -1;  // first command of the group when it transforms clip rects, else -1
    int indexStart = 0;     // IdxBuffer.Size at construction
    ImVec4 outerClip;       // clip rect current at construction
};

} // namespace facet
