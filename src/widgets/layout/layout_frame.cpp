// Per-frame work of the layout family (layout.h). At the end of a frame every container of the
// family forgets the Begins whose End was not called (the scope stack reports them by name) and
// puts back what it changed in a window, so the next frame starts clean.

#include "internal/subsystem.h"
#include "widgets/layout/layout_internal.h"

namespace facet::internal
{
namespace
{

void LayoutEndFrame()
{
    ClearSettingsRowStack();
    ClearStackStack();
    ClearGridStack();
    ClearSplitViewStack();
    ClearDependentGroupStack();
}

} // namespace

Subsystem WidgetsLayoutSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_layout";
    subsystem.endFrame = &LayoutEndFrame;
    return subsystem;
}

} // namespace facet::internal
