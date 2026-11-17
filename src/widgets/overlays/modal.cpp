// BeginModal and EndModal (overlays.h). Planned: no dialog opens until Phase 2 implements it,
// so BeginModal returns false; while *open is true, the placeholder of
// src/widgets/structure_stub.h shows the request in the page flow. *open is never written.

#include "facet/widgets/overlays.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginModal(const char* label, bool* open, const ModalOptions&)
{
    internal::StubLayer("Modal", label, open != nullptr && *open);
    return false;
}

void EndModal()
{
    // BeginModal never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginModal");
}

} // namespace facet
