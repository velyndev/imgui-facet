// ThumbnailStrip (facet/widgets/media.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/media.h"
#include "widgets/content_stub.h"

namespace facet
{
namespace
{

void ThumbnailStripStub(const char* label, const ThumbnailStripOptions& options)
{
    namespace stub = internal::content;
    float height = options.thumbnailHeight > 0.0f ? Px(options.thumbnailHeight) : stub::Spans(stub::kThumbnailSpans);
    if (options.captions)
        height += stub::LineHeight(TextStyle::Footnote);
    stub::Stub("ThumbnailStrip", label, stub::ResolveWidth(options.width, 0.0f), height);
}

} // namespace

bool ThumbnailStrip(const char* label, int*, std::span<const ImageItem>, const ThumbnailStripOptions& options)
{
    ThumbnailStripStub(label, options);
    return false;
}

bool ThumbnailStrip(const char* label, int*, int, ImageItemGetter, void*, const ThumbnailStripOptions& options)
{
    ThumbnailStripStub(label, options);
    return false;
}

} // namespace facet