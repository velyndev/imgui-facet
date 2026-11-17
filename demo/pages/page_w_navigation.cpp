// Demo page: Navigation. Calls every v1 widget of facet/widgets/navigation.h once, grouped by
// purpose. The widgets are planned, so each call draws its placeholder; Phase 2 replaces them
// one by one on this page. Sample entries are neutral ("Page 1") with built-in icons.

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

const facet::ChoiceItem kTabs[] = {
    {.label = "Tab 1", .icon = facet::Icon("list")},
    {.label = "Tab 2", .icon = facet::Icon("grid")},
    {.label = "Tab 3", .icon = facet::Icon("settings")},
};

const facet::ChoiceItem kPath[] = {{"Level 1"}, {"Level 2"}, {"Level 3"}};
const facet::ChoiceItem kSteps[] = {{"Step 1"}, {"Step 2"}, {"Step 3"}};

struct NavigationState
{
    int page = 0;
    int tab = 0;
    int pageIndex = 2;
    int step = 1;
    int wizardStep = 0;
    bool option = true;
    int radio = 0;
    bool shortcutSheet = true;
};

NavigationState& State()
{
    static NavigationState state;
    return state;
}

void PageNavigation(NavigationState& s)
{
    SectionHeader("Move between pages");
    if (facet::BeginSidebar("sidebar"))
    {
        if (facet::BeginSidebarSection("Section 1"))
        {
            if (facet::SidebarItem("Page 1", facet::Icon("settings"), {.selected = s.page == 0}))
                s.page = 0;
            if (facet::SidebarItem("Page 2", facet::Icon("layers"), {.selected = s.page == 1, .badge = 3}))
                s.page = 1;
            facet::EndSidebarSection();
        }
        facet::EndSidebar();
    }
    LabeledRow("Tab bar");
    facet::TabBar("##tabs", &s.tab, kTabs);
    LabeledRow("Breadcrumbs");
    facet::Breadcrumbs("##path", kPath);
    LabeledRow("Pagination");
    facet::Pagination("##pages", &s.pageIndex, 20);
}

void Menus(NavigationState& s)
{
    SectionHeader("Menus");
    if (facet::BeginMenuBar("menubar"))
    {
        if (facet::BeginMenu("Menu 1"))
        {
            facet::MenuItem("Item 1", {.icon = facet::Icon("copy"), .shortcut = ImGuiMod_Ctrl | ImGuiKey_C});
            facet::MenuItem("Item 2", &s.option);
            facet::MenuItem("Item 3", &s.radio, 0);
            facet::EndMenu();
        }
        facet::EndMenuBar();
    }
    Paragraph("Menu items inside an open menu: a plain item with an icon and a shortcut, a check "
              "item and a radio item. Shown here outside a menu because menus do not open yet.");
    facet::MenuItem("Item 1", {.icon = facet::Icon("copy"), .shortcut = ImGuiMod_Ctrl | ImGuiKey_C});
    facet::MenuItem("Item 2", &s.option);
    facet::MenuItem("Item 3", &s.radio, 1);
}

void Steps(NavigationState& s)
{
    SectionHeader("Steps");
    LabeledRow("Step indicator");
    facet::StepIndicator("##steps", &s.step, kSteps);
    facet::BeginWizard("wizard", &s.wizardStep, kSteps);
    Note("Content of step %d.", s.wizardStep + 1);
    facet::WizardFooter({.canAdvance = true});
    facet::EndWizard();
    Paragraph("The shortcut sheet lists the registered shortcuts in a modal sheet; it is open on "
              "this page.");
    facet::ShortcutSheet("Shortcuts", &s.shortcutSheet);
}

} // namespace

FACET_DEMO_PAGE("navigation", "Navigation", "Widgets", 70)
{
    facet::demo::PageHeader("Navigation",
                            "The sidebar, tab bars, menu bars, menus and menu items, breadcrumbs, "
                            "pagination, step indicators, wizards and the shortcut sheet. Every "
                            "widget on this page is a placeholder until Phase 2 implements it: it "
                            "shows its name and the word planned, and does not react to input.");
    Note("Declared in facet/widgets/navigation.h; the declarations and usage examples are in "
         "docs/widgets/navigation.md.");
    NavigationState& s = State();
    PageNavigation(s);
    Menus(s);
    Steps(s);
}
