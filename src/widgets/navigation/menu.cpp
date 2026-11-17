// BeginMenu and EndMenu (navigation.h). Planned: draws the placeholder of the menu title row
// from src/widgets/structure_stub.h until Phase 2 implements menus. The menu never opens, so
// BeginMenu returns false.

#include "facet/widgets/navigation.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginMenu(const char* label, const MenuOptions&)
{
    internal::StubItem("Menu", label, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Inherit));
    return false;
}

void EndMenu()
{
    // BeginMenu never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginMenu");
}

} // namespace facet
