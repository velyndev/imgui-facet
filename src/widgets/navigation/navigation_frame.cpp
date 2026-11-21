// Per-frame and per-device work of the navigation family (navigation.h). The entry is in the
// subsystem table (src/core/subsystems.cpp) from the start, so the family fills its hooks here
// without touching a shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsNavigationSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_navigation";
    return subsystem;
}

} // namespace facet::internal
