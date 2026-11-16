// Effects: Gaussian kernel, baked shadow tiles, 9-slice geometry, atlas packing and cache
// keys, the capture format table, and the backdrop blur on a WARP device (state save and
// restore, alignment of the result, multisampled and sRGB targets, failure paths).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <wrl/client.h>

#include <facet/facet.h>

#include "effects/blur_formats.h"
#include "effects/blur_pass.h"
#include "effects/d3d11_state.h"
#include "effects/shadow_atlas.h"
#include "effects/shadow_math.h"

using Microsoft::WRL::ComPtr;
namespace fi = facet::internal;

namespace
{

float TileAt(const std::vector<float>& tile, int size, int x, int y)
{
    return tile[static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)];
}

struct Warp
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;

    Warp()
    {
        D3D_FEATURE_LEVEL level = {};
        D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                          device.GetAddressOf(), &level, context.GetAddressOf());
    }
};

struct RenderTarget
{
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> view;
    UINT width = 0;
    UINT height = 0;
};

// A render target without the shader-resource bind flag, like a host's swap chain buffer.
RenderTarget MakeTarget(ID3D11Device* device, UINT width, UINT height, DXGI_FORMAT textureFormat,
                        DXGI_FORMAT viewFormat = DXGI_FORMAT_UNKNOWN, UINT samples = 1)
{
    RenderTarget target;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = textureFormat;
    desc.SampleDesc.Count = samples;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, target.texture.GetAddressOf())))
        return {};
    D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
    viewDesc.Format = viewFormat;
    viewDesc.ViewDimension = samples > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
    if (FAILED(device->CreateRenderTargetView(target.texture.Get(), viewFormat == DXGI_FORMAT_UNKNOWN ? nullptr : &viewDesc,
                                              target.view.GetAddressOf())))
        return {};
    target.width = width;
    target.height = height;
    return target;
}

void BindTarget(ID3D11DeviceContext* context, const RenderTarget& target)
{
    ID3D11RenderTargetView* view = target.view.Get();
    context->OMSetRenderTargets(1, &view, nullptr);
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(target.width);
    viewport.Height = static_cast<float>(target.height);
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
}

float HalfToFloat(std::uint16_t half)
{
    const std::uint32_t sign = (half >> 15) & 1u;
    const std::uint32_t exponent = (half >> 10) & 0x1Fu;
    const std::uint32_t mantissa = half & 0x3FFu;
    float value = 0.0f;
    if (exponent == 0)
        value = std::ldexp(static_cast<float>(mantissa), -24);
    else if (exponent == 31)
        value = mantissa == 0 ? INFINITY : NAN;
    else
        value = std::ldexp(static_cast<float>(mantissa | 0x400u), static_cast<int>(exponent) - 25);
    return sign != 0 ? -value : value;
}

// Red channel of the blur output, row-major.
std::vector<float> ReadOutputRed(ID3D11Device* device, ID3D11DeviceContext* context, const fi::BlurPass& pass)
{
    ComPtr<ID3D11Resource> resource;
    pass.OutputView()->GetResource(resource.GetAddressOf());
    ComPtr<ID3D11Texture2D> texture;
    resource.As(&texture);
    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())));
    context->CopyResource(staging.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    REQUIRE(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)));
    std::vector<float> red(static_cast<size_t>(desc.Width) * desc.Height);
    for (UINT y = 0; y < desc.Height; ++y)
    {
        const auto* row = reinterpret_cast<const std::uint16_t*>(static_cast<const std::uint8_t*>(mapped.pData) +
                                                                  static_cast<size_t>(mapped.RowPitch) * y);
        for (UINT x = 0; x < desc.Width; ++x)
            red[static_cast<size_t>(y) * desc.Width + x] = HalfToFloat(row[x * 4]);
    }
    context->Unmap(staging.Get(), 0);
    return red;
}

// Fills an RGBA8 target: white where inside(x, y) is true, black elsewhere.
template <typename Inside>
void FillPattern(ID3D11DeviceContext* context, const RenderTarget& target, Inside inside)
{
    std::vector<std::uint32_t> pixels(static_cast<size_t>(target.width) * target.height);
    for (UINT y = 0; y < target.height; ++y)
    {
        for (UINT x = 0; x < target.width; ++x)
            pixels[static_cast<size_t>(y) * target.width + x] = inside(x, y) ? 0xFFFFFFFFu : 0xFF000000u;
    }
    context->UpdateSubresource(target.texture.Get(), 0, nullptr, pixels.data(), target.width * 4, 0);
}

ComPtr<ID3DBlob> CompileTestShader(const char* source, const char* entry, const char* profile)
{
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> errors;
    D3DCompile(source, std::strlen(source), "test", nullptr, nullptr, entry, profile, 0, 0, blob.GetAddressOf(),
               errors.GetAddressOf());
    return blob;
}

} // namespace

TEST_CASE("Gaussian kernel is normalized, symmetric and three sigma wide")
{
    std::vector<float> kernel;
    fi::GaussianKernel(0.0f, kernel);
    REQUIRE(kernel.size() == 1);
    CHECK(kernel[0] == doctest::Approx(1.0f));

    for (const float sigma : {0.5f, 1.0f, 2.5f, 8.0f, 20.0f})
    {
        CAPTURE(sigma);
        fi::GaussianKernel(sigma, kernel);
        const int extent = fi::GaussianExtent(sigma);
        CHECK(extent == static_cast<int>(std::ceil(3.0f * sigma)));
        REQUIRE(kernel.size() == static_cast<size_t>(extent * 2 + 1));
        float sum = 0.0f;
        float variance = 0.0f;
        for (int i = 0; i < static_cast<int>(kernel.size()); ++i)
        {
            sum += kernel[static_cast<size_t>(i)];
            const float d = static_cast<float>(i - extent);
            variance += kernel[static_cast<size_t>(i)] * d * d;
            CHECK(kernel[static_cast<size_t>(i)] == doctest::Approx(kernel[kernel.size() - 1 - static_cast<size_t>(i)]));
            if (i > 0 && i <= extent)
                CHECK(kernel[static_cast<size_t>(i)] >= kernel[static_cast<size_t>(i - 1)]);
        }
        CHECK(sum == doctest::Approx(1.0f).epsilon(1e-5));
        // Sampling at integers keeps the variance close to sigma^2 (exact for wide kernels).
        if (sigma >= 1.0f)
            CHECK(variance == doctest::Approx(sigma * sigma).epsilon(0.03));
    }
}

TEST_CASE("Shadow keys quantize radius and sigma and pack every field")
{
    const fi::ShadowKey key = fi::MakeShadowKey(10.4f, 3.2f, false);
    CHECK(key.radius == 10);
    CHECK(key.sigmaSteps == 6);
    CHECK(key.Sigma() == doctest::Approx(3.0f));
    CHECK(fi::MakeShadowKey(10.6f, 3.3f, false).radius == 11);
    CHECK(fi::MakeShadowKey(10.4f, 3.3f, false).sigmaSteps == 7);

    // Nearby values share a key; the flag and each field change it.
    CHECK(fi::MakeShadowKey(10.2f, 3.1f, false).Packed() == key.Packed());
    CHECK(fi::MakeShadowKey(10.4f, 3.2f, true).Packed() != key.Packed());
    CHECK(fi::MakeShadowKey(11.0f, 3.2f, false).Packed() != key.Packed());
    CHECK(fi::MakeShadowKey(10.4f, 3.5f, false).Packed() != key.Packed());
    CHECK(fi::MakeShadowKey(3.0f, 10.0f, false).Packed() != fi::MakeShadowKey(10.0f, 3.0f, false).Packed());

    // Clamped to the limits; invalid input becomes zero.
    CHECK(fi::MakeShadowKey(1000.0f, 1000.0f, false).radius == fi::kMaxShadowRadius);
    CHECK(fi::MakeShadowKey(1000.0f, 1000.0f, false).Sigma() == doctest::Approx(fi::kMaxShadowSigma));
    CHECK(fi::MakeShadowKey(-4.0f, NAN, false).radius == 0);
    CHECK(fi::MakeShadowKey(-4.0f, NAN, false).sigmaSteps == 0);
}

TEST_CASE("Baked shadow tile holds the blurred corner and the edge profile")
{
    const fi::ShadowKey key = fi::MakeShadowKey(8.0f, 4.0f, false);
    const fi::ShadowTileLayout layout = fi::TileLayout(key);
    CHECK(layout.margin == 12);
    CHECK(layout.size == 8 + 2 * 12);

    std::vector<float> tile;
    fi::BakeShadowTile(key, tile);
    REQUIRE(tile.size() == static_cast<size_t>(layout.size * layout.size));
    const int last = layout.size - 1;
    for (const float value : tile)
    {
        CHECK(value >= 0.0f);
        CHECK(value <= 1.0f);
    }
    // Far outside is empty, the interior is full.
    CHECK(TileAt(tile, layout.size, 0, 0) < 0.01f);
    CHECK(TileAt(tile, layout.size, last, last) > 0.99f);
    // The last column is the straight edge profile: half strength on the edge itself,
    // rising monotonically into the shape. The last row is the same profile.
    CHECK(TileAt(tile, layout.size, last, layout.margin) == doctest::Approx(0.5f).epsilon(0.08));
    for (int i = 1; i < layout.size; ++i)
    {
        CHECK(TileAt(tile, layout.size, last, i) >= TileAt(tile, layout.size, last, i - 1) - 1e-6f);
        CHECK(TileAt(tile, layout.size, i, last) == doctest::Approx(TileAt(tile, layout.size, last, i)).epsilon(1e-4));
    }
    // The rounded corner is weaker than the straight edge at the same depth.
    CHECK(TileAt(tile, layout.size, layout.margin, layout.margin) < TileAt(tile, layout.size, last, layout.margin));

    SUBCASE("glow tiles are empty inside the shape and match outside it")
    {
        std::vector<float> glow;
        fi::BakeShadowTile(fi::MakeShadowKey(8.0f, 4.0f, true), glow);
        CHECK(TileAt(glow, layout.size, last, last) == doctest::Approx(0.0f));
        CHECK(TileAt(glow, layout.size, 2, 2) == doctest::Approx(TileAt(tile, layout.size, 2, 2)));
        CHECK(TileAt(glow, layout.size, last, layout.margin - 2) ==
              doctest::Approx(TileAt(tile, layout.size, last, layout.margin - 2)));
    }

    SUBCASE("sigma 0 gives a hard edge")
    {
        const fi::ShadowKey hard = fi::MakeShadowKey(0.0f, 0.0f, false);
        const fi::ShadowTileLayout hardLayout = fi::TileLayout(hard);
        CHECK(hardLayout.margin == 1);
        CHECK(hardLayout.size == 2);
        std::vector<float> hardTile;
        fi::BakeShadowTile(hard, hardTile);
        CHECK(TileAt(hardTile, 2, 1, 0) == doctest::Approx(0.0f));
        CHECK(TileAt(hardTile, 2, 1, 1) == doctest::Approx(1.0f));
    }
}

TEST_CASE("Shadow 9-slice maps the tile 1:1 and mirrors it")
{
    fi::ShadowTileLayout layout;
    layout.margin = 6;
    layout.size = 20;
    const ImVec2 tileUv(0.25f, 0.5f);
    const ImVec2 texel(1.0f / 1024.0f, 1.0f / 1024.0f);
    const fi::NineSlice grid = fi::ShadowNineSlice(ImVec2(100.0f, 50.0f), ImVec2(300.0f, 150.0f), layout, tileUv, texel);
    CHECK(grid.x[0] == doctest::Approx(94.0f));
    CHECK(grid.x[1] == doctest::Approx(114.0f));
    CHECK(grid.x[2] == doctest::Approx(286.0f));
    CHECK(grid.x[3] == doctest::Approx(306.0f));
    CHECK(grid.y[0] == doctest::Approx(44.0f));
    CHECK(grid.y[3] == doctest::Approx(156.0f));
    CHECK(grid.u[0] == doctest::Approx(0.25f));
    CHECK(grid.u[3] == doctest::Approx(grid.u[0]));
    CHECK(grid.u[1] == doctest::Approx(0.25f + 20.0f / 1024.0f));
    CHECK(grid.u[2] == doctest::Approx(grid.u[1]));
    CHECK(grid.v[1] == doctest::Approx(0.5f + 20.0f / 1024.0f));
    // One texel per pixel in the corners.
    CHECK((grid.u[1] - grid.u[0]) / texel.x == doctest::Approx(grid.x[1] - grid.x[0]));

    SUBCASE("small shapes shrink the corners to half the shadow")
    {
        // ("small" is a Windows header macro.)
        const fi::NineSlice narrow = fi::ShadowNineSlice(ImVec2(0.0f, 0.0f), ImVec2(10.0f, 40.0f), layout, tileUv, texel);
        // Shadow width 10 + 12 = 22, so each corner is 11 wide and they meet in the middle.
        CHECK(narrow.x[1] == doctest::Approx(5.0f));
        CHECK(narrow.x[2] == doctest::Approx(5.0f));
        CHECK(narrow.u[1] == doctest::Approx(0.25f + 11.0f / 1024.0f));
        CHECK(narrow.y[1] == doctest::Approx(14.0f));
    }
}

TEST_CASE("Shelf packer places rectangles without overlap and reports a full page")
{
    fi::ShelfPacker packer;
    packer.Reset(64, 64);
    struct Rect
    {
        int x, y, w, h;
    };
    std::vector<Rect> placed;
    const int sizes[][2] = {{20, 20}, {20, 10}, {30, 20}, {10, 10}, {64, 8}, {16, 16}, {40, 12}};
    for (const auto& size : sizes)
    {
        int x = -1;
        int y = -1;
        if (!packer.Allocate(size[0], size[1], x, y))
            continue;
        CHECK(x >= 0);
        CHECK(y >= 0);
        CHECK(x + size[0] <= 64);
        CHECK(y + size[1] <= 64);
        for (const Rect& other : placed)
        {
            const bool overlap = x < other.x + other.w && other.x < x + size[0] && y < other.y + other.h &&
                                 other.y < y + size[1];
            CHECK_FALSE(overlap);
        }
        placed.push_back({x, y, size[0], size[1]});
    }
    CHECK(placed.size() >= 5);

    int x = 0;
    int y = 0;
    CHECK_FALSE(packer.Allocate(65, 1, x, y));
    CHECK_FALSE(packer.Allocate(0, 4, x, y));
    while (packer.Allocate(16, 16, x, y))
    {
    }
    CHECK_FALSE(packer.Allocate(16, 16, x, y));
    packer.Reset(64, 64);
    CHECK(packer.Allocate(64, 64, x, y));
    CHECK(x == 0);
    CHECK(y == 0);
}

TEST_CASE("Blur plan reaches the requested radius")
{
    for (const float radius : {4.0f, 8.0f, 13.0f, 20.0f, 40.0f, 64.0f, 100.0f})
    {
        CAPTURE(radius);
        const fi::BlurPlan plan = fi::PlanBlur(radius);
        CHECK(plan.downsamples >= 1);
        CHECK(plan.downsamples <= fi::kMaxBlurDownsamples);
        CHECK(plan.offset * static_cast<float>(1 << (plan.downsamples + 1)) == doctest::Approx(radius));
        CHECK(plan.offset >= 1.0f);
        CHECK(plan.offset < 2.0f);
    }
    CHECK(fi::PlanBlur(0.0f).downsamples == 1);
    CHECK(fi::PlanBlur(0.0f).offset == doctest::Approx(0.0f));
    CHECK(fi::PlanBlur(1000.0f, 3).downsamples == 3);
    CHECK(fi::PlanBlur(NAN).offset == doctest::Approx(0.0f));
}

TEST_CASE("Capture format table")
{
    struct Row
    {
        DXGI_FORMAT resource;
        DXGI_FORMAT view;
        bool supported;
        DXGI_FORMAT copy;
        DXGI_FORMAT read;
        bool srgb;
    };
    const Row rows[] = {
        {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, true, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, false},
        {DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_UNKNOWN, true, DXGI_FORMAT_R8G8B8A8_TYPELESS,
         DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true, DXGI_FORMAT_R8G8B8A8_TYPELESS,
         DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, true},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM, true, DXGI_FORMAT_R8G8B8A8_TYPELESS,
         DXGI_FORMAT_R8G8B8A8_UNORM, false},
        {DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_UNKNOWN, true, DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM, false},
        {DXGI_FORMAT_B8G8R8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, true, DXGI_FORMAT_B8G8R8A8_TYPELESS,
         DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, true},
        {DXGI_FORMAT_B8G8R8X8_UNORM, DXGI_FORMAT_UNKNOWN, true, DXGI_FORMAT_B8G8R8X8_TYPELESS, DXGI_FORMAT_B8G8R8X8_UNORM, false},
        {DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_UNKNOWN, true, DXGI_FORMAT_R10G10B10A2_TYPELESS,
         DXGI_FORMAT_R10G10B10A2_UNORM, false},
        {DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_UNKNOWN, true, DXGI_FORMAT_R16G16B16A16_TYPELESS,
         DXGI_FORMAT_R16G16B16A16_FLOAT, false},
        {DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_UNKNOWN, true, DXGI_FORMAT_R11G11B10_FLOAT, DXGI_FORMAT_R11G11B10_FLOAT, false},
        // A typeless target needs a typed view; integer and depth formats cannot be filtered;
        // a view from another format group is invalid.
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_UNKNOWN, false, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false},
        {DXGI_FORMAT_R8G8B8A8_UINT, DXGI_FORMAT_UNKNOWN, false, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false},
        {DXGI_FORMAT_D24_UNORM_S8_UINT, DXGI_FORMAT_UNKNOWN, false, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false},
        {DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_B8G8R8A8_UNORM, false, DXGI_FORMAT_UNKNOWN, DXGI_FORMAT_UNKNOWN, false},
    };
    for (const Row& row : rows)
    {
        CAPTURE(static_cast<int>(row.resource));
        CAPTURE(static_cast<int>(row.view));
        const fi::CaptureFormat format = fi::ChooseCaptureFormat(row.resource, row.view);
        CHECK(format.supported == row.supported);
        if (!row.supported)
            continue;
        CHECK(format.copyFormat == row.copy);
        CHECK(format.viewFormat == row.read);
        CHECK(format.resolveFormat == row.read);
        CHECK(format.srgb == row.srgb);
    }
}

TEST_CASE("Shadow atlas caches tiles by key on a WARP device")
{
    Warp warp;
    REQUIRE(warp.device);
    fi::ShadowAtlas atlas;
    CHECK(atlas.Acquire(fi::MakeShadowKey(8.0f, 4.0f, false)) == nullptr);  // no device yet

    atlas.SetDevice(warp.device.Get(), warp.context.Get());
    const fi::ShadowTile* first = atlas.Acquire(fi::MakeShadowKey(8.0f, 4.0f, false));
    REQUIRE(first != nullptr);
    CHECK(atlas.BakeCount() == 1);
    CHECK(atlas.PageCount() == 1);
    CHECK(first->texture != 0);
    CHECK(first->uv.x > 0.0f);
    CHECK(first->uv.x + static_cast<float>(first->layout.size) * first->texelUv.x <= 1.0f);

    // The same key (after quantization) is a cache hit.
    CHECK(atlas.Acquire(fi::MakeShadowKey(8.2f, 4.1f, false)) == first);
    CHECK(atlas.BakeCount() == 1);

    const fi::ShadowTile* glow = atlas.Acquire(fi::MakeShadowKey(8.0f, 4.0f, true));
    REQUIRE(glow != nullptr);
    CHECK(glow != first);
    CHECK(atlas.BakeCount() == 2);
    CHECK(atlas.TileCount() == 2);
    // Tiles of one page do not overlap.
    const bool apart = glow->uv.x >= first->uv.x + static_cast<float>(first->layout.size + 1) * first->texelUv.x ||
                       glow->uv.y >= first->uv.y + static_cast<float>(first->layout.size + 1) * first->texelUv.y;
    CHECK(apart);

    // Many sizes fill more pages, never more than the limit.
    for (int radius = 0; radius <= 120; radius += 4)
        atlas.Acquire(fi::MakeShadowKey(static_cast<float>(radius), 40.0f, false));
    CHECK(atlas.PageCount() <= fi::kShadowAtlasMaxPages);

    atlas.Clear();
    CHECK(atlas.TileCount() == 0);
    const int pages = atlas.PageCount();
    CHECK(atlas.Acquire(fi::MakeShadowKey(8.0f, 4.0f, false)) != nullptr);
    CHECK(atlas.PageCount() == pages);

    atlas.ReleaseDevice();
    CHECK(atlas.PageCount() == 0);
    CHECK(atlas.Acquire(fi::MakeShadowKey(8.0f, 4.0f, false)) == nullptr);
}

TEST_CASE("Backdrop blur restores every state it touches")
{
    Warp warp;
    REQUIRE(warp.device);
    ID3D11Device* device = warp.device.Get();
    ID3D11DeviceContext* context = warp.context.Get();

    fi::BlurPass pass;
    REQUIRE(pass.CreateDeviceObjects(device));
    REQUIRE(pass.EnsureOutput(128, 96, 1) != nullptr);

    // Known state in every slot the pass uses, with values that differ from its own.
    const RenderTarget target = MakeTarget(device, 160, 120, DXGI_FORMAT_R8G8B8A8_UNORM);
    const RenderTarget second = MakeTarget(device, 160, 120, DXGI_FORMAT_B8G8R8A8_UNORM);
    REQUIRE(target.view);
    REQUIRE(second.view);
    ID3D11RenderTargetView* views[2] = {target.view.Get(), second.view.Get()};
    context->OMSetRenderTargets(2, views, nullptr);
    D3D11_VIEWPORT viewports[2] = {{16.0f, 12.0f, 128.0f, 96.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 32.0f, 32.0f, 0.25f, 0.75f}};
    context->RSSetViewports(2, viewports);
    const D3D11_RECT scissors[3] = {{1, 2, 30, 40}, {5, 6, 70, 80}, {0, 0, 10, 10}};
    context->RSSetScissorRects(3, scissors);

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED;
    ComPtr<ID3D11BlendState> blend;
    REQUIRE(SUCCEEDED(device->CreateBlendState(&blendDesc, blend.GetAddressOf())));
    const float factor[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    context->OMSetBlendState(blend.Get(), factor, 0x00FF00FFu);

    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = TRUE;
    depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    depthDesc.DepthFunc = D3D11_COMPARISON_GREATER;
    ComPtr<ID3D11DepthStencilState> depth;
    REQUIRE(SUCCEEDED(device->CreateDepthStencilState(&depthDesc, depth.GetAddressOf())));
    context->OMSetDepthStencilState(depth.Get(), 7);

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    rasterDesc.CullMode = D3D11_CULL_FRONT;
    rasterDesc.ScissorEnable = TRUE;
    ComPtr<ID3D11RasterizerState> raster;
    REQUIRE(SUCCEEDED(device->CreateRasterizerState(&rasterDesc, raster.GetAddressOf())));
    context->RSSetState(raster.Get());

    const char* shaderSource = "cbuffer C : register(b0) { float4 k; };"
                               "Texture2D t : register(t0); SamplerState s : register(s0);"
                               "float4 Vs(float2 p : POSITION) : SV_Position { return float4(p, 0, 1) + k; }"
                               "float4 Ps(float4 p : SV_Position) : SV_Target { return t.Sample(s, p.xy) + k; }";
    const ComPtr<ID3DBlob> vsBlob = CompileTestShader(shaderSource, "Vs", "vs_4_0");
    const ComPtr<ID3DBlob> psBlob = CompileTestShader(shaderSource, "Ps", "ps_4_0");
    REQUIRE(vsBlob);
    REQUIRE(psBlob);
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    REQUIRE(SUCCEEDED(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vs.GetAddressOf())));
    REQUIRE(SUCCEEDED(device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, ps.GetAddressOf())));
    context->VSSetShader(vs.Get(), nullptr, 0);
    context->PSSetShader(ps.Get(), nullptr, 0);

    const D3D11_INPUT_ELEMENT_DESC element = {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0};
    ComPtr<ID3D11InputLayout> layout;
    REQUIRE(SUCCEEDED(device->CreateInputLayout(&element, 1, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                                layout.GetAddressOf())));
    context->IASetInputLayout(layout.Get());
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = 256;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    ComPtr<ID3D11Buffer> vertexBuffer;
    REQUIRE(SUCCEEDED(device->CreateBuffer(&bufferDesc, nullptr, vertexBuffer.GetAddressOf())));
    bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ComPtr<ID3D11Buffer> indexBuffer;
    REQUIRE(SUCCEEDED(device->CreateBuffer(&bufferDesc, nullptr, indexBuffer.GetAddressOf())));
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    ComPtr<ID3D11Buffer> constantBuffer;
    REQUIRE(SUCCEEDED(device->CreateBuffer(&bufferDesc, nullptr, constantBuffer.GetAddressOf())));
    ID3D11Buffer* vb = vertexBuffer.Get();
    const UINT stride = 8;
    const UINT offset = 16;
    context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 4);
    ID3D11Buffer* cb = constantBuffer.Get();
    context->VSSetConstantBuffers(0, 1, &cb);
    context->PSSetConstantBuffers(0, 1, &cb);

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    ComPtr<ID3D11SamplerState> sampler;
    REQUIRE(SUCCEEDED(device->CreateSamplerState(&samplerDesc, sampler.GetAddressOf())));
    ID3D11SamplerState* samplerPtr = sampler.Get();
    context->PSSetSamplers(0, 1, &samplerPtr);

    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = textureDesc.Height = 4;
    textureDesc.MipLevels = textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> texture;
    REQUIRE(SUCCEEDED(device->CreateTexture2D(&textureDesc, nullptr, texture.GetAddressOf())));
    ComPtr<ID3D11ShaderResourceView> textureView;
    REQUIRE(SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, textureView.GetAddressOf())));
    ID3D11ShaderResourceView* srv = textureView.Get();
    context->PSSetShaderResources(0, 1, &srv);

    fi::D3D11StateBlock before;
    before.Capture(context);
    CHECK(before.viewportCount == 2);
    CHECK(before.scissorCount == 3);

    fi::BlurRequest request;
    request.radius = 12.0f;
    request.saturation = 1.4f;
    request.stamp = 0;
    CHECK(pass.Run(context, request) == fi::BlurStatus::Blurred);

    fi::D3D11StateBlock after;
    after.Capture(context);
    CHECK(after.SameAs(before));
    CHECK(after.renderTargets[1].Get() == second.view.Get());
    CHECK(after.viewports[0].TopLeftX == doctest::Approx(16.0f));
    CHECK(after.pixelResource.Get() == srv);

    SUBCASE("a failed capture changes no state either")
    {
        context->OMSetRenderTargets(0, nullptr, nullptr);
        fi::D3D11StateBlock empty;
        empty.Capture(context);
        CHECK(pass.Run(context, request) == fi::BlurStatus::NoRenderTarget);
        fi::D3D11StateBlock still;
        still.Capture(context);
        CHECK(still.SameAs(empty));
    }
}

TEST_CASE("Backdrop blur output is aligned with the viewport and not flipped")
{
    Warp warp;
    REQUIRE(warp.device);
    ID3D11Device* device = warp.device.Get();
    ID3D11DeviceContext* context = warp.context.Get();
    fi::BlurPass pass;
    REQUIRE(pass.CreateDeviceObjects(device));
    const RenderTarget target = MakeTarget(device, 256, 128, DXGI_FORMAT_R8G8B8A8_UNORM);
    REQUIRE(target.view);
    REQUIRE(pass.EnsureOutput(256, 128, 1) != nullptr);
    BindTarget(context, target);

    fi::BlurRequest request;
    request.radius = 8.0f;
    request.saturation = 1.0f;

    SUBCASE("left half white")
    {
        FillPattern(context, target, [](UINT x, UINT) { return x < 128; });
        REQUIRE(pass.Run(context, request) == fi::BlurStatus::Blurred);
        const std::vector<float> red = ReadOutputRed(device, context, pass);
        const auto at = [&](int x, int y) { return red[static_cast<size_t>(y) * 256 + static_cast<size_t>(x)]; };
        CHECK(at(10, 64) > 0.95f);
        CHECK(at(245, 64) < 0.05f);
        // The edge stays at x = 128: blurred to about half there, symmetric around it.
        CHECK(at(127, 64) == doctest::Approx(0.5f).epsilon(0.2));
        CHECK(at(118, 64) > at(127, 64));
        CHECK(at(138, 64) < at(128, 64));
        CHECK(at(118, 64) == doctest::Approx(1.0f - at(137, 64)).epsilon(0.15));
        // It is a blur, not a hard edge.
        CHECK(at(124, 64) < 0.97f);
        CHECK(at(131, 64) > 0.03f);
    }

    SUBCASE("top half white stays on top")
    {
        FillPattern(context, target, [](UINT, UINT y) { return y < 64; });
        REQUIRE(pass.Run(context, request) == fi::BlurStatus::Blurred);
        const std::vector<float> red = ReadOutputRed(device, context, pass);
        CHECK(red[static_cast<size_t>(10) * 256 + 128] > 0.95f);
        CHECK(red[static_cast<size_t>(118) * 256 + 128] < 0.05f);
    }

    SUBCASE("a viewport inside the target maps onto the whole output")
    {
        // White only in the right half of the viewport x = 64..192.
        FillPattern(context, target, [](UINT x, UINT) { return x >= 128 && x < 192; });
        D3D11_VIEWPORT viewport = {64.0f, 0.0f, 128.0f, 128.0f, 0.0f, 1.0f};
        context->RSSetViewports(1, &viewport);
        REQUIRE(pass.Run(context, request) == fi::BlurStatus::Blurred);
        const std::vector<float> red = ReadOutputRed(device, context, pass);
        CHECK(red[static_cast<size_t>(64) * 256 + 20] < 0.05f);
        CHECK(red[static_cast<size_t>(64) * 256 + 236] > 0.95f);
        CHECK(red[static_cast<size_t>(64) * 256 + 127] == doctest::Approx(0.5f).epsilon(0.25));
    }

    SUBCASE("one blur per stamp and target")
    {
        FillPattern(context, target, [](UINT x, UINT) { return x < 128; });
        request.stamp = 42;
        CHECK(pass.Run(context, request) == fi::BlurStatus::Blurred);
        CHECK(pass.Run(context, request) == fi::BlurStatus::Reused);
        request.stamp = 43;
        CHECK(pass.Run(context, request) == fi::BlurStatus::Blurred);
        CHECK(pass.BlurCount() == 2);
    }
}

TEST_CASE("Backdrop blur reads multisampled, sRGB and typeless targets")
{
    Warp warp;
    REQUIRE(warp.device);
    ID3D11Device* device = warp.device.Get();
    ID3D11DeviceContext* context = warp.context.Get();
    fi::BlurPass pass;
    REQUIRE(pass.CreateDeviceObjects(device));
    REQUIRE(pass.EnsureOutput(64, 64, 1) != nullptr);
    fi::BlurRequest request;
    request.radius = 6.0f;

    SUBCASE("4x multisampled target is resolved")
    {
        UINT quality = 0;
        REQUIRE(SUCCEEDED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, 4, &quality)));
        REQUIRE(quality > 0);
        const RenderTarget target = MakeTarget(device, 64, 64, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_UNKNOWN, 4);
        REQUIRE(target.view);
        BindTarget(context, target);
        const float color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
        context->ClearRenderTargetView(target.view.Get(), color);
        CHECK(pass.Run(context, request) == fi::BlurStatus::Blurred);
        const std::vector<float> red = ReadOutputRed(device, context, pass);
        CHECK(red[32 * 64 + 32] == doctest::Approx(1.0f).epsilon(0.01));
    }

    SUBCASE("typeless target with an sRGB view is read as sRGB")
    {
        const RenderTarget target =
            MakeTarget(device, 64, 64, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
        REQUIRE(target.view);
        BindTarget(context, target);
        // Clearing an sRGB view encodes the linear value 0.5 as about 0.735.
        const float color[4] = {0.5f, 0.5f, 0.5f, 1.0f};
        context->ClearRenderTargetView(target.view.Get(), color);
        CHECK(pass.Run(context, request) == fi::BlurStatus::Blurred);
        CHECK(pass.SourceFormat() == DXGI_FORMAT_R8G8B8A8_TYPELESS);
        const std::vector<float> red = ReadOutputRed(device, context, pass);
        // Read through the sRGB view, the blur sees the linear value again.
        CHECK(red[32 * 64 + 32] == doctest::Approx(0.5f).epsilon(0.02));
    }

    SUBCASE("4x multisampled typeless target with an sRGB view is resolved as sRGB")
    {
        UINT quality = 0;
        REQUIRE(SUCCEEDED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4, &quality)));
        REQUIRE(quality > 0);
        const RenderTarget target =
            MakeTarget(device, 64, 64, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 4);
        REQUIRE(target.view);
        BindTarget(context, target);
        const float color[4] = {0.25f, 0.25f, 0.25f, 1.0f};
        context->ClearRenderTargetView(target.view.Get(), color);
        CHECK(pass.Run(context, request) == fi::BlurStatus::Blurred);
        const std::vector<float> red = ReadOutputRed(device, context, pass);
        CHECK(red[32 * 64 + 32] == doctest::Approx(0.25f).epsilon(0.03));
    }

    SUBCASE("integer target is reported as unsupported")
    {
        const RenderTarget target = MakeTarget(device, 64, 64, DXGI_FORMAT_R8G8B8A8_UINT);
        REQUIRE(target.view);
        BindTarget(context, target);
        CHECK(pass.Run(context, request) == fi::BlurStatus::UnsupportedFormat);
    }

    SUBCASE("no output means not ready, and a device loss releases everything")
    {
        pass.InvalidateDeviceObjects();
        CHECK_FALSE(pass.Ready());
        CHECK(pass.OutputView() == nullptr);
        CHECK(pass.Run(context, request) == fi::BlurStatus::NotReady);
        REQUIRE(pass.CreateDeviceObjects(device));
        CHECK(pass.EnsureOutput(64, 64, 2) != nullptr);
    }
}

TEST_CASE("Backdrop blur and shadow atlas raise no debug layer messages")
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL level = {};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_DEBUG, nullptr, 0,
                                 D3D11_SDK_VERSION, device.GetAddressOf(), &level, context.GetAddressOf())))
    {
        MESSAGE("D3D11 debug layer not installed; skipped");
        return;
    }
    ComPtr<ID3D11InfoQueue> queue;
    REQUIRE(SUCCEEDED(device.As(&queue)));
    queue->ClearStoredMessages();

    fi::BlurPass pass;
    REQUIRE(pass.CreateDeviceObjects(device.Get()));
    REQUIRE(pass.EnsureOutput(96, 64, 1) != nullptr);
    fi::BlurRequest request;
    request.radius = 10.0f;
    for (const UINT samples : {1u, 4u})
    {
        UINT quality = 0;
        if (samples > 1 &&
            (FAILED(device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, samples, &quality)) || quality == 0))
            continue;
        const RenderTarget target =
            MakeTarget(device.Get(), 96, 64, DXGI_FORMAT_R8G8B8A8_TYPELESS, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, samples);
        REQUIRE(target.view);
        BindTarget(context.Get(), target);
        CHECK(pass.Run(context.Get(), request) == fi::BlurStatus::Blurred);
    }
    fi::ShadowAtlas atlas;
    atlas.SetDevice(device.Get(), context.Get());
    CHECK(atlas.Acquire(fi::MakeShadowKey(12.0f, 8.0f, false)) != nullptr);
    context->Flush();

    const UINT64 count = queue->GetNumStoredMessages();
    for (UINT64 i = 0; i < count; ++i)
    {
        SIZE_T size = 0;
        queue->GetMessage(i, nullptr, &size);
        std::vector<char> buffer(size);
        auto* message = reinterpret_cast<D3D11_MESSAGE*>(buffer.data());
        queue->GetMessage(i, message, &size);
        if (message->Severity <= D3D11_MESSAGE_SEVERITY_WARNING)
            FAIL_CHECK(message->pDescription);
    }
}

TEST_CASE("Glass follows device invalidation and recreation")
{
    Warp warp;
    REQUIRE(warp.device);
    ImGui::CreateContext();
    facet::InitDesc desc;
    desc.device = warp.device.Get();
    desc.context = warp.context.Get();
    REQUIRE(facet::Init(desc));
    CHECK(facet::GlassAvailable());

    facet::InvalidateDeviceObjects();
    CHECK_FALSE(facet::GlassAvailable());
    // Drawing while the device objects are gone records nothing and does not crash.
    facet::DrawShadow(nullptr, ImVec2(0.0f, 0.0f), ImVec2(10.0f, 10.0f), 2.0f, facet::ElevationShadow(2));
    facet::DrawGlass(nullptr, ImVec2(0.0f, 0.0f), ImVec2(10.0f, 10.0f), 2.0f);

    CHECK(facet::CreateDeviceObjects());
    CHECK(facet::GlassAvailable());
    facet::Shutdown();
    CHECK_FALSE(facet::GlassAvailable());
    ImGui::DestroyContext();
}

TEST_CASE("Glass records its shadow after the backdrop capture")
{
    Warp warp;
    REQUIRE(warp.device);
    ImGui::CreateContext();
    facet::InitDesc desc;
    desc.device = warp.device.Get();
    desc.context = warp.context.Get();
    REQUIRE(facet::Init(desc));
    REQUIRE(facet::GlassAvailable());
    // The output is sized from the main viewport, which only a frame would set.
    ImGui::GetMainViewport()->Size = ImVec2(64.0f, 64.0f);
    // Rounded shapes need the tessellation tables, which a frame start would also set up.
    ImGui::GetDrawListSharedData()->SetCircleTessellationMaxError(ImGui::GetStyle().CircleTessellationMaxError);

    // Draw lists live in this block, so they are gone before the context is destroyed.
    {
        ImDrawList drawList(ImGui::GetDrawListSharedData());
        drawList._ResetForNewFrame();
        drawList.PushClipRect(ImVec2(0.0f, 0.0f), ImVec2(64.0f, 64.0f));
        facet::GlassSpec spec;
        spec.shadow = facet::ElevationShadow(2);
        REQUIRE((spec.shadow.color & IM_COL32_A_MASK) != 0);
        facet::DrawGlass(&drawList, ImVec2(16.0f, 16.0f), ImVec2(48.0f, 48.0f), 4.0f, spec);

        // Nothing is drawn before the capture callback, so the capture cannot hold the shadow.
        int firstCallback = -1;
        unsigned int elementsBefore = 0;
        for (int i = 0; i < drawList.CmdBuffer.Size && firstCallback < 0; ++i)
        {
            if (drawList.CmdBuffer[i].UserCallback != nullptr)
                firstCallback = i;
            else
                elementsBefore += drawList.CmdBuffer[i].ElemCount;
        }
        CHECK(firstCallback >= 0);
        CHECK(elementsBefore == 0);

        // The shadow adds geometry after the capture: more than the same panel without it.
        ImDrawList plain(ImGui::GetDrawListSharedData());
        plain._ResetForNewFrame();
        plain.PushClipRect(ImVec2(0.0f, 0.0f), ImVec2(64.0f, 64.0f));
        facet::DrawGlass(&plain, ImVec2(16.0f, 16.0f), ImVec2(48.0f, 48.0f), 4.0f);
        CHECK(drawList.VtxBuffer.Size > plain.VtxBuffer.Size);
    }

    facet::Shutdown();
    ImGui::DestroyContext();
}

TEST_CASE("Blur output resize retires the old view for a few frames")
{
    Warp warp;
    REQUIRE(warp.device);
    fi::BlurPass pass;
    REQUIRE(pass.CreateDeviceObjects(warp.device.Get()));
    ID3D11ShaderResourceView* first = pass.EnsureOutput(64, 64, 10);
    REQUIRE(first != nullptr);
    CHECK(pass.EnsureOutput(64, 64, 10) == first);
    // Keep a reference only to observe the release; the pass owns the view.
    first->AddRef();
    ID3D11ShaderResourceView* second = pass.EnsureOutput(80, 60, 11);
    REQUIRE(second != nullptr);
    CHECK(second != first);
    CHECK(pass.OutputWidth() == 80);
    pass.ReleaseRetired(12);
    // Still retired one frame later: the pass and this test hold it.
    CHECK(first->AddRef() == 3);
    first->Release();
    pass.ReleaseRetired(13);
    CHECK(first->Release() == 0);
    CHECK(pass.EnsureOutput(0, 10, 13) == nullptr);
}
