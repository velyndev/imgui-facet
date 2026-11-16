// UI scale: DPI scale times user scale.

#include "internal/subsystem.h"

namespace facet::internal
{

// No hooks yet: the module keeps no state that needs the lifetime events.
Subsystem ScaleSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "scale";
    return subsystem;
}

} // namespace facet::internal
