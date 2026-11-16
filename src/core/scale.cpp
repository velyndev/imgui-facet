// UI scale: DPI scale times user scale, or a scale fitted to the display size, plus the
// pixel snapping helpers.

#include "facet/core/scale.h"

#include <algorithm>
#include <cmath>

#include "core/core_internal.h"
#include "internal/subsystem.h"

namespace facet
{
namespace
{

// The display size at which FitDisplay gives a scale of 1.
constexpr float kFitReferenceWidth = 1920.0f;
constexpr float kFitReferenceHeight = 1080.0f;

struct ScaleState
{
    float ui = 1.0f;
    float dpi = 1.0f;
    float fit = 1.0f;
    ScaleMode mode = ScaleMode::FollowDpi;
    float effective = 1.0f;
    bool manageFontScale = true;
    std::uint64_t revision = 1;
};

ScaleState& State()
{
    static ScaleState state;
    return state;
}

float Compute(const ScaleState& state)
{
    switch (state.mode)
    {
    case ScaleMode::FollowDpi:
        return state.dpi * state.ui;
    case ScaleMode::Manual:
        return state.ui;
    case ScaleMode::FitDisplay:
        return state.fit * state.ui;
    }
    return state.ui;
}

// Recomputes the effective scale and tells the other modules when it changed.
void Update()
{
    ScaleState& state = State();
    const float effective = Compute(state);
    if (effective == state.effective)
        return;
    state.effective = effective;
    ++state.revision;
    internal::ApplyFontScale();
    internal::NotifyScaleChanged(effective);
}

void NewFrame()
{
    ScaleState& state = State();
    if (state.mode == ScaleMode::FitDisplay && ImGui::GetCurrentContext() != nullptr)
    {
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        if (display.x > 0.0f && display.y > 0.0f)
        {
            state.fit = std::max(0.25f, std::min(display.x / kFitReferenceWidth, display.y / kFitReferenceHeight));
            Update();
        }
    }
    // The host may have reset its style; keep the font scale in step with Scale().
    internal::ApplyFontScale();
    // Runs after the theme hook, so a theme transition step and a scale change of this frame
    // both reach the Dear ImGui style before the first window.
    internal::SyncImGuiStyleIfNeeded();
}

} // namespace

float Scale()
{
    return State().effective;
}

float Px(float units)
{
    return units * State().effective;
}

ImVec2 Px(ImVec2 units)
{
    const float scale = State().effective;
    return ImVec2(units.x * scale, units.y * scale);
}

void SetUiScale(float scale)
{
    State().ui = std::clamp(scale, 0.5f, 4.0f);
    Update();
}

float UiScale()
{
    return State().ui;
}

void SetDpiScale(float scale)
{
    State().dpi = std::clamp(scale, 0.5f, 8.0f);
    Update();
}

float DpiScale()
{
    return State().dpi;
}

void SetScaleMode(ScaleMode mode)
{
    State().mode = mode;
    Update();
}

ScaleMode GetScaleMode()
{
    return State().mode;
}

void SetManageFontScale(bool enabled)
{
    State().manageFontScale = enabled;
    if (enabled)
        internal::ApplyFontScale();
}

bool ManagesFontScale()
{
    return State().manageFontScale;
}

float Snap(float pixels)
{
    return std::round(pixels);
}

ImVec2 Snap(ImVec2 pixels)
{
    return ImVec2(std::round(pixels.x), std::round(pixels.y));
}

float PxSnapped(float units)
{
    if (units <= 0.0f)
        return std::round(Px(units));
    return std::max(1.0f, std::round(Px(units)));
}

float SnapStroke(float pixels, float thickness)
{
    const int whole = std::max(1, static_cast<int>(std::lround(thickness)));
    return std::floor(pixels) + ((whole % 2) == 1 ? 0.5f : 0.0f);
}

void SnapRect(ImVec2& pMin, ImVec2& pMax)
{
    pMin = Snap(pMin);
    pMax = Snap(pMax);
}

namespace internal
{

std::uint64_t ScaleRevision()
{
    return State().revision;
}

void ApplyFontScale()
{
    const ScaleState& state = State();
    if (!state.manageFontScale || ImGui::GetCurrentContext() == nullptr)
        return;
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontScaleMain = state.ui;
    style.FontScaleDpi = state.effective / state.ui;
}

Subsystem ScaleSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "scale";
    subsystem.newFrame = &NewFrame;
    return subsystem;
}

} // namespace internal

} // namespace facet
