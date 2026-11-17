// BeginDrawer and EndDrawer (overlays.h). Planned: no drawer opens until Phase 2 implements it,
// so BeginDrawer returns false; while *open is true, the placeholder of
// src/widgets/structure_stub.h shows the request in the page flow. *open is never written.

#include "facet/widgets/overlays.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginDrawer(const char* label, bool* open, const DrawerOptions&)
{
    internal::StubLayer("Drawer", label, open != nullptr && *open);
    return false;
}

void EndDrawer()
{
    // BeginDrawer never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginDrawer");
}

} // namespace facet
