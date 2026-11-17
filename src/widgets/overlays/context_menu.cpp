// BeginContextMenu and EndContextMenu (overlays.h). Planned: the menu never opens until Phase 2
// implements it, so BeginContextMenu returns false and, like a closed context menu, draws
// nothing.

#include "facet/widgets/overlays.h"
#include "internal/scope_stack.h"

namespace facet
{

bool BeginContextMenu(const char*, const ContextMenuOptions&)
{
    return false;
}

void EndContextMenu()
{
    // BeginContextMenu never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginContextMenu");
}

} // namespace facet
