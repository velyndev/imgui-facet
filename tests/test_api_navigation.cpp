// The public API of the navigation family (facet/widgets/navigation.h): every options struct
// follows the conventions, and every widget is safe to call inside a real frame without input
// (stacks balanced, no value written, the neutral result), also disabled and outside a frame.
// The behaviour of each widget is tested in tests/test_navigation.cpp.

#include <type_traits>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/options_check.h"
#include "support/session.h"
#include "support/structure_api_check.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::structure;

static_assert(OptionsFollowConventions<facet::SidebarOptions>());
static_assert(OptionsFollowConventions<facet::SidebarItemOptions>());
static_assert(OptionsFollowConventions<facet::SidebarSectionOptions>());
static_assert(OptionsFollowConventions<facet::TabBarOptions>());
static_assert(OptionsFollowConventions<facet::MenuBarOptions>());
static_assert(OptionsFollowConventions<facet::MenuOptions>());
static_assert(OptionsFollowConventions<facet::MenuItemOptions>());
static_assert(OptionsFollowConventions<facet::BreadcrumbsOptions>());
static_assert(OptionsFollowConventions<facet::PaginationOptions>());
static_assert(OptionsFollowConventions<facet::StepIndicatorOptions>());
static_assert(OptionsFollowConventions<facet::WizardOptions>());
static_assert(OptionsFollowConventions<facet::WizardFooterOptions>());
static_assert(OptionsFollowConventions<facet::ShortcutSheetOptions>());

// MenuItem has no overload, so MenuItem("Save", {}) cannot bind to a value form with a null pointer.
static_assert(std::is_same_v<decltype(&facet::MenuItem), bool (*)(const char*, const facet::MenuItemOptions&)>);

// Calls every function of navigation.h once, checking balance and the neutral result of a frame
// without input.
void CallNavigationWidgets(Values& v)
{
    CHECK_BALANCED({
        const bool open = facet::BeginSidebar("Sidebar", {.height = 120.0f});
        CHECK(open);
        if (open)
        {
            CHECK_FALSE(facet::SidebarItem("Page 1", facet::Icon("settings"), {.selected = true}));
            if (facet::BeginSidebarSection("Section"))
                facet::EndSidebarSection();
            facet::EndSidebar();
        }
    });
    CHECK_BALANCED(CHECK_FALSE(facet::TabBar("Tabs", &v.index, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::TabBar("Tabs labels", &v.index, kLabels)));
    CHECK_BALANCED(CHECK(facet::TabBarClosedTab() == -1));
    CHECK_BALANCED(CHECK_FALSE(facet::TabBar("Tabs reorder", &v.index, kItems, {.closable = true, .reorderable = true})));
    CHECK_BALANCED(CHECK_FALSE(facet::TabBarMovedTab().IsValid()));
    CHECK_FALSE(facet::TabBarOptions{}.reorderable);
    CHECK_BALANCED({
        // Further down the page the bar is out of sight and returns false (rule 8).
        const bool open = facet::BeginMenuBar("Menus");
        if (open)
        {
            if (facet::BeginMenu("Menu 1"))
            {
                FAIL("BeginMenu opened without input");
                facet::EndMenu();
            }
            facet::EndMenuBar();
        }
    });
    CHECK_BALANCED(CHECK_FALSE(facet::MenuItem("Item")));
    CHECK_BALANCED(CHECK_FALSE(facet::MenuCheckItem("Check item", &v.open)));
    CHECK_BALANCED(CHECK_FALSE(facet::MenuRadioItem("Radio item", &v.index, 0)));
    CHECK_BALANCED(CHECK(facet::Breadcrumbs("Path", kItems) == -1));
    CHECK_BALANCED(CHECK(facet::Breadcrumbs("Path labels", kLabels) == -1));
    CHECK_BALANCED(CHECK_FALSE(facet::Pagination("Pages", &v.page, 10)));
    CHECK_BALANCED(facet::StepIndicator("Steps", 1, kItems));
    CHECK_BALANCED(facet::StepIndicator("Steps labels", 1, kLabels));
    CHECK_BALANCED(CHECK_FALSE(facet::StepIndicator("Steps edit", &v.step, kItems)));
    CHECK_BALANCED(CHECK_FALSE(facet::StepIndicator("Steps edit labels", &v.step, kLabels)));
    CHECK_BALANCED({
        facet::BeginWizard("Wizard", &v.step, kItems);
        ImGui::TextUnformatted("step");
        CHECK(facet::WizardFooter() == facet::WizardAction::None);
        facet::EndWizard();
    });
    CHECK_BALANCED({
        facet::WizardScope wizard("Wizard labels", &v.step, kLabels);
        ImGui::TextUnformatted("step");
    });
    // A closed sheet shows nothing and leaves the flag alone.
    CHECK_BALANCED(facet::ShortcutSheet("Shortcuts", &v.closed));
    CHECK_BALANCED(facet::ShortcutSheet("Shortcuts null", nullptr));
}

} // namespace

TEST_CASE("Every navigation widget is safe to call, also disabled, and returns the neutral result without input")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errorsBefore = facet::internal::UsageErrorCount();
    Values v;
    session.SetUi([&] {
        CallNavigationWidgets(v);
        ImGui::PushID("disabled");
        ImGui::BeginDisabled();
        CallNavigationWidgets(v);
        ImGui::EndDisabled();
        ImGui::PopID();
    });
    session.Frames(2);
    CHECK(v == Values{});
    // facet::EndFrame found no scope left open.
    CHECK(facet::internal::UsageErrorCount() == errorsBefore);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Navigation widgets take their width by the precedence of rule 6 and their control size")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    float available = 0.0f;
    ImGuiID defaultId = 0;
    session.SetUi([&] {
        available = ImGui::GetContentRegionAvail().x;
        defaultId = ImGui::GetID("default");
        facet::TabBar("units", &index, kItems, {.width = 100.0f});
        session.Track("units");
        ImGui::SetNextItemWidth(150.0f);
        facet::TabBar("next", &index, kItems);
        session.Track("next");
        facet::TabBar("default", &index, kItems);
        session.Track("default");
        facet::Pagination("content", &index, 10);
        session.Track("content");
        facet::TabBar("small", &index, kItems, {.size = facet::ControlSize::Small});
        session.Track("small");
        facet::Breadcrumbs("crumbs", kItems, {.size = facet::ControlSize::Large});
        session.Track("crumbs");
    });
    session.Frame();

    CHECK(session.ItemRect("units").GetWidth() == doctest::Approx(facet::Px(100.0f)));
    CHECK(session.ItemRect("next").GetWidth() == doctest::Approx(150.0f));
    CHECK(session.ItemRect("default").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemRect("content").GetWidth() < available);
    CHECK(session.ItemRect("default").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    CHECK(session.ItemRect("small").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Small)));
    CHECK(session.ItemRect("crumbs").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Large)));
    CHECK(session.ItemRect("crumbs").GetWidth() == doctest::Approx(available));
    CHECK(session.ItemId("default") == defaultId);
}

TEST_CASE("Navigation Begins return false where their own arguments hide the content")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values v;
    session.SetUi([&] {
        CHECK_BALANCED(CHECK_FALSE(facet::BeginSidebar("Hidden sidebar", {.mode = facet::SidebarMode::Hidden})));
        CHECK_BALANCED(CHECK_FALSE(facet::BeginSidebarSection("Closed section", {.collapsible = true, .defaultOpen = false})));
    });
    session.Frame();
}

TEST_CASE("Navigation widgets outside a frame draw nothing and return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.Frame();
    const int before = facet::internal::UsageErrorCount();
    int index = 1;
    CHECK_FALSE(facet::TabBar("outside", &index, kItems));
    CHECK_FALSE(facet::BeginSidebar("outside"));
    CHECK_FALSE(facet::BeginMenuBar("outside"));
    CHECK_FALSE(facet::BeginMenu("outside"));
    CHECK_FALSE(facet::MenuItem("outside"));
    CHECK(facet::Breadcrumbs("outside", kItems) == -1);
    CHECK_FALSE(facet::Pagination("outside", &index, 4));
    CHECK(facet::TabBarClosedTab() == -1);
    CHECK(index == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}
