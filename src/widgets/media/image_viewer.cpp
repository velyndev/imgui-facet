// ImageViewer (facet/widgets/media.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/media.h"
#include "widgets/content_stub.h"

namespace facet
{

bool ImageViewer(const char* label, ImageHandle, ImageViewerState*, const ImageViewerOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("ImageViewer", label, stub::ResolveWidth(options.width, 0.0f), stub::ResolveHeight(options.height, 0.0f));
    return false;
}

} // namespace facet