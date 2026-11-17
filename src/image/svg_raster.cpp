// SVG images rasterized per scale, and the pixel conversion shared with the icon atlas.
//
// lunasvg renders into premultiplied 32-bit ARGB words. The textures Facet creates hold
// straight RGBA (the Dear ImGui renderer blends with straight alpha), so every raster is
// converted before upload.

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include <lunasvg.h>

#include "facet/core/image.h"
#include "image/image_internal.h"
#include "internal/alloc_guard.h"

namespace facet
{

namespace internal
{

struct SvgSource
{
    std::unique_ptr<lunasvg::Document> document;
    ImVec2 designSize;  // logical size in design units, both components positive
};

void SvgSourceDeleter::operator()(SvgSource* source) const
{
    delete source;
}

void PremultipliedBgraToStraightRgba(const std::uint8_t* src, int srcStride, int width, int height,
                                     std::uint8_t* dst, bool mask)
{
    if (src == nullptr || dst == nullptr || width <= 0 || height <= 0)
        return;
    for (int y = 0; y < height; ++y)
    {
        const std::uint8_t* in = src + static_cast<std::ptrdiff_t>(y) * srcStride;
        std::uint8_t* out = dst + static_cast<std::ptrdiff_t>(y) * width * 4;
        for (int x = 0; x < width; ++x, in += 4, out += 4)
        {
            const unsigned a = in[3];
            if (mask)
            {
                out[0] = out[1] = out[2] = a != 0 ? 255 : 0;
                out[3] = static_cast<std::uint8_t>(a);
                continue;
            }
            if (a == 0)
            {
                out[0] = out[1] = out[2] = out[3] = 0;
                continue;
            }
            // Rounded division; channels above alpha (invalid input) clamp to 255.
            const auto unpremultiply = [a](unsigned channel) {
                return static_cast<std::uint8_t>(std::min(255u, (channel * 255u + a / 2u) / a));
            };
            out[0] = unpremultiply(in[2]);
            out[1] = unpremultiply(in[1]);
            out[2] = unpremultiply(in[0]);
            out[3] = static_cast<std::uint8_t>(a);
        }
    }
}

std::unique_ptr<lunasvg::Document> ParseSvgDocument(const char* data, std::size_t length)
{
    CheckAllocation();
    return lunasvg::Document::loadFromData(data, length);
}

bool RasterizeSvgDocument(const lunasvg::Document& document, int width, int height, bool mask, RgbaImage& out)
{
    out = RgbaImage{};
    if (width <= 0 || height <= 0)
        return false;
    CheckAllocation();
    const lunasvg::Bitmap bitmap = document.renderToBitmap(width, height);
    if (bitmap.isNull() || bitmap.width() != width || bitmap.height() != height)
        return false;
    out.pixels.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    out.width = width;
    out.height = height;
    PremultipliedBgraToStraightRgba(bitmap.data(), bitmap.stride(), width, height, out.pixels.data(), mask);
    return true;
}

bool RasterizeSvgSource(const SvgSource& source, float scale, RgbaImage& out)
{
    if (source.document == nullptr || !(scale > 0.0f))
        return false;
    // A raster beyond the limits fails instead of being squeezed: shrinking one side alone
    // would change the aspect ratio, and the raster runs on the frame thread (again after
    // every scale change), so its cost must stay bounded.
    const double width = std::max(1.0, std::round(static_cast<double>(source.designSize.x) * scale));
    const double height = std::max(1.0, std::round(static_cast<double>(source.designSize.y) * scale));
    if (width > kMaxImageSide || height > kMaxImageSide || width * height > kMaxSvgTexels)
        return false;
    return RasterizeSvgDocument(*source.document, static_cast<int>(width), static_cast<int>(height), false, out);
}

} // namespace internal

namespace
{

// Parses the document and resolves the design size: 0 takes the document's own size, one
// 0 component follows the document's aspect ratio.
ImageHandle CreateFromDocument(std::unique_ptr<lunasvg::Document> document, ImVec2 size)
{
    if (document == nullptr)
        return {};
    const float docWidth = document->width();
    const float docHeight = document->height();
    ImVec2 design = size;
    if (design.x <= 0.0f && design.y <= 0.0f)
    {
        design = ImVec2(docWidth, docHeight);
    }
    else if (design.x <= 0.0f)
    {
        if (docHeight <= 0.0f)
            return {};
        design.x = design.y * docWidth / docHeight;
    }
    else if (design.y <= 0.0f)
    {
        if (docWidth <= 0.0f)
            return {};
        design.y = design.x * docHeight / docWidth;
    }
    if (!(design.x > 0.0f) || !(design.y > 0.0f) || !std::isfinite(design.x) || !std::isfinite(design.y))
        return {};

    internal::SvgSourcePtr source;
    try
    {
        internal::CheckAllocation();
        source.reset(new internal::SvgSource{std::move(document), design});
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
    return internal::CreateSvgImage(std::move(source));
}

// The parsed document, or nullptr when it does not parse or memory runs out.
std::unique_ptr<lunasvg::Document> TryParse(const char* data, std::size_t length)
{
    try
    {
        return internal::ParseSvgDocument(data, length);
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }
}

} // namespace

ImageHandle LoadSvgFile(const char* path, ImVec2 size)
{
    std::vector<std::uint8_t> bytes;
    if (path == nullptr || !internal::ReadFileBytes(path, bytes))
        return {};
    return CreateFromDocument(TryParse(reinterpret_cast<const char*>(bytes.data()), bytes.size()), size);
}

ImageHandle LoadSvgMemory(const char* svg, std::size_t length, ImVec2 size)
{
    if (svg == nullptr)
        return {};
    if (length == 0)
        length = std::strlen(svg);
    if (length == 0)
        return {};
    return CreateFromDocument(TryParse(svg, length), size);
}

} // namespace facet
