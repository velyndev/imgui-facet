#pragma once

// Subsystem table: how library modules hook into the Facet lifetime.
//
// Every module that keeps state or Direct3D objects describes its hooks in one Subsystem
// value returned by its accessor below. The accessor is defined in the module's own .cpp
// file, and src/core/subsystems.cpp lists the accessors in start-up order. The table is
// explicit on purpose: self-registering static objects in a static library are dropped by
// the linker when nothing else references their translation unit.
//
// Lifetime driven by src/core/context.cpp:
//   facet::Init        init() of every module in table order, then createDeviceObjects()
//                      in table order. A failure stops and undoes everything started so far.
//   facet::NewFrame    newFrame() in table order, after the frame index and delta time
//                      were updated.
//   facet::EndFrame    endFrame() in table order.
//   facet::Shutdown    invalidateDeviceObjects() in reverse order, then shutdown() in
//                      reverse order.
//   facet::InvalidateDeviceObjects / CreateDeviceObjects
//                      the device hooks alone, reverse and table order respectively.
//   NotifyScaleChanged scaleChanged() in table order (only while initialized).
//   NotifyThemeChanged themeChanged() in table order (only while initialized).
//
// Rules for hook authors:
//   - Any member may be null; a null init or createDeviceObjects counts as success.
//   - Create Direct3D objects only in createDeviceObjects, never in init, so a device
//     reset and the first start take the same path.
//   - invalidateDeviceObjects and shutdown must be safe when nothing was created.
//   - Return false from init or createDeviceObjects only for fatal problems. Optional
//     features (for example the backdrop blur) fall back and return true.
//   - Hooks run with the Dear ImGui context of the host current.

#include <span>

namespace facet::internal
{

struct Subsystem
{
    const char* name = "";
    bool (*init)() = nullptr;
    void (*shutdown)() = nullptr;
    void (*newFrame)() = nullptr;
    void (*endFrame)() = nullptr;
    void (*scaleChanged)(float scale) = nullptr;
    void (*themeChanged)() = nullptr;
    void (*invalidateDeviceObjects)() = nullptr;
    bool (*createDeviceObjects)() = nullptr;
};

// One accessor per module, each defined in the file of its owner.
Subsystem ThemeSubsystem();         // src/core/theme.cpp
Subsystem ScaleSubsystem();         // src/core/scale.cpp
Subsystem FontSubsystem();          // src/core/text.cpp
Subsystem AnimSubsystem();          // src/core/anim.cpp
Subsystem StateStorageSubsystem();  // src/internal/state_storage.cpp
Subsystem ShadowSubsystem();        // src/effects/shadow.cpp
Subsystem BlurSubsystem();          // src/effects/blur_dx11.cpp
Subsystem TextureSubsystem();       // src/image/texture_cache.cpp
Subsystem IconSubsystem();          // src/image/icon_atlas.cpp
Subsystem WidgetsCommonSubsystem(); // src/widgets/common.cpp

// The ordered table (src/core/subsystems.cpp). Built on first use, valid for the whole run.
std::span<const Subsystem> Subsystems();

// Table drivers used by context.cpp. They take the table as a parameter so tests can run
// them on a table of their own.
//
// Runs init() in order, then createDeviceObjects() in order. When a hook fails, every
// module that was started gets invalidateDeviceObjects() and shutdown() in reverse order,
// and the function returns false.
bool StartSubsystems(std::span<const Subsystem> table);
// Runs invalidateDeviceObjects() in reverse order, then shutdown() in reverse order.
void StopSubsystems(std::span<const Subsystem> table);

// Called by the scale module when the effective scale (facet::Scale()) changed, and by the
// theme module when the current theme changed. Both do nothing while Facet is not initialized.
void NotifyScaleChanged(float scale);
void NotifyThemeChanged();

} // namespace facet::internal
