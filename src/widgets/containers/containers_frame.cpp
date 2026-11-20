// Per-frame and per-device work of the containers family (containers.h). The entry exists in the
// subsystem table (src/core/subsystems.cpp) from the start, so the family fills its hooks here
// without touching a shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsContainersSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_containers";
    return subsystem;
}

} // namespace facet::internal
