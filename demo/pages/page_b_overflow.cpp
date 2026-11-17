// Demo page: Overflow. Owner: the interaction blocks package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("overflow", "Overflow", "Blocks", 40)
{
    facet::demo::PageHeader("Overflow",
                            "Fitting a row of items into the available width, with the items that do "
                            "not fit moved into a menu.");
    facet::demo::Note("Planned: the block is being built in Phase 1c. This page will show it "
                      "working.");
}