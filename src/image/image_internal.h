#pragma once

// Shared pieces of the image module: fit math, pixel conversion, SVG rasterizing and the
// hooks the SVG loader and the icon atlas use to reach the texture cache. Everything here
// runs on the main thread unless a comment says otherwise.

#include <cstdint>
#include <memory>
#include <vector>

#include <imgui.h>

#include "facet/core/image.h"
#include "internal/image_io.h"

namespace lunasvg
{
class Document;
}

namespace facet::internal
{

// Tightly packed 8-bit RGBA pixels with straight alpha, top row first.
struct RgbaImage
{
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
};

// Reads a whole file; `path` is UTF-8. Returns false for a missing, empty or unreadable
// file and for files larger than kMaxEncodedImageBytes (internal/image_io.h). Safe on any
// thread (texture_cache.cpp).
bool ReadFileBytes(const char* path, std::vector<std::uint8_t>& out);

// The texture cache's bookkeeping, for tests: slots in the vector, slots in use, slots
// waiting for a decode, and jobs no worker has taken yet (texture_cache.cpp).
struct ImageCacheCounts
{
    int slots = 0;
    int inUse = 0;
    int loading = 0;
    int queuedJobs = 0;
};
ImageCacheCounts GetImageCacheCounts();

// Parses an SVG document (the source is copied). Returns nullptr when it does not parse.
// A failed allocation throws std::bad_alloc instead, so a caller can tell running out of
// memory apart from a broken source (svg_raster.cpp).
std::unique_ptr<lunasvg::Document> ParseSvgDocument(const char* data, std::size_t length);

// Where an image of `imageSize` pixels lands inside the box [boxMin, boxMax] and which part
// of the texture it shows. Contain shrinks the destination to the image aspect and centers
// it (full UV range); Cover keeps the whole box and crops the UV range around the center;
// Fill uses the whole box and the whole texture. An empty image or box gives the box with
// the full UV range.
struct FitResult
{
    ImVec2 min;
    ImVec2 max;
    ImVec2 uvMin;
    ImVec2 uvMax;
};
FitResult FitImage(ImVec2 boxMin, ImVec2 boxMax, ImVec2 imageSize, ImageFit fit);

// Converts premultiplied pixels stored as 32-bit ARGB words in little-endian byte order
// (bytes B, G, R, A, the lunasvg bitmap layout) to straight RGBA. `srcStride` is the
// source row size in bytes; `dst` receives width * height * 4 bytes. Fully transparent
// pixels become 0, 0, 0, 0. With `mask`, the color is dropped and every pixel becomes white
// with the source alpha, which is how single-color icons are stored for tinting.
void PremultipliedBgraToStraightRgba(const std::uint8_t* src, int srcStride, int width, int height,
                                     std::uint8_t* dst, bool mask = false);

// Rasterizes a parsed document into `width` x `height` pixels (its view box stretched to
// that size). Returns false when the size is not positive or the document has no size of
// its own. A failed allocation throws std::bad_alloc, also when it happened inside the
// renderer (a document with a size that yields no bitmap), here and in RasterizeSvgSource
// and RasterizeIcon, which call this; the texture cache and the icon atlas catch it.
bool RasterizeSvgDocument(const lunasvg::Document& document, int width, int height, bool mask,
                          RgbaImage& out);

// How RasterizeSvgDocument reads a missing bitmap: the renderer returns none for a document
// without a size of its own, which is a property of the document, and when it could not
// allocate the surface, which is not. True for the second case.
bool SvgBitmapAllocationFailed(bool bitmapIsNull, float documentWidth, float documentHeight);

// An SVG kept by the texture cache so it can be rasterized again at a new scale.
// Defined in svg_raster.cpp.
struct SvgSource;
struct SvgSourceDeleter
{
    void operator()(SvgSource* source) const;
};
using SvgSourcePtr = std::unique_ptr<SvgSource, SvgSourceDeleter>;

// Most pixels of one SVG raster: 16 M (4096 x 4096, 64 MiB of RGBA). The raster runs on the
// frame thread and again after every scale change, so its cost has to stay bounded.
inline constexpr double kMaxSvgTexels = 4096.0 * 4096.0;

// Rasterizes the source at its design size times `scale`, rounded to whole pixels. Fails
// when a side would exceed kMaxImageSide or the raster kMaxSvgTexels.
bool RasterizeSvgSource(const SvgSource& source, float scale, RgbaImage& out);

// Takes ownership of a parsed SVG, rasterizes it at Scale() and returns its handle, or an
// invalid handle when Facet is not initialized or the upload fails (texture_cache.cpp).
ImageHandle CreateSvgImage(SvgSourcePtr source);

// Rasterizes a registered icon as a white mask of `pixels` x `pixels` without touching the
// atlas or the device (icon_atlas.cpp). Returns false for an unknown icon or a source that
// does not parse.
bool RasterizeIcon(std::uint32_t iconId, int pixels, RgbaImage& out);

// Pages the icon atlas holds now; at most four (icon_atlas.cpp).
int IconAtlasPageCount();

// Drops the icon registry and empties the icon atlas, so the next use builds the registry
// again from the embedded icons. Icons registered at runtime are gone afterwards. For tests
// that need the first build to happen at a chosen moment or must see only the embedded
// set; not for use while a frame is being recorded.
void ResetIconRegistry();

// Draws the loading placeholder or the failed marker of an image box. Used by DrawImage
// and available to widgets that show their own image slots.
void DrawImageLoading(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius);
void DrawImageFailed(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const char* label);

} // namespace facet::internal
