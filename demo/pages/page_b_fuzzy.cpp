// Demo page: Fuzzy matching. Owner: the data blocks package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("fuzzy", "Fuzzy matching", "Blocks", 80)
{
    facet::demo::PageHeader("Fuzzy matching",
                            "Matching typed text against names, with the matched characters "
                            "highlighted.");
    facet::demo::Note("Planned: the block is being built in Phase 1c. This page will show it "
                      "working.");
}