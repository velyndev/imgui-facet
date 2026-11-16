// Image handles and the texture cache.

#include "internal/subsystem.h"

namespace facet::internal
{

// No hooks yet: the module keeps no state that needs the lifetime events.
Subsystem TextureSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "texture";
    return subsystem;
}

} // namespace facet::internal
