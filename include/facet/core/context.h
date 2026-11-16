#pragma once

#include <cstdint>

// Direct3D types are forward declared so that including Facet does not pull in d3d11.h.
struct ID3D11Device;
struct ID3D11DeviceContext;

namespace facet
{

// Everything Facet needs from the host application at start-up.
struct InitDesc
{
    ID3D11Device* device = nullptr;          // required; Facet keeps its own reference
    ID3D11DeviceContext* context = nullptr;  // required; the immediate context used by the host
};

// Starts Facet. Requires a current Dear ImGui context and a valid device and context.
// Returns false (and leaves Facet uninitialized) when a requirement is missing or when
// Facet is already initialized.
bool Init(const InitDesc& desc);

// Call once per frame after ImGui::NewFrame(). Reads the frame's delta time from ImGui,
// so a fixed io.DeltaTime gives fixed Facet timing as well.
void NewFrame();

// Call once per frame before ImGui::Render().
void EndFrame();

// Releases every Direct3D reference Facet holds. Safe to call when not initialized.
void Shutdown();

bool IsInitialized();

// Library version as "major.minor.patch". Never null.
const char* Version();

// Number of completed NewFrame() calls since Init().
std::uint64_t FrameIndex();

// Delta time of the current frame in seconds, as read in NewFrame().
float DeltaTime();

} // namespace facet
