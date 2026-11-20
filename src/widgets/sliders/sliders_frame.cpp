// Per-frame and per-device work of the sliders family (sliders.h). The entry exists in the
// subsystem table (src/core/subsystems.cpp) from the start, so the family fills its hooks here
// without touching a shared file. Init and Shutdown forget a typed value in progress
// (slider_core.h), so nothing carries over to another Dear ImGui context.

#include "internal/subsystem.h"
#include "widgets/sliders/slider_core.h"

namespace facet::internal
{

Subsystem WidgetsSlidersSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_sliders";
    subsystem.init = [] {
        ResetSliderTyping();
        return true;
    };
    subsystem.shutdown = [] { ResetSliderTyping(); };
    return subsystem;
}

} // namespace facet::internal
