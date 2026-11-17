// Demo page: Overlays. Owner: the structure API package, which fills it;
// until then the page says what it will show.

#include "gallery.h"
#include "pages/pages.h"

FACET_DEMO_PAGE("overlays", "Overlays", "Widgets", 100)
{
    facet::demo::PageHeader("Overlays",
                            "Modals, popovers, context menus, tooltips, toasts, drawers and the "
                            "command palette.");
    facet::demo::Note("Planned: the widgets of this family are declared in "
                      "facet/widgets/overlays.h and implemented in Phase 2. This page will show "
                      "each of them.");
}