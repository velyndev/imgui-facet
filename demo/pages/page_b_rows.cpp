// Demo page: Rows and width classes. Owner: the interaction blocks package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("rows", "Rows and width classes", "Blocks", 30)
{
    facet::demo::PageHeader("Rows and width classes",
                            "Labelled rows, and the width classes that let a layout stack its columns "
                            "in narrow regions.");
    facet::demo::Note("Planned: the block is being built in Phase 1c. This page will show it "
                      "working.");
}