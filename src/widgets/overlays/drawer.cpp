// BeginDrawer and EndDrawer (overlays.h): a panel that slides in from an edge of the display.
//
// A modal drawer is a Modal level of the popup host (popup_host.h): the scrim, the page taking no
// input, the keyboard focus kept inside and given back when it closes, Escape in the host's order. A
// drawer with modal false is a Panel level: no scrim, the page stays usable beside it, a click there
// leaves it open, and Escape closes it in its place of the host's order (popup_host.h). Either way it sits flush with its edge over the whole display side (a fixed rect, so it
// follows a window resize at once), with the window radius on its two inner corners, and slides in
// from that edge (motion.popupOpen; at once under reduced motion).
//
// Layout: the title (Title3) and, when it is dismissible, the close button on one row, then the
// content in a scroll area that fills the rest (overlay_parts.h), with the overlay bars.
// Gestures: with resizable, a band of layout.dividerBand along the inner edge drags the extent
// (kept under the id in units, so it holds at another scale); while dismissible, a drag on the title
// row toward the edge moves the drawer with the pointer, and a release past a third of the extent or
// faster than drag.flickSpeed closes it; otherwise it springs back.

#include "facet/widgets/overlays.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/scope_stack.h"
#include "internal/state_storage.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet
{
namespace
{

constexpr int kDrawerDepth = 8;
// The widest a drawer may be dragged, as a share of the display side it covers.
constexpr float kDrawerMaxShare = 0.9f;
// The share of its extent a drag of the title row toward the edge must reach to close the drawer.
constexpr float kDrawerFlickShare = 1.0f / 3.0f;

struct DrawerState
{
    float extentUnits = 0.0f;  // the extent after a resize, design units; 0 until resized
    float pull = 0.0f;         // pixels the title row drag moved the drawer toward its edge
    float pullVelocity = 0.0f; // pixels per second toward the edge while dragged
    float pressAt = 0.0f;      // pointer coordinate along the drag axis where the drag started
    float resizeFrom = 0.0f;   // the extent in pixels when a resize drag started
    bool pulling = false;
};

struct DrawerEntry
{
    ImGuiID level = 0;
    bool* open = nullptr;
    bool dismissible = false;
    bool scrolled = false;
    ImRect titleRow;
};

struct DrawerStack
{
    DrawerEntry entries[kDrawerDepth];
    int depth = 0;
};

DrawerStack& Stack()
{
    static DrawerStack stack;
    return stack;
}

bool Horizontal(DrawerSide side)
{
    return side == DrawerSide::End || side == DrawerSide::Start;
}

// +1 when moving toward the drawer's edge increases the coordinate along its axis, -1 otherwise.
float TowardEdge(DrawerSide side)
{
    return side == DrawerSide::End || side == DrawerSide::Bottom ? 1.0f : -1.0f;
}

ImRect DrawerRect(DrawerSide side, const ImRect& work, float extent, float pull)
{
    const float shift = pull * TowardEdge(side);
    switch (side)
    {
    case DrawerSide::End:
        return ImRect(work.Max.x - extent + shift, work.Min.y, work.Max.x + shift, work.Max.y);
    case DrawerSide::Start:
        return ImRect(work.Min.x + shift, work.Min.y, work.Min.x + extent + shift, work.Max.y);
    case DrawerSide::Bottom:
        return ImRect(work.Min.x, work.Max.y - extent + shift, work.Max.x, work.Max.y + shift);
    case DrawerSide::Top:
        return ImRect(work.Min.x, work.Min.y + shift, work.Max.x, work.Min.y + extent + shift);
    }
    return work;
}

ImDrawFlags InnerCorners(DrawerSide side)
{
    switch (side)
    {
    case DrawerSide::End:
        return ImDrawFlags_RoundLeft;
    case DrawerSide::Start:
        return ImDrawFlags_RoundRight;
    case DrawerSide::Bottom:
        return ImDrawFlags_RoundTop;
    case DrawerSide::Top:
        return ImDrawFlags_RoundBottom;
    }
    return 0;
}

internal::PopupSide SlideEdge(DrawerSide side)
{
    using internal::PopupSide;
    switch (side)
    {
    case DrawerSide::End:
        return PopupSide::After;
    case DrawerSide::Start:
        return PopupSide::Before;
    case DrawerSide::Bottom:
        return PopupSide::Below;
    case DrawerSide::Top:
        return PopupSide::Above;
    }
    return PopupSide::After;
}

// The resize band along the inner edge: drags the extent, with the resize cursor while it is hovered.
void DrawerResizeBand(ImGuiID level, DrawerSide side, const ImRect& rect, DrawerState& state, float extent,
                      float minExtent, float maxExtent)
{
    using namespace internal;
    const Theme& theme = T();
    const float band = Px(theme.layout.dividerBand);
    ImRect area;
    switch (side)
    {
    case DrawerSide::End:
        area = ImRect(rect.Min.x, rect.Min.y, rect.Min.x + band, rect.Max.y);
        break;
    case DrawerSide::Start:
        area = ImRect(rect.Max.x - band, rect.Min.y, rect.Max.x, rect.Max.y);
        break;
    case DrawerSide::Bottom:
        area = ImRect(rect.Min.x, rect.Min.y, rect.Max.x, rect.Min.y + band);
        break;
    case DrawerSide::Top:
        area = ImRect(rect.Min.x, rect.Max.y - band, rect.Max.x, rect.Max.y);
        break;
    }
    ItemOptions options;
    options.press = PressMode::Click;
    options.navigable = false;  // a mouse affordance, like the scroll bars
    options.hitTarget = false;
    options.handCursor = false;
    const ItemState item = PartBehavior(ImHashStr("##facet.resize", 0, level), area, options);
    const bool horizontal = Horizontal(side);
    if (item.hovered || item.held)
        ImGui::SetMouseCursor(horizontal ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_ResizeNS);
    const ImGuiIO& io = ImGui::GetIO();
    const float pointer = horizontal ? io.MousePos.x : io.MousePos.y;
    if (item.activated)
    {
        state.pressAt = pointer;
        state.resizeFrom = extent;
    }
    if (item.held)
    {
        // Moving away from the edge grows the drawer.
        const float grown = state.resizeFrom - (pointer - state.pressAt) * TowardEdge(side);
        state.extentUnits = std::clamp(grown, minExtent, maxExtent) / Px(1.0f);
    }
    if (item.hovered || item.held)
    {
        // A short grip on the edge shows where it drags.
        const float length = Px(theme.spacing.xxxl);
        const float thickness = Px(theme.metrics.sliderTrackHeight);
        const ImVec2 c = horizontal ? ImVec2(TowardEdge(side) > 0.0f ? rect.Min.x + band * 0.5f : rect.Max.x - band * 0.5f,
                                             rect.GetCenter().y)
                                    : ImVec2(rect.GetCenter().x,
                                             TowardEdge(side) > 0.0f ? rect.Min.y + band * 0.5f : rect.Max.y - band * 0.5f);
        const ImVec2 half = horizontal ? ImVec2(thickness * 0.5f, length * 0.5f) : ImVec2(length * 0.5f, thickness * 0.5f);
        FillRect(ImGui::GetWindowDrawList(), ImVec2(c.x - half.x, c.y - half.y), ImVec2(c.x + half.x, c.y + half.y),
                 ToU32(item.held ? theme.colors.labelSecondary : theme.colors.labelTertiary), thickness * 0.5f);
    }
}

} // namespace

namespace internal
{

void OverlayDrawerResetStack()
{
    Stack().depth = 0;
}

} // namespace internal

bool BeginDrawer(const char* label, bool* open, const DrawerOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const ForcedItemRequest forced = TakeForcedItemState();
    const Theme& theme = T();
    const ImGuiID level = window->GetID(label);
    DrawerState& state = GetState<DrawerState>(level);

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImRect work(viewport->WorkPos, ImVec2(viewport->WorkPos.x + viewport->WorkSize.x,
                                                viewport->WorkPos.y + viewport->WorkSize.y));
    const bool horizontal = Horizontal(options.side);
    const float side = horizontal ? work.GetWidth() : work.GetHeight();
    const float minExtent = std::min(Px(theme.layout.minRowControl), side);
    const float maxExtent = std::max(minExtent, side * kDrawerMaxShare);
    const float defaultUnits = options.extent > 0.0f ? options.extent
                                                     : (horizontal ? theme.overlay.drawerWidth : theme.overlay.drawerHeight);
    const float units = options.resizable && state.extentUnits > 0.0f ? state.extentUnits : defaultUnits;
    const float extent = std::clamp(Px(units), minExtent, maxExtent);
    if (open == nullptr || !*open)
        state.pull = 0.0f;
    const ImRect rect = DrawerRect(options.side, work, extent, std::max(0.0f, state.pull));

    PopupOptions popup;
    popup.kind = options.modal ? PopupKind::Modal : PopupKind::Panel;
    popup.closeOnEscape = options.dismissible;
    popup.size = rect.GetSize();
    popup.placeRect = rect;
    popup.slideIn = true;
    popup.slideEdge = SlideEdge(options.side);
    popup.corners = InnerCorners(options.side);
    popup.radius = Px(theme.radii.window);
    popup.elevation = 3;
    const bool forcedOpen = Has(forced.state, ForcedItemState::Open);
    if (forcedOpen)
        ForcePopupLevelOpen(level, {}, popup);
    const OverlayLayerSync sync = OverlaySyncLayer(level, open, {}, popup);
    if (sync.open || forcedOpen)
        SetPopupLevelLayout(level, rect.GetSize(), rect);
    if (sync.open && options.dismissible && options.modal && PopupLevelScrimClicked(level))
    {
        ClosePopupLevel(level);
        *open = false;
        return false;
    }
    if (!BeginPopupLevel(level))
        return false;

    DrawerStack& stack = Stack();
    if (stack.depth >= kDrawerDepth)
    {
        ReportUsageError("BeginDrawer nested deeper than %d", kDrawerDepth);
        EndPopupLevel();
        return false;
    }
    ImGuiWindow* drawer = GImGui->CurrentWindow;
    const ImVec2 origin = drawer->DC.CursorPos;
    const float padding = Px(theme.spacing.xl);
    const float inner = std::max(1.0f, rect.GetWidth() - padding * 2.0f);
    const float titleHeight = OverlayTitleHeight();
    DrawerEntry& entry = stack.entries[stack.depth++];
    entry = DrawerEntry{};
    entry.level = level;
    entry.open = open;
    entry.dismissible = options.dismissible;
    entry.titleRow = ImRect(origin, ImVec2(origin.x + inner, origin.y + titleHeight));

    const bool input = !forcedOpen && sync.open;
    if (input && options.resizable)
        DrawerResizeBand(level, options.side, rect, state, extent, minExtent, maxExtent);

    // The pull: a drag on the title row toward the edge.
    if (input && options.dismissible)
    {
        ItemOptions pullOptions;
        pullOptions.press = PressMode::Click;
        pullOptions.navigable = false;
        pullOptions.hitTarget = false;
        pullOptions.allowOverlap = true;
        pullOptions.handCursor = false;
        const ItemState pull = PartBehavior(ImHashStr("##facet.pull", 0, level), entry.titleRow, pullOptions);
        const ImGuiIO& io = ImGui::GetIO();
        const float pointer = horizontal ? io.MousePos.x : io.MousePos.y;
        const float toward = TowardEdge(options.side);
        const float dt = std::max(DeltaTime(), 1.0e-4f);
        if (pull.activated)
        {
            state.pulling = true;
            state.pressAt = pointer;
            state.pullVelocity = 0.0f;
        }
        if (state.pulling && pull.held)
        {
            const float moved = std::max(0.0f, (pointer - state.pressAt) * toward);
            const float delta = (horizontal ? io.MouseDelta.x : io.MouseDelta.y) * toward / dt;
            state.pullVelocity += (delta - state.pullVelocity) * std::min(1.0f, dt / 0.05f);
            state.pull = moved;
        }
        else if (state.pulling)
        {
            state.pulling = false;
            if (state.pull > extent * kDrawerFlickShare || state.pullVelocity > Px(theme.drag.flickSpeed))
            {
                ClosePopupLevel(level);
                *open = false;
            }
        }
        if (!state.pulling)
        {
            // Back into place on release.
            state.pull = ReduceMotion() ? 0.0f
                                        : state.pull * std::exp(-dt / std::max(1.0e-3f, theme.motion.durationFast));
            if (state.pull < 0.5f)
                state.pull = 0.0f;
        }
    }

    const float closeRoom = options.dismissible ? ControlHeight(ControlSize::Small) + Px(theme.spacing.sm) : 0.0f;
    DrawOverlayTitle(level, label, origin, inner - closeRoom);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + titleHeight + Px(theme.spacing.md)));
    OverlayScrollDesc scroll;
    scroll.fill = true;
    scroll.surface = theme.colors.ElevatedSurface(3);
    entry.scrolled = BeginOverlayScroll(ImHashStr("##facet.drawer", 0, level), scroll);
    PushScope("BeginDrawer");
    return true;
}

void EndDrawer()
{
    using namespace internal;
    if (!PopScope("BeginDrawer"))
        return;
    DrawerStack& stack = Stack();
    if (stack.depth <= 0)
        return;
    const DrawerEntry entry = stack.entries[--stack.depth];
    if (entry.scrolled)
        EndOverlayScroll();
    if (entry.dismissible && OverlayCloseButton(entry.level, entry.titleRow.Max.x, entry.titleRow.GetCenter().y))
    {
        ClosePopupLevel(entry.level);
        if (entry.open != nullptr)
            *entry.open = false;
    }
    EndPopupLevel();
}

} // namespace facet
