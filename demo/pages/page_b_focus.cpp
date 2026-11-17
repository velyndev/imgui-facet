// Demo page: Focus. Owner: the interaction blocks package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("focus", "Focus", "Blocks", 20)
{
    facet::demo::PageHeader("Focus",
                            "The focus ring, which appears only after keyboard input, and Escape "
                            "closing one layer at a time.");
    facet::demo::Note("Planned: the block is being built in Phase 1c. This page will show it "
                      "working.");
}