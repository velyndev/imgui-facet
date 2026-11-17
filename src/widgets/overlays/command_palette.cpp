// CommandPalette (overlays.h). Planned: no palette opens until Phase 2 implements it; while
// *open is true, the placeholder of src/widgets/structure_stub.h shows the request in the page
// flow. Returns false and never writes *open or *picked.

#include "facet/widgets/overlays.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool CommandPalette(const char* label, bool* open, int*, std::span<const ChoiceItem>, const CommandPaletteOptions&)
{
    internal::StubLayer("CommandPalette", label, open != nullptr && *open);
    return false;
}

bool CommandPalette(const char* label, bool* open, int*, std::span<const char* const>, const CommandPaletteOptions&)
{
    internal::StubLayer("CommandPalette", label, open != nullptr && *open);
    return false;
}

bool CommandPalette(const char* label, bool* open, ShortcutId*, const CommandPaletteOptions&)
{
    internal::StubLayer("CommandPalette", label, open != nullptr && *open);
    return false;
}

} // namespace facet
