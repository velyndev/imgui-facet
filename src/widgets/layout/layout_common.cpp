// Helpers shared by the widgets of the layout family (layout_internal.h).

#include "widgets/layout/layout_internal.h"

#include <algorithm>
#include <cmath>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/focus.h"
#include "internal/state_storage.h"

namespace facet::internal
{
namespace
{

// Where a mouse drag of a divider grabbed it, from the divider's offset.
struct LayoutDividerGrab
{
    float grab = 0.0f;
    bool mouse = false;  // the active session is a mouse drag
};

} // namespace

float LayoutGapPixels(Gap gap)
{
    const Spacing& spacing = T().spacing;
    switch (gap)
    {
    case Gap::None:
        return 0.0f;
    case Gap::Tight:
        return Px(spacing.xs);
    case Gap::Loose:
        return Px(spacing.lg);
    case Gap::Section:
        return Px(spacing.xxl);
    case Gap::Item:
    default:
        return Px(spacing.sm);
    }
}

LayoutWorkRect NarrowLayoutWorkRect(ImGuiWindow* window, float right)
{
    LayoutWorkRect saved;
    if (window == nullptr)
        return saved;
    saved.window = window;
    saved.workMaxX = window->WorkRect.Max.x;
    saved.contentMaxX = window->ContentRegionRect.Max.x;
    window->WorkRect.Max.x = std::min(window->WorkRect.Max.x, right);
    window->ContentRegionRect.Max.x = std::min(window->ContentRegionRect.Max.x, right);
    return saved;
}

void RestoreLayoutWorkRect(const LayoutWorkRect& saved)
{
    if (saved.window == nullptr)
        return;
    saved.window->WorkRect.Max.x = saved.workMaxX;
    saved.window->ContentRegionRect.Max.x = saved.contentMaxX;
}

LayoutDividerInput LayoutDivider(ImGuiID id, const ImRect& band, ImGuiAxis axis, float split, float origin,
                                 const ItemOptions& options)
{
    LayoutDividerInput input;
    ImGuiContext& g = *GImGui;
    const Theme& theme = T();
    const ImGuiIO& io = g.IO;

    ItemOptions itemOptions = options;
    itemOptions.press = PressMode::Click;
    itemOptions.hitTarget = false;
    itemOptions.handCursor = false;
    const ItemState item = PartBehavior(id, band, itemOptions);
    input.item = item;
    const bool real = item.forced == ForcedItemState::None;

    LayoutDividerGrab& grab = GetState<LayoutDividerGrab>(id);
    if (real && item.activated && ImGui::IsMouseDown(ImGuiMouseButton_Left) && item.hovered)
    {
        grab.grab = io.MousePos[axis] - (origin + split);
        grab.mouse = true;
        input.started = true;
    }
    if (!item.active)
        grab.mouse = false;
    if (real && item.active && grab.mouse)
    {
        input.dragging = true;
        input.dragTarget = io.MousePos[axis] - origin - grab.grab;
        ImGui::SetKeyOwner(ImGuiKey_Escape, id);
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
        {
            input.canceled = true;
            input.dragging = false;
            grab.mouse = false;
            ImGui::ClearActiveID();
        }
    }
    if (!item.disabled && (item.hovered || input.dragging))
        ImGui::SetMouseCursor(axis == ImGuiAxis_X ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    if (real && (item.hovered || item.active) && !item.disabled && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
        // The second press of a double click resets; it does not start a drag.
        input.reset = true;
        input.dragging = false;
        input.started = false;
        grab.mouse = false;
        if (item.active)
            ImGui::ClearActiveID();
    }

    // Keys while the divider has the keyboard focus.
    if (real && item.focused && g.NavId == id && !item.disabled && !input.dragging)
    {
        const ImGuiKey decKey = axis == ImGuiAxis_X ? ImGuiKey_LeftArrow : ImGuiKey_UpArrow;
        const ImGuiKey incKey = axis == ImGuiAxis_X ? ImGuiKey_RightArrow : ImGuiKey_DownArrow;
        const ImGuiKey keys[] = {decKey, incKey, ImGuiKey_Home, ImGuiKey_End};
        for (const ImGuiKey key : keys)
            ImGui::SetKeyOwner(key, id);
        const int inc = ImGui::IsKeyPressed(incKey, ImGuiInputFlags_Repeat, id) ? 1 : 0;
        const int dec = ImGui::IsKeyPressed(decKey, ImGuiInputFlags_Repeat, id) ? 1 : 0;
        input.steps = inc - dec;
        input.home = ImGui::IsKeyPressed(ImGuiKey_Home, ImGuiInputFlags_None, id);
        input.end = ImGui::IsKeyPressed(ImGuiKey_End, ImGuiInputFlags_None, id);
        input.toggle = item.pressed && item.keyboardPressed;
        if (input.steps != 0 || input.home || input.end)
            SetKeyboardModality(true);
    }
    if (io.KeyCtrl)
        input.stepPixels = Px(theme.spacing.xxxl) * kLayoutDividerCoarseSteps;
    else if (io.KeyShift)
        input.stepPixels = Px(theme.spacing.xs);
    else
        input.stepPixels = Px(theme.spacing.lg);
    return input;
}

void DrawLayoutDivider(const ItemState& item, const ImRect& band, ImGuiAxis axis)
{
    if (!item.visible)
        return;
    const Theme& theme = T();
    const Colors& c = theme.colors;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 center = band.GetCenter();
    const float hairline = HairlineThickness();
    if (axis == ImGuiAxis_X)
        HairlineV(drawList, std::round(center.x - hairline * 0.5f), band.Min.y, band.Max.y, ToU32(c.separator));
    else
        HairlineH(drawList, band.Min.x, band.Max.x, std::round(center.y - hairline * 0.5f), ToU32(c.separator));

    // The grip: the part of the divider that reaches 3:1, so it reads as something to drag.
    const float thickness = Px(theme.spacing.xs);
    const float bandLength = axis == ImGuiAxis_X ? band.GetHeight() : band.GetWidth();
    const float length = std::min(Px(theme.spacing.xxl), std::max(0.0f, bandLength - Px(theme.spacing.xs) * 2.0f));
    if (length > 0.0f)
    {
        const ImVec2 half = axis == ImGuiAxis_X ? ImVec2(thickness * 0.5f, length * 0.5f) : ImVec2(length * 0.5f, thickness * 0.5f);
        const ImVec2 gripMin = Snap(ImVec2(center.x - half.x, center.y - half.y));
        const ImVec2 gripMax(gripMin.x + half.x * 2.0f, gripMin.y + half.y * 2.0f);
        const float lift = item.disabled ? 0.0f : std::max({item.hoverAmount, item.pressAmount, item.focusAmount});
        ImVec4 grip = Mix(c.scrollThumb, c.scrollThumbHover, lift);
        if (item.disabled)
            grip = c.labelTertiary;
        FillRect(drawList, gripMin, gripMax, ToU32(grip), thickness * 0.5f);
    }
    ItemState ring = item;
    ring.rect = band;
    DrawItemFocusRing(ring, Px(theme.radii.element));
}

} // namespace facet::internal
