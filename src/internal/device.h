#pragma once

// Library-internal access to the Direct3D device and the frame clock.

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace facet::internal
{

// The device and immediate context passed to facet::Init. Raw pointers, owned by the
// context module: valid from the start of Init until the end of Shutdown (including inside
// every subsystem hook), nullptr otherwise. Do not Release them.
ID3D11Device* Device();
ID3D11DeviceContext* DeviceContext();

// Draw callbacks (ImDrawList::AddCallback) that issue their own Direct3D calls use
// DeviceContext(): facet::Init requires the same immediate context that the app gives to
// its Dear ImGui DX11 renderer, so it is the context that runs the callback. The library
// never includes the renderer backend header (it is not on the library's include path, and
// a host that supplies its own Dear ImGui target may not have it), so callbacks do not read
// ImGui_ImplDX11_RenderState. A callback saves and restores any state it changes that the
// renderer does not set per draw command, and the draw list that adds it follows it with
// ImGui::GetPlatformIO().DrawCallback_ResetRenderState, so the renderer rebinds its own
// pipeline state for the commands after it.

// Number of NewFrame calls since Init; the first frame is 1.
std::uint64_t FrameIndex();

// Delta time of the current frame in seconds, read from ImGui in facet::NewFrame.
float DeltaTime();

} // namespace facet::internal
