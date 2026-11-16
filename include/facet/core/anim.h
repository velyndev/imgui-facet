#pragma once

// Motion: frame-rate independent animation of floats and colors.
//
// Every animated value lives in one store, keyed by an ImGui id plus a small sub key so a
// widget can animate several values (hover, press, knob position) under its own id. The
// store advances by facet::DeltaTime(), so results do not depend on the frame rate.
// The first query of a key returns the target at once (nothing animates in on the first
// frame), and keys that were not queried during a frame are dropped at the end of it.
//
// Durations and spring responses are multiplied by T().motion.scale. While ReduceMotion()
// is true every function returns its target immediately.

#include <imgui.h>

namespace facet
{

// Easing curves for Tween. All map 0 to 0 and 1 to 1.
enum class Ease
{
    Linear,
    InCubic,
    OutCubic,
    InOutCubic,
    OutQuint,
    OutBack,  // overshoots slightly before settling
};

// A damped spring. `response` is the period of the undamped oscillation in seconds (how
// quickly the value follows), `dampingRatio` 1 means no overshoot, below 1 a little bounce.
struct SpringSpec
{
    float response = 0.35f;
    float dampingRatio = 0.85f;
};

// The easing curve evaluated at t (clamped to 0 to 1).
float EaseValue(Ease ease, float t);

// Exponential approach towards `target`: each second the remaining distance shrinks by the
// factor exp(-speed).
float Smooth(ImGuiID id, int sub, float target, float speed);

// Spring towards `target`, keeping velocity when the target changes mid-flight.
float Spring(ImGuiID id, int sub, float target, const SpringSpec& spec);

// Timed transition: when `target` changes, the value moves from where it is to the new
// target over `duration` seconds along `ease`.
float Tween(ImGuiID id, int sub, float target, float duration, Ease ease = Ease::OutCubic);

// Smooth for colors; interpolates in OKLab so hue shifts look even.
ImVec4 SmoothColor(ImGuiID id, int sub, const ImVec4& target, float speed);

// True when animations should be skipped (the theme's motion.reduceMotion token).
bool ReduceMotion();

} // namespace facet
