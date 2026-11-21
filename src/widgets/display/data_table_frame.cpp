// Per-frame and per-device work of DataTable and its cell editing (display.h). The rest of the
// display family has its own entry (display_frame.cpp). The entry is in the subsystem table
// (src/core/subsystems.cpp) from the start, so DataTable fills its hooks here without touching a
// shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsDataTableSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_data_table";
    return subsystem;
}

} // namespace facet::internal
