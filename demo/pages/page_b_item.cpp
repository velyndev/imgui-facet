// Demo page: Item behaviour. Owner: the interaction blocks package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("item", "Item behaviour", "Blocks", 10)
{
    facet::demo::PageHeader("Item behaviour",
                            "The hover, press, focus and disabled logic every Facet widget shares, "
                            "and the interaction states a capture run can show without input.");
    facet::demo::Note("Planned: the block is being built in Phase 1c. This page will show it "
                      "working.");
}