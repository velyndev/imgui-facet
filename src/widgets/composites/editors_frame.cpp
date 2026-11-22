// Per-frame and per-device work of the editor and canvas composites (PropertyGrid, CurveEditor,
// BezierEditor, GradientEditor, PreviewCanvas, CanvasElement, LayerList). The entry is in the
// subsystem table (src/core/subsystems.cpp) from the start, so the package fills its hooks here
// without touching a shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsEditorsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_editors";
    return subsystem;
}

} // namespace facet::internal
