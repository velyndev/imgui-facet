// Per-frame work of the widget families lists, navigation, containers, layout and overlays,
// for example the toast stack, which facet::EndFrame draws.
// Owner from Phase 1c part A on: the structure API package, which fills the hooks it needs.
// For now the module has no hooks.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsStructureSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_structure";
    return subsystem;
}

} // namespace facet::internal
