// BeginSidebar, SidebarItem, BeginSidebarSection and their Ends (navigation.h). Planned: draws
// the placeholders of src/widgets/structure_stub.h until Phase 2 implements the sidebar. The
// sidebar shows its items unless the caller hides it, and its sections show the open state they
// start with; items never activate.

#include "facet/widgets/navigation.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginSidebar(const char* id, const SidebarOptions& options)
{
    if (options.mode == SidebarMode::Hidden || !internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginSidebar", "Sidebar", id, false);
    return true;
}

void EndSidebar()
{
    internal::EndStubFrame("BeginSidebar");
}

bool SidebarItem(const char* label, Icon, const SidebarItemOptions&)
{
    internal::StubItem("SidebarItem", label, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Inherit));
    return false;
}

bool BeginSidebarSection(const char* label, const SidebarSectionOptions& options)
{
    if (!internal::StubCanDraw())
        return false;
    if (options.collapsible && !options.defaultOpen)
    {
        // No open state is kept yet: a section that starts closed stays closed.
        internal::StubItem("SidebarSection", label, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Inherit));
        return false;
    }
    internal::BeginStubFrame("BeginSidebarSection", "SidebarSection", label, false);
    return true;
}

void EndSidebarSection()
{
    internal::EndStubFrame("BeginSidebarSection");
}

} // namespace facet
