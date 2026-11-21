// Per-frame work of the lists family (lists.h): the stacks of the Begin / End pairs of the family
// (combos, tree views) are reset at facet::EndFrame and at Shutdown, so a Begin whose End was never
// called (facet::EndFrame reports it by name through the scope stack) does not leak into the next
// frame.

#include "internal/subsystem.h"
#include "widgets/lists/list_parts.h"

namespace facet::internal
{
namespace
{

void ListsEndFrame()
{
    ListsComboEndFrame();
}

} // namespace

Subsystem WidgetsListsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_lists";
    subsystem.endFrame = &ListsEndFrame;
    subsystem.shutdown = &ListsEndFrame;
    return subsystem;
}

} // namespace facet::internal
