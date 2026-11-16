// Backdrop blur pass on Direct3D 11.
//
// The blur pass does not exist yet: GlassAvailable() is false and DrawGlass draws the
// opaque fallback panel (the tint at full opacity plus the hairline border).

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/effects.h"
#include "facet/core/theme.h"
#include "internal/subsystem.h"

namespace facet
{

void DrawGlass(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const GlassSpec& spec)
{
    if (drawList == nullptr)
        return;
    const Theme& theme = T();
    const ImVec4 tint = spec.tint.w > 0.0f ? spec.tint : theme.colors.glassTint;
    FillRect(drawList, pMin, pMax, ToU32(WithAlpha(tint, 1.0f)), radius);
    if (spec.hairline)
        StrokeRect(drawList, pMin, pMax, ToU32(theme.colors.separator), radius, HairlineThickness());
}

bool GlassAvailable()
{
    return false;
}

namespace internal
{

// No hooks yet: the blur pass creates no device objects.
Subsystem BlurSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "blur";
    return subsystem;
}

} // namespace internal

} // namespace facet
