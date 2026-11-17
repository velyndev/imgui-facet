// Demo page: Inputs. Owner: the controls API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("inputs", "Inputs", "Widgets", 40)
{
    facet::demo::PageHeader("Inputs",
                            "Text, search and number fields, drag and vector fields, special-purpose "
                            "fields, key bind capture and ratings.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/inputs.h and implemented in Phase 2. This page will show "
                      "each of them.");
}