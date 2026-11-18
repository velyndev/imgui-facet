// Per-frame work of the widget families buttons, selection, sliders, inputs and pickers.
// The widgets are placeholders until Phase 2, so the module has no hooks yet.

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
