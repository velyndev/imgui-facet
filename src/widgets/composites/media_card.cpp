// MediaCard (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

bool MediaCard(const char* label, ImageHandle, const MediaCardOptions& options)
{
    namespace stub = internal::content;
    const float width = stub::ResolveWidth(options.width, stub::Spans(stub::kCardWidthSpans));
    stub::Stub("MediaCard", label, width, width * stub::kWideAspect + stub::ControlRows(stub::kTallControlRows));
    return false;
}

} // namespace facet