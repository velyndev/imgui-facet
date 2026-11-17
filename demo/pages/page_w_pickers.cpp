// Demo page: Pickers. Owner: the controls API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("pickers", "Pickers", "Widgets", 50)
{
    facet::demo::PageHeader("Pickers",
                            "Color, date, time, angle, icon and thumbnail pickers, and the file "
                            "browser.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/pickers.h and implemented in Phase 2. This page will show "
                      "each of them.");
}