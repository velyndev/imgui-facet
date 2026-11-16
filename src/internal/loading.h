#pragma once

// Drawing helpers for loading states: the skeleton shimmer, the spinner and their shared
// clock. Every indicator reads the same motion clock, so all skeletons on screen sweep in
// step and every spinner turns at the same angle.
//
// Timing comes from the motion tokens (shimmerPeriod, spinnerPeriod, times motion.scale).
// While facet::ReduceMotion() is true nothing moves: skeletons are flat and the spinner fades
// in and out slowly in place instead of turning.

#include <imgui.h>

namespace facet::internal
{

// Position of the shimmer sweep from 0 to 1 over one shimmerPeriod; -1 while motion is
// reduced (no sweep).
float ShimmerPhase();

// Rotation of the spinner in radians, from 0 to 2 pi over one spinnerPeriod.
float SpinnerAngle();

// Opacity factor of the spinner, 1 normally; while motion is reduced it pulses between
// about 0.4 and 1 over two spinner periods.
float SpinnerOpacity();

// A placeholder block: fill[2] in the shape of FillRect (radius in pixels), with a brighter
// fill[3] band sweeping from left to right. The band is clipped to the shape.
void DrawSkeleton(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius);

// A spinner: three quarters of a ring with round caps, `radius` to the middle of the stroke,
// turning clockwise. A faint full ring in fill[1] marks the track.
void DrawSpinner(ImDrawList* drawList, ImVec2 center, float radius, ImU32 color, float thickness);

} // namespace facet::internal
