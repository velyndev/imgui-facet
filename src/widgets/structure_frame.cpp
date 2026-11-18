// Per-frame work of the widget families lists, navigation, containers, layout and overlays.
// While the widgets are planned, the end of a frame forgets the placeholder frames a caller left
// open (the scope stack, which runs after this module, reports them). The toast stack of
// overlays.h is planned to be drawn from here once Phase 2 implements it.

#include "internal/subsystem.h"
#include "widgets/structure_stub.h"

namespace facet::internal
{

Subsystem WidgetsStructureSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_structure";
    subsystem.endFrame = &ClearStubFrames;
    subsystem.shutdown = &ClearStubFrames;
    return subsystem;
}

} // namespace facet::internal
