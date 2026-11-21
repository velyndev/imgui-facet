// The public API of the containers family (facet/widgets/containers.h): every options struct
// follows the conventions, every Begin and query outside a frame gives the neutral result, and every
// End without its Begin is reported and changes no stack. Every widget of the family is ready; the
// behaviour tests are in tests/test_containers.cpp.

#include <array>

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

} // namespace

TEST_CASE("Every containers Begin and query outside a frame gives the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.Frame();
    const int before = facet::internal::UsageErrorCount();
    bool open = true;
    bool pinned = true;
    ImGuiSelectionBasicStorage selection;
    CHECK_FALSE(facet::BeginShell("outside", &open));
    CHECK_FALSE(facet::BeginAppFrame("outside"));
    CHECK_FALSE(facet::BeginOverlay("outside", &open));
    CHECK_FALSE(facet::BeginMiniWindow("outside", &pinned));
    CHECK_FALSE(facet::BeginGroupBox("outside"));
    CHECK_FALSE(facet::BeginDisclosure("outside", &open));
    CHECK_FALSE(facet::BeginScrollArea("outside"));
    CHECK_FALSE(facet::BeginVirtualList("outside", 10, &selection));
    facet::BeginCard("outside");
    facet::EndCard();
    facet::BeginToolbar("outside");
    CHECK_FALSE(facet::ToolbarButton("A", facet::Icon("plus")));
    facet::EndToolbar();
    facet::BeginStatusBar("outside");
    facet::StatusItem("outside");
    facet::EndStatusBar();
    facet::BeginTitleBar("outside");
    facet::EndTitleBar();
    facet::BeginAccordion("outside");
    CHECK_FALSE(facet::BeginAccordionSection("outside"));
    facet::EndAccordion();
    facet::BeginInspectorPane("outside", &open);
    CHECK_FALSE(facet::InspectorPaneNext());
    facet::EndInspectorPane();
    CHECK(facet::TitleBarButtonPressed() == facet::WindowButton::None);
    CHECK_FALSE(facet::VirtualListRow(0));
    CHECK(open);
    CHECK(pinned);
    CHECK(selection.Size == 0);
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}

TEST_CASE("Every containers End without its Begin is reported and changes no stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    int reported = 0;
    session.SetUi([&] {
        const int before = facet::internal::UsageErrorCount();
        const Snapshot start = Snapshot::Take(v);
        facet::EndShell();
        facet::EndAppFrame();
        facet::EndTitleBar();
        facet::EndOverlay();
        facet::EndMiniWindow();
        facet::EndToolbar();
        facet::EndStatusBar();
        facet::EndAccordionSection();
        facet::EndAccordion();
        facet::EndDisclosure();
        facet::EndCard();
        facet::EndGroupBox();
        facet::EndScrollArea();
        facet::EndVirtualList();
        facet::EndInspectorPane();
        Snapshot end = Snapshot::Take(v);
        reported = facet::internal::UsageErrorCount() - before;
        end.errors = start.errors;
        CHECK(end == start);
    });
    session.Frame();
    CHECK(reported == 15);
}
