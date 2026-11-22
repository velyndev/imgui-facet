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

// ---- InventoryGrid ------------------------------------------------------------------------------

TEST_CASE("Inventory grid: the columns fit the width, reflow when it changes, and the tiles keep the slot size")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag = SixSlots();
    bag.options.columns = 0;
    bag.width = 600.0f;
    session.SetUi([&] { bag.Ui(session); });
    session.Frames(2);
    const float outset = fi::FocusRingOutset();
    // Six slots in one row at 600 pixels.
    CHECK(session.ItemRect("grid").GetWidth() == doctest::Approx(6.0f * 68.0f - 4.0f + outset * 2.0f));
    // At 160 pixels two columns: slot 2 is the first of the second row.
    bag.width = 160.0f;
    session.Frames(2);
    CHECK(session.ItemRect("grid").GetWidth() == doctest::Approx(2.0f * 68.0f - 4.0f + outset * 2.0f));
    session.Click(Bag::Tile(session, 2, 2));
    CHECK(bag.selected == 2);
    // Narrower than one slot: one column, and the tiles shrink to the width.
    bag.width = 40.0f;
    session.Frames(2);
    CHECK(session.ItemRect("grid").GetWidth() == doctest::Approx(40.0f));
}

TEST_CASE("Inventory grid: no slots show the empty state, one slot is one tile")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag;
    session.SetUi([&] { bag.Ui(session); });
    session.Frames(2);
    CHECK(session.ItemRect("grid").GetHeight() > 0.0f);
    session.Click(session.ItemCenter("grid"));
    CHECK(bag.selected == -1);
    bag.slots = {Item("Item 1")};
    session.Seconds(0.5f);  // so the next click is not a double click
    session.Click(Bag::Tile(session, 0, 1));
    CHECK(bag.selected == 0);
    CHECK(bag.actions == 0);
}

TEST_CASE("Inventory grid: ten thousand slots draw what a hundred draw")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag;
    bag.options.columns = 8;
    bag.slots.assign(100, Item("Item", 2));
    session.SetUi([&] { bag.Ui(session); });
    session.Frames(3);
    const ImDrawData* data = ImGui::GetDrawData();
    const int few = data != nullptr ? data->TotalVtxCount : 0;
    bag.slots.assign(10000, Item("Item", 2));
    session.Frames(3);
    data = ImGui::GetDrawData();
    const int many = data != nullptr ? data->TotalVtxCount : 0;
    REQUIRE(few > 0);
    // The same rows are visible; only the scroll bar thumb may differ.
    CHECK(std::abs(many - few) < few / 10);
}

TEST_CASE("Inventory grid: a click, Ctrl+click and Shift+click select, and *selected follows the focused slot")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag = SixSlots();
    session.SetUi([&] { bag.Ui(session); });
    session.Frames(2);
    session.Click(Bag::Tile(session, 0, 3));
    CHECK(bag.selected == 0);
    session.KeyDown(ImGuiMod_Ctrl);
    session.Click(Bag::Tile(session, 2, 3));
    session.KeyUp(ImGuiMod_Ctrl);
    CHECK(bag.selected == 2);
    session.KeyDown(ImGuiMod_Shift);
    session.Click(Bag::Tile(session, 4, 3));
    session.KeyUp(ImGuiMod_Shift);
    CHECK(bag.selected == 4);
    // The caller sets the selection: the grid takes it.
    bag.selected = 1;
    session.Frames(2);
    CHECK(bag.selected == 1);
    CHECK(bag.actions == 0);
}

TEST_CASE("Inventory grid: the arrow keys move the selection in two dimensions, Enter and a double click activate")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag = SixSlots();
    session.SetUi([&] { bag.Ui(session); });
    session.Frames(2);
    session.Click(Bag::Tile(session, 0, 3));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(bag.selected == 1);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(bag.selected == 4);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(bag.last.action == InventoryAction::Activated);
    CHECK(bag.last.to == 4);
    session.DoubleClick(Bag::Tile(session, 1, 3));
    session.Frames(2);
    CHECK(bag.last.action == InventoryAction::Activated);
    CHECK(bag.last.to == 1);
}

TEST_CASE("Inventory grid: Ctrl+arrow moves the focused item to an empty slot, swaps, and refuses a locked slot")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag = SixSlots();
    session.SetUi([&] { bag.Ui(session); });
    session.Frames(2);
    // Item 2 (slot 1) to the empty slot 2; the selection and the focus go with it.
    session.Click(Bag::Tile(session, 1, 3));
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_RightArrow);
    CHECK(bag.last.action == InventoryAction::Moved);
    CHECK(LabelOf(bag.slots[2]) == "Item 2");
    CHECK(bag.slots[1].label == nullptr);
    CHECK(bag.selected == 2);
    // Slot 5 below is locked: nothing moves.
    const int before = bag.actions;
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_DownArrow);
    CHECK(bag.actions == before);
    // Back to the empty slot 1, then a swap with Item 1 in slot 0.
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiMod_Ctrl | ImGuiKey_LeftArrow);
    CHECK(bag.last.action == InventoryAction::Swapped);
    CHECK(LabelOf(bag.slots[0]) == "Item 2");
    CHECK(LabelOf(bag.slots[1]) == "Item 1");
}

TEST_CASE("Inventory grid: a pointer drag moves, swaps and merges, and Shift splits half a stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag = SixSlots();
    session.SetUi([&] { bag.Ui(session); });
    session.Frames(2);
    // Item 2 onto the empty slot 4.
    session.Drag({Bag::Tile(session, 1, 3), Bag::Tile(session, 2, 3), Bag::Tile(session, 4, 3)}, 6);
    session.Frames(2);
    CHECK(bag.last.action == InventoryAction::Moved);
    CHECK(bag.last.from == 1);
    CHECK(bag.last.to == 4);
    CHECK(LabelOf(bag.slots[4]) == "Item 2");
    // Item 1 (4 of key 7) onto Item 3 (2 of key 7, at most 10): all four merge.
    session.Drag({Bag::Tile(session, 0, 3), Bag::Tile(session, 1, 3), Bag::Tile(session, 3, 3)}, 6);
    session.Frames(2);
    CHECK(bag.last.action == InventoryAction::Merged);
    CHECK(bag.last.count == 4);
    CHECK(bag.slots[3].count == 6);
    CHECK(bag.slots[0].label == nullptr);
    // Item 3 onto Item 2: a swap.
    session.Drag({Bag::Tile(session, 3, 3), Bag::Tile(session, 4, 3)}, 6);
    session.Frames(2);
    CHECK(bag.last.action == InventoryAction::Swapped);
    CHECK(LabelOf(bag.slots[4]) == "Item 3");
    // Shift held: half of the six onto the empty slot 0.
    session.KeyDown(ImGuiMod_Shift);
    session.Drag({Bag::Tile(session, 4, 3), Bag::Tile(session, 1, 3), Bag::Tile(session, 0, 3)}, 6);
    session.KeyUp(ImGuiMod_Shift);
    session.Frames(2);
    CHECK(bag.last.action == InventoryAction::Split);
    CHECK(bag.last.count == 3);
    CHECK(bag.slots[0].count == 3);
    CHECK(bag.slots[4].count == 3);
}

TEST_CASE("Inventory grid: a locked slot refuses a drop, and Escape cancels a pointer drag")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag = SixSlots();
    session.SetUi([&] { bag.Ui(session); });
    session.Frames(2);
    session.Drag({Bag::Tile(session, 1, 3), Bag::Tile(session, 4, 3), Bag::Tile(session, 5, 3)}, 6);
    session.Frames(2);
    CHECK(bag.actions == 0);
    session.MouseMove(Bag::Tile(session, 1, 3));
    session.MouseDown();
    session.MouseMove(Bag::Tile(session, 2, 3));
    session.Frames(2);
    CHECK(bag.dragging);
    session.KeyPress(ImGuiKey_Escape);
    session.MouseUp();
    session.Frames(2);
    CHECK(bag.actions == 0);
    CHECK(LabelOf(bag.slots[1]) == "Item 2");
}

TEST_CASE("Inventory grid: Space picks a slot up for a keyboard drag, the arrows carry it, Space drops, Escape puts it back")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag = SixSlots();
    session.SetUi([&] { bag.Ui(session); });
    session.Frames(2);
    session.Click(Bag::Tile(session, 1, 3));
    session.KeyPress(ImGuiKey_Space);
    CHECK(bag.dragging);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK(bag.actions == 0);
    CHECK_FALSE(bag.dragging);
    // Again, and drop it one slot to the right (the empty slot 2).
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_Space);
    session.Frames(2);
    CHECK(bag.last.action == InventoryAction::Moved);
    CHECK(bag.last.from == 1);
    CHECK(bag.last.to == 2);
    CHECK(bag.selected == 2);
}

TEST_CASE("Inventory grid: read-only shows the selection and writes nothing, starts no drag and accepts no drop")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag = SixSlots();
    bag.selected = 0;
    bag.options.readOnly = true;
    session.SetUi([&] { bag.Ui(session); });
    session.Frames(2);
    session.Click(Bag::Tile(session, 2, 3));
    CHECK(bag.selected == 0);
    session.Drag({Bag::Tile(session, 1, 3), Bag::Tile(session, 2, 3)}, 6);
    session.Frames(2);
    CHECK(bag.actions == 0);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(bag.actions == 0);
}

TEST_CASE("Inventory grid: while loading or failed the slots take no input")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Bag bag = SixSlots();
    for (const facet::DataState state : {facet::DataState::Loading, facet::DataState::Error})
    {
        bag.options.state = state;
        session.SetUi([&] { bag.Ui(session); });
        session.Frames(2);
        CHECK(session.ItemRect("grid").GetHeight() > 0.0f);
        session.Click(session.ItemCenter("grid"));
        CHECK(bag.selected == -1);
    }
}
