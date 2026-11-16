// Blurred translucent panels ("glass") on top of the backdrop blur pass (blur_pass.h).
//
// DrawGlass records three things into the draw list:
//   1. a draw callback that runs BlurPass::Run on the render target the renderer draws into,
//      at most once per frame and render target; later panels reuse that blur,
//   2. the renderer's reset-render-state callback, so it rebinds its own pipeline,
//   3. the panel's shadow, after the capture so it stays out of the blurred backdrop,
//   4. the panel: the blurred output drawn in the panel's shape with UVs that map the
//      viewport onto the output, the glass tint on top, and a hairline border.
// When blurring is not possible the panel is an opaque tint (DrawGlassFallback). A failure
// found only while rendering fills the output with the opaque tint for that frame, so the
// panel already looks like the fallback, and GlassAvailable() reports false afterwards.

#include "facet/core/effects.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "effects/blur_pass.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/device.h"
#include "internal/subsystem.h"

namespace facet
{
namespace internal
{
// Frames between capture attempts after a failure.
constexpr std::uint64_t kGlassRetryFrames = 120;

namespace
{

struct BlurState
{
    BlurPass pass;
    bool captureOk = true;          // result of the most recent capture
    std::uint64_t failedFrame = 0;  // frame of the most recent failed capture
    bool shaderCompileFailed = false;
};

BlurState& State()
{
    static BlurState state;
    return state;
}

// Copied into the draw list by AddCallback, so it lives as long as the draw data.
struct GlassCallbackData
{
    BlurRequest request;
    float fallback[4];  // opaque tint, used when the capture fails
};

void GlassCallback(const ImDrawList*, const ImDrawCmd* command)
{
    if (command == nullptr || command->UserCallbackData == nullptr ||
        command->UserCallbackDataSize != static_cast<int>(sizeof(GlassCallbackData)))
        return;
    GlassCallbackData data;
    std::memcpy(&data, command->UserCallbackData, sizeof(data));
    BlurState& state = State();
    ID3D11DeviceContext* context = DeviceContext();
    if (context == nullptr)
        return;
    const BlurStatus status = state.pass.Run(context, data.request);
    state.captureOk = BlurSucceeded(status);
    if (!state.captureOk)
    {
        state.failedFrame = FrameIndex();
        state.pass.ClearOutput(context, data.fallback);
    }
}

bool InitBlur()
{
    // Compiling once at start-up keeps the first glass panel from stalling a frame. A
    // missing compiler only disables the blur.
    State().shaderCompileFailed = !BlurPass::CompileShaders();
    return true;
}

bool CreateBlurDeviceObjects()
{
    BlurState& state = State();
    if (!state.shaderCompileFailed)
        state.pass.CreateDeviceObjects(Device());
    state.captureOk = true;
    // Optional feature: a failure leaves the pass not ready and glass falls back.
    return true;
}

void InvalidateBlurDeviceObjects()
{
    State().pass.InvalidateDeviceObjects();
}

void BlurNewFrame()
{
    State().pass.ReleaseRetired(FrameIndex());
}

struct ResolvedGlass
{
    float blurRadius;
    float tintOpacity;
    float saturation;
    ImVec4 tint;
};

ResolvedGlass Resolve(const GlassSpec& spec)
{
    const Theme& theme = T();
    ResolvedGlass glass;
    glass.blurRadius = spec.blurRadius >= 0.0f ? spec.blurRadius : Px(theme.glass.blurRadius);
    glass.tintOpacity = std::clamp(spec.tintOpacity >= 0.0f ? spec.tintOpacity : theme.glass.tintOpacity, 0.0f, 1.0f);
    glass.saturation = spec.saturation >= 0.0f ? spec.saturation : theme.glass.saturation;
    glass.tint = spec.tint.w > 0.0f ? spec.tint : theme.colors.glassTint;
    return glass;
}

void DrawGlassBorder(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const GlassSpec& spec)
{
    if (spec.hairline)
        StrokeRect(drawList, pMin, pMax, ToU32(T().colors.separator), radius, HairlineThickness());
}

} // namespace
} // namespace internal

void DrawGlassFallback(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const GlassSpec& spec)
{
    if (drawList == nullptr)
        return;
    const internal::ResolvedGlass glass = internal::Resolve(spec);
    DrawShadow(drawList, pMin, pMax, radius, spec.shadow);
    FillRect(drawList, pMin, pMax, ToU32(WithAlpha(glass.tint, 1.0f)), radius);
    internal::DrawGlassBorder(drawList, pMin, pMax, radius, spec);
}

void DrawGlass(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const GlassSpec& spec)
{
    if (drawList == nullptr)
        return;
    internal::BlurState& state = internal::State();
    // After a failed capture, try again now and then: the host may have changed its target.
    // A failing try looks like the fallback, because the callback fills the output with it.
    const bool retry = !state.captureOk && internal::FrameIndex() >= state.failedFrame + internal::kGlassRetryFrames;
    if (!state.pass.Ready() || (!state.captureOk && !retry) || internal::DeviceContext() == nullptr)
    {
        DrawGlassFallback(drawList, pMin, pMax, radius, spec);
        return;
    }

    // The output covers the main viewport at framebuffer resolution.
    const ImGuiIO& io = ImGui::GetIO();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 framebufferScale(io.DisplayFramebufferScale.x > 0.0f ? io.DisplayFramebufferScale.x : 1.0f,
                                  io.DisplayFramebufferScale.y > 0.0f ? io.DisplayFramebufferScale.y : 1.0f);
    const int width = static_cast<int>(std::lround(viewport->Size.x * framebufferScale.x));
    const int height = static_cast<int>(std::lround(viewport->Size.y * framebufferScale.y));
    ID3D11ShaderResourceView* output = state.pass.EnsureOutput(width, height, internal::FrameIndex());
    if (output == nullptr)
    {
        DrawGlassFallback(drawList, pMin, pMax, radius, spec);
        return;
    }

    const internal::ResolvedGlass glass = internal::Resolve(spec);
    internal::GlassCallbackData data = {};
    data.request.radius = glass.blurRadius * std::max(framebufferScale.x, framebufferScale.y);
    data.request.saturation = glass.saturation;
    // FrameIndex starts at 1 once facet::NewFrame ran; outside frames every panel blurs.
    data.request.stamp = internal::FrameIndex();
    data.fallback[0] = glass.tint.x;
    data.fallback[1] = glass.tint.y;
    data.fallback[2] = glass.tint.z;
    data.fallback[3] = 1.0f;
    drawList->AddCallback(&internal::GlassCallback, &data, sizeof(data));
    if (ImDrawCallback reset = ImGui::GetPlatformIO().DrawCallback_ResetRenderState)
        drawList->AddCallback(reset, nullptr);

    // The shadow comes after the capture, so it never darkens the blurred backdrop. The
    // blurred output is opaque and hides the part of the shadow under the panel.
    DrawShadow(drawList, pMin, pMax, radius, spec.shadow);

    // The panel shape in white with the blurred output, UVs mapping the viewport onto 0..1.
    drawList->PushTexture(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(output))));
    const int firstVertex = drawList->VtxBuffer.Size;
    FillRect(drawList, pMin, pMax, IM_COL32_WHITE, radius);
    const ImVec2 origin = viewport->Pos;
    const ImVec2 inverseSize(1.0f / std::max(1.0f, viewport->Size.x), 1.0f / std::max(1.0f, viewport->Size.y));
    for (int i = firstVertex; i < drawList->VtxBuffer.Size; ++i)
    {
        ImDrawVert& vertex = drawList->VtxBuffer[i];
        vertex.uv = ImVec2((vertex.pos.x - origin.x) * inverseSize.x, (vertex.pos.y - origin.y) * inverseSize.y);
    }
    drawList->PopTexture();

    FillRect(drawList, pMin, pMax, ToU32(WithAlpha(glass.tint, glass.tintOpacity)), radius);
    internal::DrawGlassBorder(drawList, pMin, pMax, radius, spec);
}

bool GlassAvailable()
{
    const internal::BlurState& state = internal::State();
    return state.pass.Ready() && state.captureOk;
}

namespace internal
{

Subsystem BlurSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "blur";
    subsystem.init = &InitBlur;
    subsystem.shutdown = &InvalidateBlurDeviceObjects;
    subsystem.newFrame = &BlurNewFrame;
    subsystem.invalidateDeviceObjects = &InvalidateBlurDeviceObjects;
    subsystem.createDeviceObjects = &CreateBlurDeviceObjects;
    return subsystem;
}

} // namespace internal

} // namespace facet
