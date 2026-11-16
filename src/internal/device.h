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

// Number of NewFrame calls since Init; the first frame is 1.
std::uint64_t FrameIndex();

// Delta time of the current frame in seconds, read from ImGui in facet::NewFrame.
float DeltaTime();

} // namespace facet::internal
