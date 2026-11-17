// Demo page: Navigation. Calls every v1 widget of facet/widgets/navigation.h once, grouped by
// purpose. The widgets are planned, so each call draws its placeholder; Phase 2 replaces them
// one by one on this page. Sample entries are neutral ("Page 1") with built-in icons. The
// shortcut sheet is a modal layer: it starts closed and a switch opens it; the capture state
// "shortcuts" shows it open.

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
    bool shortcutSheet = false;
};

// Capture states of the page, in the order of the FACET_DEMO_PAGE registration.
enum CaptureState
{
    kCaptureDefault = 0,
    kCaptureShortcuts = 1,
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

void Steps(facet::demo::PageCtx& ctx, NavigationState& s)
{
    SectionHeader("Steps");
    LabeledRow("Step indicator");
    facet::StepIndicator("##steps", &s.step, kSteps);
    facet::BeginWizard("wizard", &s.wizardStep, kSteps);
    Note("Content of step %d.", s.wizardStep + 1);
    facet::WizardFooter({.canAdvance = true});
    facet::EndWizard();
    Paragraph("The shortcut sheet lists the registered shortcuts in a modal sheet. The switch sets "
              "its open flag; a planned sheet shows one placeholder line while the flag is on.");
    if (ctx.capture)
        s.shortcutSheet = ctx.captureState == kCaptureShortcuts;
    LabeledRow("Shortcut sheet");
    ImGui::Checkbox("##shortcuts", &s.shortcutSheet);
    ctx.Probe("navigation.shortcuts");
    facet::ShortcutSheet("Shortcuts", &s.shortcutSheet);
    if (ctx.capture && ctx.captureState == kCaptureShortcuts)
        ctx.RevealInCapture();
}

} // namespace

FACET_DEMO_PAGE("navigation", "Navigation", "Widgets", 70, "default", "shortcuts")
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
    Steps(ctx, s);
}

// The shortcut sheet starts closed, so a finished sheet never opens over the gallery by itself,
// and the switch opens and closes it.
FACET_DEMO_SELFTEST("navigation")
{
    test.Expect("the shortcut sheet starts closed", [] { return !NavigationState{}.shortcutSheet; });
    test.Do([] { State().shortcutSheet = false; });
    test.Click("navigation.shortcuts");
    test.Expect("the switch opens the shortcut sheet", [] { return State().shortcutSheet; });
    test.Click("navigation.shortcuts");
    test.Expect("the switch closes the shortcut sheet", [] { return !State().shortcutSheet; });
}
