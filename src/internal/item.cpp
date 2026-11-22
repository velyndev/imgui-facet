// Item behaviour (item.h): one call for ItemSize, ItemAdd and the button behaviour, the forced
// state request, and the widget entry and exit helpers.

#include "internal/item.h"

#include <algorithm>

#include "facet/core/anim.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/focus.h"

namespace facet::internal
{
namespace
{

ForcedItemRequest& Pending()
{
    static ForcedItemRequest request;
    return request;
}

// Sub keys of the animated amounts.
constexpr int kHoverSub = kItemAnimSubBase + 0;
constexpr int kPressSub = kItemAnimSubBase + 1;
constexpr int kFocusSub = kItemAnimSubBase + 2;

bool RequestApplies(const ForcedItemRequest& request, int part)
{
    if (request.state == ForcedItemState::None)
        return false;
    return request.part == part || (request.part < 0 && part == 0);
}

ImGuiButtonFlags PressFlags(PressMode press)
{
    switch (press)
    {
    case PressMode::Click:
        return ImGuiButtonFlags_PressedOnClick;
    case PressMode::Release:
        return ImGuiButtonFlags_PressedOnRelease;
    default:
        return ImGuiButtonFlags_PressedOnClickRelease;
    }
}

ItemState RunBehavior(ImGuiID id, const ImRect& rect, const ItemOptions& options, bool layout)
{
    ImGuiContext& g = *GImGui;
    const Theme& theme = T();

    ItemState state;
    state.id = id;
    state.rect = rect;
    state.hitRect = options.hitTarget ? HitRect(rect, Px(theme.metrics.minHitTarget)) : rect;

    // Taken whether or not the item is visible: one request per widget.
    const ForcedItemRequest forced = options.forced != nullptr ? *options.forced : TakeForcedItemState();

    if (layout)
        ImGui::ItemSize(rect.GetSize(), options.baseline);

    ImGuiItemFlags extra = 0;
    if (options.disabled)
        extra |= ImGuiItemFlags_Disabled;
    if (options.readOnly)
        extra |= ImGuiItemFlags_ReadOnly;
    if (options.mixed)
        extra |= ImGuiItemFlags_MixedValue;
    if (options.repeat)
        extra |= ImGuiItemFlags_ButtonRepeat;
    if (!options.tabStop)
        extra |= ImGuiItemFlags_NoTabStop;
    if (!options.navigable)
        extra |= ImGuiItemFlags_NoNav | ImGuiItemFlags_NoTabStop;
    if (options.allowOverlap)
        extra |= ImGuiItemFlags_AllowOverlap;
    // The grown margin yields to every other item: while the pointer is in the margin and not on
    // the visible rectangle, the item is in Dear ImGui's overlap mode, so a later item whose own
    // rectangle is under the pointer takes the hover (an earlier one already has it). The margin
    // then only catches clicks no neighbour claims, a frame after the pointer arrives.
    if (state.hitRect.Min.x != rect.Min.x || state.hitRect.Min.y != rect.Min.y || state.hitRect.Max.x != rect.Max.x ||
        state.hitRect.Max.y != rect.Max.y)
    {
        const ImVec2 mouse = g.IO.MousePos;
        if (g.ActiveId != id && state.hitRect.Contains(mouse) && !rect.Contains(mouse))
            extra |= ImGuiItemFlags_AllowOverlap;
    }

    // The hit rectangle takes the mouse; the visible one is the navigation rectangle.
    state.visible = ImGui::ItemAdd(state.hitRect, id, &rect, extra);
    const ImGuiItemFlags itemFlags = g.LastItemData.ItemFlags;
    state.disabled = (itemFlags & ImGuiItemFlags_Disabled) != 0;
    state.readOnly = (itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    state.mixed = (itemFlags & ImGuiItemFlags_MixedValue) != 0;

    if (state.visible)
    {
        // A repeating item presses on the click, then again while held.
        const PressMode press = options.repeat && options.press == PressMode::ClickRelease ? PressMode::Click : options.press;
        ImGuiButtonFlags buttonFlags = PressFlags(press) | ImGuiButtonFlags_NoHoveredOnFocus | options.buttonFlags;
        if (!options.navigable)
            buttonFlags |= ImGuiButtonFlags_NoNavFocus;
        bool hovered = false;
        bool held = false;
        state.pressed = ImGui::ButtonBehavior(state.hitRect, id, &hovered, &held, buttonFlags);
        state.hovered = hovered;
        state.held = held;
    }

    // Dear ImGui reports the visible rectangle as the item rectangle.
    g.LastItemData.Rect = rect;

    state.active = g.ActiveId == id;
    state.activated = state.active && g.ActiveIdPreviousFrame != id;
    state.deactivated = !state.active && g.ActiveIdPreviousFrame == id;
    const bool keyboardSource = g.ActiveIdSource == ImGuiInputSource_Keyboard || g.ActiveIdSource == ImGuiInputSource_Gamepad;
    state.keyboardPressed = state.pressed && state.active && keyboardSource;
    state.dragging = state.held && state.active && g.ActiveIdSource == ImGuiInputSource_Mouse;
    state.released = state.deactivated && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    state.focused = !state.disabled && ImGui::IsItemFocused();
    state.focusVisible = state.focused && KeyboardModality();

    if (state.visible && !state.disabled && state.hovered && options.handCursor)
        SetHandCursor();
    if (state.disabled && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        SetNotAllowedCursor();

    if (RequestApplies(forced, options.part))
    {
        state.forced = forced.state;
        if (Has(forced.state, ForcedItemState::Hovered))
            state.hovered = true;
        if (Has(forced.state, ForcedItemState::Pressed))
            state.held = true;
        if (Has(forced.state, ForcedItemState::Dragging))
        {
            state.held = true;
            state.dragging = true;
        }
        if (Has(forced.state, ForcedItemState::Focused))
        {
            state.focused = true;
            state.focusVisible = true;
        }
        if (Has(forced.state, ForcedItemState::Open))
            state.open = true;
    }

    const float duration = theme.motion.durationFast;
    state.hoverAmount = Tween(id, kHoverSub, state.hovered ? 1.0f : 0.0f, duration);
    state.pressAmount = Tween(id, kPressSub, state.held ? 1.0f : 0.0f, duration);
    state.focusAmount = Tween(id, kFocusSub, state.focusVisible ? 1.0f : 0.0f, duration);
    return state;
}

} // namespace

void ForceNextItemState(ForcedItemState state, int part)
{
    ForcedItemRequest& request = Pending();
    request.state = state;
    request.part = part;
}

ForcedItemRequest TakeForcedItemState()
{
    const ForcedItemRequest request = Pending();
    Pending() = {};
    return request;
}

void ClearForcedItemState()
{
    Pending() = {};
}

ImRect HitRect(const ImRect& rect, float minSize)
{
    ImRect hit = rect;
    const float growX = std::max(0.0f, minSize - rect.GetWidth()) * 0.5f;
    const float growY = std::max(0.0f, minSize - rect.GetHeight()) * 0.5f;
    hit.Expand(ImVec2(growX, growY));
    return hit;
}

ItemState ItemBehavior(ImGuiID id, ImVec2 size, const ItemOptions& options)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect rect(pos, ImVec2(pos.x + std::max(0.0f, size.x), pos.y + std::max(0.0f, size.y)));
    return RunBehavior(id, rect, options, true);
}

ItemState PartBehavior(ImGuiID id, const ImRect& rect, const ItemOptions& options)
{
    return RunBehavior(id, rect, options, false);
}

NextItemData TakeNextItemData()
{
    ImGuiContext& g = *GImGui;
    NextItemData next;
    next.data = g.NextItemData;
    next.itemFlags = g.CurrentItemFlags | g.NextItemData.ItemFlagsSet;
    next.hasWidth = (g.NextItemData.HasFlags & ImGuiNextItemDataFlags_HasWidth) != 0;
    next.width = g.NextItemData.Width;
    g.NextItemData.ClearFlags();
    return next;
}

void ApplyNextItemData(const NextItemData& next)
{
    GImGui->NextItemData = next.data;
}

float ResolveItemWidth(float optionsWidth, const NextItemData& next, float defaultWidth)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const float available = ImGui::GetContentRegionAvail().x;
    // Dear ImGui's meaning of a width in pixels: > 0 as is, < 0 from the right edge.
    const auto fromPixels = [&](float width) { return width < 0.0f ? available + width : width; };

    float width = 0.0f;
    if (optionsWidth > 0.0f)
        width = Px(optionsWidth);
    else if (optionsWidth < 0.0f)
        width = available - Px(-optionsWidth);
    else if (next.hasWidth)
        width = fromPixels(next.width);
    else if (window->DC.ItemWidthStack.Size > 0)
        width = fromPixels(window->DC.ItemWidth);
    else
        width = fromPixels(defaultWidth);
    return std::max(1.0f, ImTrunc(width));
}

void FinishWidgetGroup(ImGuiID widgetId)
{
    ImGuiContext& g = *GImGui;
    if (g.LastItemData.ID == 0)
        g.LastItemData.ID = widgetId;
}

void MarkWidgetEdited(ImGuiID id)
{
    ImGuiContext& g = *GImGui;
    // The cases Dear ImGui's own check accepts (the assert in ImGui::MarkItemEdited).
    if (g.DragDropActive || g.ActiveId == id || g.ActiveId == 0 || g.DeactivatedItemData.ID == id ||
        g.ActiveIdPreviousFrame == id || g.NavJustMovedToId != 0 ||
        (g.CurrentMultiSelect != nullptr && g.BoxSelectState.IsActive))
    {
        ImGui::MarkItemEdited(id);
        return;
    }
    g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_EditedInternal;
    if ((g.LastItemData.ItemFlags & ImGuiItemFlags_NoMarkEdited) == 0)
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_Edited;
}

} // namespace facet::internal
