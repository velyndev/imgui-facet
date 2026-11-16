// Icon registry and atlas.

#include "internal/subsystem.h"

namespace facet::internal
{

// No hooks yet: the module keeps no state that needs the lifetime events.
Subsystem IconSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "icon";
    return subsystem;
}

} // namespace facet::internal
