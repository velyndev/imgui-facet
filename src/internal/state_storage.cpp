// Per-widget state storage.

#include "internal/subsystem.h"

namespace facet::internal
{

// No hooks yet: the module keeps no state that needs the lifetime events.
Subsystem StateStorageSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "state_storage";
    return subsystem;
}

} // namespace facet::internal
