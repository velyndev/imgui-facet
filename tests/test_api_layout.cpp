// The frozen API of the layout family (facet/widgets/layout.h): every options struct follows the
// conventions, every widget is safe to call inside a real frame without input (stacks balanced, no
// value written), also disabled and outside a frame, returns false only where its own arguments hide
// the content, and an End without its Begin is reported. The behaviour of each widget is tested in
// tests/test_layout_widgets.cpp.

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

// Calls every function of layout.h once, checking balance and that nothing is written.
void CallLayoutWidgets(Values& v)
{
    ImGuiSelectionBasicStorage selection;
    CHECK_BALANCED({
        facet::BeginStack("Stack");
        ImGui::TextUnformatted("child");
        facet::StackSpacer();
        facet::EndStack();
    });
    CHECK_BALANCED({
        facet::BeginGrid("Grid");
        facet::GridNext();
        ImGui::TextUnformatted("cell");
        facet::EndGrid();
    });
    CHECK_BALANCED({
        facet::BeginFlow("Flow");
        if (facet::FlowNext())
            ImGui::TextUnformatted("item");
        facet::EndFlow();
    });
    CHECK_BALANCED({
        facet::BeginSplitView("Split", &v.ratio, {.height = 80.0f});
        ImGui::TextUnformatted("first");
        facet::SplitViewNext();
        ImGui::TextUnformatted("second");
        facet::EndSplitView();
    });
    CHECK_BALANCED({
        facet::BeginPanels("Panels", v.sizes, {.height = 80.0f});
        if (facet::BeginPanel("Panel"))
            facet::EndPanel();
        if (facet::BeginPanel("Other", {.collapsible = false}))
            facet::EndPanel();
        facet::EndPanels();
    });
    CHECK_BALANCED({
        facet::BeginListDetail("ListDetail", &v.closed, {.height = 80.0f});
        if (facet::BeginListPane())
            facet::EndListPane();
        if (facet::BeginDetailPane())
            facet::EndDetailPane();
        facet::EndListDetail();
    });
    CHECK_BALANCED({
        facet::BeginSettingsRow("Row");
        ImGui::TextUnformatted("control");
        facet::EndSettingsRow();
    });
    CHECK_BALANCED(CHECK_FALSE(facet::SettingsRowResetPressed()));
    CHECK_BALANCED({
        const bool open = facet::BeginDependentGroup("Dependent", false);
        CHECK(open);
        if (open)
            facet::EndDependentGroup();
    });
    CHECK_BALANCED(facet::Separator());
    CHECK_BALANCED(facet::SeparatorText("Section"));
    CHECK_BALANCED({
        const bool open = facet::BeginItemGrid("Items", 50, &selection, {.height = 80.0f});
        CHECK(open);
        if (open)
        {
            facet::RowRange tiles;
            while (facet::NextVisibleRows(&tiles))
                for (int i = tiles.first; i < tiles.last; ++i)
                    CHECK_FALSE(facet::ItemGridTile(i));
            facet::EndItemGrid();
        }
    });
    CHECK(selection.Size == 0);
}

} // namespace

TEST_CASE("Every layout widget is safe to call without input, also disabled, and writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errorsBefore = facet::internal::UsageErrorCount();
    Values v;
    session.SetUi([&] {
        CallLayoutWidgets(v);
        ImGui::BeginDisabled();
        CallLayoutWidgets(v);
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(v == Values{});
    // facet::EndFrame found no scope left open.
    CHECK(facet::internal::UsageErrorCount() == errorsBefore);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Layout Begins return false where their own arguments hide the content")
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
        CHECK_BALANCED({
            facet::DependentGroupScope group("hidden", false, {.mode = facet::DependentMode::Hide});
            CHECK_FALSE(static_cast<bool>(group));
        });
        CHECK_BALANCED({
            facet::ItemGridScope grid("empty", 0, nullptr);
            CHECK_FALSE(static_cast<bool>(grid));
        });
    });
    session.Frame();
}

TEST_CASE("Layout widgets outside a frame draw nothing and keep the scopes balanced")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.Frame();
    const int before = facet::internal::UsageErrorCount();
    facet::BeginStack("outside");
    facet::EndStack();
    facet::BeginGrid("outside");
    facet::EndGrid();
    facet::BeginFlow("outside");
    facet::EndFlow();
    facet::Separator();
    facet::SeparatorText("outside");
    CHECK_FALSE(facet::BeginItemGrid("outside", 10, nullptr));
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
        facet::EndFlow();
        facet::EndSplitView();
        facet::EndPanels();
        facet::EndPanel();
        facet::EndListDetail();
        facet::EndListPane();
        facet::EndDetailPane();
        facet::EndSettingsRow();
        facet::EndDependentGroup();
        facet::EndItemGrid();
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 12);
}
