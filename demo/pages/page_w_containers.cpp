// Demo page: Containers. Owner: the structure API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("containers", "Containers", "Widgets", 80)
{
    facet::demo::PageHeader("Containers",
                            "The floating shell, the app frame, tool and status bars, accordions, "
                            "cards, scroll areas and virtual lists.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/containers.h and implemented in Phase 2. This page will "
                      "show each of them.");
}