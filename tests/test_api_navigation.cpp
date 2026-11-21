// The frozen API of the navigation family (facet/widgets/navigation.h): every options struct
// follows the conventions, and every widget that is still planned is safe to call inside a real
// frame (stacks balanced, no value written, the neutral result) and returns false only where its
// own arguments hide the content. The placeholder helpers themselves are tested in
// tests/test_api_structure.cpp.

#include <type_traits>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/options_check.h"
#include "support/session.h"
#include "support/structure_stub_check.h"
#include "widgets/structure_stub.h"

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

// Calls every function of navigation.h once, checking balance and the neutral result.
void CallNavigationStubs(Values& v)
{
    CHECK_BALANCED({
        const bool open = facet::BeginSidebar("Sidebar");
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
    CHECK_BALANCED(CHECK_FALSE(facet::TabBar("Tabs reorder", &v.index, kItems, {.reorderable = true})));
    CHECK_BALANCED(CHECK_FALSE(facet::TabBarMovedTab().IsValid()));
    CHECK_FALSE(facet::TabBarOptions{}.reorderable);
    CHECK_BALANCED({
        const bool open = facet::BeginMenuBar("Menus");
        CHECK(open);
        if (open)
        {
            if (facet::BeginMenu("Menu 1"))
            {
                FAIL("BeginMenu opened");
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
    CHECK_BALANCED(facet::ShortcutSheet("Shortcuts", &v.open));
}

} // namespace

TEST_CASE("Every navigation widget stub is safe to call, also disabled, and returns the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errorsBefore = facet::internal::UsageErrorCount();
    Values v;
    session.SetUi([&] {
        CallNavigationStubs(v);
        ImGui::BeginDisabled();
        CallNavigationStubs(v);
        ImGui::EndDisabled();
    });
    session.Frames(2);
    CHECK(v == Values{});
    // facet::EndFrame found no scope left open.
    CHECK(facet::internal::UsageErrorCount() == errorsBefore);
    CHECK(facet::internal::OpenScopeCount() == 0);
}

TEST_CASE("Navigation placeholders take their width by the precedence of rule 6 and their control size")
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
    });
    session.Frame();

    CHECK(session.ItemRect("units").GetWidth() == doctest::Approx(facet::Px(100.0f)));
    CHECK(session.ItemRect("next").GetWidth() == doctest::Approx(150.0f));
    CHECK(session.ItemRect("default").GetWidth() == doctest::Approx(available));
    const float content = session.ItemRect("content").GetWidth();
    CHECK(content == doctest::Approx(facet::internal::StubContentWidth("Pagination", "content")));
    CHECK(content < available);
    CHECK(session.ItemRect("default").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    CHECK(session.ItemRect("small").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Small)));
    CHECK(session.ItemId("default") == defaultId);
}

TEST_CASE("Menu item placeholders name their own function in the caption")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::Theme saved = facet::T();
    facet::SetTheme(facet::Theme::Light(), false);
    const ImU32 secondary = facet::ToU32(facet::T().colors.labelSecondary);
    // One glyph quad (4 vertices) per character that is not a space.
    const auto glyphVertices = [](const char* text) {
        int count = 0;
        for (const char* c = text; *c != '\0'; ++c)
            count += *c != ' ' ? 4 : 0;
        return count;
    };
    bool checked = false;
    int selection = 0;
    int plain = 0;
    int check = 0;
    int radio = 0;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        int from = drawList->VtxBuffer.Size;
        facet::MenuItem("Item 1");
        plain = VerticesWithColor(drawList, from, secondary);
        from = drawList->VtxBuffer.Size;
        facet::MenuCheckItem("Item 2", &checked);
        check = VerticesWithColor(drawList, from, secondary);
        from = drawList->VtxBuffer.Size;
        facet::MenuRadioItem("Item 3", &selection, 1);
        radio = VerticesWithColor(drawList, from, secondary);
    });
    session.Frame();
    facet::SetTheme(saved, false);
    CHECK(plain == glyphVertices("MenuItem: planned (Item 1)"));
    CHECK(check == glyphVertices("MenuCheckItem: planned (Item 2)"));
    CHECK(radio == glyphVertices("MenuRadioItem: planned (Item 3)"));
}

TEST_CASE("Navigation stubs return false where their own arguments hide the content")
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

TEST_CASE("Navigation stubs outside a frame draw nothing and return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.Frame();
    const int before = facet::internal::UsageErrorCount();
    int index = 1;
    CHECK_FALSE(facet::TabBar("outside", &index, kItems));
    CHECK_FALSE(facet::BeginSidebar("outside"));
    CHECK(index == 1);
    CHECK(facet::internal::OpenScopeCount() == 0);
    CHECK(facet::internal::UsageErrorCount() == before);
}
