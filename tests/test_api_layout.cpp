// The frozen API of the layout family (facet/widgets/layout.h): every options struct follows the
// conventions, and every widget that is still planned is safe to call inside a real frame (stacks
// balanced, no value written, the neutral result) and returns false only where its own arguments
// hide the content. The placeholder helpers themselves are tested in tests/test_api_structure.cpp.

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/options_check.h"
#include "support/session.h"
#include "support/structure_stub_check.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::structure;

static_assert(OptionsFollowConventions<facet::StackOptions>());
static_assert(OptionsFollowConventions<facet::GridOptions>());
static_assert(OptionsFollowConventions<facet::GridCellOptions>());
static_assert(OptionsFollowConventions<facet::FlowOptions>());
static_assert(OptionsFollowConventions<facet::SplitViewOptions>());
static_assert(OptionsFollowConventions<facet::PanelsOptions>());
static_assert(OptionsFollowConventions<facet::PanelOptions>());
static_assert(OptionsFollowConventions<facet::ListDetailOptions>());
static_assert(OptionsFollowConventions<facet::SettingsRowOptions>());
static_assert(OptionsFollowConventions<facet::DependentGroupOptions>());
static_assert(OptionsFollowConventions<facet::SeparatorOptions>());
static_assert(OptionsFollowConventions<facet::SeparatorTextOptions>());
static_assert(OptionsFollowConventions<facet::ItemGridOptions>());

// The item grid is a data view (rule 7).
static_assert(HasDataBlock<facet::ItemGridOptions>());

// Calls every function of layout.h once, checking balance and the neutral result.
void CallLayoutStubs(Values& v)
{
    ImGuiSelectionBasicStorage selection;
    CHECK_BALANCED({
        const bool open = facet::BeginItemGrid("Items", 50, &selection);
        CHECK(open);
        if (open)
        {
            CHECK_FALSE(facet::ItemGridTile(0));
            facet::EndItemGrid();
        }
    });
    CHECK(selection.Size == 0);
}

} // namespace

TEST_CASE("Every layout widget stub is safe to call, also disabled, and returns the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errorsBefore = facet::internal::UsageErrorCount();
    Values v;
    session.SetUi([&] {
        CallLayoutStubs(v);
        ImGui::BeginDisabled();
        CallLayoutStubs(v);
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(v == Values{});
    // facet::EndFrame found no scope left open.
    CHECK(facet::internal::UsageErrorCount() == errorsBefore);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Layout stubs return false where their own arguments hide the content")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED(CHECK_FALSE(facet::BeginDependentGroup("Hidden group", false, {.mode = facet::DependentMode::Hide})));
        CHECK_BALANCED({
            const bool open = facet::BeginDependentGroup("Shown group", true, {.mode = facet::DependentMode::Hide});
            CHECK(open);
            if (open)
                facet::EndDependentGroup();
        });
        CHECK_BALANCED(CHECK_FALSE(facet::BeginItemGrid("Failed grid", 10, nullptr, {.state = facet::DataState::Error})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginItemGrid("Empty grid", 0, nullptr)));
    });
    session.Frame();
}

TEST_CASE("Layout scope classes end only what began")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED({
            facet::StackScope stack("stack");
            facet::SettingsRowScope row("row");
        });
    });
    session.Frame();
}

TEST_CASE("Layout stubs outside a frame draw nothing and keep the scopes balanced")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.Frame();
    const int before = facet::internal::UsageErrorCount();
    facet::BeginStack("outside");
    facet::EndStack();
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}

TEST_CASE("A layout End without its Begin is reported and changes no stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        const int before = facet::internal::UsageErrorCount();
        const Snapshot start = Snapshot::Take(v);
        facet::EndStack();
        facet::EndGrid();
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 2);
}
