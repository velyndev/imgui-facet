// Demo page: Composites. Owner: the content API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("composites", "Composites", "Widgets", 140)
{
    facet::demo::PageHeader("Composites",
                            "Curve and gradient editors, property grids, console views, key bind and "
                            "preset lists, and inventory grids.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/composites.h and implemented in Phase 2. This page will "
                      "show each of them.");
}