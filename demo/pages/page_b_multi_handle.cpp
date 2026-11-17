// Demo page: Multi-handle. Owner: the interaction blocks package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("multi-handle", "Multi-handle", "Blocks", 60)
{
    facet::demo::PageHeader("Multi-handle",
                            "Several handles on one track that never cross, keep a minimum gap or "
                            "push each other.");
    facet::demo::Note("Planned: the block is being built in Phase 1c. This page will show it "
                      "working.");
}