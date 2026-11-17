// The only translation unit of the library that compiles the vendored stb_image decoder.
// STB_IMAGE_STATIC gives every stbi_* function internal linkage, so none of them leaves
// facet.lib. The header is included as an external header, so its warnings stay silent.

#include "internal/image_io.h"

#include <climits>

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
// Files are read by the callers; the decoder only ever sees memory.
#define STBI_NO_STDIO
// Only the formats image.h documents are compiled in.
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#define STBI_ONLY_GIF
// The decoder rejects a larger side before it allocates anything: kMaxImageSide, the Direct3D
// 11 limit D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION. A macro has to be a literal here.
#define STBI_MAX_DIMENSIONS 16384
static_assert(STBI_MAX_DIMENSIONS == facet::internal::kMaxImageSide);

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
    if (data == nullptr || size == 0 || size > kMaxEncodedImageBytes || size > static_cast<std::size_t>(INT_MAX))
        return false;

    // The header alone gives the size, so an image beyond the pixel budget fails before the
    // decoder allocates its output.
    const auto* bytes = static_cast<const stbi_uc*>(data);
    int width = 0;
    int height = 0;
    int channels = 0;
    if (stbi_info_from_memory(bytes, static_cast<int>(size), &width, &height, &channels) == 0 || width <= 0 ||
        height <= 0 || static_cast<long long>(width) * height > kMaxImagePixels)
        return false;

    stbi_uc* pixels = stbi_load_from_memory(bytes, static_cast<int>(size), &width, &height, &channels, 4);
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
