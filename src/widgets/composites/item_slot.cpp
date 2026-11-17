// ItemSlot (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

InventoryResult ItemSlot(const char* label, const InventoryItem&, const ItemSlotOptions& options)
{
    namespace stub = internal::content;
    const float slot = options.slotSize > 0.0f ? Px(options.slotSize) : stub::Spans(stub::kSlotSpans);
    stub::Stub("ItemSlot", label, stub::ContentWidth("ItemSlot", slot), slot);
    return {};
}

} // namespace facet