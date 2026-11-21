// The public API of the lists family (facet/widgets/lists.h): every options struct follows the
// conventions, every widget that is still a placeholder is safe to call inside a real frame
// (stacks balanced, no value written, the neutral result), and ApplyReorderMove works. Behaviour
// tests of implemented widgets go into tests/test_lists.cpp.

#include <array>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/list_view.h"
#include "support/options_check.h"
#include "support/session.h"
#include "support/structure_stub_check.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::structure;

static_assert(OptionsFollowConventions<facet::DropdownOptions>());
static_assert(OptionsFollowConventions<facet::MultiDropdownOptions>());
static_assert(OptionsFollowConventions<facet::ComboOptions>());
static_assert(OptionsFollowConventions<facet::ComboItemOptions>());
static_assert(OptionsFollowConventions<facet::ListBoxOptions>());
static_assert(OptionsFollowConventions<facet::SelectableListOptions>());
static_assert(OptionsFollowConventions<facet::ReorderableListOptions>());
static_assert(OptionsFollowConventions<facet::TreeViewOptions>());
static_assert(OptionsFollowConventions<facet::TreeNodeOptions>());
static_assert(OptionsFollowConventions<facet::CheckboxTreeOptions>());
static_assert(OptionsFollowConventions<facet::TransferListOptions>());
static_assert(OptionsFollowConventions<facet::SearchableChecklistOptions>());

// Every data view of the family: the lists that show caller data and the tree.
static_assert(HasDataBlock<facet::ListBoxOptions>());
static_assert(HasDataBlock<facet::SelectableListOptions>());
static_assert(HasDataBlock<facet::ReorderableListOptions>());
static_assert(HasDataBlock<facet::TreeViewOptions>());
static_assert(HasDataBlock<facet::CheckboxTreeOptions>());
static_assert(HasDataBlock<facet::TransferListOptions>());
static_assert(HasDataBlock<facet::SearchableChecklistOptions>());

// The framed lists that are fields of a form carry the validation members of rule 7.
static_assert(HasFieldMessages<facet::ListBoxOptions>());
static_assert(HasFieldMessages<facet::SelectableListOptions>());
static_assert(HasFieldMessages<facet::SearchableChecklistOptions>());

// Calls every placeholder of lists.h once, checking balance and the neutral result.
void CallListStubs(Values& v)
{
    ImGuiSelectionBasicStorage selection;
    const std::span<bool> checked(v.checked);
    const std::span<facet::CheckState> states(v.states);

    CHECK_BALANCED(CHECK_FALSE(facet::TransferList("Transfer", checked, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::TransferList("Transfer labels", checked, kLabels)));
    CHECK(selection.Size == 0);
}

} // namespace

TEST_CASE("Every lists placeholder is safe to call, also disabled, and returns the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errorsBefore = facet::internal::UsageErrorCount();
    Values v;
    session.SetUi([&] {
        CallListStubs(v);
        ImGui::BeginDisabled();
        CallListStubs(v);
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(v == Values{});
    CHECK(facet::internal::UsageErrorCount() == errorsBefore);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Lists placeholders outside a frame draw nothing and return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.Frame();
    const int before = facet::internal::UsageErrorCount();
    int index = 1;
    CHECK_FALSE(facet::Dropdown("outside", &index, kItems));
    CHECK_FALSE(facet::BeginTreeView("outside", nullptr));
    CHECK(index == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}

TEST_CASE("A reorderable list takes one row per item or one row for its state")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::ReorderMove move;
    session.SetUi([&] {
        facet::ReorderableList("ready", &move, kItems);
        session.Track("ready");
        facet::ReorderableList("loading", &move, kItems, {.state = facet::DataState::Loading});
        session.Track("loading");
        facet::ReorderableList("error", &move, kLabels, {.state = facet::DataState::Error});
        session.Track("error");
        facet::ReorderableList("empty", &move, std::span<const facet::ChoiceItem>());
        session.Track("empty");
    });
    session.Frame();
    const float row = facet::internal::ListRowHeight(facet::ControlSize::Regular);
    CHECK(session.ItemRect("ready").GetHeight() == doctest::Approx(row * static_cast<float>(std::size(kItems))));
    CHECK(session.ItemRect("loading").GetHeight() == doctest::Approx(row));
    CHECK(session.ItemRect("error").GetHeight() == doctest::Approx(row));
    CHECK(session.ItemRect("empty").GetHeight() == doctest::Approx(row));
    CHECK_FALSE(move.IsValid());
}

TEST_CASE("Lists placeholders end only what began and stay closed where their arguments hide the content")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::ComboScope combo("combo", "Option 1");
            CHECK_FALSE(static_cast<bool>(combo));
        });
        CHECK_BALANCED(CHECK_FALSE(facet::BeginTreeView("Loading tree", nullptr, {.state = facet::DataState::Loading})));
        // An End without its Begin is reported and changes no stack.
        const int before = facet::internal::UsageErrorCount();
        Snapshot start = Snapshot::Take(v);
        facet::EndCombo();
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 1);
}

TEST_CASE("ApplyReorderMove moves one element and shifts the others")
{
    // `to` is the moved element's index after the move.
    std::vector<int> data = {0, 1, 2, 3, 4};
    facet::ApplyReorderMove(data, {1, 3});
    CHECK(data == std::vector<int>{0, 2, 3, 1, 4});
    CHECK(data[3] == 1);
    facet::ApplyReorderMove(data, {3, 0});
    CHECK(data == std::vector<int>{1, 0, 2, 3, 4});
    facet::ApplyReorderMove(data, {4, 4});
    facet::ApplyReorderMove(data, {-1, 2});
    facet::ApplyReorderMove(data, {2, 5});
    CHECK(data == std::vector<int>{1, 0, 2, 3, 4});
    CHECK_FALSE(facet::ReorderMove{}.IsValid());
    CHECK(facet::ReorderMove{0, 1}.IsValid());
}

TEST_CASE("ApplyReorderMove takes the caller's containers as they are")
{
    // A C array, a std::array, a std::span and a std::vector<bool> without spelling a span.
    int array[3] = {0, 1, 2};
    facet::ApplyReorderMove(array, {0, 2});
    CHECK(array[0] == 1);
    CHECK(array[1] == 2);
    CHECK(array[2] == 0);
    std::array<facet::ChoiceItem, 3> items = {{{"Option 1"}, {"Option 2"}, {"Option 3"}}};
    facet::ApplyReorderMove(items, {2, 0});
    CHECK(std::string(items[0].label) == "Option 3");
    CHECK(std::string(items[1].label) == "Option 1");
    std::vector<int> data = {0, 1, 2};
    facet::ApplyReorderMove(std::span<int>(data).subspan(1), {0, 1});
    CHECK(data == std::vector<int>{0, 2, 1});
    std::vector<bool> flags = {true, false, false};
    facet::ApplyReorderMove(flags, {0, 2});
    CHECK(flags == std::vector<bool>{false, false, true});
}
