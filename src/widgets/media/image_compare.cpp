// ImageCompare (facet/widgets/media.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/media.h"
#include "widgets/content_stub.h"

namespace facet
{

bool ImageCompare(const char* label, float*, ImageHandle before, ImageHandle, const ImageCompareOptions& options)
{
    namespace stub = internal::content;
    const float width = stub::ResolveWidth(options.width, 0.0f);
    const ImVec2 natural = ImageSize(before);
    const float aspect = natural.x > 0.0f && natural.y > 0.0f ? natural.y / natural.x : stub::kWideAspect;
    stub::Stub("ImageCompare", label, width, stub::ResolveHeight(options.height, width * aspect));
    return false;
}

} // namespace facet