#pragma once

// Motion: frame-rate independent animation of floats and colors.
//
// Every animated value lives in one store, keyed by an ImGui id plus a small sub key so a
// widget can animate several values (hover, press, knob position) under its own id. The
// store advances by facet::DeltaTime() once per facet::NewFrame, and every motion is solved
// in closed form for the elapsed time, so results do not depend on the frame rate and stay
// stable at any frame time. Querying a key twice in one frame returns the same value.
// The first query of a key returns the target at once (nothing animates in on the first
// frame). Time passes for a key only while it is queried: a widget that was not drawn for a
// few frames and comes back with a new target animates from the start of the motion. Keys
// that were not queried for both 30 frames and 0.5 s of frame time are dropped in
// facet::EndFrame (and after 600 frames whatever the frame time, so a delta time of 0 does
// not keep them forever); a key queried again after that starts at its target, so an owner
// that wants a motion when it reappears after a long gap keeps querying its key while
// hidden. Queries do not allocate memory; the store
// grows only when more keys are alive at once than ever before. A key keeps one kind: using
// the same id and sub key for, say, Smooth and Spring restarts it each time.
//
// Durations and spring responses are multiplied by T().motion.scale. While ReduceMotion()
// is true every function returns its target immediately. Values snap exactly onto their
// target once the remaining distance is negligible. The store is not synchronized: query it
// only from the thread that runs the Dear ImGui frame (see context.h).

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

// True when animations should be skipped: the theme's motion.reduceMotion token is on, or the
// Windows setting "Animation effects" is off while Facet follows it.
bool ReduceMotion();

// The Windows "Animation effects" setting is off (SPI_GETCLIENTAREAANIMATION). Read at Init and
// again about once per second of frame time, so a change applies without a restart.
bool SystemReduceMotion();

// Whether ReduceMotion() follows the Windows setting (default true). Tools that must render
// the same frames on every machine, such as screenshot runs, turn it off.
void SetFollowSystemMotion(bool follow);
bool FollowsSystemMotion();

} // namespace facet
