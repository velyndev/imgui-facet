// Per-frame and per-device work of the charts family (charts.h), for example sampling io.DeltaTime
// in every facet::NewFrame for FrameTimeGraph. The entry is in the subsystem table
// (src/core/subsystems.cpp) from the start, so the family fills its hooks here without touching a
// shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsChartsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_charts";
    return subsystem;
}

} // namespace facet::internal
