// Soft shadows and glow.

#include "internal/subsystem.h"

namespace facet::internal
{

// No hooks yet: the module keeps no state that needs the lifetime events.
Subsystem ShadowSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "shadow";
    return subsystem;
}

} // namespace facet::internal
