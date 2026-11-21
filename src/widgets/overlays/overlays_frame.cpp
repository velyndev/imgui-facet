// Per-frame and per-device work of the overlays family (overlays.h), for example drawing the toast
// stack at the end of the frame, after every page has drawn. The entry is in the subsystem table
// (src/core/subsystems.cpp) from the start, so the family fills its hooks here without touching a
// shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsOverlaysSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_overlays";
    return subsystem;
}

} // namespace facet::internal
