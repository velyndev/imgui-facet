// ImageView (facet/widgets/media.h): planned. A placeholder until Phase 2 implements it. The
// placeholder takes the image's own size while it is known, like the finished widget.

#include "facet/widgets/media.h"
#include "widgets/content_stub.h"

namespace facet
{
namespace
{

// `natural` is the image size in pixels, zero while unknown.
void ImageViewStub(const char* label, ImVec2 natural, const ImageViewOptions& options)
{
    namespace stub = internal::content;
    if (natural.x <= 0.0f || natural.y <= 0.0f)
        natural = ImVec2(stub::ControlRows(1.0f, ControlSize::Large), stub::ControlRows(1.0f, ControlSize::Large));
    float width = options.width > 0.0f ? Px(options.width) : 0.0f;
    float height = options.height > 0.0f ? Px(options.height) : 0.0f;
    if (width <= 0.0f && height <= 0.0f)
    {
        width = natural.x;
        height = natural.y;
    }
    else if (width <= 0.0f)
        width = height * natural.x / natural.y;
    else if (height <= 0.0f)
        height = width * natural.y / natural.x;
    stub::Stub("ImageView", label, width, height);
}

} // namespace

bool ImageView(const char* label, ImageHandle image, const ImageViewOptions& options)
{
    ImageViewStub(label, ImageSize(image), options);
    return false;
}

bool ImageView(const char* label, ID3D11ShaderResourceView*, const ImageViewOptions& options)
{
    ImageViewStub(label, ImVec2(0.0f, 0.0f), options);
    return false;
}

} // namespace facet