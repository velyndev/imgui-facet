// Per-frame work of the pickers family. The buttons, selection, sliders and inputs families have
// entries of their own (src/internal/subsystem.h). The pickers are placeholders, so the module
// has no hooks yet.

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
