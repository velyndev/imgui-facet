#pragma once

// Images: raster files, SVG documents and app-owned textures.
//
// Facet owns the textures behind an ImageHandle. A handle is an index plus a generation:
// after ReleaseImage the slot may be reused, and the old handle then reports failed
// instead of showing another image. Loading needs Facet to be initialized; before Init
// every load returns an invalid handle, and handles do not survive Shutdown.
//
// Every function here is for the thread that runs the Dear ImGui frame. Asynchronous loads
// decode on worker threads owned by Facet and upload in a later facet::NewFrame.
//
// Textures are released one frame late: ReleaseImage (and the replacement of an SVG raster
// after a scale change) drops the handle at once, but the texture itself is released in
// the next facet::NewFrame, after the draw data that may still use it was rendered.
//
// Device loss: InvalidateDeviceObjects releases every texture. CreateDeviceObjects then
// rasterizes SVG images again and reloads images that came from a file (asynchronously,
// so they show the loading state for a moment). Images created from memory or from pixels
// are not kept on the CPU; they report ImageStatus::Lost and must be created again by the
// app.
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

// What a handle currently refers to.
enum class ImageStatus
{
    Invalid,  // default handle, released, or from before the last Shutdown
    Loading,  // an asynchronous decode is pending
    Ready,    // the texture can be drawn
    Failed,   // the source could not be read or decoded, or the upload failed
    Lost,     // the device objects were recreated and the pixels were not kept
};

// Decodes a PNG, JPEG, BMP, TGA or GIF file (UTF-8 path) and uploads it. Returns an
// invalid handle when the file cannot be read or decoded. Limits, which apply to every
// load: files and memory up to 256 MiB, sides up to 16384 pixels (the Direct3D 11 texture
// limit), and at most 64 M pixels (8192 x 8192); larger images fail like broken data.
ImageHandle LoadImageFile(const char* path);
// The same for an encoded file in memory; the data is not kept.
ImageHandle LoadImageMemory(const void* data, std::size_t size);
// Returns at once and decodes on a worker thread; the upload happens in a later NewFrame.
// Watch IsImageReady and ImageFailed. A file that cannot be read or decoded gives a valid
// handle whose status becomes Failed.
ImageHandle LoadImageFileAsync(const char* path);
// The same for an encoded file in memory. The data is copied before the call returns.
ImageHandle LoadImageMemoryAsync(const void* data, std::size_t size);
// Uploads 8-bit RGBA pixels with straight alpha. `stride` is the row size in bytes,
// 0 for tightly packed rows. The pixels are not kept.
ImageHandle CreateImageRGBA(const void* pixels, int width, int height, int stride = 0);

// Rasterizes an SVG document at `size` design units (0 for the document's own size) times
// Scale(), and again whenever the scale changes, so it stays sharp. When only one of the
// two components is 0, it follows the document's aspect ratio. Colors are kept as written
// in the document (currentColor is black).
ImageHandle LoadSvgFile(const char* path, ImVec2 size = ImVec2(0.0f, 0.0f));
// `length` 0 means the source is zero terminated. The source is copied.
ImageHandle LoadSvgMemory(const char* svg, std::size_t length = 0, ImVec2 size = ImVec2(0.0f, 0.0f));

// Releases the texture. The handle and all copies of it become invalid. Safe on invalid
// handles; a pending decode of the handle is discarded when it finishes.
void ReleaseImage(ImageHandle image);

ImageStatus GetImageStatus(ImageHandle image);
// True once the texture can be drawn.
bool IsImageReady(ImageHandle image);
// True when loading failed, or the handle is invalid, released or lost.
bool ImageFailed(ImageHandle image);
// Texture size in pixels; zero while not ready.
ImVec2 ImageSize(ImageHandle image);
// The texture for ImGui or ImDrawList calls; an empty ImTextureRef while not ready.
ImTextureRef ImageTexture(ImageHandle image);

// Blocks until the pending decode of `image` finished and uploads it, or until
// `timeoutSeconds` passed. Returns IsImageReady(image). For loading screens and tests;
// widgets watch the status instead.
bool WaitForImage(ImageHandle image, float timeoutSeconds);

// The image as an ImGui item of `size` pixels. A size component of 0 takes the image's
// own size (keeping its aspect ratio when the other component is given); while the size
// is unknown, a square of Px(T().metrics.controlHeightLarge) is used. With showStates, a
// pending image shows a pulsing placeholder and a failed one an icon with a short label.
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
