#pragma once

// Image decoding for the library. The decoder is compiled with internal linkage inside
// src/image/image_io.cpp, so facet.lib exports none of its symbols and a host application
// can link its own copy of the same decoder without clashes.

#include <cstddef>

namespace facet::internal
{

// Limits that keep a small hostile file from costing gigabytes. Images beyond them fail like
// undecodable data.
//
// The longest side Direct3D 11 can upload (D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION); a larger
// image would be decoded only to be rejected at the upload.
inline constexpr int kMaxImageSide = 16384;
// The most pixels decoded for one image: 64 Mpx, 256 MiB of RGBA (8192 x 8192).
inline constexpr long long kMaxImagePixels = 64ll << 20;
// The largest encoded image accepted from a file or from memory (256 MiB).
inline constexpr std::size_t kMaxEncodedImageBytes = std::size_t{256} << 20;

// Decoded pixels: tightly packed 8-bit RGBA rows, top row first, straight (not
// premultiplied) alpha. Release with FreeImage.
struct DecodedImage
{
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
};

// Decodes a PNG, JPEG, BMP, TGA or GIF (first frame) image from memory. Returns false and
// leaves `out` empty when the data cannot be decoded or is beyond the limits above.
bool DecodeImage(const void* data, std::size_t size, DecodedImage& out);

// Frees the pixels of a decoded image and resets it. Safe on an empty image.
void FreeImage(DecodedImage& image);

} // namespace facet::internal
