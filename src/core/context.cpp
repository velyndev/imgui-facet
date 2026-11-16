#include "facet/core/context.h"

#include <d3d11.h>
#include <imgui.h>
#include <wrl/client.h>

static_assert(IMGUI_VERSION_NUM >= 19300, "Facet requires Dear ImGui 1.93.0 or newer");

#ifndef FACET_VERSION_STRING
#error "FACET_VERSION_STRING must be defined by the build"
#endif

namespace facet
{
namespace
{

struct ContextState
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    std::uint64_t frameIndex = 0;
    float deltaTime = 0.0f;
    bool initialized = false;
    bool inFrame = false;
};

ContextState& State()
{
    static ContextState state;
    return state;
}

} // namespace

bool Init(const InitDesc& desc)
{
    ContextState& state = State();
    if (state.initialized)
        return false;
    if (desc.device == nullptr || desc.context == nullptr)
        return false;
    if (ImGui::GetCurrentContext() == nullptr)
        return false;

    state.device = desc.device;
    state.context = desc.context;
    state.frameIndex = 0;
    state.deltaTime = 0.0f;
    state.inFrame = false;
    state.initialized = true;
    return true;
}

void NewFrame()
{
    ContextState& state = State();
    if (!state.initialized || ImGui::GetCurrentContext() == nullptr)
        return;
    state.deltaTime = ImGui::GetIO().DeltaTime;
    state.inFrame = true;
    ++state.frameIndex;
}

void EndFrame()
{
    ContextState& state = State();
    state.inFrame = false;
}

void Shutdown()
{
    ContextState& state = State();
    state.context.Reset();
    state.device.Reset();
    state.frameIndex = 0;
    state.deltaTime = 0.0f;
    state.inFrame = false;
    state.initialized = false;
}

bool IsInitialized()
{
    return State().initialized;
}

const char* Version()
{
    return FACET_VERSION_STRING;
}

std::uint64_t FrameIndex()
{
    return State().frameIndex;
}

float DeltaTime()
{
    return State().deltaTime;
}

} // namespace facet
