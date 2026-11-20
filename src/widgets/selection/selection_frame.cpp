// Per-frame and per-device work of the selection family (selection.h). The entry exists in the
// subsystem table (src/core/subsystems.cpp) from the start, so the family fills its hooks here
// without touching a shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsSelectionSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_selection";
    return subsystem;
}

} // namespace facet::internal
