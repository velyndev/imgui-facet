// Demo page: Sliders. Owner: the controls API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("sliders", "Sliders", "Widgets", 30)
{
    facet::demo::PageHeader("Sliders",
                            "Sliders, range sliders, multi-handle sliders, knobs and two-dimensional "
                            "pads.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/sliders.h and implemented in Phase 2. This page will show "
                      "each of them.");
}