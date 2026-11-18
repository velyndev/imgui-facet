// InventorySlot (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

InventoryResult InventorySlot(const char* label, const InventoryItem&, const InventorySlotOptions& options)
{
    namespace stub = internal::content;
    const float slot = options.slotSize > 0.0f ? Px(options.slotSize) : stub::Spans(stub::kSlotSpans);
    stub::Stub("InventorySlot", label, stub::ContentWidth("InventorySlot", slot), slot);
    return {};
}

} // namespace facet