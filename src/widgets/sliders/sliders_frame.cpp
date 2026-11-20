// Per-frame and per-device work of the sliders family (sliders.h). The entry exists in the
// subsystem table (src/core/subsystems.cpp) from the start, so the family fills its hooks here
// without touching a shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsSlidersSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_sliders";
    return subsystem;
}

} // namespace facet::internal
