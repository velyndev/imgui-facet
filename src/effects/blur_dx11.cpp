// Backdrop blur pass on Direct3D 11.

#include "internal/subsystem.h"

namespace facet::internal
{

// No hooks yet: the module keeps no state that needs the lifetime events.
Subsystem BlurSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "blur";
    return subsystem;
}

} // namespace facet::internal
