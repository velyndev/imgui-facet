// BeginPopover and EndPopover (overlays.h): a floating card next to the last item or the pointer.
//
// The card is a Panel level of the popup host (popup_host.h): it blocks nothing, so the page around
// it stays hoverable and clickable. While it is dismissible, Escape closes it (before the levels
// that were open when it opened) and a press outside it and its anchor closes it and still reaches
// what was pressed; a press on the anchor is left to the anchor, so a button that toggles the flag
// closes it. The host places it on the chosen side, flips it to the other side and moves it into the
// display where it does not fit, follows the anchor while it scrolls, plays the open and close
// motion, draws the blurred surface and moves the keyboard focus in and back. This file maps the
// options, keeps the open flag (overlay_parts.h), opens on hover, draws the arrow toward the anchor
// and puts the content into a scroll area once it outgrows the display.

#include "facet/widgets/overlays.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet
{
namespace
{

struct PopoverState
{
    ImVec2 mousePoint;           // PopoverAnchor::Mouse: the pointer when it opened
    bool wasOpen = false;        // the open flag of the last frame
    bool hoverOpened = false;    // openOnHover wrote the flag: leaving closes it again
    double hoverStart = -1.0;    // when the pointer came to rest on the anchor; < 0 while it is elsewhere
    internal::PopupSide autoSide = internal::PopupSide::Below;  // Auto: the side taken when it opened
};

constexpr int kPopoverDepth = 8;

struct PopoverStack
{
    ImGuiID levels[kPopoverDepth] = {};
    bool scrolled[kPopoverDepth] = {};
    int depth = 0;
};

PopoverStack& Stack()
{
    static PopoverStack stack;
    return stack;
}

// Room in pixels on each side of `anchor` inside the work area, past the gap.
struct PopoverRoom
{
    float below = 0.0f;
    float above = 0.0f;
    float after = 0.0f;
    float before = 0.0f;
};

PopoverRoom RoomAround(const ImRect& anchor, const ImRect& work, float gap)
{
    PopoverRoom room;
    room.below = std::max(0.0f, work.Max.y - anchor.Max.y - gap);
    room.above = std::max(0.0f, anchor.Min.y - work.Min.y - gap);
    room.after = std::max(0.0f, work.Max.x - anchor.Max.x - gap);
    room.before = std::max(0.0f, anchor.Min.x - work.Min.x - gap);
    return room;
}

// Auto: below or above, whichever has more room, unless neither holds two control heights and a
// side beside the anchor has more.
internal::PopupSide AutoSide(const PopoverRoom& room)
{
    using internal::PopupSide;
    const float vertical = std::max(room.below, room.above);
    const float horizontal = std::max(room.after, room.before);
    const float least = Px(T().metrics.controlHeight) * 2.0f;
    if (vertical < least && horizontal > vertical)
        return room.after >= room.before ? PopupSide::After : PopupSide::Before;
    return room.below >= room.above ? PopupSide::Below : PopupSide::Above;
}

internal::PopupSide SideOf(PopoverPlacement placement, internal::PopupSide autoSide)
{
    using internal::PopupSide;
    switch (placement)
    {
    case PopoverPlacement::Below:
        return PopupSide::Below;
    case PopoverPlacement::Above:
        return PopupSide::Above;
    case PopoverPlacement::Start:
        return PopupSide::Before;
    case PopoverPlacement::End:
        return PopupSide::After;
    case PopoverPlacement::Auto:
        break;
    }
    return autoSide;
}

// The arrow: a triangle from the card's edge toward the anchor, its base twice overlay.arrowSize,
// in the card's surface color with the hairline along its two slanted sides. Drawn over the
// viewport so it reaches past the card's own window.
void DrawPopoverArrow(ImDrawList* drawList, const ImRect& card, const ImRect& anchor, internal::PopupSide side,
                      float radius, bool glass)
{
    using internal::PopupSide;
    const Theme& theme = T();
    const float size = Px(theme.overlay.arrowSize);
    if (size <= 0.0f || card.Overlaps(anchor))
        return;
    const bool vertical = side == PopupSide::Below || side == PopupSide::Above;
    const float lo = (vertical ? card.Min.x : card.Min.y) + radius + size;
    const float hi = (vertical ? card.Max.x : card.Max.y) - radius - size;
    if (hi < lo)
        return;
    const float center = std::round(std::clamp(vertical ? anchor.GetCenter().x : anchor.GetCenter().y, lo, hi));
    const float hairline = HairlineThickness();
    ImVec2 tip;
    ImVec2 a;
    ImVec2 b;
    switch (side)
    {
    case PopupSide::Below:
        tip = ImVec2(center, card.Min.y - size);
        a = ImVec2(center - size, card.Min.y + hairline);
        b = ImVec2(center + size, card.Min.y + hairline);
        break;
    case PopupSide::Above:
        tip = ImVec2(center, card.Max.y + size);
        a = ImVec2(center + size, card.Max.y - hairline);
        b = ImVec2(center - size, card.Max.y - hairline);
        break;
    case PopupSide::After:
        tip = ImVec2(card.Min.x - size, center);
        a = ImVec2(card.Min.x + hairline, center + size);
        b = ImVec2(card.Min.x + hairline, center - size);
        break;
    case PopupSide::Before:
        tip = ImVec2(card.Max.x + size, center);
        a = ImVec2(card.Max.x - hairline, center - size);
        b = ImVec2(card.Max.x - hairline, center + size);
        break;
    }
    ImVec4 fill = theme.colors.ElevatedSurface(2);
    if (glass)
    {
        fill = theme.colors.glassTint;
        if (GlassAvailable())
            fill.w *= theme.glass.tintOpacity;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    drawList->PushClipRect(viewport->Pos, ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
                           false);
    drawList->AddTriangleFilled(a, tip, b, ToU32(fill));
    const ImU32 line = ToU32(theme.colors.separator);
    drawList->AddLine(a, tip, line, hairline);
    drawList->AddLine(tip, b, line, hairline);
    drawList->PopClipRect();
}

} // namespace

namespace internal
{

void OverlayPopoverResetStack()
{
    Stack().depth = 0;
}

} // namespace internal

bool BeginPopover(const char* id, bool* open, const PopoverOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const ForcedItemRequest forced = TakeForcedItemState();
    const ImGuiContext& g = *GImGui;
    const Theme& theme = T();
    const ImGuiID level = window->GetID(id);
    PopoverState& state = GetState<PopoverState>(level);

    // The anchor: the last item (or the cursor when there is none), or the pointer when it opened.
    ImRect item = g.LastItemData.Rect;
    if (item.GetWidth() <= 0.0f && item.GetHeight() <= 0.0f)
        item = ImRect(window->DC.CursorPos, window->DC.CursorPos);
    const bool opening = open != nullptr && *open && !state.wasOpen;
    if (opening)
        state.mousePoint = ImGui::GetIO().MousePos;
    ImRect anchor = options.anchor == PopoverAnchor::Mouse ? ImRect(state.mousePoint, state.mousePoint) : item;
    const float gap = Px(theme.overlay.anchorGap);
    const float arrowReach = options.arrow ? std::max(0.0f, Px(theme.overlay.arrowSize) - gap) : 0.0f;
    ImRect placed = anchor;
    placed.Expand(arrowReach);  // the host keeps anchorGap from this rect: the arrow fits in between

    const ImRect work = OverlayWorkArea();
    const PopoverRoom room = RoomAround(placed, work, gap);
    if (opening || !state.wasOpen)
        state.autoSide = AutoSide(room);
    const PopupSide side = SideOf(options.placement, state.autoSide);

    // Open on hover: resting on the anchor for overlay.submenuDelay opens it; leaving the anchor and
    // the card (and the way between them) closes what hover opened.
    if (options.openOnHover && open != nullptr)
    {
        const bool anchorHovered = options.anchor == PopoverAnchor::LastItem &&
                                   ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
        const double now = ImGui::GetTime();
        if (anchorHovered && !*open)
        {
            if (state.hoverStart < 0.0)
                state.hoverStart = now;
            const double delay = theme.overlay.submenuDelay * std::max(0.0f, theme.motion.scale);
            if (now - state.hoverStart >= delay)
            {
                *open = true;
                state.hoverOpened = true;
                state.autoSide = AutoSide(room);
            }
        }
        else if (!anchorHovered)
        {
            state.hoverStart = -1.0;
        }
        if (*open && state.hoverOpened && !anchorHovered)
        {
            ImRect way = PopupLevelRect(level);
            const ImVec2 pointer = ImGui::GetIO().MousePos;
            if (way.GetWidth() > 0.0f)
                way.Add(anchor);
            if (way.GetWidth() <= 0.0f || !way.Contains(pointer))
                *open = false;
        }
        if (!*open)
            state.hoverOpened = false;
    }

    const float padding = Px(theme.spacing.md);
    OverlayScrollDesc scroll;
    scroll.width = options.width > 0.0f ? std::max(1.0f, Px(options.width) - padding * 2.0f) : 0.0f;
    scroll.maxWidth = std::max(1.0f, work.GetWidth() - padding * 2.0f);
    const bool vertical = side == PopupSide::Below || side == PopupSide::Above;
    const float sideRoom = side == PopupSide::Below ? room.below : room.above;
    scroll.maxHeight = std::max(Px(theme.metrics.controlHeight), (vertical ? sideRoom : work.GetHeight()) - padding * 2.0f);
    scroll.surface = theme.colors.ElevatedSurface(2);
    scroll.edgeFades = false;  // over the blurred surface a solid fade would show
    const ImGuiID content = ImHashStr("##facet.popover", 0, level);
    const ImVec2 area = OverlayScrollSize(content, scroll);
    const ImVec2 size(area.x + padding * 2.0f, area.y + padding * 2.0f);

    PopupAnchor at;
    at.rect = placed;
    at.side = side;
    at.align = PopupAlign::Center;
    PopupOptions popup;
    popup.kind = PopupKind::Panel;
    popup.takeFocus = !state.hoverOpened;
    popup.glass = true;
    popup.size = size;
    popup.padding = ImVec2(padding, padding);
    popup.closeOnEscape = options.dismissible;
    popup.closeOnClickOutside = options.dismissible;

    if (Has(forced.state, ForcedItemState::Open))
        ForcePopupLevelOpen(level, at, popup);
    const OverlayLayerSync sync = OverlaySyncLayer(level, open, at, popup);
    state.wasOpen = open != nullptr && *open;
    if (sync.dismissed)
        state.hoverOpened = false;
    if (sync.open || Has(forced.state, ForcedItemState::Open))
    {
        SetPopupLevelAnchor(level, at);
        SetPopupLevelLayout(level, size, ImRect());
    }
    if (!BeginPopupLevel(level))
        return false;

    DrawPopoverArrow(GImGui->CurrentWindow->DrawList, PopupLevelRect(level), anchor, PopupLevelSide(level),
                     Px(theme.radii.popover), true);
    PopoverStack& stack = Stack();
    if (stack.depth >= kPopoverDepth)
    {
        ReportUsageError("BeginPopover nested deeper than %d", kPopoverDepth);
        EndPopupLevel();
        return false;
    }
    const bool scrolled = BeginOverlayScroll(content, scroll);
    stack.levels[stack.depth] = level;
    stack.scrolled[stack.depth] = scrolled;
    ++stack.depth;
    PushScope("BeginPopover");
    return true;
}

void EndPopover()
{
    using namespace internal;
    if (!PopScope("BeginPopover"))
        return;
    PopoverStack& stack = Stack();
    if (stack.depth <= 0)
        return;
    --stack.depth;
    if (stack.scrolled[stack.depth])
        EndOverlayScroll();
    EndPopupLevel();
}

} // namespace facet
