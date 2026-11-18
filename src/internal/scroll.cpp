// Scroll core: overlay scrollbars, smooth wheel scrolling and edge fades.
// Planned for Phase 1c part B: scroll.h and the behaviour. For now the module has no hooks.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem ScrollSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "scroll";
    return subsystem;
}

} // namespace facet::internal