// Drag and drop core: the drag in progress, drop targets, and the drag preview drawn on top
// of everything at facet::EndFrame.
// Planned for Phase 1c part B: drag_drop.h and the behaviour. For now the module has no hooks.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem DragDropSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "drag_drop";
    return subsystem;
}

} // namespace facet::internal