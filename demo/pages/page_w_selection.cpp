// Demo page: Selection. Owner: the controls API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("selection", "Selection", "Widgets", 20)
{
    facet::demo::PageHeader("Selection",
                            "Toggles, checkboxes, radio groups, segmented controls, chips, cycle "
                            "selectors, option cards and the key bind mode selector.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/selection.h and implemented in Phase 2. This page will "
                      "show each of them.");
}