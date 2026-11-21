// The public API of the containers family (facet/widgets/containers.h): every options struct
// follows the conventions, and every widget that is still a placeholder is safe to call inside a
// real frame (stacks balanced, no value written, the neutral result), returns false where its own
// arguments hide the content, and ends only what began. Behaviour tests of implemented widgets go
// into tests/test_containers.cpp.

#include <array>
#include <cfloat>

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

static_assert(OptionsFollowConventions<facet::ShellOptions>());
static_assert(OptionsFollowConventions<facet::AppFrameOptions>());
static_assert(OptionsFollowConventions<facet::TitleBarOptions>());
static_assert(OptionsFollowConventions<facet::OverlayOptions>());
static_assert(OptionsFollowConventions<facet::MiniWindowOptions>());
static_assert(OptionsFollowConventions<facet::ToolbarOptions>());
static_assert(OptionsFollowConventions<facet::ToolbarButtonOptions>());
static_assert(OptionsFollowConventions<facet::StatusBarOptions>());
static_assert(OptionsFollowConventions<facet::StatusItemOptions>());
static_assert(OptionsFollowConventions<facet::AccordionOptions>());
static_assert(OptionsFollowConventions<facet::AccordionSectionOptions>());
static_assert(OptionsFollowConventions<facet::DisclosureOptions>());
static_assert(OptionsFollowConventions<facet::CardOptions>());
static_assert(OptionsFollowConventions<facet::GroupBoxOptions>());
static_assert(OptionsFollowConventions<facet::ScrollAreaOptions>());
static_assert(OptionsFollowConventions<facet::VirtualListOptions>());
static_assert(OptionsFollowConventions<facet::InspectorPaneOptions>());

static_assert(HasDataBlock<facet::VirtualListOptions>());

// Calls every placeholder of containers.h once, checking balance and the neutral result.
void CallContainerStubs(Values& v)
{
    ImGuiSelectionBasicStorage selection;
    CHECK_BALANCED({
        const bool open = facet::BeginAppFrame("App");
        CHECK(open);
        if (open)
        {
            ImGui::TextUnformatted("child");
            facet::EndAppFrame();
        }
    });
    CHECK_BALANCED(CHECK(facet::AppFrameHitTest(ImVec2(1.0f, 1.0f)) == facet::FrameHit::Client));
    CHECK_BALANCED(CHECK(facet::AppFrameMinSize().x == 0.0f));
    CHECK_BALANCED(CHECK(facet::AppFrameMaxSize().y == 0.0f));
    CHECK_BALANCED(facet::SaveAppFramePlacement("app", v.placement));
    CHECK_BALANCED(CHECK_FALSE(facet::LoadAppFramePlacement("app", &v.placement)));
    CHECK_BALANCED({
        facet::BeginTitleBar("Title", {.windowButtons = true});
        ImGui::TextUnformatted("child");
        facet::EndTitleBar();
    });
    CHECK_BALANCED(CHECK(facet::TitleBarButtonPressed() == facet::WindowButton::None));
    CHECK_BALANCED({
        const bool open = facet::BeginOverlay("Overlay", &v.visible);
        CHECK(open);
        if (open)
            facet::EndOverlay();
    });
    CHECK_BALANCED(CHECK_FALSE(facet::OverlayWantsMouse()));
    CHECK_BALANCED(CHECK_FALSE(facet::OverlayWantsKeyboard()));
    CHECK_BALANCED(CHECK_FALSE(facet::OverlayHitTest(ImVec2(1.0f, 1.0f))));
    CHECK_BALANCED({
        std::array<facet::ScreenRegion, 2> regions;
        CHECK(facet::GetOverlayInputRegions(regions) == 0);
    });
    CHECK_BALANCED(if (facet::BeginMiniWindow("Mini", &v.pinned)) {
        FAIL("BeginMiniWindow opened");
        facet::EndMiniWindow();
    });
    CHECK_BALANCED({
        facet::BeginStatusBar("Status");
        facet::StatusItem("Item 1");
        facet::EndStatusBar();
    });
    CHECK_BALANCED({
        const bool open = facet::BeginVirtualList("Virtual", 100, &selection);
        CHECK(open);
        if (open)
        {
            facet::RowRange rows;
            while (facet::NextVisibleRows(&rows))
                for (int row = rows.first; row < rows.last; ++row)
                    facet::VirtualListRow(row);
            CHECK_FALSE(facet::VirtualListRow(0));
            facet::EndVirtualList();
        }
    });
    CHECK_BALANCED(facet::SetNextVirtualListScrollTo(10));
    CHECK_BALANCED({
        facet::BeginInspectorPane("Inspector", &v.visible);
        ImGui::TextUnformatted("main");
        if (facet::InspectorPaneNext())
            ImGui::TextUnformatted("pane");
        facet::EndInspectorPane();
    });
    CHECK(selection.Size == 0);
}

} // namespace

TEST_CASE("Every containers placeholder is safe to call, also disabled, and returns the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errorsBefore = facet::internal::UsageErrorCount();
    Values v;
    session.SetUi([&] {
        CallContainerStubs(v);
        ImGui::BeginDisabled();
        CallContainerStubs(v);
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(v == Values{});
    CHECK(facet::internal::UsageErrorCount() == errorsBefore);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Containers placeholders outside a frame draw nothing and return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.Frame();
    const int before = facet::internal::UsageErrorCount();
    CHECK_FALSE(facet::BeginOverlay("outside", nullptr));
    facet::BeginStatusBar("outside");
    facet::EndStatusBar();
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}

TEST_CASE("Containers placeholders end only what began")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        CHECK_BALANCED({ facet::StatusBarScope status("status"); });
        // An End without its Begin is reported and changes no stack.
        const int before = facet::internal::UsageErrorCount();
        const Snapshot start = Snapshot::Take(v);
        facet::EndStatusBar();
        facet::EndShell();
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 2);
}

TEST_CASE("Containers placeholders return false where their own arguments hide the content")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        bool hidden = false;
        CHECK_BALANCED({
            facet::BeginInspectorPane("Hidden pane", &hidden);
            CHECK_FALSE(facet::InspectorPaneNext());
            facet::EndInspectorPane();
        });
        CHECK_BALANCED({
            facet::BeginInspectorPane("Pane without a flag", nullptr);
            CHECK(facet::InspectorPaneNext());
            facet::EndInspectorPane();
        });
        CHECK_BALANCED(CHECK_FALSE(facet::BeginOverlay("Hidden overlay", &hidden)));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginOverlay("Overlay without a flag", nullptr)));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginVirtualList("Loading list", 10, nullptr, {.state = facet::DataState::Loading})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginVirtualList("Empty list", 0, nullptr)));
        CHECK(hidden == false);
    });
    session.Frame();
}

TEST_CASE("An outer inspector pane placeholder keeps its state past a nested one")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    bool outerPane = false;
    bool innerPane = true;
    session.SetUi([&] {
        CHECK_BALANCED({
            bool outerVisible = true;
            bool innerVisible = false;
            facet::BeginInspectorPane("Outer pane", &outerVisible);
            facet::BeginInspectorPane("Inner pane", &innerVisible);
            innerPane = facet::InspectorPaneNext();
            facet::EndInspectorPane();
            outerPane = facet::InspectorPaneNext();
            facet::EndInspectorPane();
        });
    });
    session.Frame();
    CHECK_FALSE(innerPane);
    CHECK(outerPane);
}
