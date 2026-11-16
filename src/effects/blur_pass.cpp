#include "effects/blur_pass.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <d3dcompiler.h>

namespace facet::internal
{
namespace
{

using Microsoft::WRL::ComPtr;

// Intermediate and output format: 16-bit float keeps the gradients of a wide blur free of
// banding and holds linear values when the target is sRGB.
constexpr DXGI_FORMAT kWorkFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

constexpr char kShaderSource[] = R"hlsl(
cbuffer BlurConstants : register(b0)
{
    float4 uvRect;      // source region: xy origin, zw size (UV)
    float2 texel;       // size of one source texel (UV)
    float offset;       // sample offset in source texels
    float saturation;   // final pass only
};

Texture2D sourceTexture : register(t0);
SamplerState linearClamp : register(s0);

struct VsOut
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

// One triangle that covers the whole viewport; uv runs 0..1 over it, top to bottom.
VsOut FullscreenVs(uint id : SV_VertexID)
{
    VsOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.uv = uv;
    o.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    return o;
}

float3 Tap(float2 uv)
{
    // Stay inside the source region so a viewport smaller than its target never reads
    // the pixels around it.
    float2 lo = uvRect.xy + texel * 0.5;
    float2 hi = uvRect.xy + uvRect.zw - texel * 0.5;
    return sourceTexture.SampleLevel(linearClamp, clamp(uv, lo, hi), 0).rgb;
}

// Five taps: the center and the four diagonal neighbours.
float4 DownPs(VsOut i) : SV_Target
{
    float2 uv = uvRect.xy + i.uv * uvRect.zw;
    float2 h = texel * offset;
    float3 sum = Tap(uv) * 4.0;
    sum += Tap(uv - h);
    sum += Tap(uv + h);
    sum += Tap(uv + float2(h.x, -h.y));
    sum += Tap(uv - float2(h.x, -h.y));
    return float4(sum / 8.0, 1.0);
}

// Eight taps on a diamond around the center.
float3 UpBlur(float2 uv)
{
    float2 h = texel * offset;
    float3 sum = Tap(uv + float2(-h.x * 2.0, 0.0));
    sum += Tap(uv + float2(-h.x, h.y)) * 2.0;
    sum += Tap(uv + float2(0.0, h.y * 2.0));
    sum += Tap(uv + float2(h.x, h.y)) * 2.0;
    sum += Tap(uv + float2(h.x * 2.0, 0.0));
    sum += Tap(uv + float2(h.x, -h.y)) * 2.0;
    sum += Tap(uv + float2(0.0, -h.y * 2.0));
    sum += Tap(uv + float2(-h.x, -h.y)) * 2.0;
    return sum / 12.0;
}

float4 UpPs(VsOut i) : SV_Target
{
    return float4(UpBlur(uvRect.xy + i.uv * uvRect.zw), 1.0);
}

// Last pass: upsample and scale the distance of every color from its luminance.
float4 FinalPs(VsOut i) : SV_Target
{
    float3 color = UpBlur(uvRect.xy + i.uv * uvRect.zw);
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    color = max(lerp(luminance.xxx, color, saturation), 0.0);
    return float4(color, 1.0);
}
)hlsl";

struct alignas(16) BlurConstants
{
    float uvRect[4];
    float texel[2];
    float offset;
    float saturation;
};
static_assert(sizeof(BlurConstants) == 32);

struct CompiledShaders
{
    bool attempted = false;
    bool ok = false;
    ComPtr<ID3DBlob> vertex;
    ComPtr<ID3DBlob> down;
    ComPtr<ID3DBlob> up;
    ComPtr<ID3DBlob> finalPass;
};

CompiledShaders& Compiled()
{
    static CompiledShaders shaders;
    return shaders;
}

bool Compile(const char* entry, const char* profile, ComPtr<ID3DBlob>& blob)
{
    ComPtr<ID3DBlob> errors;
    const HRESULT hr = D3DCompile(kShaderSource, sizeof(kShaderSource) - 1, "facet_blur", nullptr, nullptr, entry,
                                  profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, blob.ReleaseAndGetAddressOf(),
                                  errors.GetAddressOf());
    return SUCCEEDED(hr);
}

} // namespace

BlurPlan PlanBlur(float radiusPixels, int maxDownsamples)
{
    BlurPlan plan;
    const int limit = std::max(1, maxDownsamples);
    const float radius = std::isfinite(radiusPixels) ? std::max(0.0f, radiusPixels) : 0.0f;
    // Reach of n halvings with offset 1 is about 2^(n+1) pixels.
    const int passes = static_cast<int>(std::floor(std::log2(std::max(radius, 4.0f) * 0.5f)));
    plan.downsamples = std::clamp(passes, 1, limit);
    plan.offset = radius / static_cast<float>(1 << (plan.downsamples + 1));
    return plan;
}

BlurPass::~BlurPass()
{
    InvalidateDeviceObjects();
}

bool BlurPass::CompileShaders()
{
    CompiledShaders& shaders = Compiled();
    if (shaders.attempted)
        return shaders.ok;
    shaders.attempted = true;
    shaders.ok = Compile("FullscreenVs", "vs_4_0", shaders.vertex) && Compile("DownPs", "ps_4_0", shaders.down) &&
                 Compile("UpPs", "ps_4_0", shaders.up) && Compile("FinalPs", "ps_4_0", shaders.finalPass);
    return shaders.ok;
}

bool BlurPass::CreateDeviceObjects(ID3D11Device* newDevice)
{
    InvalidateDeviceObjects();
    if (newDevice == nullptr || newDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0 || !CompileShaders())
        return false;
    const CompiledShaders& shaders = Compiled();

    bool ok = SUCCEEDED(newDevice->CreateVertexShader(shaders.vertex->GetBufferPointer(), shaders.vertex->GetBufferSize(),
                                                      nullptr, vertexShader.GetAddressOf()));
    ok = ok && SUCCEEDED(newDevice->CreatePixelShader(shaders.down->GetBufferPointer(), shaders.down->GetBufferSize(),
                                                      nullptr, downShader.GetAddressOf()));
    ok = ok && SUCCEEDED(newDevice->CreatePixelShader(shaders.up->GetBufferPointer(), shaders.up->GetBufferSize(),
                                                      nullptr, upShader.GetAddressOf()));
    ok = ok && SUCCEEDED(newDevice->CreatePixelShader(shaders.finalPass->GetBufferPointer(),
                                                      shaders.finalPass->GetBufferSize(), nullptr,
                                                      finalShader.GetAddressOf()));

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    ok = ok && SUCCEEDED(newDevice->CreateSamplerState(&samplerDesc, sampler.GetAddressOf()));

    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ok = ok && SUCCEEDED(newDevice->CreateBlendState(&blendDesc, blendState.GetAddressOf()));

    D3D11_DEPTH_STENCIL_DESC depthDesc = {};
    depthDesc.DepthEnable = FALSE;
    depthDesc.StencilEnable = FALSE;
    ok = ok && SUCCEEDED(newDevice->CreateDepthStencilState(&depthDesc, depthState.GetAddressOf()));

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.DepthClipEnable = TRUE;
    ok = ok && SUCCEEDED(newDevice->CreateRasterizerState(&rasterDesc, rasterizerState.GetAddressOf()));

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = sizeof(BlurConstants);
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ok = ok && SUCCEEDED(newDevice->CreateBuffer(&bufferDesc, nullptr, constants.GetAddressOf()));

    if (!ok)
    {
        InvalidateDeviceObjects();
        return false;
    }
    device = newDevice;
    return true;
}

void BlurPass::InvalidateDeviceObjects()
{
    saved.Release();
    retired.clear();
    levels.clear();
    output = {};
    outputWidth = outputHeight = 0;
    source.Reset();
    sourceView.Reset();
    sourceWidth = sourceHeight = 0;
    sourceCopyFormat = sourceViewFormat = DXGI_FORMAT_UNKNOWN;
    constants.Reset();
    rasterizerState.Reset();
    depthState.Reset();
    blendState.Reset();
    sampler.Reset();
    finalShader.Reset();
    upShader.Reset();
    downShader.Reset();
    vertexShader.Reset();
    device.Reset();
    lastResource = nullptr;
    lastStamp = 0;
}

bool BlurPass::CreateTarget(Target& target, int width, int height)
{
    target = {};
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = kWorkFormat;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, target.texture.GetAddressOf())) ||
        FAILED(device->CreateShaderResourceView(target.texture.Get(), nullptr, target.view.GetAddressOf())) ||
        FAILED(device->CreateRenderTargetView(target.texture.Get(), nullptr, target.target.GetAddressOf())))
    {
        target = {};
        return false;
    }
    target.width = width;
    target.height = height;
    return true;
}

ID3D11ShaderResourceView* BlurPass::EnsureOutput(int width, int height, std::uint64_t frame)
{
    if (!Ready() || width <= 0 || height <= 0 || width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return nullptr;
    if (output.view && outputWidth == width && outputHeight == height)
        return output.view.Get();

    if (output.view)
        retired.push_back({frame, std::move(output)});
    output = {};
    levels.clear();
    outputWidth = outputHeight = 0;

    Target created;
    if (!CreateTarget(created, width, height))
        return nullptr;
    // Halve until a side would drop below two pixels.
    std::vector<Target> chain;
    for (int k = 1; k <= kMaxBlurDownsamples; ++k)
    {
        const int w = width >> k;
        const int h = height >> k;
        if (w < 2 || h < 2)
            break;
        Target level;
        if (!CreateTarget(level, w, h))
            return nullptr;
        chain.push_back(std::move(level));
    }
    if (chain.empty())
        return nullptr;
    output = std::move(created);
    levels = std::move(chain);
    outputWidth = width;
    outputHeight = height;
    lastResource = nullptr;
    return output.view.Get();
}

void BlurPass::ReleaseRetired(std::uint64_t frame, std::uint64_t keepFrames)
{
    std::erase_if(retired, [&](const Retired& entry) { return entry.frame + keepFrames <= frame; });
}

bool BlurPass::EnsureSource(const D3D11_TEXTURE2D_DESC& desc, const CaptureFormat& format)
{
    if (source && sourceWidth == desc.Width && sourceHeight == desc.Height && sourceCopyFormat == format.copyFormat &&
        sourceViewFormat == format.viewFormat)
        return true;
    source.Reset();
    sourceView.Reset();
    sourceWidth = sourceHeight = 0;
    sourceCopyFormat = sourceViewFormat = DXGI_FORMAT_UNKNOWN;

    D3D11_TEXTURE2D_DESC copyDesc = {};
    copyDesc.Width = desc.Width;
    copyDesc.Height = desc.Height;
    copyDesc.MipLevels = 1;
    copyDesc.ArraySize = 1;
    copyDesc.Format = format.copyFormat;
    copyDesc.SampleDesc.Count = 1;
    copyDesc.Usage = D3D11_USAGE_DEFAULT;
    copyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device->CreateTexture2D(&copyDesc, nullptr, source.GetAddressOf())))
        return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
    viewDesc.Format = format.viewFormat;
    viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    viewDesc.Texture2D.MipLevels = 1;
    if (FAILED(device->CreateShaderResourceView(source.Get(), &viewDesc, sourceView.GetAddressOf())))
    {
        source.Reset();
        return false;
    }
    sourceWidth = desc.Width;
    sourceHeight = desc.Height;
    sourceCopyFormat = format.copyFormat;
    sourceViewFormat = format.viewFormat;
    return true;
}

void BlurPass::DrawPass(ID3D11DeviceContext* context, ID3D11PixelShader* shader, ID3D11ShaderResourceView* input,
                        const Target& destination, const float uvRect[4], float texelX, float texelY, float offset,
                        float saturation)
{
    // Unbind the previous input first: it may be this pass's destination.
    ID3D11ShaderResourceView* none = nullptr;
    context->PSSetShaderResources(0, 1, &none);
    ID3D11RenderTargetView* target = destination.target.Get();
    context->OMSetRenderTargets(1, &target, nullptr);

    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(destination.width);
    viewport.Height = static_cast<float>(destination.height);
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (SUCCEEDED(context->Map(constants.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        BlurConstants values = {};
        std::memcpy(values.uvRect, uvRect, sizeof(values.uvRect));
        values.texel[0] = texelX;
        values.texel[1] = texelY;
        values.offset = offset;
        values.saturation = saturation;
        std::memcpy(mapped.pData, &values, sizeof(values));
        context->Unmap(constants.Get(), 0);
    }

    context->PSSetShader(shader, nullptr, 0);
    context->PSSetShaderResources(0, 1, &input);
    context->Draw(3, 0);
}

BlurStatus BlurPass::Run(ID3D11DeviceContext* context, const BlurRequest& request)
{
    if (!Ready() || context == nullptr || !output.view || levels.empty())
        return BlurStatus::NotReady;

    ComPtr<ID3D11RenderTargetView> renderTarget;
    context->OMGetRenderTargets(1, renderTarget.GetAddressOf(), nullptr);
    if (!renderTarget)
        return BlurStatus::NoRenderTarget;

    D3D11_RENDER_TARGET_VIEW_DESC viewDesc = {};
    renderTarget->GetDesc(&viewDesc);
    const bool multisampled = viewDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS;
    if (!multisampled && !(viewDesc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D && viewDesc.Texture2D.MipSlice == 0))
        return BlurStatus::UnsupportedView;

    ComPtr<ID3D11Resource> resource;
    renderTarget->GetResource(resource.GetAddressOf());
    ComPtr<ID3D11Texture2D> texture;
    if (!resource || FAILED(resource.As(&texture)))
        return BlurStatus::UnsupportedView;

    if (request.stamp != 0 && request.stamp == lastStamp && resource.Get() == lastResource)
        return BlurStatus::Reused;

    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    if (multisampled != (desc.SampleDesc.Count > 1))
        return BlurStatus::UnsupportedView;
    const CaptureFormat format = ChooseCaptureFormat(desc.Format, viewDesc.Format);
    if (!format.supported)
        return BlurStatus::UnsupportedFormat;
    UINT support = 0;
    if (FAILED(device->CheckFormatSupport(format.viewFormat, &support)) ||
        (support & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) == 0 ||
        (multisampled && (support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE) == 0))
        return BlurStatus::UnsupportedFormat;
    if (!EnsureSource(desc, format))
        return BlurStatus::ResourceFailed;

    // The renderer's viewport is the region of the target the output covers.
    D3D11_VIEWPORT viewport = {};
    UINT viewportCount = 1;
    context->RSGetViewports(&viewportCount, &viewport);
    const float targetWidth = static_cast<float>(desc.Width);
    const float targetHeight = static_cast<float>(desc.Height);
    if (viewportCount == 0 || viewport.Width <= 0.0f || viewport.Height <= 0.0f)
    {
        viewport.TopLeftX = viewport.TopLeftY = 0.0f;
        viewport.Width = targetWidth;
        viewport.Height = targetHeight;
    }
    const float x0 = std::clamp(viewport.TopLeftX, 0.0f, targetWidth);
    const float y0 = std::clamp(viewport.TopLeftY, 0.0f, targetHeight);
    const float x1 = std::clamp(viewport.TopLeftX + viewport.Width, x0, targetWidth);
    const float y1 = std::clamp(viewport.TopLeftY + viewport.Height, y0, targetHeight);
    if (x1 - x0 < 1.0f || y1 - y0 < 1.0f)
        return BlurStatus::NoRenderTarget;
    const float viewportRect[4] = {x0 / targetWidth, y0 / targetHeight, (x1 - x0) / targetWidth, (y1 - y0) / targetHeight};
    const float fullRect[4] = {0.0f, 0.0f, 1.0f, 1.0f};

    saved.Capture(context);

    if (multisampled)
        context->ResolveSubresource(source.Get(), 0, texture.Get(), 0, format.resolveFormat);
    else
        context->CopySubresourceRegion(source.Get(), 0, 0, 0, 0, texture.Get(), 0, nullptr);

    ID3D11Buffer* none = nullptr;
    const UINT zero = 0;
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->IASetVertexBuffers(0, 1, &none, &zero, &zero);
    context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    context->VSSetShader(vertexShader.Get(), nullptr, 0);
    context->HSSetShader(nullptr, nullptr, 0);
    context->DSSetShader(nullptr, nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    ID3D11Buffer* constantBuffer = constants.Get();
    context->VSSetConstantBuffers(0, 1, &none);
    context->PSSetConstantBuffers(0, 1, &constantBuffer);
    ID3D11SamplerState* samplerState = sampler.Get();
    context->PSSetSamplers(0, 1, &samplerState);
    const float blendFactor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    context->OMSetBlendState(blendState.Get(), blendFactor, 0xFFFFFFFFu);
    context->OMSetDepthStencilState(depthState.Get(), 0);
    context->RSSetState(rasterizerState.Get());
    context->RSSetScissorRects(0, nullptr);

    const BlurPlan plan = PlanBlur(request.radius, static_cast<int>(levels.size()));
    const int n = plan.downsamples;

    // Down: the viewport region of the copy, then each level into the next smaller one.
    DrawPass(context, downShader.Get(), sourceView.Get(), levels[0], viewportRect, 1.0f / targetWidth,
             1.0f / targetHeight, plan.offset, 1.0f);
    for (int k = 1; k < n; ++k)
    {
        const Target& from = levels[static_cast<size_t>(k - 1)];
        DrawPass(context, downShader.Get(), from.view.Get(), levels[static_cast<size_t>(k)], fullRect,
                 1.0f / static_cast<float>(from.width), 1.0f / static_cast<float>(from.height), plan.offset, 1.0f);
    }
    // Up: back through the levels, and the last step into the output.
    for (int k = n - 1; k >= 1; --k)
    {
        const Target& from = levels[static_cast<size_t>(k)];
        DrawPass(context, upShader.Get(), from.view.Get(), levels[static_cast<size_t>(k - 1)], fullRect,
                 1.0f / static_cast<float>(from.width), 1.0f / static_cast<float>(from.height), plan.offset, 1.0f);
    }
    const Target& last = levels[0];
    DrawPass(context, finalShader.Get(), last.view.Get(), output, fullRect, 1.0f / static_cast<float>(last.width),
             1.0f / static_cast<float>(last.height), plan.offset, std::max(0.0f, request.saturation));

    saved.Restore(context);
    saved.Release();

    lastResource = resource.Get();
    lastStamp = request.stamp;
    ++blurCount;
    return BlurStatus::Blurred;
}

void BlurPass::ClearOutput(ID3D11DeviceContext* context, const float rgba[4])
{
    if (context != nullptr && output.target)
        context->ClearRenderTargetView(output.target.Get(), rgba);
}

} // namespace facet::internal
