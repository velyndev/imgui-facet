#pragma once

// Outline points of rounded rectangles, shared by the drawing helpers.

#include <imgui.h>

namespace facet::internal
{

// Superellipse exponent of the continuous corner and how far the corner reaches along each
// edge, in multiples of the radius. The factor puts the corner's middle point (at 45 degrees)
// where a circular arc of the same radius has it: (1 - 1/sqrt(2)) / (1 - 2^(-1/3)).
inline constexpr float kContinuousExponent = 3.0f;
inline constexpr float kContinuousExtent = 1.41975f;

// Most points one corner can have, and the most an outline can have.
inline constexpr int kMaxCornerSegments = 32;
inline constexpr int kMaxOutlinePoints = 4 * (kMaxCornerSegments + 1);

// Segments per corner for a corner reaching `extent` pixels along the edges. This is the
// radius bucket of the cached unit tables.
int CornerSegments(float extent);

// Points of one unit corner quadrant for `segments` (4 to kMaxCornerSegments): entry k holds
// (|cos d|^(2/e), |sin d|^(2/e)) for d = k * 90 degrees / segments, so segments + 1 entries.
// Computed once per segment count and cached.
const ImVec2* ContinuousCornerTable(int segments);

// ImDrawFlags corner flags made explicit: 0 means every corner, as in ImDrawList.
ImDrawFlags ResolveCorners(ImDrawFlags flags);

// Writes the clockwise outline of a rounded rectangle into `out` (room for
// kMaxOutlinePoints) and returns the number of points. `continuous` selects the corner type.
int RoundRectOutline(ImVec2 pMin, ImVec2 pMax, float radius, ImDrawFlags corners, bool continuous, ImVec2* out);

// Room for a convex polygon clipped against an outline: four corners plus one point per edge.
inline constexpr int kMaxClipPoints = kMaxOutlinePoints + 8;

// Sutherland-Hodgman: clips the convex polygon in against the convex clockwise polygon
// clip, writes the result to out (room for kMaxClipPoints, scratch the same) and
// returns its point count.
int ClipToConvex(const ImVec2* in, int inCount, const ImVec2* clip, int clipCount, ImVec2* out, ImVec2* scratch);

// The radius at which FillRect switches to continuous corners, in pixels, and whether a
// radius uses them with the current theme.
bool UsesContinuousCorners(float radius);

} // namespace facet::internal
