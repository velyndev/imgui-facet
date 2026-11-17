// Demo page: Chart core. Owner: the data blocks package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("chart-core", "Chart core", "Blocks", 100)
{
    facet::demo::PageHeader("Chart core",
                            "The scales, ticks and series layout the charts share.");
    facet::demo::Note("Planned: the block is being built in Phase 1c. This page will show it "
                      "working.");
}