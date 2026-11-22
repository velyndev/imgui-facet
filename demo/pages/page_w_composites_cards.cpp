// Demo page: Inventory and cards. Calls every inventory, card and people composite and the public
// drag and drop calls of facet/widgets/composites.h once with neutral arguments. A planned widget
// draws the placeholder of its declared size; when it is implemented, its call here grows into a
// section with its variants and states, capture states and selftest steps. The search and app
// controls and the editors have pages of their own (page_w_composites_settings.cpp and
// page_w_composites_editors.cpp).

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"
#include "selftest.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::SectionHeader;

// Payload type of the drag and drop example.
constexpr const char* kDemoPayload = "facet.demo.item";

// Placeholder data: names like "Item 1" and built-in icons, nothing real.
const facet::InventoryItem kSlots[] = {
    {"Item 1", {}, facet::Icon("file"), 3},
    {"Item 2", {}, facet::Icon("folder")},
    {},
    {"Item 3", {}, facet::Icon("image"), 1, 0, 0, 0, true},
    {},
    {},
};
const facet::InventoryStat kStats[] = {{"Stat 1", 4.0, 3.0}, {"Stat 2", 2.0}};
const char* const kTags[] = {"Tag 1", "Tag 2"};
const char* const kActions[] = {"Action 1", "Action 2"};
const facet::AvatarItem kProfiles[] = {{"Profile 1"}, {"Profile 2"}};

struct CardsUi
{
    int slot = 0;
    int profile = 0;
    int drops = 0;  // payloads the drop target accepted
};

CardsUi ui;

} // namespace

FACET_DEMO_PAGE("composites-cards", "Inventory and cards", "Widgets", 140)
{
    facet::demo::PageHeader("Inventory and cards",
                            "Inventory grids, item tiles, slots and detail cards, media cards, user chips, profile "
                            "switchers and the drag and drop calls.");
    facet::demo::PlannedFamilyNote("composites");

    SectionHeader("Inventory");
    LabeledRow("Inventory grid");
    facet::InventoryGrid("inventory", &ui.slot, kSlots, {.columns = 3});
    ctx.Probe("composites.inventory");
    LabeledRow("Inventory tile");
    facet::InventoryTile("tile", kSlots[0], {.selected = true});
    LabeledRow("Inventory slot");
    facet::InventorySlot("Slot 1", facet::InventoryItem{}, {.placeholderIcon = facet::Icon("grid")});
    LabeledRow("Inventory detail card");
    facet::InventoryDetailCard("Item 1", facet::ImageHandle{},
                               {.description = "Placeholder description.", .stats = kStats, .tags = kTags,
                                .actions = kActions});

    SectionHeader("Cards and people");
    LabeledRow("Media card");
    facet::MediaCard("Title", facet::ImageHandle{}, {.description = "Detail", .badges = kTags});
    LabeledRow("User chip");
    facet::UserChip("Profile 1", facet::ImageHandle{}, {.description = "Placeholder"});
    LabeledRow("Profile switcher");
    facet::ProfileSwitcher("profiles", &ui.profile, kProfiles, {.addLabel = "Add profile"});

    SectionHeader("Drag and drop");
    facet::demo::Paragraph("BeginDragSource and BeginDropTarget mark the last item. They return false until they are "
                           "built on the drag and drop core.");
    LabeledRow("Drag source");
    facet::StyledText("Drag source");
    ctx.Probe("composites.dragsource");
    if (facet::BeginDragSource(kDemoPayload, &ui.slot, sizeof(ui.slot), {.previewText = "Item 1"}))
        facet::EndDragSource();
    LabeledRow("Drop target");
    facet::StyledText("Drop target");
    ctx.Probe("composites.droptarget");
    if (facet::BeginDropTarget())
    {
        if (facet::AcceptDrop(kDemoPayload, {.indicator = facet::DropIndicator::Box}) != nullptr)
            ++ui.drops;
        facet::EndDropTarget();
    }
}

FACET_DEMO_SELFTEST("composites-cards")
{
    // The placeholders write nothing: a click on the grid keeps the selection, and a drag from the
    // source to the target delivers no payload.
    test.Do([] { ui = CardsUi{}; });
    test.Click("composites.inventory");
    test.Expect("a click on the inventory placeholder keeps the selection", [] { return ui.slot == 0; });
    using facet::demo::Target;
    test.Drag({Target::Item("composites.dragsource"), Target::Item("composites.droptarget")});
    test.Expect("the drag and drop placeholders deliver no payload", [] { return ui.drops == 0; });
}
