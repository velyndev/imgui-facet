// MenuItem, MenuCheckItem and MenuRadioItem (navigation.h). Planned: each draws the placeholder
// of src/widgets/structure_stub.h under its own name until Phase 2 implements menu items.
// Returns false, never writes *checked or *selection and never triggers a shortcut.

#include "facet/widgets/navigation.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawMenuItem(const char* widget, const char* label)
{
    internal::StubItem(widget, label, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Inherit));
}

} // namespace

bool MenuItem(const char* label, const MenuItemOptions&)
{
    DrawMenuItem("MenuItem", label);
    return false;
}

bool MenuCheckItem(const char* label, bool*, const MenuItemOptions&)
{
    DrawMenuItem("MenuCheckItem", label);
    return false;
}

bool MenuRadioItem(const char* label, int*, int, const MenuItemOptions&)
{
    DrawMenuItem("MenuRadioItem", label);
    return false;
}

} // namespace facet
