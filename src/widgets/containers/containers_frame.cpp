// Per-frame work of the containers family (containers.h). At the end of a frame every in-flow
// container forgets the Begins whose End was not called (the scope stack reports them by name)
// and puts back the work rect it narrowed, and the backdrop splits a missing End left open are
// merged, so the next frame starts clean. At shutdown the overlay records are forgotten.

#include "internal/subsystem.h"
#include "widgets/containers/container_layout.h"

namespace facet::internal
{
namespace
{

void ContainersEndFrame()
{
    ClearAccordionStack();
    ClearDisclosureStack();
    ClearGroupBoxStack();
    ClearCardStack();
    ClearShellStack();
    ClearMiniWindowStack();
    ClearInspectorPaneStack();
    ClearContainerBackdrops();
    ClearTitleBarStack();
    RotateTitleBarRegions();
    RotateAppFrameRecord();
    RotateOverlayRegions();
    ClearTitleBarPress();
}

void ContainersShutdown()
{
    ResetOverlayRecords();
}

} // namespace

Subsystem WidgetsContainersSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_containers";
    subsystem.endFrame = &ContainersEndFrame;
    subsystem.shutdown = &ContainersShutdown;
    return subsystem;
}

} // namespace facet::internal
