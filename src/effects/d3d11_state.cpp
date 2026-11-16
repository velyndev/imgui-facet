#include "effects/d3d11_state.h"

#include <cstring>

namespace facet::internal
{
namespace
{

using Stage = D3D11StateBlock::ShaderStage;

static_assert(sizeof(Microsoft::WRL::ComPtr<ID3D11ClassInstance>) == sizeof(ID3D11ClassInstance*),
              "class instance arrays are read through the ComPtr storage");

void ResetStage(Stage& stage)
{
    stage.shader.Reset();
    for (UINT i = 0; i < stage.instanceCount; ++i)
        stage.instances[i].Reset();
    stage.instanceCount = 0;
}

// Reads one stage. The getters AddRef what they return; the ComPtrs take that reference.
template <typename Shader, typename Getter>
void CaptureStage(Stage& stage, Getter get)
{
    ResetStage(stage);
    Shader* shader = nullptr;
    UINT count = D3D11_SHADER_MAX_INTERFACES;
    get(&shader, stage.instances[0].GetAddressOf(), &count);
    stage.shader.Attach(shader);
    stage.instanceCount = count;
}

ID3D11ClassInstance* const* Instances(const Stage& stage)
{
    return stage.instanceCount > 0 ? stage.instances[0].GetAddressOf() : nullptr;
}

bool SameStage(const Stage& a, const Stage& b)
{
    if (a.shader.Get() != b.shader.Get() || a.instanceCount != b.instanceCount)
        return false;
    for (UINT i = 0; i < a.instanceCount; ++i)
    {
        if (a.instances[i].Get() != b.instances[i].Get())
            return false;
    }
    return true;
}

} // namespace

void D3D11StateBlock::Capture(ID3D11DeviceContext* context)
{
    Release();

    ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, depthStencilView.GetAddressOf());
    for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        renderTargets[i].Attach(targets[i]);
    context->OMGetBlendState(blendState.GetAddressOf(), blendFactor, &sampleMask);
    context->OMGetDepthStencilState(depthStencilState.GetAddressOf(), &stencilRef);

    context->RSGetState(rasterizerState.GetAddressOf());
    // With a null array the getters report how many entries are bound; reading exactly that
    // many keeps the count, so Restore binds the same number of viewports and rects.
    viewportCount = 0;
    context->RSGetViewports(&viewportCount, nullptr);
    if (viewportCount > 0)
        context->RSGetViewports(&viewportCount, viewports);
    scissorCount = 0;
    context->RSGetScissorRects(&scissorCount, nullptr);
    if (scissorCount > 0)
        context->RSGetScissorRects(&scissorCount, scissors);

    context->IAGetInputLayout(inputLayout.GetAddressOf());
    context->IAGetPrimitiveTopology(&topology);
    context->IAGetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &vertexStride, &vertexOffset);
    context->IAGetIndexBuffer(indexBuffer.GetAddressOf(), &indexFormat, &indexOffset);

    CaptureStage<ID3D11VertexShader>(vertex, [&](auto s, auto i, auto c) { context->VSGetShader(s, i, c); });
    CaptureStage<ID3D11HullShader>(hull, [&](auto s, auto i, auto c) { context->HSGetShader(s, i, c); });
    CaptureStage<ID3D11DomainShader>(domain, [&](auto s, auto i, auto c) { context->DSGetShader(s, i, c); });
    CaptureStage<ID3D11GeometryShader>(geometry, [&](auto s, auto i, auto c) { context->GSGetShader(s, i, c); });
    CaptureStage<ID3D11PixelShader>(pixel, [&](auto s, auto i, auto c) { context->PSGetShader(s, i, c); });

    context->VSGetConstantBuffers(0, 1, vertexConstants.GetAddressOf());
    context->PSGetConstantBuffers(0, 1, pixelConstants.GetAddressOf());
    context->PSGetSamplers(0, 1, pixelSampler.GetAddressOf());
    context->PSGetShaderResources(0, 1, pixelResource.GetAddressOf());
}

void D3D11StateBlock::Restore(ID3D11DeviceContext* context) const
{
    // Shader resources first: binding the render targets would otherwise unbind a resource
    // that is also an input, and the other way round.
    ID3D11ShaderResourceView* resource = pixelResource.Get();
    ID3D11ShaderResourceView* none = nullptr;
    context->PSSetShaderResources(0, 1, &none);

    ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        targets[i] = renderTargets[i].Get();
    context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, targets, depthStencilView.Get());
    context->OMSetBlendState(blendState.Get(), blendFactor, sampleMask);
    context->OMSetDepthStencilState(depthStencilState.Get(), stencilRef);

    context->RSSetState(rasterizerState.Get());
    context->RSSetViewports(viewportCount, viewportCount > 0 ? viewports : nullptr);
    context->RSSetScissorRects(scissorCount, scissorCount > 0 ? scissors : nullptr);

    context->IASetInputLayout(inputLayout.Get());
    context->IASetPrimitiveTopology(topology);
    ID3D11Buffer* vb = vertexBuffer.Get();
    context->IASetVertexBuffers(0, 1, &vb, &vertexStride, &vertexOffset);
    context->IASetIndexBuffer(indexBuffer.Get(), indexFormat, indexOffset);

    context->VSSetShader(static_cast<ID3D11VertexShader*>(vertex.shader.Get()), Instances(vertex), vertex.instanceCount);
    context->HSSetShader(static_cast<ID3D11HullShader*>(hull.shader.Get()), Instances(hull), hull.instanceCount);
    context->DSSetShader(static_cast<ID3D11DomainShader*>(domain.shader.Get()), Instances(domain), domain.instanceCount);
    context->GSSetShader(static_cast<ID3D11GeometryShader*>(geometry.shader.Get()), Instances(geometry),
                         geometry.instanceCount);
    context->PSSetShader(static_cast<ID3D11PixelShader*>(pixel.shader.Get()), Instances(pixel), pixel.instanceCount);

    ID3D11Buffer* vsConstants = vertexConstants.Get();
    context->VSSetConstantBuffers(0, 1, &vsConstants);
    ID3D11Buffer* psConstants = pixelConstants.Get();
    context->PSSetConstantBuffers(0, 1, &psConstants);
    ID3D11SamplerState* sampler = pixelSampler.Get();
    context->PSSetSamplers(0, 1, &sampler);
    context->PSSetShaderResources(0, 1, &resource);
}

void D3D11StateBlock::Release()
{
    for (auto& target : renderTargets)
        target.Reset();
    depthStencilView.Reset();
    blendState.Reset();
    depthStencilState.Reset();
    rasterizerState.Reset();
    inputLayout.Reset();
    vertexBuffer.Reset();
    indexBuffer.Reset();
    ResetStage(vertex);
    ResetStage(hull);
    ResetStage(domain);
    ResetStage(geometry);
    ResetStage(pixel);
    vertexConstants.Reset();
    pixelConstants.Reset();
    pixelSampler.Reset();
    pixelResource.Reset();
}

bool D3D11StateBlock::SameAs(const D3D11StateBlock& other) const
{
    for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
    {
        if (renderTargets[i].Get() != other.renderTargets[i].Get())
            return false;
    }
    if (depthStencilView.Get() != other.depthStencilView.Get() || blendState.Get() != other.blendState.Get() ||
        std::memcmp(blendFactor, other.blendFactor, sizeof(blendFactor)) != 0 || sampleMask != other.sampleMask ||
        depthStencilState.Get() != other.depthStencilState.Get() || stencilRef != other.stencilRef)
        return false;
    if (rasterizerState.Get() != other.rasterizerState.Get() || viewportCount != other.viewportCount ||
        scissorCount != other.scissorCount ||
        std::memcmp(viewports, other.viewports, sizeof(D3D11_VIEWPORT) * viewportCount) != 0 ||
        std::memcmp(scissors, other.scissors, sizeof(D3D11_RECT) * scissorCount) != 0)
        return false;
    if (inputLayout.Get() != other.inputLayout.Get() || topology != other.topology ||
        vertexBuffer.Get() != other.vertexBuffer.Get() || vertexStride != other.vertexStride ||
        vertexOffset != other.vertexOffset || indexBuffer.Get() != other.indexBuffer.Get() ||
        indexFormat != other.indexFormat || indexOffset != other.indexOffset)
        return false;
    if (!SameStage(vertex, other.vertex) || !SameStage(hull, other.hull) || !SameStage(domain, other.domain) ||
        !SameStage(geometry, other.geometry) || !SameStage(pixel, other.pixel))
        return false;
    return vertexConstants.Get() == other.vertexConstants.Get() && pixelConstants.Get() == other.pixelConstants.Get() &&
           pixelSampler.Get() == other.pixelSampler.Get() && pixelResource.Get() == other.pixelResource.Get();
}

} // namespace facet::internal
