// Per-frame and per-device work of the media family (media.h). The entry is in the subsystem table
// (src/core/subsystems.cpp) from the start, so the family fills its hooks here without touching a
// shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsMediaSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_media";
    return subsystem;
}

} // namespace facet::internal
