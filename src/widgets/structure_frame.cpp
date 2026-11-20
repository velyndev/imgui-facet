// Per-frame work of the widget families navigation, layout and overlays, and of the placeholder
// frames of every structure placeholder (lists and containers have entries of their own, see
// src/internal/subsystem.h, but their placeholders still use these frames). The end of a frame
// forgets the placeholder frames a caller left open (the scope stack, which runs after this
// module, reports them). The toast stack of overlays.h is planned to be drawn from here.

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
