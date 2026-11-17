// Texture cache, decoding, fit math, pixel conversion and SVG rasterizing. The cases that
// need a device run on a WARP device, so they work on machines without a GPU.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>

#include <facet/facet.h>

#include "image/image_internal.h"
#include "internal/alloc_guard.h"
#include "internal/image_io.h"

#pragma comment(lib, "d3d11.lib")

namespace
{

// 2 x 2 PNG with alpha: red, green / blue, white at alpha 128.
const unsigned char kPng[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xB6, 0x0D,
    0x24, 0x00, 0x00, 0x00, 0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
    0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC, 0x61, 0x05, 0x00, 0x00,
    0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3, 0x01, 0xC7,
    0x6F, 0xA8, 0x64, 0x00, 0x00, 0x00, 0x14, 0x49, 0x44, 0x41, 0x54, 0x18, 0x57, 0x63, 0xF8, 0xCF,
    0xC0, 0xF0, 0x1F, 0x0C, 0x19, 0x18, 0xFE, 0x83, 0x40, 0x03, 0x00, 0x49, 0x49, 0x09, 0x78, 0xCE,
    0xD7, 0x63, 0xF7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

// 8 x 8 baseline JPEG filled with grey 128.
const unsigned char kJpeg[] = {
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x01, 0x00, 0x60,
    0x00, 0x60, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x10, 0x0B, 0x0C, 0x0E, 0x0C, 0x0A, 0x10,
    0x0E, 0x0D, 0x0E, 0x12, 0x11, 0x10, 0x13, 0x18, 0x28, 0x1A, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
    0x25, 0x1D, 0x28, 0x3A, 0x33, 0x3D, 0x3C, 0x39, 0x33, 0x38, 0x37, 0x40, 0x48, 0x5C, 0x4E, 0x40,
    0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6D, 0x51, 0x57, 0x5F, 0x62, 0x67, 0x68, 0x67, 0x3E, 0x4D,
    0x71, 0x79, 0x70, 0x64, 0x78, 0x5C, 0x65, 0x67, 0x63, 0xFF, 0xDB, 0x00, 0x43, 0x01, 0x11, 0x12,
    0x12, 0x18, 0x15, 0x18, 0x2F, 0x1A, 0x1A, 0x2F, 0x63, 0x42, 0x38, 0x42, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
    0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0xFF, 0xC0,
    0x00, 0x11, 0x08, 0x00, 0x08, 0x00, 0x08, 0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11,
    0x01, 0xFF, 0xC4, 0x00, 0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
    0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05,
    0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,
    0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23,
    0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
    0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,
    0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A,
    0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
    0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
    0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5,
    0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1,
    0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xC4, 0x00, 0x1F, 0x01, 0x00, 0x03,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x11, 0x00,
    0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01, 0x02, 0x77, 0x00,
    0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13,
    0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0, 0x15,
    0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27,
    0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
    0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
    0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88,
    0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
    0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4,
    0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE2,
    0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9,
    0xFA, 0xFF, 0xDA, 0x00, 0x0C, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3F, 0x00, 0x28,
    0xA2, 0x8A, 0x00, 0xFF, 0xD9,
};

const char kSvg24[] = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"24\" viewBox=\"0 0 24 24\">"
                      "<rect x=\"2\" y=\"2\" width=\"20\" height=\"20\" fill=\"#ff0000\"/></svg>";

const unsigned char kGarbage[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99};

void PutBigEndian(std::vector<unsigned char>& out, std::uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8)
        out.push_back(static_cast<unsigned char>(value >> shift));
}

void PutChunk(std::vector<unsigned char>& out, const char* type, const std::vector<unsigned char>& data)
{
    PutBigEndian(out, static_cast<std::uint32_t>(data.size()));
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    PutBigEndian(out, 0);  // CRC: the decoder does not check it
}

// A valid 8-bit grey PNG of any size, all pixels 0x80. The image data is stored without
// compression (deflate blocks of type 0), so the file is about width * height bytes and the
// decoder really has to produce every pixel.
std::vector<unsigned char> MakeGreyPng(std::uint32_t width, std::uint32_t height)
{
    std::vector<unsigned char> raw;
    raw.reserve(static_cast<std::size_t>(width + 1) * height);
    for (std::uint32_t y = 0; y < height; ++y)
    {
        raw.push_back(0);  // filter: none
        raw.insert(raw.end(), width, 0x80);
    }
    std::vector<unsigned char> zlib = {0x78, 0x01};
    constexpr std::size_t kBlock = 65535;
    for (std::size_t offset = 0; offset < raw.size(); offset += kBlock)
    {
        const std::size_t length = std::min(kBlock, raw.size() - offset);
        zlib.push_back(offset + length == raw.size() ? 1 : 0);
        zlib.push_back(static_cast<unsigned char>(length & 0xFF));
        zlib.push_back(static_cast<unsigned char>(length >> 8));
        zlib.push_back(static_cast<unsigned char>(~length & 0xFF));
        zlib.push_back(static_cast<unsigned char>((~length >> 8) & 0xFF));
        zlib.insert(zlib.end(), raw.begin() + static_cast<std::ptrdiff_t>(offset),
                    raw.begin() + static_cast<std::ptrdiff_t>(offset + length));
    }
    PutBigEndian(zlib, 0);  // Adler-32: not checked either

    std::vector<unsigned char> png = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<unsigned char> header;
    PutBigEndian(header, width);
    PutBigEndian(header, height);
    header.insert(header.end(), {8, 0, 0, 0, 0});  // 8-bit grey, no interlace
    PutChunk(png, "IHDR", header);
    PutChunk(png, "IDAT", zlib);
    PutChunk(png, "IEND", {});
    return png;
}

// An ImGui context, a WARP device and an initialized Facet for the lifetime of the object.
struct Session
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    bool ok = false;

    Session()
    {
        ImGui::CreateContext();
        ImGui::GetIO().DisplaySize = ImVec2(800.0f, 600.0f);
        const HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                             D3D11_SDK_VERSION, &device, nullptr, &context);
        if (SUCCEEDED(hr))
        {
            facet::InitDesc desc;
            desc.device = device.Get();
            desc.context = context.Get();
            ok = facet::Init(desc);
        }
    }
    ~Session()
    {
        facet::Shutdown();
        ImGui::DestroyContext();
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
};

void CheckNear(ImVec2 a, ImVec2 b)
{
    CHECK(a.x == doctest::Approx(b.x));
    CHECK(a.y == doctest::Approx(b.y));
}

std::vector<unsigned char> SolidPixels(int width, int height)
{
    return std::vector<unsigned char>(static_cast<std::size_t>(width) * height * 4u, 0xFF);
}

} // namespace

TEST_CASE("Contain letterboxes and keeps the full texture")
{
    // A 2:1 image in a 100 x 100 box: 100 x 50, centered vertically.
    const auto fit = facet::internal::FitImage(ImVec2(0, 0), ImVec2(100, 100), ImVec2(200, 100),
                                               facet::ImageFit::Contain);
    CheckNear(fit.min, ImVec2(0, 25));
    CheckNear(fit.max, ImVec2(100, 75));
    CheckNear(fit.uvMin, ImVec2(0, 0));
    CheckNear(fit.uvMax, ImVec2(1, 1));

    // A 1:2 image in a 100 x 100 box: 50 x 100, centered horizontally.
    const auto tall = facet::internal::FitImage(ImVec2(10, 10), ImVec2(110, 110), ImVec2(50, 100),
                                                facet::ImageFit::Contain);
    CheckNear(tall.min, ImVec2(35, 10));
    CheckNear(tall.max, ImVec2(85, 110));
}

TEST_CASE("Cover fills the box and crops the texture around its center")
{
    const auto wide = facet::internal::FitImage(ImVec2(0, 0), ImVec2(100, 100), ImVec2(200, 100),
                                                facet::ImageFit::Cover);
    CheckNear(wide.min, ImVec2(0, 0));
    CheckNear(wide.max, ImVec2(100, 100));
    CheckNear(wide.uvMin, ImVec2(0.25f, 0.0f));
    CheckNear(wide.uvMax, ImVec2(0.75f, 1.0f));

    const auto tall = facet::internal::FitImage(ImVec2(0, 0), ImVec2(200, 100), ImVec2(100, 100),
                                                facet::ImageFit::Cover);
    CheckNear(tall.uvMin, ImVec2(0.0f, 0.25f));
    CheckNear(tall.uvMax, ImVec2(1.0f, 0.75f));
}

TEST_CASE("Fill stretches, and empty sizes fall back to the whole box")
{
    const auto fill = facet::internal::FitImage(ImVec2(0, 0), ImVec2(100, 40), ImVec2(10, 90),
                                                facet::ImageFit::Fill);
    CheckNear(fill.min, ImVec2(0, 0));
    CheckNear(fill.max, ImVec2(100, 40));
    CheckNear(fill.uvMax, ImVec2(1, 1));

    const auto empty = facet::internal::FitImage(ImVec2(0, 0), ImVec2(100, 40), ImVec2(0, 0),
                                                 facet::ImageFit::Contain);
    CheckNear(empty.max, ImVec2(100, 40));
    CheckNear(empty.uvMin, ImVec2(0, 0));
}

TEST_CASE("Premultiplied BGRA converts to straight RGBA")
{
    // Pixels as lunasvg stores them: B, G, R, A with premultiplied color.
    const std::uint8_t src[] = {
        16, 32, 64, 128,     // straight 32, 64, 128 at alpha 128
        0, 0, 0, 0,          // transparent
        10, 20, 30, 255,     // opaque: unchanged
        200, 200, 200, 100,  // invalid (color above alpha): clamped
    };
    std::uint8_t dst[16] = {};
    facet::internal::PremultipliedBgraToStraightRgba(src, 16, 4, 1, dst);
    const std::uint8_t expected[] = {128, 64, 32, 128, 0, 0, 0, 0, 30, 20, 10, 255, 255, 255, 255, 100};
    for (int i = 0; i < 16; ++i)
    {
        CAPTURE(i);
        CHECK(dst[i] == expected[i]);
    }

    // Mask mode keeps the coverage only, as white.
    facet::internal::PremultipliedBgraToStraightRgba(src, 16, 4, 1, dst, true);
    const std::uint8_t mask[] = {255, 255, 255, 128, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 100};
    for (int i = 0; i < 16; ++i)
    {
        CAPTURE(i);
        CHECK(dst[i] == mask[i]);
    }
}

TEST_CASE("Source rows are read with their stride")
{
    // Two rows of one pixel each, padded to 8 bytes per row.
    const std::uint8_t src[] = {0, 0, 255, 255, 9, 9, 9, 9, 255, 0, 0, 255, 9, 9, 9, 9};
    std::uint8_t dst[8] = {};
    facet::internal::PremultipliedBgraToStraightRgba(src, 8, 1, 2, dst);
    const std::uint8_t expected[] = {255, 0, 0, 255, 0, 0, 255, 255};
    for (int i = 0; i < 8; ++i)
        CHECK(dst[i] == expected[i]);
}

TEST_CASE("A small PNG decodes to exact straight RGBA")
{
    facet::internal::DecodedImage image;
    REQUIRE(facet::internal::DecodeImage(kPng, sizeof(kPng), image));
    CHECK(image.width == 2);
    CHECK(image.height == 2);
    const unsigned char expected[] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 128};
    for (int i = 0; i < 16; ++i)
    {
        CAPTURE(i);
        CHECK(image.pixels[i] == expected[i]);
    }
    facet::internal::FreeImage(image);
    CHECK(image.pixels == nullptr);
}

TEST_CASE("A small JPEG decodes close to its grey")
{
    facet::internal::DecodedImage image;
    REQUIRE(facet::internal::DecodeImage(kJpeg, sizeof(kJpeg), image));
    CHECK(image.width == 8);
    CHECK(image.height == 8);
    for (int i = 0; i < 8 * 8; ++i)
    {
        CAPTURE(i);
        for (int channel = 0; channel < 3; ++channel)
            CHECK(std::abs(static_cast<int>(image.pixels[i * 4 + channel]) - 128) <= 4);
        CHECK(image.pixels[i * 4 + 3] == 255);
    }
    facet::internal::FreeImage(image);
}

TEST_CASE("Damaged data fails to decode and leaves the output empty")
{
    facet::internal::DecodedImage image;
    CHECK_FALSE(facet::internal::DecodeImage(kGarbage, sizeof(kGarbage), image));
    CHECK(image.pixels == nullptr);
    // A PNG cut after its header.
    CHECK_FALSE(facet::internal::DecodeImage(kPng, 40, image));
    CHECK_FALSE(facet::internal::DecodeImage(nullptr, 10, image));
    CHECK_FALSE(facet::internal::DecodeImage(kPng, 0, image));
}

TEST_CASE("Loads before Init return invalid handles")
{
    CHECK_FALSE(facet::CreateImageRGBA(SolidPixels(1, 1).data(), 1, 1).IsValid());
    CHECK_FALSE(facet::LoadImageMemory(kPng, sizeof(kPng)).IsValid());
    CHECK_FALSE(facet::LoadImageMemoryAsync(kPng, sizeof(kPng)).IsValid());
    CHECK_FALSE(facet::LoadSvgMemory(kSvg24).IsValid());
    CHECK(facet::GetImageStatus(facet::ImageHandle{0, 1}) == facet::ImageStatus::Invalid);
}

TEST_CASE("Handles carry a generation; released slots are reused with a new one")
{
    Session session;
    REQUIRE(session.ok);
    const auto pixels = SolidPixels(4, 3);

    const facet::ImageHandle first = facet::CreateImageRGBA(pixels.data(), 4, 3);
    REQUIRE(first.IsValid());
    CHECK(facet::IsImageReady(first));
    CHECK_FALSE(facet::ImageFailed(first));
    CHECK(facet::ImageSize(first).x == 4.0f);
    CHECK(facet::ImageSize(first).y == 3.0f);
    CHECK(facet::ImageTexture(first).GetTexID() != ImTextureID_Invalid);

    facet::ReleaseImage(first);
    CHECK(facet::GetImageStatus(first) == facet::ImageStatus::Invalid);
    CHECK(facet::ImageFailed(first));
    CHECK(facet::ImageSize(first).x == 0.0f);
    CHECK(facet::ImageTexture(first).GetTexID() == ImTextureID_Invalid);
    facet::ReleaseImage(first);  // releasing twice is harmless

    const facet::ImageHandle second = facet::CreateImageRGBA(pixels.data(), 4, 3);
    REQUIRE(second.IsValid());
    CHECK(second.index == first.index);
    CHECK(second.generation != first.generation);
    CHECK(facet::IsImageReady(second));
    CHECK_FALSE(facet::IsImageReady(first));

    // Bad arguments.
    CHECK_FALSE(facet::CreateImageRGBA(nullptr, 4, 3).IsValid());
    CHECK_FALSE(facet::CreateImageRGBA(pixels.data(), 0, 3).IsValid());
    CHECK_FALSE(facet::CreateImageRGBA(pixels.data(), 4, 3, 8).IsValid());  // stride below 4 * width

    // The deferred release runs in the next frame without touching live images.
    facet::NewFrame();
    facet::EndFrame();
    CHECK(facet::IsImageReady(second));
}

TEST_CASE("Handles do not survive Shutdown")
{
    facet::ImageHandle old;
    {
        Session session;
        REQUIRE(session.ok);
        const auto pixels = SolidPixels(2, 2);
        old = facet::CreateImageRGBA(pixels.data(), 2, 2);
        REQUIRE(facet::IsImageReady(old));
    }
    Session session;
    REQUIRE(session.ok);
    const auto pixels = SolidPixels(2, 2);
    const facet::ImageHandle fresh = facet::CreateImageRGBA(pixels.data(), 2, 2);
    CHECK(fresh.index == old.index);
    CHECK(facet::GetImageStatus(old) == facet::ImageStatus::Invalid);
    CHECK(facet::IsImageReady(fresh));
}

TEST_CASE("Encoded images load synchronously from memory and from a file")
{
    Session session;
    REQUIRE(session.ok);

    const facet::ImageHandle png = facet::LoadImageMemory(kPng, sizeof(kPng));
    REQUIRE(facet::IsImageReady(png));
    CHECK(facet::ImageSize(png).x == 2.0f);
    const facet::ImageHandle jpeg = facet::LoadImageMemory(kJpeg, sizeof(kJpeg));
    REQUIRE(facet::IsImageReady(jpeg));
    CHECK(facet::ImageSize(jpeg).y == 8.0f);
    CHECK_FALSE(facet::LoadImageMemory(kGarbage, sizeof(kGarbage)).IsValid());

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "facet_test_image_sync.png";
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(kPng), sizeof(kPng));
    }
    const std::string utf8 = reinterpret_cast<const char*>(path.u8string().c_str());
    const facet::ImageHandle fromFile = facet::LoadImageFile(utf8.c_str());
    CHECK(facet::IsImageReady(fromFile));
    CHECK_FALSE(facet::LoadImageFile("facet-test-missing-file.png").IsValid());
    std::filesystem::remove(path);
}

TEST_CASE("Asynchronous loads decode on workers and upload on the main thread")
{
    Session session;
    REQUIRE(session.ok);

    const facet::ImageHandle png = facet::LoadImageMemoryAsync(kPng, sizeof(kPng));
    REQUIRE(png.IsValid());
    // Nothing uploads outside NewFrame and WaitForImage, so the handle is still pending.
    CHECK(facet::GetImageStatus(png) == facet::ImageStatus::Loading);
    CHECK_FALSE(facet::ImageFailed(png));
    CHECK(facet::WaitForImage(png, 10.0f));
    CHECK(facet::ImageSize(png).x == 2.0f);

    const facet::ImageHandle broken = facet::LoadImageMemoryAsync(kGarbage, sizeof(kGarbage));
    CHECK_FALSE(facet::WaitForImage(broken, 10.0f));
    CHECK(facet::GetImageStatus(broken) == facet::ImageStatus::Failed);
    CHECK(facet::ImageFailed(broken));

    const facet::ImageHandle missing = facet::LoadImageFileAsync("facet-test-missing-file.png");
    REQUIRE(missing.IsValid());
    CHECK_FALSE(facet::WaitForImage(missing, 10.0f));
    CHECK(facet::GetImageStatus(missing) == facet::ImageStatus::Failed);

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "facet_test_image_async.jpg";
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(kJpeg), sizeof(kJpeg));
    }
    const std::string utf8 = reinterpret_cast<const char*>(path.u8string().c_str());
    const facet::ImageHandle jpeg = facet::LoadImageFileAsync(utf8.c_str());
    // Uploads also happen in NewFrame: run frames until the decode arrives.
    for (int frame = 0; frame < 1000 && facet::GetImageStatus(jpeg) == facet::ImageStatus::Loading; ++frame)
    {
        facet::NewFrame();
        facet::EndFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CHECK(facet::IsImageReady(jpeg));
    std::filesystem::remove(path);

    // A handle released while its decode runs never becomes ready; its slot is reused.
    const facet::ImageHandle dropped = facet::LoadImageMemoryAsync(kPng, sizeof(kPng));
    facet::ReleaseImage(dropped);
    const auto pixels = SolidPixels(1, 1);
    const facet::ImageHandle reuse = facet::CreateImageRGBA(pixels.data(), 1, 1);
    CHECK(reuse.index == dropped.index);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    facet::NewFrame();
    facet::EndFrame();
    CHECK(facet::GetImageStatus(dropped) == facet::ImageStatus::Invalid);
    CHECK(facet::IsImageReady(reuse));
    CHECK(facet::ImageSize(reuse).x == 1.0f);
}

TEST_CASE("Shutdown with decodes in flight stops cleanly")
{
    // Many requests, so several decodes are still queued or running at Shutdown.
    {
        Session session;
        REQUIRE(session.ok);
        for (int i = 0; i < 32; ++i)
            CHECK(facet::LoadImageMemoryAsync(kJpeg, sizeof(kJpeg)).IsValid());
    }
    Session session;
    REQUIRE(session.ok);
    const facet::ImageHandle after = facet::LoadImageMemoryAsync(kPng, sizeof(kPng));
    CHECK(facet::WaitForImage(after, 10.0f));
}

TEST_CASE("Device objects: SVG and file images come back, pixel images are lost")
{
    Session session;
    REQUIRE(session.ok);
    const auto pixels = SolidPixels(2, 2);
    const facet::ImageHandle rgba = facet::CreateImageRGBA(pixels.data(), 2, 2);
    const facet::ImageHandle svg = facet::LoadSvgMemory(kSvg24);
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "facet_test_image_device.png";
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(kPng), sizeof(kPng));
    }
    const std::string utf8 = reinterpret_cast<const char*>(path.u8string().c_str());
    const facet::ImageHandle file = facet::LoadImageFile(utf8.c_str());
    REQUIRE(facet::IsImageReady(rgba));
    REQUIRE(facet::IsImageReady(svg));
    REQUIRE(facet::IsImageReady(file));

    facet::InvalidateDeviceObjects();
    CHECK(facet::GetImageStatus(rgba) == facet::ImageStatus::Lost);
    CHECK(facet::GetImageStatus(svg) == facet::ImageStatus::Lost);
    CHECK(facet::ImageTexture(svg).GetTexID() == ImTextureID_Invalid);
    // No new textures while the device objects are released.
    CHECK_FALSE(facet::CreateImageRGBA(pixels.data(), 2, 2).IsValid());

    REQUIRE(facet::CreateDeviceObjects());
    CHECK(facet::GetImageStatus(rgba) == facet::ImageStatus::Lost);
    CHECK(facet::ImageFailed(rgba));
    CHECK(facet::IsImageReady(svg));
    CHECK(facet::ImageSize(svg).x == 24.0f);
    CHECK(facet::WaitForImage(file, 10.0f));
    std::filesystem::remove(path);
}

TEST_CASE("SVG rasterizes at its design size times the scale")
{
    Session session;
    REQUIRE(session.ok);
    facet::SetUiScale(1.0f);
    facet::NewFrame();
    facet::EndFrame();

    const facet::ImageHandle own = facet::LoadSvgMemory(kSvg24);
    const facet::ImageHandle sized = facet::LoadSvgMemory(kSvg24, 0, ImVec2(40.0f, 0.0f));
    REQUIRE(facet::IsImageReady(own));
    REQUIRE(facet::IsImageReady(sized));
    CHECK(facet::ImageSize(own).x == 24.0f);
    CHECK(facet::ImageSize(own).y == 24.0f);
    CHECK(facet::ImageSize(sized).y == 40.0f);  // aspect ratio kept

    facet::SetUiScale(2.0f);
    facet::NewFrame();  // rasterized again at the start of the next frame
    facet::EndFrame();
    CHECK(facet::ImageSize(own).x == 48.0f);
    CHECK(facet::ImageSize(sized).x == 80.0f);

    facet::SetUiScale(1.5f);
    facet::NewFrame();
    facet::EndFrame();
    CHECK(facet::ImageSize(own).x == 36.0f);

    facet::SetUiScale(1.0f);
    facet::NewFrame();
    facet::EndFrame();

    CHECK_FALSE(facet::LoadSvgMemory("<svg").IsValid());
    CHECK_FALSE(facet::LoadSvgMemory("").IsValid());
    CHECK_FALSE(facet::LoadSvgFile("facet-test-missing-file.svg").IsValid());
}
TEST_CASE("The decoder rejects images beyond the side limit and the pixel budget")
{
    facet::internal::DecodedImage image;
    // The longest side Direct3D 11 can upload still decodes.
    const auto edge = MakeGreyPng(facet::internal::kMaxImageSide, 1);
    REQUIRE(facet::internal::DecodeImage(edge.data(), edge.size(), image));
    CHECK(image.width == facet::internal::kMaxImageSide);
    facet::internal::FreeImage(image);
    // One pixel more, or a far larger side, fails before decoding.
    for (const std::uint32_t width : {16385u, 20000u})
    {
        CAPTURE(width);
        const auto wide = MakeGreyPng(width, 1);
        CHECK_FALSE(facet::internal::DecodeImage(wide.data(), wide.size(), image));
        CHECK(image.pixels == nullptr);
    }
    // Both sides within the limit, but more pixels than the budget.
    const auto many = MakeGreyPng(facet::internal::kMaxImageSide, 4100);
    CHECK_FALSE(facet::internal::DecodeImage(many.data(), many.size(), image));
    CHECK(image.pixels == nullptr);
}

TEST_CASE("The decoder accepts only the documented formats")
{
    // A valid 1 x 1 binary PGM; the PNM format is not part of the documented set.
    const unsigned char pgm[] = {'P', '5', '\n', '1', ' ', '1', '\n', '2', '5', '5', '\n', 0x80};
    facet::internal::DecodedImage image;
    CHECK_FALSE(facet::internal::DecodeImage(pgm, sizeof(pgm), image));
    CHECK(facet::internal::DecodeImage(kJpeg, sizeof(kJpeg), image));
    facet::internal::FreeImage(image);
}

TEST_CASE("Loads refuse files and memory beyond the size limit")
{
    Session session;
    REQUIRE(session.ok);
    const std::size_t tooLarge = facet::internal::kMaxEncodedImageBytes + 1;
    // Refused before the bytes are read or copied: the buffer is far shorter than the size.
    CHECK_FALSE(facet::LoadImageMemory(kPng, tooLarge).IsValid());
    CHECK_FALSE(facet::LoadImageMemoryAsync(kPng, tooLarge).IsValid());

    // A sparse file of that size: refused without reading it.
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "facet_test_image_large.png";
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(kPng), sizeof(kPng));
    }
    std::filesystem::resize_file(path, tooLarge);
    const std::string utf8 = reinterpret_cast<const char*>(path.u8string().c_str());
    CHECK_FALSE(facet::LoadImageFile(utf8.c_str()).IsValid());
    const facet::ImageHandle async = facet::LoadImageFileAsync(utf8.c_str());
    REQUIRE(async.IsValid());
    CHECK_FALSE(facet::WaitForImage(async, 10.0f));
    CHECK(facet::GetImageStatus(async) == facet::ImageStatus::Failed);
    std::filesystem::remove(path);
}
TEST_CASE("WaitForImage with an unlimited timeout waits for the decode")
{
    Session session;
    REQUIRE(session.ok);
    for (const float timeout : {INFINITY, 1e30f, 1e10f})
    {
        CAPTURE(timeout);
        const facet::ImageHandle image = facet::LoadImageMemoryAsync(kPng, sizeof(kPng));
        REQUIRE(image.IsValid());
        CHECK(facet::WaitForImage(image, timeout));
        facet::ReleaseImage(image);
    }
}
TEST_CASE("An SVG raster beyond the texel budget or the side limit fails")
{
    Session session;
    REQUIRE(session.ok);
    // Within both limits.
    CHECK(facet::LoadSvgMemory(kSvg24, 0, ImVec2(2000.0f, 1.0f)).IsValid());
    // 25 M pixels: above the 16 M budget.
    CHECK_FALSE(facet::LoadSvgMemory(kSvg24, 0, ImVec2(5000.0f, 5000.0f)).IsValid());
    // One side above the Direct3D limit: fails instead of being squeezed out of shape.
    CHECK_FALSE(facet::LoadSvgMemory(kSvg24, 0, ImVec2(20000.0f, 1.0f)).IsValid());
}
TEST_CASE("The icon atlas stays within its page budget and refills after a frame start")
{
    Session session;
    REQUIRE(session.ok);
    ImDrawList drawList(ImGui::GetDrawListSharedData());
    drawList._ResetForNewFrame();
    drawList.PushClipRect(ImVec2(0.0f, 0.0f), ImVec2(800.0f, 600.0f));
    facet::NewFrame();
    // Every size from 1 to 256 pixels: more glyphs than four pages hold.
    for (int side = 1; side <= 256; ++side)
        facet::DrawIcon(&drawList, facet::Icon("check"), ImVec2(0.0f, 0.0f), static_cast<float>(side), IM_COL32_WHITE);
    CHECK(facet::internal::IconAtlasPageCount() <= 4);
    // The next frame start empties the atlas; sizes in use are drawn again from there.
    facet::NewFrame();
    CHECK(facet::internal::IconAtlasPageCount() == 0);
    const int vertices = drawList.VtxBuffer.Size;
    facet::DrawIcon(&drawList, facet::Icon("check"), ImVec2(0.0f, 0.0f), 16.0f, IM_COL32_WHITE);
    CHECK(facet::internal::IconAtlasPageCount() == 1);
    // A textured quad, not the crossed placeholder box.
    CHECK(drawList.VtxBuffer.Size - vertices == 4);
}

namespace
{

// For n = 1, 2, ... makes the n-th guarded allocation of `load` throw std::bad_alloc, until
// a call gets through without reaching it. Every call that hit the failure must return an
// invalid handle and leave no slot in use, no slot loading and no queued job behind. The call
// that gets through must load, reusing the slots the failed calls gave back.
template <typename Load>
void CheckLoadSurvivesFailedAllocations(Load load, bool async)
{
    const facet::internal::ImageCacheCounts before = facet::internal::GetImageCacheCounts();
    int failures = 0;
    for (int n = 1; n <= 16; ++n)
    {
        CAPTURE(n);
        facet::internal::FailGuardedAllocation(n);
        facet::ImageHandle handle;
        CHECK_NOTHROW(handle = load());
        const bool failed = facet::internal::GuardedAllocationFailed();
        facet::internal::FailGuardedAllocation(0);
        const facet::internal::ImageCacheCounts after = facet::internal::GetImageCacheCounts();
        if (failed)
        {
            ++failures;
            CHECK_FALSE(handle.IsValid());
            CHECK(after.inUse == before.inUse);
            CHECK(after.loading == before.loading);
            CHECK(after.queuedJobs == before.queuedJobs);
            continue;
        }
        REQUIRE(handle.IsValid());
        CHECK(after.slots <= before.slots + 1);
        if (async)
            CHECK(facet::WaitForImage(handle, 10.0f));
        CHECK(facet::IsImageReady(handle));
        facet::ReleaseImage(handle);
        CHECK(failures > 0);
        return;
    }
    FAIL("the load never got past its guarded allocations");
}

std::string WriteTempFile(const char* name, const void* data, std::size_t size)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    {
        std::ofstream file(path, std::ios::binary);
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    }
    return reinterpret_cast<const char*>(path.u8string().c_str());
}

} // namespace

TEST_CASE("Image loads return an invalid handle when an allocation fails")
{
    Session session;
    REQUIRE(session.ok);
    const std::string png = WriteTempFile("facet_test_alloc.png", kPng, sizeof(kPng));
    const std::string svg = WriteTempFile("facet_test_alloc.svg", kSvg24, sizeof(kSvg24) - 1);
    const auto pixels = SolidPixels(2, 2);

    SUBCASE("LoadImageFile")
    {
        CheckLoadSurvivesFailedAllocations([&] { return facet::LoadImageFile(png.c_str()); }, false);
    }
    SUBCASE("LoadImageMemory")
    {
        CheckLoadSurvivesFailedAllocations([&] { return facet::LoadImageMemory(kPng, sizeof(kPng)); }, false);
    }
    SUBCASE("LoadImageFileAsync")
    {
        CheckLoadSurvivesFailedAllocations([&] { return facet::LoadImageFileAsync(png.c_str()); }, true);
    }
    SUBCASE("LoadImageMemoryAsync")
    {
        CheckLoadSurvivesFailedAllocations([&] { return facet::LoadImageMemoryAsync(kPng, sizeof(kPng)); }, true);
    }
    SUBCASE("CreateImageRGBA")
    {
        CheckLoadSurvivesFailedAllocations([&] { return facet::CreateImageRGBA(pixels.data(), 2, 2); }, false);
    }
    SUBCASE("LoadSvgFile")
    {
        CheckLoadSurvivesFailedAllocations([&] { return facet::LoadSvgFile(svg.c_str()); }, false);
    }
    SUBCASE("LoadSvgMemory")
    {
        CheckLoadSurvivesFailedAllocations([&] { return facet::LoadSvgMemory(kSvg24); }, false);
    }
    std::filesystem::remove(png);
    std::filesystem::remove(svg);
}

TEST_CASE("Releasing images after failed loads needs no allocation")
{
    Session session;
    REQUIRE(session.ok);
    // Slots handed out before a failure are still released cleanly, and their textures go
    // through the release queue whose room was reserved when the slots were taken.
    std::vector<facet::ImageHandle> handles;
    for (int i = 0; i < 8; ++i)
        handles.push_back(facet::LoadImageMemory(kPng, sizeof(kPng)));
    facet::internal::FailGuardedAllocation(1);
    CHECK_FALSE(facet::LoadImageMemory(kPng, sizeof(kPng)).IsValid());
    CHECK(facet::internal::GuardedAllocationFailed());
    facet::internal::FailGuardedAllocation(0);
    for (const facet::ImageHandle handle : handles)
    {
        CHECK(facet::IsImageReady(handle));
        CHECK_NOTHROW(facet::ReleaseImage(handle));
    }
    CHECK(facet::internal::GetImageCacheCounts().inUse == 0);
    facet::NewFrame();
    facet::EndFrame();
    CHECK(facet::IsImageReady(facet::LoadImageMemory(kPng, sizeof(kPng))));
}
