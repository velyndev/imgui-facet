// StatTile (facet/widgets/display.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

void StatTile(const char* label, double, const StatTileOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("StatTile", label, stub::ResolveWidth(options.width, 0.0f), stub::Spans(stub::kSlotSpans));
}

} // namespace facet