// Per-frame and per-device work of the editor and canvas composites (PropertyGrid, CurveEditor,
// BezierEditor, GradientEditor, PreviewCanvas, CanvasElement, LayerList). The entry is in the
// subsystem table (src/core/subsystems.cpp). At facet::EndFrame every Begin of these composites whose
// End was not called is forgotten (the scope stack has reported it), so the next frame starts clean.

#include "internal/subsystem.h"
#include "widgets/composites/editors_frame.h"

namespace facet::internal
{
namespace
{

void EditorsEndFrame()
{
    ClearPropertyGridStacks();
    ClearPreviewCanvasStack();
}

void EditorsShutdown()
{
    ClearPropertyGridStacks();
    ClearPreviewCanvasStack();
}

} // namespace

Subsystem WidgetsEditorsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_editors";
    subsystem.endFrame = &EditorsEndFrame;
    subsystem.shutdown = &EditorsShutdown;
    return subsystem;
}

} // namespace facet::internal
