// BeginPopover and EndPopover (overlays.h). Planned: no popover opens until Phase 2 implements
// it, so BeginPopover returns false; while *open is true, the placeholder of
// src/widgets/structure_stub.h shows the request in the page flow. *open is never written.

#include "facet/widgets/overlays.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginPopover(const char* id, bool* open, const PopoverOptions&)
{
    internal::StubLayer("Popover", id, open != nullptr && *open);
    return false;
}

void EndPopover()
{
    // BeginPopover never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginPopover");
}

} // namespace facet
