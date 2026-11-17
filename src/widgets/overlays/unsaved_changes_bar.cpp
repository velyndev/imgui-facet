// UnsavedChangesBar (overlays.h). Planned: while changeCount > 0, draws the placeholder of
// src/widgets/structure_stub.h in place of the bar until Phase 2 implements it. Returns None.

#include "facet/widgets/overlays.h"
#include "widgets/structure_stub.h"

namespace facet
{

UnsavedChangesAction UnsavedChangesBar(const char* id, int changeCount, const UnsavedChangesBarOptions&)
{
    internal::StubLayer("UnsavedChangesBar", id, changeCount > 0);
    return UnsavedChangesAction::None;
}

} // namespace facet
