// Demo page: Navigation. Owner: the structure API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("navigation", "Navigation", "Widgets", 70)
{
    facet::demo::PageHeader("Navigation",
                            "The sidebar, tab bars, menu bars, menus and menu items, breadcrumbs, "
                            "pagination and wizards.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/navigation.h and implemented in Phase 2. This page will "
                      "show each of them.");
}