// Scroll core: overlay scrollbars, smooth wheel scrolling and edge fades.
// Owner from Phase 1c part B on: the surface blocks package, which adds scroll.h and the
// behaviour. For now the module has no hooks.

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