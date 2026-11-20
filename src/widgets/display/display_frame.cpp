// Per-frame and per-device work of the display family (display.h), DataTable excepted, for example
// the frame clock of pulsing status dots. The entry exists in the subsystem table
// (src/core/subsystems.cpp) from the start, so the family fills its hooks here without touching a
// shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsDisplaySubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_display";
    return subsystem;
}

} // namespace facet::internal
