// Demo page: Selection pill. Owner: the interaction blocks package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("pill", "Selection pill", "Blocks", 70)
{
    facet::demo::PageHeader("Selection pill",
                            "The highlight that springs to the selected entry, shared by tabs, the "
                            "sidebar, segmented controls and lists.");
    facet::demo::Note("Planned: the block is being built in Phase 1c. This page will show it "
                      "working.");
}