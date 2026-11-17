#include "facet/core/context.h"

#include <d3d11.h>
#include <imgui.h>
#include <wrl/client.h>

#include "internal/device.h"
#include "internal/subsystem.h"

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
    bool deviceObjectsValid = false;
};

ContextState& State()
{
    static ContextState state;
    return state;
}

void InvalidateAll(std::span<const internal::Subsystem> table)
{
    for (size_t i = table.size(); i-- > 0;)
    {
        if (table[i].invalidateDeviceObjects != nullptr)
            table[i].invalidateDeviceObjects();
    }
}

// Creates the device objects of every module in order. On failure, every module gets
// invalidateDeviceObjects() in reverse order (the hook is safe when nothing was created).
bool CreateAll(std::span<const internal::Subsystem> table)
{
    for (const internal::Subsystem& subsystem : table)
    {
        if (subsystem.createDeviceObjects != nullptr && !subsystem.createDeviceObjects())
        {
            InvalidateAll(table);
            return false;
        }
    }
    return true;
}

void ShutdownFirst(std::span<const internal::Subsystem> table, size_t count)
{
    for (size_t i = count; i-- > 0;)
    {
        if (table[i].shutdown != nullptr)
            table[i].shutdown();
    }
}

void ResetState(ContextState& state)
{
    state.context.Reset();
    state.device.Reset();
    state.frameIndex = 0;
    state.deltaTime = 0.0f;
    state.inFrame = false;
    state.initialized = false;
    state.deviceObjectsValid = false;
}

} // namespace

namespace internal
{

bool StartSubsystems(std::span<const Subsystem> table)
{
    size_t started = 0;
    for (; started < table.size(); ++started)
    {
        const Subsystem& subsystem = table[started];
        if (subsystem.init != nullptr && !subsystem.init())
            break;
    }
    if (started < table.size())
    {
        ShutdownFirst(table, started);
        return false;
    }
    if (!CreateAll(table))
    {
        ShutdownFirst(table, table.size());
        return false;
    }
    return true;
}

void StopSubsystems(std::span<const Subsystem> table)
{
    InvalidateAll(table);
    ShutdownFirst(table, table.size());
}

void NotifyScaleChanged(float scale)
{
    if (!State().initialized)
        return;
    for (const Subsystem& subsystem : Subsystems())
    {
        if (subsystem.scaleChanged != nullptr)
            subsystem.scaleChanged(scale);
    }
}

void NotifyThemeChanged()
{
    if (!State().initialized)
        return;
    for (const Subsystem& subsystem : Subsystems())
    {
        if (subsystem.themeChanged != nullptr)
            subsystem.themeChanged();
    }
}

ID3D11Device* Device()
{
    return State().device.Get();
}

ID3D11DeviceContext* DeviceContext()
{
    return State().context.Get();
}

std::uint64_t FrameIndex()
{
    return State().frameIndex;
}

float DeltaTime()
{
    return State().deltaTime;
}

} // namespace internal

bool Init(const InitDesc& desc)
{
    ContextState& state = State();
    if (state.initialized)
        return false;
    if (desc.device == nullptr || desc.context == nullptr)
        return false;
    if (ImGui::GetCurrentContext() == nullptr)
        return false;

    // The device is reachable through internal::Device() while the hooks run.
    state.device = desc.device;
    state.context = desc.context;
    state.frameIndex = 0;
    state.deltaTime = 0.0f;
    state.inFrame = false;
    if (!internal::StartSubsystems(internal::Subsystems()))
    {
        ResetState(state);
        return false;
    }
    state.deviceObjectsValid = true;
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
    for (const internal::Subsystem& subsystem : internal::Subsystems())
    {
        if (subsystem.newFrame != nullptr)
            subsystem.newFrame();
    }
}

void EndFrame()
{
    ContextState& state = State();
    if (!state.initialized || !state.inFrame)
        return;
    for (const internal::Subsystem& subsystem : internal::Subsystems())
    {
        if (subsystem.endFrame != nullptr)
            subsystem.endFrame();
    }
    state.inFrame = false;
}

void Shutdown()
{
    ContextState& state = State();
    if (state.initialized)
        internal::StopSubsystems(internal::Subsystems());
    ResetState(state);
}

void InvalidateDeviceObjects()
{
    ContextState& state = State();
    if (!state.initialized || !state.deviceObjectsValid)
        return;
    // Draw commands recorded in this frame still point at the objects released here.
    IM_ASSERT(!state.inFrame && "facet::InvalidateDeviceObjects called inside a frame");
    InvalidateAll(internal::Subsystems());
    state.deviceObjectsValid = false;
}

bool CreateDeviceObjects()
{
    ContextState& state = State();
    if (!state.initialized)
        return false;
    if (state.deviceObjectsValid)
        return true;
    IM_ASSERT(!state.inFrame && "facet::CreateDeviceObjects called inside a frame");
    state.deviceObjectsValid = CreateAll(internal::Subsystems());
    return state.deviceObjectsValid;
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
