// Per-frame work of the overlays family (overlays.h). NewFrame opens the toast queue; EndFrame, which
// runs after every page has drawn, draws the toast stack and empties the stacks of Begin / End pairs
// left open (the scope stack reports them); Shutdown forgets every toast.

#include "internal/subsystem.h"
#include "widgets/overlays/overlay_parts.h"

namespace facet::internal
{
namespace
{

void OverlaysNewFrame()
{
    OverlayToastNewFrame();
}

void OverlaysEndFrame()
{
    OverlayScrollResetStack();
    OverlayPopoverResetStack();
    OverlayModalResetStack();
    OverlayToastEndFrame();
}

void OverlaysShutdown()
{
    OverlayToastShutdown();
}

} // namespace

Subsystem WidgetsOverlaysSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_overlays";
    subsystem.newFrame = &OverlaysNewFrame;
    subsystem.endFrame = &OverlaysEndFrame;
    subsystem.shutdown = &OverlaysShutdown;
    return subsystem;
}

} // namespace facet::internal
