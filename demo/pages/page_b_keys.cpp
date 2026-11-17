// Demo page: Keys and shortcuts. Owner: the data blocks package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("keys", "Keys and shortcuts", "Blocks", 90)
{
    facet::demo::PageHeader("Keys and shortcuts",
                            "Key chords as text, key binds with their modes, and the shortcut "
                            "registry.");
    facet::demo::Note("Planned: the block is being built in Phase 1c. This page will show it "
                      "working.");
}