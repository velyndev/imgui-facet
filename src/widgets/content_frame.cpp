// Per-frame work of DataTable and the widget families charts, media and composites, for example
// sampling frame times for the frame time graph (the rest of display.h has an entry of its own,
// see src/internal/subsystem.h). The widgets are placeholders, so the module has no hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsContentSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_content";
    return subsystem;
}

} // namespace facet::internal
