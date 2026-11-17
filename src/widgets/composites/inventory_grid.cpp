// InventoryGrid (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include <algorithm>

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

InventoryResult InventoryGrid(const char* id, int*, std::span<const InventoryItem> slots,
                              const InventoryGridOptions& options)
{
    namespace stub = internal::content;
    const float slot = options.slotSize > 0.0f ? Px(options.slotSize) : stub::Spans(stub::kSlotSpans);
    const float gap = Px(T().spacing.xs);
    const float width = stub::ResolveWidth(options.width, 0.0f);
    const int fitting = std::max(1, static_cast<int>((width + gap) / (slot + gap)));
    const int columns = options.columns > 0 ? options.columns : fitting;
    const int count = std::max(1, static_cast<int>(slots.size()));
    const int rows = (count + columns - 1) / columns;
    stub::Stub("InventoryGrid", id, static_cast<float>(columns) * (slot + gap) - gap,
               static_cast<float>(rows) * (slot + gap) - gap);
    return {};
}

} // namespace facet