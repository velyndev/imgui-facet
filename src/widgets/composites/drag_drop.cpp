// The public drag and drop calls (facet/widgets/composites.h), built on the drag and drop core
// (src/internal/drag_drop.h).
//
// BeginDragSource makes the last item a source of the core: a pointer drag after the start
// threshold, or a keyboard drag picked up with Ctrl+Space while the item has the focus (Space alone
// stays the item's own activation). The core draws the ghost from the options at facet::EndFrame;
// content submitted between BeginDragSource and EndDragSource goes into a floating layer that
// follows the ghost, on the elevated surface with the level 2 shadow, so it never lands in the
// caller's window.
//
// BeginDropTarget makes the last item a target. It returns true while a drag of any type is in
// progress and the item is visible, so that each AcceptDrop declares the item as a zone of the core
// for its type: a keyboard drag can then reach the item before the drag gets there, and a drag of a
// type no AcceptDrop names is refused (the core's not-allowed cursor, no indicator). AcceptDrop draws
// the indicator while a drag of its type is over the item and returns the payload on the drop (with
// peek, also while it hovers). A disabled or read-only item refuses every drag.

#include "facet/widgets/composites.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/drag_drop.h"
#include "internal/scope_stack.h"

namespace facet
{
namespace
{

constexpr const char* kDragSourceScope = "BeginDragSource";
constexpr const char* kDropTargetScope = "BeginDropTarget";

struct PublicDragState
{
    // The open source: its floating layer for extra preview content.
    bool sourceOpen = false;
    bool layerOpen = false;
    ImVec2 layerStart = ImVec2(0.0f, 0.0f);
    // The open target.
    bool targetOpen = false;
    ImGuiID targetId = 0;
    ImRect targetRect;
    bool targetBlocked = false;
};

PublicDragState& PublicDrag()
{
    static PublicDragState state;
    return state;
}

// Where the ghost of the drag in progress is drawn (screen pixels), from the core's drag info.
ImRect GhostRectOf(const internal::DragDropInfo& info)
{
    const ImVec2 size = info.sourceSize.x >= 1.0f ? info.sourceSize : ImVec2(Px(T().metrics.controlHeight), Px(T().metrics.controlHeight));
    const ImVec2 offset = info.keyboard ? ImVec2(size.x * 0.5f, size.y * 0.5f) : info.grabOffset;
    const ImVec2 min(info.pointer.x - offset.x, info.pointer.y - offset.y);
    return ImRect(min, ImVec2(min.x + size.x, min.y + size.y));
}

// Opens the floating layer of the extra preview content under the ghost.
void OpenPreviewLayer(PublicDragState& s)
{
    internal::DragDropInfo info;
    if (!internal::DragDropActive(nullptr, &info))
        return;
    const Theme& t = T();
    const ImRect ghost = GhostRectOf(info);
    const float pad = Px(t.spacing.sm);
    ImGui::SetNextWindowPos(ImVec2(ghost.Min.x, ghost.Max.y + Px(t.spacing.xs)));
    ImGui::SetNextWindowBgAlpha(0.0f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                                        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_Tooltip;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##facet.dragsource.preview", nullptr, kFlags);
    ImGui::PopStyleVar(2);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    // The surface goes under the content once its size is known.
    window->DrawList->ChannelsSplit(2);
    window->DrawList->ChannelsSetCurrent(1);
    s.layerOpen = true;
    s.layerStart = window->DC.CursorPos;
}

void ClosePreviewLayer(PublicDragState& s)
{
    if (!s.layerOpen)
        return;
    s.layerOpen = false;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* drawList = window->DrawList;
    const ImVec2 end = window->DC.CursorMaxPos;
    drawList->ChannelsSetCurrent(0);
    if (end.x > s.layerStart.x && end.y > s.layerStart.y)
    {
        const Theme& t = T();
        const float pad = Px(t.spacing.sm);
        const ImVec2 min(s.layerStart.x - pad, s.layerStart.y - pad);
        const ImVec2 max(end.x + pad, end.y + pad);
        const float radius = Px(t.radii.control);
        DrawElevation(drawList, min, max, radius, 2);
        FillRect(drawList, min, max, ToU32(t.colors.ElevatedSurface(2)), radius);
        StrokeRect(drawList, min, max, ToU32(t.colors.separator), radius, HairlineThickness());
    }
    drawList->ChannelsMerge();
    ImGui::End();
}

// The zone id of the target for `type`: one zone per accepted type over the same item.
ImGuiID TargetZoneId(ImGuiID target, const char* type)
{
    return ImHashStr(type, 0, ImHashStr("##facet.droptarget", 0, target));
}

} // namespace

bool BeginDragSource(const char* type, const void* data, std::size_t size, const DragSourceOptions& options)
{
    if (internal::ControlWindow() == nullptr)
        return false;
    PublicDragState& s = PublicDrag();
    if (s.sourceOpen)
    {
        internal::ReportUsageError("BeginDragSource called again before EndDragSource");
        return false;
    }
    internal::DragDropPreview preview;
    preview.previewText = options.previewText;
    preview.icon = options.icon;
    preview.image = options.image;
    preview.keyboard = true;
    preview.pickUpChord = ImGuiMod_Ctrl | ImGuiKey_Space;
    if (!internal::DragDropBeginSource(type, data, size, preview))
        return false;
    internal::PushScope(kDragSourceScope);
    s.sourceOpen = true;
    OpenPreviewLayer(s);
    return true;
}

void EndDragSource()
{
    if (!internal::PopScope(kDragSourceScope))
        return;
    PublicDragState& s = PublicDrag();
    if (!s.sourceOpen)
        return;
    ClosePreviewLayer(s);
    s.sourceOpen = false;
    internal::DragDropEndSource();
}

bool BeginDropTarget()
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    PublicDragState& s = PublicDrag();
    if (s.targetOpen)
    {
        internal::ReportUsageError("BeginDropTarget called again before EndDropTarget");
        return false;
    }
    const ImGuiLastItemData& last = GImGui->LastItemData;
    if (last.Rect.GetWidth() <= 0.0f || last.Rect.GetHeight() <= 0.0f)
        return false;
    if (!internal::DragDropActive(nullptr, nullptr))
        return false;
    if (!ImGui::IsRectVisible(last.Rect.Min, last.Rect.Max))
        return false;
    s.targetOpen = true;
    s.targetId = last.ID != 0 ? last.ID : ImHashData(&last.Rect, sizeof(last.Rect), window->ID);
    s.targetRect = last.Rect;
    s.targetBlocked = (last.ItemFlags & (static_cast<ImGuiItemFlags>(ImGuiItemFlags_Disabled) | static_cast<ImGuiItemFlags>(ImGuiItemFlags_ReadOnly))) != 0;
    internal::PushScope(kDropTargetScope);
    return true;
}

const ImGuiPayload* AcceptDrop(const char* type, const AcceptDropOptions& options)
{
    PublicDragState& s = PublicDrag();
    if (!s.targetOpen || type == nullptr || type[0] == '\0' || internal::ControlWindow() == nullptr)
        return nullptr;
    internal::DragDropZoneOptions zone;
    zone.accept = !s.targetBlocked;
    const internal::DragDropZoneResult result = internal::DragDropZone(TargetZoneId(s.targetId, type), s.targetRect, type, zone);
    if (result.hovered)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImRect rects[] = {s.targetRect};
        switch (options.indicator)
        {
        case DropIndicator::Box:
            internal::DragDropDrawTarget(drawList, s.targetRect, Px(T().radii.control));
            break;
        case DropIndicator::LineBefore:
        case DropIndicator::LineAfter:
            internal::DragDropDrawInsertLine(
                drawList, internal::DragDropInsertLineRect(rects, options.indicator == DropIndicator::LineBefore ? 0 : 1,
                                                           Orientation::Vertical));
            break;
        case DropIndicator::None:
            break;
        }
    }
    if (result.delivered)
        return result.payload;
    if (options.peek && result.hovered)
        return result.payload;
    return nullptr;
}

void EndDropTarget()
{
    if (!internal::PopScope(kDropTargetScope))
        return;
    PublicDragState& s = PublicDrag();
    s.targetOpen = false;
}

namespace internal
{

void CardsClearPublicDragDrop()
{
    // A Begin left open was reported by the scope stack; the next frame starts clean.
    PublicDrag() = PublicDragState{};
}

} // namespace internal

} // namespace facet
