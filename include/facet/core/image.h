#pragma once

// Images: raster files, SVG documents and app-owned textures.
//
// Facet owns the textures behind an ImageHandle. A handle is an index plus a generation:
// after ReleaseImage the slot may be reused, and the old handle then reports failed
// instead of showing another image. Loading needs Facet to be initialized; before Init
// every load returns an invalid handle.
//
// Sizes passed to Image are pixels, as in Dear ImGui (convert design units with Px()).

#include <cstddef>
#include <cstdint>

#include <imgui.h>

struct ID3D11ShaderResourceView;

namespace facet
{

struct ImageHandle
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;  // never 0 for a handle returned by a successful call

    constexpr bool IsValid() const { return generation != 0; }
    friend constexpr bool operator==(const ImageHandle&, const ImageHandle&) = default;
};

// How an image fills a box of another aspect ratio.
enum class ImageFit
{
    Contain,  // whole image visible, letterboxed
    Cover,    // box filled, the image cropped
    Fill,     // stretched to the box
};

struct ImageOptions
{
    ImageFit fit = ImageFit::Contain;
    float radius = 0.0f;           // corner radius in pixels; corners follow FillRect
    ImU32 tint = IM_COL32_WHITE;   // multiplies the image colors
    bool showStates = true;        // skeleton while loading, a failed marker on error
};

// Decodes a PNG, JPEG, BMP, TGA or GIF file (UTF-8 path) and uploads it. Returns an
// invalid handle when the file cannot be read or decoded.
ImageHandle LoadImageFile(const char* path);
// The same for an encoded file in memory; the data is not kept.
ImageHandle LoadImageMemory(const void* data, std::size_t size);
// Returns at once and decodes on a worker thread; the upload happens in a later NewFrame.
// Watch IsImageReady and ImageFailed.
ImageHandle LoadImageFileAsync(const char* path);
// Uploads 8-bit RGBA pixels with straight alpha. `stride` is the row size in bytes,
// 0 for tightly packed rows. The pixels are not kept.
ImageHandle CreateImageRGBA(const void* pixels, int width, int height, int stride = 0);

// Rasterizes an SVG document at `size` design units (0 for the document's own size) times
// Scale(), and again whenever the scale changes, so it stays sharp.
ImageHandle LoadSvgFile(const char* path, ImVec2 size = ImVec2(0.0f, 0.0f));
// `length` 0 means the source is zero terminated. The source is copied.
ImageHandle LoadSvgMemory(const char* svg, std::size_t length = 0, ImVec2 size = ImVec2(0.0f, 0.0f));

// Releases the texture. The handle and all copies of it become invalid. Safe on invalid
// handles.
void ReleaseImage(ImageHandle image);

// True once the texture can be drawn.
bool IsImageReady(ImageHandle image);
// True when loading failed, or the handle is invalid or released.
bool ImageFailed(ImageHandle image);
// Texture size in pixels; zero while not ready.
ImVec2 ImageSize(ImageHandle image);
// The texture for ImGui or ImDrawList calls; an empty ImTextureRef while not ready.
ImTextureRef ImageTexture(ImageHandle image);

// The image as an ImGui item of `size` pixels.
void Image(ImageHandle image, ImVec2 size, const ImageOptions& options = {});
// An app-owned texture as an item. Facet reads the texture size from the view and does not
// keep a reference to it.
void Image(ID3D11ShaderResourceView* texture, ImVec2 size, const ImageOptions& options = {});

// Draws into a draw list without creating an item, for custom layouts.
void DrawImage(ImDrawList* drawList, ImageHandle image, ImVec2 pMin, ImVec2 pMax,
               const ImageOptions& options = {});
void DrawImage(ImDrawList* drawList, ID3D11ShaderResourceView* texture, ImVec2 pMin, ImVec2 pMax,
               const ImageOptions& options = {});

} // namespace facet
