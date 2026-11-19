// TabBar, TabBarClosedTab and TabBarMovedTab (navigation.h). Planned: draws the placeholder of
// src/widgets/structure_stub.h until Phase 2 implements the tab bar. Returns false, never
// writes *index and reports no closed or moved tab.

#include "facet/widgets/navigation.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawTabBar(const char* id, const TabBarOptions& options)
{
    internal::StubItem("TabBar", id, internal::StubWidth(options.width, true), ControlHeight(options.size));
}

} // namespace

bool TabBar(const char* id, int*, std::span<const ChoiceItem>, const TabBarOptions& options)
{
    DrawTabBar(id, options);
    return false;
}

bool TabBar(const char* id, int*, std::span<const char* const>, const TabBarOptions& options)
{
    DrawTabBar(id, options);
    return false;
}

int TabBarClosedTab()
{
    return -1;
}

ReorderMove TabBarMovedTab()
{
    return {};
}

} // namespace facet
