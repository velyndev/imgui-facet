// Carousel (facet/widgets/media.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/media.h"
#include "widgets/content_stub.h"

namespace facet
{

bool Carousel(const char* label, int*, std::span<const ImageItem>, const CarouselOptions& options)
{
    namespace stub = internal::content;
    const float width = stub::ResolveWidth(options.width, 0.0f);
    stub::Stub("Carousel", label, width, stub::ResolveHeight(options.height, width * stub::kWideAspect));
    return false;
}

} // namespace facet