// The only translation unit of the library that compiles the vendored stb_image decoder.
// STB_IMAGE_STATIC gives every stbi_* function internal linkage, so none of them leaves
// facet.lib. The header is included as an external header, so its warnings stay silent.

#include "internal/image_io.h"

#include <climits>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
// Files are read by the callers; the decoder only ever sees memory.
#define STBI_NO_STDIO

#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif

#include <stb_image.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace facet::internal
{

bool DecodeImage(const void* data, std::size_t size, DecodedImage& out)
{
    out = DecodedImage{};
    if (data == nullptr || size == 0 || size > static_cast<std::size_t>(INT_MAX))
        return false;

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(static_cast<const stbi_uc*>(data), static_cast<int>(size),
                                            &width, &height, &channels, 4);
    if (pixels == nullptr)
        return false;
    out.pixels = pixels;
    out.width = width;
    out.height = height;
    return true;
}

void FreeImage(DecodedImage& image)
{
    if (image.pixels != nullptr)
        stbi_image_free(image.pixels);
    image = DecodedImage{};
}

} // namespace facet::internal
