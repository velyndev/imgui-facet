// Demo page: Lists. Owner: the structure API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("lists", "Lists", "Widgets", 60)
{
    facet::demo::PageHeader("Lists",
                            "Dropdowns, combo boxes, list boxes, selectable and reorderable lists, "
                            "tree views and transfer lists.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/lists.h and implemented in Phase 2. This page will show "
                      "each of them.");
}