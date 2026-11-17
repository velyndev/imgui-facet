// Demo page: Media. Owner: the content API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("media", "Media", "Widgets", 130)
{
    facet::demo::PageHeader("Media",
                            "Images in design units, avatars, thumbnail strips, carousels, image "
                            "viewers and drop zones.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/media.h and implemented in Phase 2. This page will show "
                      "each of them.");
}