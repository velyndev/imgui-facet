// The declared API of the inventory, card and people composites and the public drag and drop calls
// (facet/widgets/composites.h) while they are placeholders: every function is called once inside a
// real Dear ImGui frame, in a window and again inside ImGui::BeginDisabled. After each call the id,
// window, group and Facet scope stacks must be as before, a widget must have submitted one item of
// non-zero size with the id of its label, the neutral result must come back and no value argument
// may have changed. Every options struct is checked against the conventions at compile time. The
// other composites have files of their own (tests/test_api_composites_settings.cpp and
// test_api_composites_editors.cpp); the behaviour tests of these widgets go into
// tests/test_composites_cards.cpp.

#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/scope_stack.h"
#include "support/content_stub_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::content;

// Entries that can change order carry a stable id (rule 2 of the conventions).
static_assert(HasEntryId<facet::InventoryItem>);

// Views of caller data have the data block of rule 7 (HasDataBlock in support/content_stub_check.h).
static_assert(HasDataBlock<facet::InventoryGridOptions>());
static_assert(HasDataBlock<facet::ProfileSwitcherOptions>());

// Every widget that writes a value, a selection or item members has readOnly (rule 7).
static_assert(HasReadOnly<facet::InventoryGridOptions>);
static_assert(HasReadOnly<facet::ProfileSwitcherOptions>);

static_assert(OptionsFollowConventions<facet::InventoryGridOptions>());
static_assert(OptionsFollowConventions<facet::InventoryTileOptions>());
static_assert(OptionsFollowConventions<facet::InventorySlotOptions>());
static_assert(OptionsFollowConventions<facet::InventoryDetailCardOptions>());
static_assert(OptionsFollowConventions<facet::MediaCardOptions>());
static_assert(OptionsFollowConventions<facet::UserChipOptions>());
static_assert(OptionsFollowConventions<facet::ProfileSwitcherOptions>());
static_assert(OptionsFollowConventions<facet::DragSourceOptions>());
static_assert(OptionsFollowConventions<facet::AcceptDropOptions>());

void InventoryCalls()
{
    using namespace facet;
    static const InventoryItem slots[] = {{"Item 1", {}, Icon("file"), 2}, {}, {"Item 2"}};
    const auto neutral = [](const InventoryResult& result) {
        return result.action == InventoryAction::None && result.source == 0 && result.from == -1 &&
               result.to == -1 && result.count == 0;
    };
    int selected = 2;
    Check("InventoryGrid", Expect::Item, "bag", [&] { CHECK(neutral(InventoryGrid("bag", &selected, slots))); });
    Check("InventoryGrid, columns", Expect::Item, "bag 2",
          [&] { CHECK(neutral(InventoryGrid("bag 2", &selected, slots, {.columns = 2, .slotSize = 40.0f}))); });
    CHECK(selected == 2);
    Check("InventoryTile", Expect::Item, "tile", [] { CHECK_FALSE(InventoryTile("tile", slots[0], {.selected = true})); });
    Check("InventorySlot", Expect::Item, "Slot", [&] { CHECK(neutral(InventorySlot("Slot", slots[1]))); });

    static const InventoryStat stats[] = {{"Stat", 2.0, 1.0}};
    static const char* const actions[] = {"Action"};
    Check("InventoryDetailCard", Expect::Item, "Item", [] {
        CHECK(InventoryDetailCard("Item", ImageHandle{}, {.stats = stats, .actions = actions}) == -1);
    });
    Check("MediaCard", Expect::Item, "Card", [] { CHECK_FALSE(MediaCard("Card", ImageHandle{}, {.selected = true})); });
    Check("UserChip", Expect::Item, "Name", [] { CHECK_FALSE(UserChip("Name", ImageHandle{}, {.description = "Detail"})); });
    Check("UserChip, compact", Expect::Item, "Name 2", [] { CHECK_FALSE(UserChip("Name 2", ImageHandle{}, {.compact = true})); });

    static const AvatarItem profiles[] = {{"Profile 1"}, {"Profile 2"}};
    int current = 0;
    Check("ProfileSwitcher", Expect::Item, "profiles", [&] {
        CHECK(ProfileSwitcher("profiles", &current, profiles, {.addLabel = "Add"}) == ProfileSwitcherAction::None);
    });
    CHECK(current == 0);
}

void DragDropCalls()
{
    using namespace facet;
    const int payload = 7;
    Check("BeginDragSource", Expect::NoItem, nullptr, [&] {
        ImGui::Button("source");
        if (BeginDragSource("test.item", &payload, sizeof(payload), {.previewText = "Item"}))
            EndDragSource();
        DragSourceScope scope("test.item", &payload, sizeof(payload));
        CHECK_FALSE(static_cast<bool>(scope));
    });
    Check("BeginDropTarget", Expect::NoItem, nullptr, [] {
        ImGui::Button("target");
        if (BeginDropTarget())
            EndDropTarget();
        CHECK(AcceptDrop("test.item", {.indicator = DropIndicator::LineBefore, .peek = true}) == nullptr);
        DropTargetScope scope;
        CHECK_FALSE(static_cast<bool>(scope));
    });
}

void CardCalls()
{
    InventoryCalls();
    DragDropCalls();
}

} // namespace

TEST_CASE("The inventory, card and drag and drop stubs submit one item each and keep every stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int runs = 0;
    RunFrame(session, &runs, &CardCalls);
    CHECK(runs == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Card stubs follow kFill and widths relative to the available width")
{
    // Rule 6: kFill is the available width, any other negative width the available width minus
    // that many units.
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        const auto fills = [&](const char* widget, float units, auto&& call) {
            INFO("widget: " << std::string(widget) << ", width " << units);
            const float expected = ImGui::GetContentRegionAvail().x + (units == kFill ? 0.0f : Px(units));
            call(units);
            CHECK(ImGui::GetItemRectSize().x == doctest::Approx(expected));
            ++checked;
        };
        for (const float units : {kFill, -10.0f})
        {
            fills("InventoryDetailCard", units, [](float w) { InventoryDetailCard("Item", ImageHandle{}, {.width = w}); });
            fills("MediaCard", units, [](float w) { MediaCard("Card", ImageHandle{}, {.width = w}); });
        }
    });
    session.Frame();
    CHECK(checked == 4);
}

TEST_CASE("Content-sized inventory and people stubs are wide enough to name the widget and say planned")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int checked = 0;
    session.SetUi([&] {
        using namespace facet;
        const auto wide = [&](const char* widget, auto&& call) {
            INFO("widget: " << std::string(widget));
            call();
            char text[64];
            ImFormatString(text, sizeof(text), "%s: planned", widget);
            CHECK(ImGui::GetItemRectSize().x >= MeasureText(TextStyle::Footnote, text).x);
            ++checked;
        };
        const InventoryItem item{"Item"};
        wide("InventoryTile", [&] { InventoryTile("tile", item); });
        wide("InventorySlot", [&] { InventorySlot("A", item); });
        wide("UserChip", [] { UserChip("A", ImageHandle{}, {.compact = true}); });
    });
    session.Frame();
    CHECK(checked == 3);
}

TEST_CASE("A drag and drop End without its Begin is reported and changes no Dear ImGui stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int reported = 0;
    session.SetUi([&] {
        const Stacks before = CurrentStacks();
        const int errors = facet::internal::UsageErrorCount();
        facet::EndDragSource();
        facet::EndDropTarget();
        reported = facet::internal::UsageErrorCount() - errors;
        Stacks after = CurrentStacks();
        after.usageErrors = before.usageErrors;
        CHECK(after == before);
    });
    session.Frame();
    CHECK(reported == 2);
}
