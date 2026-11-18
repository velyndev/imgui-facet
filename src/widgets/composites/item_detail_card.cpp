// ItemDetailCard (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

int ItemDetailCard(const char* label, ImageHandle, const ItemDetailCardOptions& options)
{
    namespace stub = internal::content;
    const float width = stub::ResolveWidth(options.width, stub::Spans(stub::kCardWidthSpans));
    stub::Stub("ItemDetailCard", label, width, stub::Spans(stub::kChartHeightSpans));
    return -1;
}

} // namespace facet