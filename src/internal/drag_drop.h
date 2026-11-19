#pragma once

// Drag and drop core: typed drags that start from an item, drop zones that may be any
// rectangle (a row, the gap between rows, an empty list), where a drop would land in an ordered
// zone, the drag preview drawn on top of everything at facet::EndFrame, edge scrolling of the
// scroll areas under a drag (through the scroll core, scroll.h), and a keyboard path that works
// with the same zones.
//
// The list core reorders rows through this header, and Phase 2 builds the public drag and drop
// calls of composites.h, the reorderable list, the tree view, the transfer list, the layer list,
// the inventory grid and slots, the swatch palette, the media DropZone and tab reordering on it.
// Every name here starts with DragDrop, so none of them is mistaken for the public widgets
// (facet::DropZone and its options in media.h, BeginDragSource in composites.h).
//
// Payloads are Dear ImGui's: a type string of at most 32 characters (names starting with '_'
// are Dear ImGui's own) and at most kDragDropMaxPayloadSize bytes, copied once when the drag
// starts. So a stock ImGui::BeginDragDropTarget / AcceptDragDropPayload accepts a Facet drag, and
// a Facet zone accepts a stock ImGui::BeginDragDropSource of the same type. A payload holds small
// values (an index, an id) or a caller-owned pointer the caller keeps alive for the drag.
//
// Starting: a press on the source item becomes a drag after drag.startThreshold (design units)
// of pointer travel, measured from the press. A click never starts one. Only the item that
// holds the mouse can start a drag, so while a slider, a text field selecting text or any other
// item holds the mouse, no drag starts from a row behind it; a text field is never a source, and
// an item that drags a value itself (a slider handle) must not be made one.
// Disabled and read-only items (ImGui::BeginDisabled, ItemOptions::disabled and readOnly,
// ImGuiItemFlags_ReadOnly) never start one.
//
// Ending: releasing over a zone that takes the payload delivers it there (that zone reports
// `delivered` on that frame). Escape, a right click, or releasing where no zone takes it ends the
// drag without a drop: DragDropCanceled() is true on that frame, nothing was delivered, and the
// mouse buttons are held back from every other item until they are released, so the release
// clicks nothing. The core never changes the caller's data, so a cancel restores everything by
// itself; entries animated with DragDropAnimatedPosition spring back to their places.
//
// Keyboard: while a source item has the keyboard focus, Space picks it up (DragDropPreview::
// keyboard). The arrow keys then move it between the places it can land: the gaps of every
// ordered zone of its type and every single zone, in the direction of the arrow, scrolling the
// place into view (ScrollAreaRevealRect). Space or Enter drops it there, Escape or a click puts
// it back. Zones report a keyboard drag exactly like a pointer drag (hovered, insertIndex,
// delivered, hoverOpen), so owners draw the same feedback; DragDropZoneResult::keyboard tells
// them apart. Give rows ids made from stable keys (ImGui::PushID(key)), so the keyboard focus
// stays on the moved row after the caller reorders its data.
//
// The preview (the ghost): drawn at facet::EndFrame on the foreground draw list at the pointer,
// offset by where the item was grabbed, the size of the source item, at drag.ghostOpacity, with
// the level 3 shadow. It shows a copy of DragDropPreview taken when the drag starts (text,
// icon, image handle, count badge) or calls DragDropPreview::draw with the payload copy, so it
// never reads the source again: a source row that scrolls away or stops being submitted mid-drag
// leaves the ghost and the drag as they are. A keyboard drag shows the ghost over the place it
// would land, with the focus ring. Drags from stock sources show their own Dear ImGui tooltip.
//
// Feedback is drawn by the zone's owner from the zone result, in the focusRing color, which
// reaches 3:1 on every surface: DragDropDrawInsertLine for a line, DragDropDrawTarget for a
// highlighted target, DragDropGapShift and DragDropAnimatedPosition for rows that move apart.
// A zone of another type, or one that refuses (DragDropZoneOptions::accept false), reports
// `rejected`, takes nothing and shows the not-allowed cursor when no other zone takes the drag;
// its owner draws no highlight.
//
// Edge scrolling: while a pointer drag rests in the drag.autoScrollZone band along the edge of a
// scroll area that holds a zone of its type (or one of that area's parents), or past that edge,
// the innermost such area that can still scroll toward that edge is asked to scroll
// (RequestScrollAreaEdgeScroll; the scroll core sets the speed). Rectangles are screen pixels.
//
// A reorderable list, every frame (rows laid out in data order, then drawn where they move to):
//
//     struct RowPayload { ImGuiID list; int index; };
//     DragDropInfo drag;
//     const bool dragging = DragDropActive("list.row", &drag);
//     for (int i = 0; i < count; ++i)                       // pass 1: items, sources, slots
//     {
//         ImGui::PushID(static_cast<int>(keys[i]));
//         const ItemState row = ItemBehavior(ImGui::GetID("row"), rowSize);
//         slots[i] = row.rect;
//         const RowPayload payload{listId, i};
//         if (DragDropBeginSource("list.row", &payload, sizeof(payload), {.previewText = labels[i]}))
//         {
//             lifted = i;                                   // drawn by the ghost, not in the list
//             DragDropEndSource();
//         }
//         ImGui::PopID();
//     }
//     const DragDropZoneResult drop = DragDropZone(listId, listRect, "list.row", {.slots = slots});
//     const int source = SourceIndexInThisList(drop.payload or drag.payload);   // -1 from elsewhere
//     const float extent = drag.sourceSize.y + spacing;
//     for (int i = 0; i < count; ++i)                       // pass 2: draw
//     {
//         const float target = slots[i].Min.y - origin.y +
//                              DragDropGapShift(i, source, drop.hovered ? drop.insertIndex : -1, extent);
//         const float y = origin.y + DragDropAnimatedPosition(listId, keys[i], target);
//         ... draw row i at y unless it is lifted ...
//     }
//     if (drop.delivered && source >= 0)
//         move = DragDropReorderMove(source, drop.insertIndex);   // the caller applies it
//
// A virtualized list passes the rects of the rows it submitted and adds the index of the first one
// to insertIndex. A list that keeps the dragged row submitted while it scrolls away (ListRowsDesc::
// keepAlive) keeps it a source, but the drag goes on without it too.
//
// Captures: a widget passes a forced Dragging state of its item behaviour on with
// DragDropForceNextSource, and the next source then shows a drag without input (see there).

#include <cstddef>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "facet/core/image.h"
#include "facet/widgets/common.h"
#include "facet/widgets/lists.h"

namespace facet::internal
{

// The largest payload a Facet source carries. Larger data stays with the caller, and the
// payload carries a pointer or an index to it.
inline constexpr std::size_t kDragDropMaxPayloadSize = 64;

// Redraws the dragged item for the ghost into `rect` (screen pixels, the source item's size) of
// `drawList`, reading only `payload` (the copy made when the drag started) and data the payload
// points to. The core draws the ghost surface and shadow first and applies the ghost opacity.
using DragDropDrawFn = void (*)(ImDrawList* drawList, const ImRect& rect, const ImGuiPayload& payload);

// What the drag preview shows, drawn with the drag.ghostOpacity token. Copied when the drag starts.
struct DragDropPreview
{
    const char* previewText = nullptr;  // copied when the drag starts; null for none
    Icon icon;                          // none for none
    ImageHandle image;                  // invalid for none
    int count = 0;                      // items moved together (a multi-selection): above 1 a count badge
    DragDropDrawFn draw = nullptr;      // redraws the item instead of text, icon and image; null for none
    bool keyboard = true;               // Space on the focused item picks it up for a keyboard drag
};

// Makes the last item a drag source for a payload of `type` with `size` bytes at `data` (at most
// kDragDropMaxPayloadSize; `data` may be null only when `size` is 0). Returns true while this
// item's drag is in progress, pointer or keyboard (from the frame the threshold is passed or Space
// picks it up to the frame of the drop or cancel); then call DragDropEndSource. Returns false
// otherwise; call DragDropEndSource only after true. Nothing happens for a disabled or read-only
// item. A type longer than 32 characters or a payload over the limit is a usage error (reported,
// and no drag starts). Calls Dear ImGui's BeginDragDropSource without its preview tooltip; for
// that call alone it sets io.MouseDragThreshold to the start threshold and puts it back.
[[nodiscard]] bool DragDropBeginSource(const char* type, const void* data, std::size_t size,
                                       const DragDropPreview& preview = {});
void DragDropEndSource();

// What a drop would do, from the modifiers held at the moment of the drop (or now, while hovering).
enum class DragDropEffect
{
    Move,   // the default
    Copy,   // Ctrl held, on a zone that allows copies
    Split,  // Shift held, on a zone that allows splitting a stack
};

// The drag in progress.
struct DragDropInfo
{
    const ImGuiPayload* payload = nullptr;  // Dear ImGui's payload: type, data, size, source id
    ImVec2 pointer = {0.0f, 0.0f};          // where the pointer is (screen pixels); a keyboard drag: the
                                            // center of the place it would land
    ImVec2 grabOffset = {0.0f, 0.0f};       // pointer minus the source item's top-left corner when the drag started
    ImVec2 sourceSize = {0.0f, 0.0f};       // the source item's size when the drag started (pixels);
                                            // zero for a stock source
    bool keyboard = false;                  // picked up with Space and moved with the arrow keys
    bool facet = false;                     // started by DragDropBeginSource (a ghost is drawn)
};

// Whether a drag is in progress, of `type` when it is not null. *info (may be null) receives it.
bool DragDropActive(const char* type, DragDropInfo* info);

// How a drop zone takes a payload.
struct DragDropZoneOptions
{
    std::span<const ImRect> slots = {};          // ordered zone: the rects of its entries in order; empty: one target
    Orientation axis = Orientation::Vertical;    // ordered zone: the axis the entries run along
    bool accept = true;                          // false: the zone refuses this drag (a node into its own
                                                 // child): it reports `rejected` and takes nothing
    bool allowCopy = false;                      // Ctrl held reports DragDropEffect::Copy
    bool allowSplit = false;                     // Shift held reports DragDropEffect::Split
    bool wrap = false;                           // ordered zone: the slots wrap into lines across the axis
                                                 // (a swatch or item grid); see DragDropInsertIndexWrapped
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
    bool rejected = false;                  // a drag of another type, or one the zone refuses, is over it: draw no highlight
    DragDropEffect effect = DragDropEffect::Move;  // while hovered or delivered
    bool keyboard = false;                  // the drag is a keyboard drag
};

// Declares a drop zone `id` covering `rect` in the current window for payloads of `type`, for
// this frame. A zone inside another one takes the payload first (the smaller one wins, as in Dear
// ImGui). Hovering is reported from the frame after the pointer enters, when Dear ImGui has picked
// the winning zone. Hovering a scroll area near its edge scrolls it. The zone draws no indicator;
// its owner draws one from the result. Positions in a zone count only where the zone is visible
// (the window's clip rectangle).
DragDropZoneResult DragDropZone(ImGuiID id, const ImRect& rect, const char* type,
                                const DragDropZoneOptions& options = {});

// The same zone taking payloads of any of `types` (the media DropZone): it takes the drag when its
// type is one of them, and reports `rejected` only when it is none of them.
DragDropZoneResult DragDropZone(ImGuiID id, const ImRect& rect, std::span<const char* const> types,
                                const DragDropZoneOptions& options = {});

// Where in an ordered zone a payload at `point` lands: the index of the slot it goes before
// (slots.size() after the last one). A point before the middle of a slot along `axis` goes before
// it. -1 for no slots. Uses the slots as laid out, never as drawn, so rows that move apart under
// the pointer cannot make the index flicker.
int DragDropInsertIndex(std::span<const ImRect> slots, Orientation axis, ImVec2 point);

// The same for slots that wrap into lines (a grid filled line by line along `axis`, the lines
// stacked across it): the line under `point` (or the nearest one) is found across the axis, then
// the slot it goes before along the axis within that line. -1 for no slots.
int DragDropInsertIndexWrapped(std::span<const ImRect> slots, Orientation axis, ImVec2 point);

// True on the frame a drag started by DragDropBeginSource ended without a drop (Escape, a right
// click, a click during a keyboard drag, or a release where no zone took it). An owner undoes what
// it did for the drag alone, such as a node it opened on hoverOpen.
bool DragDropCanceled();

// Ends the drag in progress without a drop, as Escape does. For owners that lose their content
// mid-drag (a list that is closed). Does nothing without a drag.
void DragDropCancel();

// ---- Ordered zones: results and feedback ------------------------------------------------------

// The move a drop at `insertIndex` stands for when the dragged entry came from index `source` of
// the same ordered zone: ReorderMove (lists.h) in the caller's indices. Invalid (from == to) for
// a drop onto the entry's own place (insertIndex source or source + 1) or for a negative index.
// A drop from another zone needs no conversion: insert the item before `insertIndex` (and, for a
// move, remove it from its source); a copy only inserts.
ReorderMove DragDropReorderMove(int source, int insertIndex);

// How far entry `index` of an ordered zone moves along the axis while a drag hovers at
// `insertIndex`, so the entries show the gap where it would land: 0, -extent or +extent.
// `source` is the index of the dragged entry when it comes from this zone (its own place then
// closes and the gap opens at the insertion point), -1 when it comes from elsewhere (entries
// from insertIndex on move by `extent`). `insertIndex` -1 (nothing hovers) gives 0, so the
// dragged entry's place stays open while the pointer is outside the zone. `extent` is the
// dragged item's size along the axis plus the spacing between entries (pixels).
float DragDropGapShift(int index, int source, int insertIndex, float extent);

// The slot entry `index` is drawn in while a drag hovers at `insertIndex`, for zones whose entries
// do not line up on one axis (wrapped grids): index - 1, index or index + 1, by the same rule as
// DragDropGapShift. index + 1 may be one past the last slot (the next place of the grid); animate
// each axis with DragDropAnimatedPosition under its own key.
int DragDropGapSlot(int index, int source, int insertIndex);

// Where the dragged entry would land, as drawn with DragDropGapShift: the open gap (screen
// pixels, `size` long along the axis, the cross size of the slots), which is also where the entry
// is laid out after the caller applies the move. `size` is the dragged item's size along the
// axis; the spacing between entries is read from the slots. An empty rect for no slots or
// insertIndex -1. Owners draw the landing place there; the keyboard ghost sits on it.
ImRect DragDropGapRect(std::span<const ImRect> slots, int source, int insertIndex, Orientation axis, float size);

// The animated position of entry `key` (a stable item id; the index only when the caller never
// reorders) of zone `zoneId` along the axis, in pixels from an origin that moves with the content
// (for a scrolling list the window position minus the scroll, so scrolling does not animate). Pass
// the target every frame for every entry, also when it does not move, so the spring keeps its
// state. A spring of the theme's standard motion, frame-rate independent, that jumps while motion
// is reduced and starts at the target the first time a key is seen. Animating the position rather
// than the offset is what keeps rows still when the caller applies the move: a row's target
// (layout plus gap shift) is the same before and after its data moves. For the lifted entry pass
// the ghost's position as its target, so after the drop it settles from where the ghost was.
float DragDropAnimatedPosition(ImGuiID zoneId, ImGuiID key, float target);

// The insertion line of an ordered zone at `insertIndex`: centered on the boundary before slot
// insertIndex (after the last slot for slots.size()), the cross size of the slots, as thick as the
// focus ring. An empty rect for no slots or a negative index.
ImRect DragDropInsertLineRect(std::span<const ImRect> slots, int insertIndex, Orientation axis);

// The middle half of a tree row along `axis`: the part where a drop goes into the node rather than
// before or after it. Submit it as a zone of its own after the row's zone, so it takes the payload
// first (the smaller zone wins); the rest of the row orders before and after.
ImRect DragDropIntoRect(const ImRect& row, Orientation axis = Orientation::Vertical);

// Draws an insertion line (from DragDropInsertLineRect) in the focusRing color with round ends.
void DragDropDrawInsertLine(ImDrawList* drawList, const ImRect& line);

// Draws the highlight of a target that takes the drag: an accentSubtle fill and a focusRing outline
// as wide as the focus ring, inside `rect`, with corner `radius` (pixels).
void DragDropDrawTarget(ImDrawList* drawList, const ImRect& rect, float radius);

// ---- Captures and tests -----------------------------------------------------------------------

// Captures: makes the next DragDropBeginSource of this frame show its item as dragged with the
// pointer at the item's center plus `pointerOffset` (pixels), without input: the ghost is drawn,
// zones under that point report hovered (never delivered), nothing scrolls. A widget calls it
// when its item behaviour reports ForcedItemState::Dragging, so a page's ForceNextItemState
// request reaches the drag core. The drag lasts while it is requested every frame. Not for apps.
void DragDropForceNextSource(ImVec2 pointerOffset);

// The ghost drawn at the last facet::EndFrame (screen pixels), empty when none was drawn. For tests.
ImRect DragDropGhostRect();

// Whether the last frame's drag asked the scroll area with window id `windowId` to edge scroll.
// For tests and diagnostics: the scroll core decides how far it moves.
bool DragDropRequestedEdgeScroll(ImGuiID windowId);

} // namespace facet::internal
