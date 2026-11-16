// Icon registry and atlas.
//
// The registry does not exist yet: no icon is registered, IconGlyph reserves its layout
// space without drawing, and DrawIcon draws nothing.

#include "facet/core/icons.h"

#include "internal/subsystem.h"

namespace facet
{

void IconGlyph(Icon, float size, ImU32)
{
    const float side = size > 0.0f ? size : ImGui::GetFontSize();
    ImGui::Dummy(ImVec2(side, side));
}

void DrawIcon(ImDrawList*, Icon, ImVec2, float, ImU32)
{
}

Icon RegisterIcon(const char*, const char*)
{
    return {};
}

Icon FindIcon(const char*)
{
    return {};
}

namespace internal
{

// No hooks yet: there is no atlas.
Subsystem IconSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "icon";
    return subsystem;
}

} // namespace internal

} // namespace facet
