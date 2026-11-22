#pragma once

// Parts shared by the inventory, card and people composites (InventoryGrid, InventoryTile,
// InventorySlot, MediaCard, UserChip, ProfileSwitcher): the look of an item tile, the inventory drag
// source and drop target on the drag and drop core (src/internal/drag_drop.h), and the rules that
// turn a drop into an InventoryResult. Every name carries the module (Inventory or Cards), because
// every group of composites links into the same library.
//
// One tile, the same way in a grid, a standalone tile and a slot:
//
//     const ItemState item = ItemBehavior(id, size);                   // or ItemGridTile
//     InventoryTileLook look;
//     look.disabled = item.disabled;
//     look.lifted = InventoryDragSource({.owner = id, .slot = -1, .item = &item, .type = type});
//     const InventoryTileDrawn drawn = InventoryDrawTile(drawList, item.rect, entry, look);
//     const InventoryDropOutcome drop = InventoryDropTarget({.zoneId = zone, .rect = item.rect, ...});
//     InventoryTileTooltip(item.id, item.rect, item.hovered, item.focusVisible, entry, nullptr);
//
// Drops: a drop on an empty slot moves the stack (Shift held: half of it, a split), on a stack
// with the same non-zero stackKey and room left it merges (Shift held: half), on any other item it
// swaps. A locked target and a read-only or disabled one refuse the drag (the core shows the
// not-allowed cursor and the target no highlight); the place an item came from takes it back
// without an action. The dragged item's stack data (key, room, look) comes from a record that the
// source writes while it is dragged, so the payload stays the public InventoryPayload; a payload
// of the inventory type from a custom source never merges.

#include <cstdint>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "facet/core/image.h"
#include "facet/widgets/composites.h"

namespace facet::internal
{

// ---- The tile ---------------------------------------------------------------------------------

// The tile follows the row rule of DESIGN.md: fill[2] with the controlBorder hairline at rest
// (controlBorderHover by hoverAmount), a selected tile accentSubtle with the accent outline at the
// one border width, a disabled one fill[3] with a separator hairline. No state changes the width of
// the outline, and the tile never draws the focus ring: its widget draws it inside the tile
// (DrawFocusRingInside, focus.h) while the keyboard modality is on.
struct InventoryTileLook
{
    bool disabled = false;      // the disabled colors, the picture faded
    bool selected = false;      // accentSubtle and the accent outline
    // The selection fill is already under the tile (the row block of an item grid draws it), so a
    // selected tile draws only its outline over it instead of a second accentSubtle layer.
    bool selectionFilled = false;
    float hoverAmount = 0.0f;   // 0 to 1: the outline blends toward controlBorderHover
    bool lifted = false;        // the item is being dragged: its place shows it faded
    Icon placeholderIcon;       // drawn faintly while the slot is empty; none for none
};

struct InventoryTileDrawn
{
    bool titleCut = false;      // the title was cut with an ellipsis
};

// The corner radius of a tile, in pixels (radii.control).
float InventoryTileRadius();

// Draws a slot with `item` into `rect` (screen pixels): the slot surface and its boundary, the
// picture (the image with the loading and failed states of the media family, else the icon), the
// cooldown sweep, the count chip, the lock chip, the title cut to the width (left out where the tile
// is too small for it) and the category bar. An empty item (null label) draws the empty slot with
// the placeholder icon.
InventoryTileDrawn InventoryDrawTile(ImDrawList* drawList, const ImRect& rect, const InventoryItem& item,
                                     const InventoryTileLook& look);

// The name of a tile on hover and on visible keyboard focus: the item label and, for a locked item,
// Strings::locked under it; for an empty slot `emptyName` (null: Strings::emptySlot).
void InventoryTileTooltip(ImGuiID id, const ImRect& rect, bool hovered, bool focusVisible, const InventoryItem& item,
                          const char* emptyName);

// ---- Dragging ---------------------------------------------------------------------------------

struct InventorySourceDesc
{
    ImGuiID owner = 0;              // the grid, tile or slot: InventoryPayload::source
    int slot = -1;                  // slot index in a grid, -1 for a tile or slot widget
    const InventoryItem* item = nullptr;
    const char* type = nullptr;     // the payload type
    ImGuiKeyChord pickUp = ImGuiKey_Space;  // the keyboard pick-up chord while the item is focused
    bool forced = false;            // a capture's forced Dragging state (shows the drag without input)
};

// Makes the last item (the tile) the drag source of `desc.item` when it can be dragged (not empty,
// not locked; the core refuses disabled and read-only items). Returns true while it is dragged,
// pointer or keyboard; the caller then draws the tile lifted.
bool InventoryDragSource(const InventorySourceDesc& desc);

// The stack data of the item being dragged, when the drag came from an inventory widget of this
// module; null otherwise (a custom source, or no drag).
struct InventoryDragged
{
    ImGuiID source = 0;
    int slot = -1;
    int count = 0;
    std::uint32_t stackKey = 0;
    int maxStack = 0;
};
const InventoryDragged* InventoryDraggedOf(const ImGuiPayload* payload);

// The InventoryPayload in `payload`, or null when it holds something else.
const InventoryPayload* InventoryPayloadOf(const ImGuiPayload* payload);

struct InventoryTargetDesc
{
    ImGuiID zoneId = 0;             // a zone id of its own, stable across frames
    ImRect rect;                    // screen pixels
    const char* type = nullptr;     // the payload type
    const InventoryItem* item = nullptr;  // what the target holds now
    ImGuiID owner = 0;              // the widget the target belongs to
    int slot = -1;                  // its slot index, -1 for a slot widget
    bool accept = true;             // false: refuses every drag (read-only, disabled)
    bool draw = true;               // draws the target highlight while a drag that it takes hovers it
};

struct InventoryDropOutcome
{
    InventoryResult result;         // the action of a drop this frame; None otherwise
    bool hovered = false;           // a drag it takes is over it
    bool delivered = false;         // something was dropped on it this frame (also onto its own place)
    bool keyboard = false;          // that drag is a keyboard drag
};

// Declares the target as a drop zone for this frame and turns a drop into the result for its
// owner. The zone refuses a locked item.
InventoryDropOutcome InventoryDropTarget(const InventoryTargetDesc& desc);

// What dropping `dragged` (`count` items from `source` and `from`) onto `target` at `to` does, by
// the rules at the top of this file. `split` is a drop with Shift held.
InventoryResult InventoryDropRule(const InventoryPayload& dragged, const InventoryDragged* stack, bool split,
                                  const InventoryItem& target, int to);

// ---- Frame hooks ------------------------------------------------------------------------------

// Forgets a public drag source or drop target left open (drag_drop.cpp), at facet::EndFrame.
void CardsClearPublicDragDrop();

} // namespace facet::internal
