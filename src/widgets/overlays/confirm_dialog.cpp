// ConfirmDialog (overlays.h). Planned: no dialog opens until Phase 2 implements it; while *open
// is true, the placeholder of src/widgets/structure_stub.h shows the request in the page flow.
// Returns None and never writes *open.

#include "facet/widgets/overlays.h"
#include "widgets/structure_stub.h"

namespace facet
{

DialogResult ConfirmDialog(const char* label, bool* open, const char*, const ConfirmDialogOptions&)
{
    internal::StubLayer("ConfirmDialog", label, open != nullptr && *open);
    return DialogResult::None;
}

} // namespace facet
