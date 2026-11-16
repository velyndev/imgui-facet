// Fonts and text styles.

#include "internal/subsystem.h"

namespace facet::internal
{

// No hooks yet: the module keeps no state that needs the lifetime events.
Subsystem FontSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "font";
    return subsystem;
}

} // namespace facet::internal
