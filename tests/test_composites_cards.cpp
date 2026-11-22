// Behaviour of the inventory, card and people composites and the public drag and drop calls
// (InventoryGrid, InventoryTile, InventorySlot, InventoryDetailCard, MediaCard, UserChip,
// ProfileSwitcher, BeginDragSource and BeginDropTarget). The drag and drop core is tested in
// tests/test_drag_drop.cpp.
//
// One test case per behaviour; docs/writing-a-widget.md describes the pattern. The API checks are in
// tests/test_api_composites_cards.cpp. The inventory tests run one bag model: a grid of slots and
// one equipment slot, to which every returned InventoryResult is applied as an application would.

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/drag_drop.h"
#include "internal/focus.h"
#include "internal/popup_host.h"
#include "support/session.h"

namespace
{

namespace fi = facet::internal;
using facet::InventoryAction;
using facet::InventoryItem;
using facet::InventoryResult;

// A bag: the grid's slots and one equipment slot, with the results applied to them.
struct Bag
{
    std::vector<InventoryItem> slots;
    InventoryItem equip;
    int selected = -1;
    InventoryResult last;
    int actions = 0;
    ImGuiID gridId = 0;
    ImGuiID slotId = 0;
    facet::InventoryGridOptions options;
    bool withSlot = false;
    float width = 0.0f;      // > 0: ImGui::SetNextItemWidth before the grid
    bool dragging = false;   // an inventory drag was in progress at the end of the last frame

    InventoryItem* Place(ImGuiID owner, int index)
    {
        if (owner == gridId && index >= 0 && index < static_cast<int>(slots.size()))
            return &slots[static_cast<std::size_t>(index)];
        if (owner == slotId && slotId != 0)
            return &equip;
        return nullptr;
    }

    void Apply(const InventoryResult& r, ImGuiID target)
    {
        if (r.action == InventoryAction::None)
            return;
        last = r;
        ++actions;
        InventoryItem* from = Place(r.source, r.from);
        InventoryItem* to = Place(target, r.to);
        if (from == nullptr || to == nullptr)
            return;
        switch (r.action)
        {
        case InventoryAction::Moved:
            *to = *from;
            *from = {};
            break;
        case InventoryAction::Swapped:
            std::swap(*to, *from);
            break;
        case InventoryAction::Merged:
            to->count += r.count;
            from->count -= r.count;
            if (from->count <= 0)
                *from = {};
            break;
        case InventoryAction::Split:
            *to = *from;
            to->count = r.count;
            from->count -= r.count;
            break;
        default:
            break;
        }
    }

    void Ui(facet::test::Session& session)
    {
        ImGui::Button("before");
        session.Track("before");
        gridId = ImGui::GetID("bag");
        if (width > 0.0f)
            ImGui::SetNextItemWidth(width);
        const InventoryResult r = facet::InventoryGrid("bag", &selected, slots, options);
        session.Track("grid");
        Apply(r, gridId);
        if (withSlot)
        {
            slotId = ImGui::GetID("Slot");
            const InventoryResult s = facet::InventorySlot("Slot", equip);
            session.Track("slot");
            Apply(s, slotId);
        }
        dragging = fi::DragDropActive(facet::kInventoryPayloadType, nullptr);
    }

    // The center of slot `index` of the grid at scale 1 (slot 64, gap 4).
    static ImVec2 Tile(const facet::test::Session& session, int index, int columns)
    {
        const ImRect rect = session.ItemRect("grid");
        const float outset = fi::FocusRingOutset();
        const float pitch = facet::Px(64.0f) + facet::Px(4.0f);
        const float x = rect.Min.x + outset + static_cast<float>(index % columns) * pitch + facet::Px(32.0f);
        const float y = rect.Min.y + outset + static_cast<float>(index / columns) * pitch + facet::Px(32.0f);
        return ImVec2(x, y);
    }
};

InventoryItem Item(const char* label, int count = 1, std::uint32_t key = 0, int maxStack = 0, bool locked = false)
{
    InventoryItem item;
    item.label = label;
    item.icon = facet::Icon("file");
    item.count = count;
    item.stackKey = key;
    item.maxStack = maxStack;
    item.locked = locked;
    return item;
}

[[maybe_unused]] Bag SixSlots()
{
    Bag bag;
    bag.slots = {Item("Item 1", 4, 7), Item("Item 2"), {}, Item("Item 3", 2, 7, 10), {}, Item("Item 4", 1, 0, 0, true)};
    bag.options.columns = 3;
    return bag;
}

[[maybe_unused]] std::string LabelOf(const InventoryItem& item)
{
    return item.label != nullptr ? std::string(item.label) : std::string();
}

} // namespace

// ---- InventoryTile ------------------------------------------------------------------------------

TEST_CASE("Inventory tile: a click, Space and Enter activate it, and it is a Tab stop")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    const InventoryItem item = Item("Item 1", 3);
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::InventoryTile("tile", item))
            ++presses;
        session.Track("tile");
    });
    session.Frame();
    session.Click(session.ItemCenter("tile"));
    CHECK(presses == 1);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("tile"));
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(presses == 3);
}

TEST_CASE("Inventory tile: the tile is a square of the slot size at every scale, long titles included")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const InventoryItem item = Item("A title that is far too long for one slot", 12);
    for (const float scale : {0.75f, 1.0f, 2.0f})
    {
        facet::SetUiScale(scale);
        session.SetUi([&] {
            (void)facet::InventoryTile("tile", item);
            session.Track("tile");
            (void)facet::InventoryTile("small", item, {.slotSize = 40.0f});
            session.Track("small");
        });
        session.Frames(2);
        INFO("scale " << scale);
        CHECK(session.ItemRect("tile").GetWidth() == doctest::Approx(facet::Px(64.0f)));
        CHECK(session.ItemRect("tile").GetHeight() == doctest::Approx(facet::Px(64.0f)));
        CHECK(session.ItemRect("small").GetWidth() == doctest::Approx(facet::Px(40.0f)));
    }
    facet::SetUiScale(1.0f);
}

TEST_CASE("Inventory tile: a disabled tile does not activate and a locked one starts no drag")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    const InventoryItem locked = Item("Item 1", 1, 0, 0, true);
    const InventoryItem plain = Item("Item 2");
    bool dragging = false;
    session.SetUi([&] {
        ImGui::BeginDisabled();
        if (facet::InventoryTile("off", plain))
            ++presses;
        session.Track("off");
        ImGui::EndDisabled();
        (void)facet::InventoryTile("locked", locked);
        session.Track("locked");
        dragging = fi::DragDropActive(nullptr, nullptr);
    });
    session.Frame();
    session.Click(session.ItemCenter("off"));
    CHECK(presses == 0);
    const ImVec2 from = session.ItemCenter("locked");
    session.MouseMove(from);
    session.MouseDown();
    session.MouseMove(ImVec2(from.x + 40.0f, from.y + 40.0f));
    session.Frames(2);
    CHECK_FALSE(dragging);
    session.MouseUp();
}
