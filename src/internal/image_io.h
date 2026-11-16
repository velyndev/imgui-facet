#pragma once

// Image decoding for the library. The decoder is compiled with internal linkage inside
// src/image/image_io.cpp, so facet.lib exports none of its symbols and a host application
// can link its own copy of the same decoder without clashes.

#include <cstddef>

namespace facet::internal
{

// Decoded pixels: tightly packed 8-bit RGBA rows, top row first, straight (not
// premultiplied) alpha. Release with FreeImage.
struct DecodedImage
{
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
};

// Decodes a PNG, JPEG, BMP, TGA, GIF (first frame), PSD, HDR (tone mapped to 8 bit) or PNM
// image from memory. Returns false and leaves `out` empty when the data cannot be decoded.
bool DecodeImage(const void* data, std::size_t size, DecodedImage& out);

// Frees the pixels of a decoded image and resets it. Safe on an empty image.
void FreeImage(DecodedImage& image);

} // namespace facet::internal
