#pragma once

// Library lifetime: Init, the per-frame calls, device object handling and Shutdown.
//
// Threading: every Facet function is for the thread that runs the Dear ImGui frame. The
// only work on other threads is the image decoding of asynchronous loads, on worker threads
// Facet owns (image.h).
// The draw data of a frame is rendered on that same thread, with the context given to Init,
// before the next facet::NewFrame. Facet supports one Dear ImGui context at a time.

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
    // Required: the immediate context that renders the Dear ImGui draw data (the one given
    // to the DX11 renderer backend). Facet's draw callbacks issue their calls on it.
    ID3D11DeviceContext* context = nullptr;
};

// Starts Facet. Requires a current Dear ImGui context and a valid device and context.
// Returns false (and leaves Facet uninitialized) when a requirement is missing, when a
// module fails to start, or when Facet is already initialized.
// Call it before the first ImGui::NewFrame: Dear ImGui reads its ini file there, and Facet's
// saved widget state is part of that file. A later Init reads Facet's sections from
// io.IniFilename itself. A host that loads the ini text on its own (io.IniFilename null,
// ImGui::LoadIniSettingsFromMemory) must call Init before that, or Facet's sections are
// skipped and dropped at the next save.
bool Init(const InitDesc& desc);

// Call once per frame after ImGui::NewFrame(). Reads the frame's delta time from ImGui,
// so a fixed io.DeltaTime gives fixed Facet timing as well.
void NewFrame();

// Call once per frame before ImGui::Render().
void EndFrame();

// Releases every Direct3D reference Facet holds. Safe to call when not initialized.
void Shutdown();

// Releases every Direct3D object Facet created (textures, atlases, shaders, blur targets)
// while keeping all other state. Call it before the host tears down device-dependent
// resources it shares with Facet, then CreateDeviceObjects() to rebuild them on the same
// device. For a new device, use Shutdown() and Init() instead. Does nothing when Facet is
// not initialized or the objects are already released.
// Call it only outside a frame: before facet::NewFrame, or after the renderer has rendered
// the previous frame's draw data. Never between recording a frame and rendering it: draw
// commands already recorded hold raw texture pointers (images, icons, shadows, glass) that
// this releases, and the renderer would then bind freed objects. Debug builds assert when it
// is called between facet::NewFrame and facet::EndFrame.
void InvalidateDeviceObjects();

// Recreates the objects released by InvalidateDeviceObjects(). Returns true when they
// exist afterwards (also when they were never released), false when Facet is not
// initialized or a module failed to create its objects. The same timing rule applies.
bool CreateDeviceObjects();

bool IsInitialized();

// Library version as "major.minor.patch": FACET_VERSION of the library build (version.h).
// Never null.
const char* Version();

// Number of completed NewFrame() calls since Init().
std::uint64_t FrameIndex();

// Delta time of the current frame in seconds, as read in NewFrame().
float DeltaTime();

} // namespace facet
