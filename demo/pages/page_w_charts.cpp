// Demo page: Charts. Owner: the content API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("charts", "Charts", "Widgets", 120)
{
    facet::demo::PageHeader("Charts",
                            "Sparklines, line, bar and donut charts, gauges, meters and legends.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/charts.h and implemented in Phase 2. This page will show "
                      "each of them.");
}