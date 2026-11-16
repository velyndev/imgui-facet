#pragma once

// Dual-filter backdrop blur on Direct3D 11.
//
// Run() is called from a draw callback while the renderer draws into its render target. It
// copies render target 0 (or resolves it when it is multisampled) into a texture of its own,
// so the target never needs the shader-resource bind flag, then halves the image `n` times
// with a five-tap filter and doubles it back with an eight-tap filter. The last step writes
// the output texture, boosts the saturation and forces the alpha to 1. Each step widens the
// blur, so a few passes at falling resolution give a wide, smooth blur at low cost.
//
// The output covers the renderer's viewport and is sized for the display at record time, so
// its shader resource view can be recorded into a draw list before the callback runs: UV
// (0,0) to (1,1) maps onto the viewport. The copy and the intermediate textures are sized
// and formatted from the render target when the callback runs.
//
// Every state the pass changes is saved before and restored after it (d3d11_state.h).

#include <cstdint>
#include <vector>

#include <wrl/client.h>

#include "effects/blur_formats.h"
#include "effects/d3d11_state.h"

namespace facet::internal
{

// Passes and sample offset for a blur radius in pixels. Each halving roughly doubles the
// reach of the filter, so the number of passes follows log2 of the radius and the offset
// (in source texels) fine-tunes the reach between two pass counts.
struct BlurPlan
{
    int downsamples = 1;
    float offset = 1.0f;
};
inline constexpr int kMaxBlurDownsamples = 6;
BlurPlan PlanBlur(float radiusPixels, int maxDownsamples = kMaxBlurDownsamples);

enum class BlurStatus
{
    Blurred,            // the target was captured and blurred into the output
    Reused,             // this target was already blurred for this stamp; output unchanged
    NotReady,           // no device objects or no output texture
    NoRenderTarget,     // render target slot 0 is empty
    UnsupportedView,    // not a single-level 2D or 2D multisample view
    UnsupportedFormat,  // see blur_formats.h, or the device cannot sample or resolve it
    ResourceFailed,     // a texture or view could not be created
};

inline bool BlurSucceeded(BlurStatus status)
{
    return status == BlurStatus::Blurred || status == BlurStatus::Reused;
}

struct BlurRequest
{
    float radius = 0.0f;      // framebuffer pixels
    float saturation = 1.0f;  // 1 keeps the colors, larger values strengthen them
    std::uint64_t stamp = 0;  // one blur per stamp and render target; 0 always blurs
};

class BlurPass
{
public:
    BlurPass() = default;
    ~BlurPass();
    BlurPass(const BlurPass&) = delete;
    BlurPass& operator=(const BlurPass&) = delete;

    // Compiles the HLSL once per process. False when the compiler is missing or fails.
    static bool CompileShaders();

    // Creates shaders, states, the sampler and the constant buffer on `device`. Returns
    // false and stays not ready when anything fails (the feature level must be 10.0 or up).
    bool CreateDeviceObjects(ID3D11Device* device);
    // Releases every Direct3D object, including retired outputs.
    void InvalidateDeviceObjects();
    bool Ready() const { return device != nullptr; }

    // The output texture for a viewport of width x height pixels, created or resized when
    // needed. A replaced output stays alive (retired) until ReleaseRetired passes its frame,
    // because draw data recorded earlier may still reference it. nullptr on failure.
    ID3D11ShaderResourceView* EnsureOutput(int width, int height, std::uint64_t frame);
    // Releases outputs retired at least `keepFrames` frames before `frame`.
    void ReleaseRetired(std::uint64_t frame, std::uint64_t keepFrames = 2);
    int OutputWidth() const { return outputWidth; }
    int OutputHeight() const { return outputHeight; }
    ID3D11ShaderResourceView* OutputView() const { return output.view.Get(); }

    // Captures and blurs the render target bound on `context` (see the file comment).
    BlurStatus Run(ID3D11DeviceContext* context, const BlurRequest& request);
    // Fills the output with one color, used when Run failed so the panel shows a solid tint.
    void ClearOutput(ID3D11DeviceContext* context, const float rgba[4]);

    // Statistics for tests and diagnostics.
    int BlurCount() const { return blurCount; }
    DXGI_FORMAT SourceFormat() const { return sourceCopyFormat; }

private:
    struct Target
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
        int width = 0;
        int height = 0;
    };
    struct Retired
    {
        std::uint64_t frame = 0;
        Target target;
    };

    bool CreateTarget(Target& target, int width, int height);
    bool EnsureSource(const struct D3D11_TEXTURE2D_DESC& desc, const CaptureFormat& format);
    void DrawPass(ID3D11DeviceContext* context, ID3D11PixelShader* shader, ID3D11ShaderResourceView* source,
                  const Target& destination, const float uvRect[4], float texelX, float texelY, float offset,
                  float saturation);

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> downShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> upShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> finalShader;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthState;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants;

    // Copy of the render target.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> source;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> sourceView;
    UINT sourceWidth = 0;
    UINT sourceHeight = 0;
    DXGI_FORMAT sourceCopyFormat = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT sourceViewFormat = DXGI_FORMAT_UNKNOWN;

    Target output;
    std::vector<Target> levels;  // levels[k] is the output size halved k + 1 times
    std::vector<Retired> retired;
    int outputWidth = 0;
    int outputHeight = 0;

    const void* lastResource = nullptr;  // identity only, never dereferenced
    std::uint64_t lastStamp = 0;
    int blurCount = 0;

    D3D11StateBlock saved;
};

} // namespace facet::internal
