// Per-frame work of the overlays family (overlays.h). EndFrame, which runs after every page has drawn,
// empties the stacks of Begin / End pairs left open (the scope stack reports them).

#include "internal/subsystem.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet::internal
{
namespace
{

void OverlaysEndFrame()
{
    OverlayScrollResetStack();
    OverlayPopoverResetStack();
    OverlayModalResetStack();
}

} // namespace

Subsystem WidgetsOverlaysSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_overlays";
    subsystem.endFrame = &OverlaysEndFrame;
    return subsystem;
}

} // namespace facet::internal
