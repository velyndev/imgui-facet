// Focus ring and keyboard modality (focus.h). The focus subsystem also drops forced item
// states no widget took at the end of the frame, and resets the state of the interaction blocks
// at Init and Shutdown.

#include "internal/focus.h"

#include <algorithm>
#include <iterator>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/multi_handle.h"
#include "internal/subsystem.h"
#include "internal/value_drag.h"

namespace facet::internal
{
namespace
{

bool& Modality()
{
    static bool keyboard = false;
    return keyboard;
}

// Keys whose press turns the keyboard modality on: navigation and activation.
constexpr ImGuiKey kModalityKeys[] = {
    ImGuiKey_Tab,      ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow,
    ImGuiKey_PageUp,   ImGuiKey_PageDown,  ImGuiKey_Home,       ImGuiKey_End,     ImGuiKey_Space,
    ImGuiKey_Enter,    ImGuiKey_KeypadEnter, ImGuiKey_Escape,
};

// Non-text contrast a ring needs against the surface around it (WCAG 2.2, 1.4.11).
constexpr float kRingContrast = 3.0f;
// Share of the ring color in its halo.
constexpr float kHaloAlpha = 0.35f;

void UpdateModality()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;
    const ImGuiIO& io = ImGui::GetIO();
    for (int button = 0; button < ImGuiMouseButton_COUNT; ++button)
    {
        if (ImGui::IsMouseClicked(button))
        {
            Modality() = false;
            return;
        }
    }
    if (io.MouseWheel != 0.0f || io.MouseWheelH != 0.0f)
    {
        Modality() = false;
        return;
    }
    for (const ImGuiKey key : kModalityKeys)
    {
        if (ImGui::IsKeyPressed(key, false))
        {
            Modality() = true;
            return;
        }
    }
    for (int key = ImGuiKey_Gamepad_BEGIN; key < ImGuiKey_Gamepad_END; ++key)
    {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false))
        {
            Modality() = true;
            return;
        }
    }
}

// The interaction blocks keep a little state between frames (the modality, the edit session in
// progress, a pending forced state); a new start forgets all of it.
void ResetInteraction()
{
    Modality() = false;
    ClearForcedItemState();
    ResetValueDragState();
    ResetMultiHandleState();
}

bool FocusInit()
{
    ResetInteraction();
    return true;
}

void FocusShutdown()
{
    ResetInteraction();
}

void FocusNewFrame()
{
    UpdateModality();
}

void FocusEndFrame()
{
    ClearForcedItemState();
}

} // namespace

bool KeyboardModality()
{
    return Modality();
}

void SetKeyboardModality(bool keyboard)
{
    Modality() = keyboard;
}

ImVec4 FocusRingColor()
{
    const Colors& c = T().colors;
    const ImVec4* surfaces[] = {&c.bg, &c.bgSecondary, &c.bgTertiary, &c.bgElevated,
                                &c.bgElevatedLevels[0], &c.bgElevatedLevels[1], &c.bgElevatedLevels[2]};
    for (const ImVec4* surface : surfaces)
    {
        if (ContrastRatio(c.focusRing, *surface) < kRingContrast)
            return c.accentText;
    }
    return c.focusRing;
}

float FocusRingOutset()
{
    return HairlineThickness() + PxSnapped(T().metrics.focusRingWidth);
}

ImRect FocusRingRect(const ImRect& rect)
{
    ImRect ring = rect;
    ring.Expand(FocusRingOutset());
    return ring;
}

void DrawFocusRing(ImDrawList* drawList, const ImRect& rect, float radius, float amount, ImDrawFlags corners,
                   RingShape shape)
{
    amount = std::clamp(amount, 0.0f, 1.0f);
    if (drawList == nullptr || amount <= 0.0f)
        return;
    const Theme& theme = T();
    const float width = PxSnapped(theme.metrics.focusRingWidth);
    const float offset = FocusRingOutset();
    const ImRect ring = FocusRingRect(rect);
    const float ringRadius = std::max(0.0f, radius) + offset;
    const ImVec4 color = FocusRingColor();
    DrawGlow(drawList, ring.Min, ring.Max, ringRadius, ToU32(WithAlpha(color, color.w * kHaloAlpha * amount)),
             Px(theme.spacing.xs));
    const ImU32 stroke = ToU32(WithAlpha(color, color.w * amount));
    if (shape == RingShape::Round)
        StrokeRoundRect(drawList, ring.Min, ring.Max, stroke, ringRadius, width, corners);
    else
        StrokeRect(drawList, ring.Min, ring.Max, stroke, ringRadius, width, corners);
}

void DrawItemFocusRing(const ItemState& item, float radius, ImDrawFlags corners, RingShape shape)
{
    if (!item.visible || item.focusAmount <= 0.0f || ImGui::GetCurrentContext() == nullptr)
        return;
    DrawFocusRing(ImGui::GetWindowDrawList(), item.rect, radius, item.focusAmount, corners, shape);
}

Subsystem FocusSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "focus";
    subsystem.init = &FocusInit;
    subsystem.shutdown = &FocusShutdown;
    subsystem.newFrame = &FocusNewFrame;
    subsystem.endFrame = &FocusEndFrame;
    return subsystem;
}

} // namespace facet::internal
