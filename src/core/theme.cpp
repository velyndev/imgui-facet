// Theme tokens and the current theme.

#include "internal/subsystem.h"

namespace facet::internal
{

// No hooks yet: the module keeps no state that needs the lifetime events.
Subsystem ThemeSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "theme";
    return subsystem;
}

} // namespace facet::internal
