// The ordered subsystem table. See src/internal/subsystem.h for the hook rules.
//
// Order and its reasons:
//   theme, scale     plain token state that every later module reads
//   state storage    per-widget state other modules may keep entries in
//   anim             motion store; reads theme motion tokens
//   font             font atlas setup; needs the scale
//   texture          image and texture cache; the icon atlas uploads through it
//   icon             icon atlas; rasterized per scale
//   shadow           shadow and glow atlas; rasterized per scale
//   blur             backdrop blur pass; last, it is optional and may fall back
// Shutdown runs in reverse, so every module still finds what it depends on.

#include "internal/subsystem.h"

#include <array>

namespace facet::internal
{

std::span<const Subsystem> Subsystems()
{
    static const std::array<Subsystem, 9> table = {
        ThemeSubsystem(),
        ScaleSubsystem(),
        StateStorageSubsystem(),
        AnimSubsystem(),
        FontSubsystem(),
        TextureSubsystem(),
        IconSubsystem(),
        ShadowSubsystem(),
        BlurSubsystem(),
    };
    return table;
}

} // namespace facet::internal
