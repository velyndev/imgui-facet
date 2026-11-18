// InventoryDetailCard (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

int InventoryDetailCard(const char* label, ImageHandle, const InventoryDetailCardOptions& options)
{
    namespace stub = internal::content;
    const float width = stub::ResolveWidth(options.width, stub::Spans(stub::kCardWidthSpans));
    stub::Stub("InventoryDetailCard", label, width, stub::Spans(stub::kChartHeightSpans));
    return -1;
}

} // namespace facet