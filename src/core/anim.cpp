// Motion store: smoothing, springs and tweens keyed by ImGui id.
//
// The store itself does not exist yet: every animation function returns its target, which
// is what reduced motion produces as well, so callers already behave correctly.

#include "facet/core/anim.h"

#include <algorithm>
#include <cmath>

#include "internal/subsystem.h"

namespace facet
{

float EaseValue(Ease ease, float t)
{
    t = std::clamp(t, 0.0f, 1.0f);
    switch (ease)
    {
    case Ease::Linear:
        return t;
    case Ease::InCubic:
        return t * t * t;
    case Ease::OutCubic:
    {
        const float u = 1.0f - t;
        return 1.0f - u * u * u;
    }
    case Ease::InOutCubic:
    {
        if (t < 0.5f)
            return 4.0f * t * t * t;
        const float u = -2.0f * t + 2.0f;
        return 1.0f - u * u * u * 0.5f;
    }
    case Ease::OutQuint:
        return 1.0f - std::pow(1.0f - t, 5.0f);
    case Ease::OutBack:
    {
        constexpr float kOvershoot = 1.70158f;
        const float u = t - 1.0f;
        return 1.0f + (kOvershoot + 1.0f) * u * u * u + kOvershoot * u * u;
    }
    }
    return t;
}

float Smooth(ImGuiID, int, float target, float)
{
    return target;
}

float Spring(ImGuiID, int, float target, const SpringSpec&)
{
    return target;
}

float Tween(ImGuiID, int, float target, float, Ease)
{
    return target;
}

ImVec4 SmoothColor(ImGuiID, int, const ImVec4& target, float)
{
    return target;
}

bool ReduceMotion()
{
    return false;
}

namespace internal
{

// No hooks yet: the store does not exist.
Subsystem AnimSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "anim";
    return subsystem;
}

} // namespace internal

} // namespace facet
