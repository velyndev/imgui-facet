// Per-frame work of the lists family (lists.h): the stack of open combos is reset at facet::EndFrame
// and at Shutdown, so a BeginCombo whose EndCombo was never called (facet::EndFrame reports it by
// name through the scope stack) does not leak into the next frame. The tree view keeps no stack of
// its own: the list core resets its trees.

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
