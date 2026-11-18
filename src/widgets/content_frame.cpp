// Per-frame work of the widget families display, charts, media and composites, for example
// sampling frame times for the frame time graph.
// The widgets are placeholders until Phase 2, so the module has no hooks yet.

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
