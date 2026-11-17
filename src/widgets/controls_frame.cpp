// Per-frame work of the widget families buttons, selection, sliders, inputs and pickers.
// Owner from Phase 1c part A on: the controls API package, which fills the hooks it needs.
// For now the module has no hooks.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsControlsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_controls";
    return subsystem;
}

} // namespace facet::internal
