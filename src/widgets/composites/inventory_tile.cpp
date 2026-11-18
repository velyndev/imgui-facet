// InventoryTile (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool InventoryTile(const char* id, const InventoryItem&, const InventoryTileOptions& options)
{
    namespace stub = internal::content;
    const float slot = options.slotSize > 0.0f ? Px(options.slotSize) : stub::Spans(stub::kSlotSpans);
    stub::Stub("InventoryTile", id, stub::ContentWidth("InventoryTile", slot), slot);
    return false;
}

} // namespace facet