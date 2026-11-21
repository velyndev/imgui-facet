// The placeholder frames of the structure placeholders (src/widgets/structure_stub.h): the end of a
// frame forgets the frames a caller left open (the scope stack, which runs after this module,
// reports them). The families themselves have entries of their own (src/internal/subsystem.h).

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
