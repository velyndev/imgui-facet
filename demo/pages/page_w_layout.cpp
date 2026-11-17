// Demo page: Layout. Owner: the structure API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("layout", "Layout", "Widgets", 90)
{
    facet::demo::PageHeader("Layout",
                            "Stacks, grids, flow layout, split views, panels, settings rows, "
                            "separators and item grids.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/layout.h and implemented in Phase 2. This page will show "
                      "each of them.");
}