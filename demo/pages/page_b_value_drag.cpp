// Demo page: Value drag. Owner: the interaction blocks package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("value-drag", "Value drag", "Blocks", 50)
{
    facet::demo::PageHeader("Value drag",
                            "Mapping pointer and key input onto a value: linear, logarithmic and "
                            "stepped ranges, with fine and coarse steps.");
    facet::demo::Note("Planned: the block is being built in Phase 1c. This page will show it "
                      "working.");
}