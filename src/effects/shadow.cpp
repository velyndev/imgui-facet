// Soft shadows and glow.
//
// The shadow atlas does not exist yet: DrawShadow, DrawElevation and DrawGlow draw
// nothing. ElevationShadow already returns the scaled token values.

#include "facet/core/effects.h"

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/subsystem.h"

namespace facet
{

ShadowSpec ElevationShadow(int level)
{
    const Theme& theme = T();
    const ShadowLevel& shadow = theme.elevation.Level(level);
    ShadowSpec spec;
    spec.offset = Px(shadow.offset);
    spec.blur = Px(shadow.blur);
    spec.spread = Px(shadow.spread);
    spec.color = ToU32(WithAlpha(theme.colors.shadow, shadow.opacity));
    return spec;
}

void DrawShadow(ImDrawList*, ImVec2, ImVec2, float, const ShadowSpec&)
{
}

void DrawElevation(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, int level)
{
    if (level <= 0)
        return;
    DrawShadow(drawList, pMin, pMax, radius, ElevationShadow(level));
}

void DrawGlow(ImDrawList*, ImVec2, ImVec2, float, ImU32, float)
{
}

namespace internal
{

// No hooks yet: the shadow atlas does not exist.
Subsystem ShadowSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "shadow";
    return subsystem;
}

} // namespace internal

} // namespace facet
