// Per-frame and per-device work of the inventory, card and people composites and the public drag
// and drop calls (InventoryGrid, InventoryTile, InventorySlot, InventoryDetailCard, MediaCard,
// UserChip, ProfileSwitcher, BeginDragSource and the rest of composites.h's drag and drop part).
// The entry is in the subsystem table (src/core/subsystems.cpp) from the start, so the package
// fills its hooks here without touching a shared file. At facet::EndFrame and at shutdown it forgets
// a public drag source or drop target the caller left open.

#include "internal/subsystem.h"
#include "widgets/composites/cards_look.h"

namespace facet::internal
{

Subsystem WidgetsCardsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_cards";
    subsystem.endFrame = &CardsClearPublicDragDrop;
    subsystem.shutdown = &CardsClearPublicDragDrop;
    return subsystem;
}

} // namespace facet::internal
