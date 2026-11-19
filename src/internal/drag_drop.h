#pragma once

// Drag and drop core: typed drags that start from an item, drop zones that may be any
// rectangle (a row, the gap between rows, an empty list), where a drop would land in an ordered
// zone, the drag preview drawn on top of everything at facet::EndFrame, and edge scrolling of
// the scroll area under a drag (through the scroll core, scroll.h).
//
// This header holds what the other blocks call: the list core reorders rows through it, and
// Phase 2 builds the public drag and drop calls of composites.h, the reorderable list, the tree
// view, the inventory, the media DropZone and the tab strips on it. The drag and drop core owns
// the rest and adds it to this file and to further drag_* files. Every name here starts with
// DragDrop, so none of them is mistaken for the public widgets (facet::DropZone and its options
// in media.h, BeginDragSource in composites.h).
//
// Payloads are Dear ImGui's: a type string of at most 32 characters (names starting with '_'
// are Dear ImGui's own) and bytes that are copied when the drag starts, so a stock
// ImGui::BeginDragDropTarget / AcceptDragDropPayload accepts a Facet drag too, and the other way
// round. A press becomes a drag after drag.startThreshold of pointer travel; a held drag that
// rests over a zone for drag.hoverOpenDelay reports it in DragDropZoneResult::hoverOpen. Escape
// cancels a drag like any active edit (rule 8). Rectangles are screen pixels.
//
//     // A row of the list core: source and ordered zone.
//     if (DragDropBeginSource("list.row", &rowIndex, sizeof(rowIndex), {.previewText = label}))
//         DragDropEndSource();
//     ...
//     const DragDropZoneResult drop = DragDropZone(listId, listRect, "list.row", {.slots = rowRects});
//     if (drop.delivered)
//         move = {*static_cast<const int*>(drop.payload->Data), drop.insertIndex};

#include <cstddef>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "facet/core/image.h"
#include "facet/widgets/common.h"

namespace facet::internal
{

// What the drag preview shows next to the pointer, drawn with the drag.ghostOpacity token.
struct DragDropPreview
{
    const char* previewText = nullptr;  // copied when the drag starts; null for none
    Icon icon;                          // none for none
    ImageHandle image;                  // invalid for none
};

// Makes the last item a drag source for a payload of `type` with `size` bytes at `data`.
// Returns true while this item's drag is in progress (from the frame the threshold is passed to
// the frame of the drop or cancel); then call DragDropEndSource. Returns false otherwise; call
// DragDropEndSource only after true. Nothing happens for a disabled or read-only item.
[[nodiscard]] bool DragDropBeginSource(const char* type, const void* data, std::size_t size,
                                       const DragDropPreview& preview = {});
void DragDropEndSource();

// The drag in progress.
struct DragDropInfo
{
    const ImGuiPayload* payload = nullptr;  // Dear ImGui's payload: type, data, size, source id
    ImVec2 pointer = {0.0f, 0.0f};          // where the pointer is (screen pixels)
    ImVec2 grabOffset = {0.0f, 0.0f};       // pointer minus the source item's top-left corner when the drag started
};

// Whether a drag is in progress, of `type` when it is not null. *info (may be null) receives it.
bool DragDropActive(const char* type, DragDropInfo* info);

// How a drop zone takes a payload.
struct DragDropZoneOptions
{
    std::span<const ImRect> slots = {};          // ordered zone: the rects of its entries in order; empty: one target
    Orientation axis = Orientation::Vertical;    // ordered zone: the axis the entries run along
};

// What a drop on a zone would do, for this frame.
struct DragDropZoneResult
{
    bool hovered = false;                   // a payload of the zone's type is over the zone
    bool delivered = false;                 // it was released over the zone on this frame: use it now
    bool hoverOpen = false;                 // it has rested over the zone for drag.hoverOpenDelay (opens a node, tab or group)
    int insertIndex = -1;                   // ordered zone while hovered or delivered: the payload lands before
                                            // slot insertIndex (slots.size(): after the last one); -1 otherwise
    const ImGuiPayload* payload = nullptr;  // while hovered or delivered; valid until the end of the frame
};

// Declares a drop zone `id` covering `rect` in the current window for payloads of `type`, for
// this frame. A zone inside another one takes the payload first. Hovering a scroll area near its
// edge scrolls it. The zone draws no indicator; its owner draws one from the result (in the
// focusRing color, which reaches 3:1 on every surface).
DragDropZoneResult DragDropZone(ImGuiID id, const ImRect& rect, const char* type,
                                const DragDropZoneOptions& options = {});

// Where in an ordered zone a payload at `point` lands: the index of the slot it goes before
// (slots.size() after the last one). A point before the middle of a slot along `axis` goes before
// it. -1 for no slots.
int DragDropInsertIndex(std::span<const ImRect> slots, Orientation axis, ImVec2 point);

} // namespace facet::internal
