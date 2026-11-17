// BeginMiniWindow and EndMiniWindow (containers.h). Planned: until Phase 2 implements the mini
// window, no window opens and BeginMiniWindow returns false; the placeholder of
// src/widgets/structure_stub.h shows the call in the page flow. *pinned is never written.

#include "facet/widgets/containers.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginMiniWindow(const char* label, bool*, const MiniWindowOptions&)
{
    // A mini window shows while the overlay menu is visible, and the stub controller always
    // shows the menu.
    internal::StubLayer("MiniWindow", label, true);
    return false;
}

void EndMiniWindow()
{
    // BeginMiniWindow never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginMiniWindow");
}

} // namespace facet
