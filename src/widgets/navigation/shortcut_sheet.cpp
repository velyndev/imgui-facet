// ShortcutSheet (navigation.h). Planned: no sheet opens until Phase 2 implements it; while the
// caller asks for it to be open, the placeholder of src/widgets/structure_stub.h shows the
// request in the page flow. *open is never written.

#include "facet/widgets/navigation.h"
#include "widgets/structure_stub.h"

namespace facet
{

void ShortcutSheet(const char* label, bool* open, const ShortcutSheetOptions&)
{
    internal::StubLayer("ShortcutSheet", label, open != nullptr && *open);
}

} // namespace facet
