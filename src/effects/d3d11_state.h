#pragma once

// Save and restore of Direct3D 11 device-context state around library-owned rendering
// inside a draw callback.
//
// Capture() records the pipeline state that a full-screen pass changes; Restore() binds it
// again exactly, including null bindings. It covers the output merger (render targets, depth
// view, blend and depth-stencil state), the rasterizer (state, every viewport and scissor
// rect), the input assembler (layout, topology, vertex buffer slot 0, index buffer), every
// shader stage except compute (with class instances), and slot 0 of the vertex and pixel
// constant buffers, pixel samplers and pixel shader resources. Compute, stream output and
// higher slots are not part of it: code that uses them needs its own save.
//
// Usable by any module that renders from a draw callback; see also internal/device.h.

#include <d3d11.h>
#include <wrl/client.h>

namespace facet::internal
{

struct D3D11StateBlock
{
    template <typename T>
    using Ptr = Microsoft::WRL::ComPtr<T>;

    struct ShaderStage
    {
        Ptr<ID3D11DeviceChild> shader;
        Ptr<ID3D11ClassInstance> instances[D3D11_SHADER_MAX_INTERFACES];
        UINT instanceCount = 0;
    };

    // Output merger.
    Ptr<ID3D11RenderTargetView> renderTargets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
    Ptr<ID3D11DepthStencilView> depthStencilView;
    Ptr<ID3D11BlendState> blendState;
    FLOAT blendFactor[4] = {};
    UINT sampleMask = 0;
    Ptr<ID3D11DepthStencilState> depthStencilState;
    UINT stencilRef = 0;

    // Rasterizer.
    Ptr<ID3D11RasterizerState> rasterizerState;
    UINT viewportCount = 0;
    D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    UINT scissorCount = 0;
    D3D11_RECT scissors[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};

    // Input assembler.
    Ptr<ID3D11InputLayout> inputLayout;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    Ptr<ID3D11Buffer> vertexBuffer;
    UINT vertexStride = 0;
    UINT vertexOffset = 0;
    Ptr<ID3D11Buffer> indexBuffer;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
    UINT indexOffset = 0;

    // Shader stages: vertex, hull, domain, geometry, pixel.
    ShaderStage vertex;
    ShaderStage hull;
    ShaderStage domain;
    ShaderStage geometry;
    ShaderStage pixel;

    // Slot 0 bindings.
    Ptr<ID3D11Buffer> vertexConstants;
    Ptr<ID3D11Buffer> pixelConstants;
    Ptr<ID3D11SamplerState> pixelSampler;
    Ptr<ID3D11ShaderResourceView> pixelResource;

    void Capture(ID3D11DeviceContext* context);
    void Restore(ID3D11DeviceContext* context) const;
    // Drops every reference held by the block.
    void Release();

    // True when both blocks hold the same objects and values.
    bool SameAs(const D3D11StateBlock& other) const;
};

} // namespace facet::internal
