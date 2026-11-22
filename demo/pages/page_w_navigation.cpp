// Demo page: Navigation. One section per widget of facet/widgets/navigation.h with its variants and
// states, operated by the selftest at the end of the file. Sample entries are neutral ("Tab 1",
// "Page 1", "Level 1") with built-in icons; the counts on badges are placeholders and say so. The
// page registers one sample shortcut, as an app would, so a menu row and the shortcut sheet have
// something real to show and the chord runs the same action with every menu closed. The shortcut
// sheet is a modal layer: it starts closed and a switch opens it.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "pages/pages.h"
#include "widgets/navigation/navigation_parts.h"
#include "widgets/selection/selection_parts.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;
namespace fi = facet::internal;

// Capture states of the page, in the order of the FACET_DEMO_PAGE registration.
enum CaptureState
{
    kCaptureDefault = 0,
    kCaptureFocus = 1,
    kCaptureMenu = 2,
    kCaptureSubmenu = 3,
    kCaptureTabsMore = 4,
    kCaptureDragging = 5,
    kCaptureRail = 6,
    kCaptureWizardError = 7,
    kCaptureShortcuts = 8,
};

constexpr int kSampleTabs = 4;
constexpr int kOverflowTabs = 6;
constexpr int kPathDepth = 5;
// Narrow rows that show the overflow of tab bars, menu bars and breadcrumbs, in units.
constexpr float kNarrowTabs = 260.0f;
constexpr float kNarrowMenuBar = 100.0f;
constexpr float kNarrowPath = 220.0f;
constexpr float kSidebarHeight = 236.0f;

const char* const kTabLabels[kSampleTabs] = {"Tab 1", "Tab 2", "Tab 3", "Tab 4"};
const char* const kOverflowLabels[kOverflowTabs] = {"Tab 1", "Tab 2", "Tab 3", "Tab 4", "Tab 5", "Tab 6"};
const facet::ChoiceItem kIconTabs[] = {
    {.label = "List", .icon = facet::Icon("list")},
    {.label = "Grid", .icon = facet::Icon("grid")},
    // The detail is a placeholder count, shown as the tab's badge.
    {.label = "Settings", .icon = facet::Icon("settings"), .detail = "2"},
    {.label = "Unavailable", .icon = facet::Icon("lock"), .disabled = true},
};
const char* const kPathLabels[kPathDepth] = {"Level 1", "Level 2", "Level 3", "Level 4", "Level 5"};
const facet::ChoiceItem kSteps[] = {{"Step 1"}, {"Step 2"}, {"Step 3"}, {"Step 4"}};
const char* const kSidebarModes[] = {"Auto", "Expanded", "Rail"};
const char* const kDensities[] = {"Compact", "Regular", "Spacious"};

struct NavigationState
{
    // Tab bars
    std::vector<facet::ChoiceItem> tabs;
    int tab = 0;
    int closedCount = 0;
    int movedCount = 0;
    int iconTab = 0;
    int underlineTab = 0;
    int overflowTab = 0;
    int smallTab = 0;
    int largeTab = 0;
    // Sidebar
    int page = 0;
    int sidebarMode = 0;
    // Menus
    bool grid = true;
    int density = 1;
    int sampleRuns = 0;
    int menuPresses = 0;
    const char* lastMenu = "none";
    // Breadcrumbs
    int depth = kPathDepth;
    int crumbsActivated = -1;
    // Pagination
    int pageIndex = 0;
    int compactPage = 0;
    int fewPage = 0;
    // Steps
    int indicatorStep = 2;
    int wizardStep = 0;
    std::string name;
    bool skipSecond = false;
    bool optionOn = false;
    const char* wizardResult = "none";
    // Shortcut sheet
    bool shortcutSheet = false;

    void ResetTabs()
    {
        tabs.clear();
        for (int i = 0; i < kSampleTabs; ++i)
            tabs.push_back({.label = kTabLabels[i], .id = static_cast<ImGuiID>(i + 1)});
        tab = 0;
    }
};

NavigationState& State()
{
    static NavigationState state = [] {
        NavigationState s;
        s.ResetTabs();
        return s;
    }();
    return state;
}

// The sample shortcut, registered once like an app would.
facet::ShortcutId SampleShortcut()
{
    static const facet::ShortcutId id = facet::RegisterShortcut(
        "navigation.sample", ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K,
        {.label = "Run the sample action", .group = "Navigation page", .scope = facet::ShortcutScope::Window});
    return id;
}

// A probe for part `part` of the widget `widgetId`, with the id of that part.
void ProbePart(facet::demo::PageCtx& ctx, const char* name, ImGuiID widgetId, int part, ImGuiID partId)
{
    const ImRect r = fi::NavigationPartRect(widgetId, part);
    if (r.GetWidth() > 0.0f)
        facet::demo::ProbeRect(ctx, name, partId, r.Min, r.Max);
}

// The same for a part on a popup level (a menu row, the shortcut sheet), which can lie outside the
// page's visible area: recorded against the display, so the selftest reaches it without scrolling.
void ProbeFloatingPart(facet::demo::PageCtx& ctx, const char* name, ImGuiID widgetId, int part, ImGuiID partId)
{
    const ImRect r = fi::NavigationPartRect(widgetId, part);
    if (r.GetWidth() > 0.0f)
        facet::demo::FloatingProbeRect(ctx, name, partId, r.Min, r.Max);
}

// A caption line above a bar: bars take the whole row, so their names go above them instead of
// into a label column that would leave them half the width.
void Caption(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, facet::T().colors.labelSecondary);
    facet::Text(facet::TextStyle::Footnote, "%s", text);
    ImGui::PopStyleColor();
}

// --- Tab bars -------------------------------------------------------------------------------------

void TabBars(facet::demo::PageCtx& ctx, NavigationState& s)
{
    SectionHeader("TabBar");
    Paragraph("The selection pill slides to the chosen tab. The bar is one Tab stop: Left and Right choose, Home "
              "and End jump, Ctrl+Page Down and Up switch while the focus is anywhere in this window.");
    Caption("Icons and a badge");
    if (ctx.capture && ctx.captureState == kCaptureFocus)
        ctx.ForceNextItemState(ForcedItemState::Focused, 1);
    facet::TabBar("##icons", &s.iconTab, kIconTabs);
    Note("Showing the content of %s. The badge number is a placeholder.", kIconTabs[s.iconTab].label);

    Paragraph("Closable and reorderable: the close button, a middle click or Delete closes a tab, and the next one "
              "takes over. Drag a tab to move it, or press Ctrl+Space, move it with Left and Right and drop it with "
              "Space; Escape puts it back.");
    Caption("Closable");
    const ImGuiID closableId = ImGui::GetID("##closable");
    if (ctx.capture && ctx.captureState == kCaptureDragging)
        ctx.ForceNextItemState(ForcedItemState::Dragging, 0);
    facet::TabBar("##closable", &s.tab, s.tabs, {.closable = true, .reorderable = true});
    const int closed = facet::TabBarClosedTab();
    if (closed >= 0 && closed < static_cast<int>(s.tabs.size()))
    {
        s.tabs.erase(s.tabs.begin() + closed);
        ++s.closedCount;
    }
    const facet::ReorderMove moved = facet::TabBarMovedTab();
    if (moved.IsValid())
    {
        facet::ApplyReorderMove(s.tabs, moved);
        ++s.movedCount;
    }
    for (int i = 0; i < static_cast<int>(s.tabs.size()) && i < kSampleTabs; ++i)
    {
        char name[48];
        std::snprintf(name, sizeof(name), "navigation.tabs.%d", i);
        const ImGuiID tabId = fi::SelectionEntryId(closableId, s.tabs[static_cast<std::size_t>(i)].id, i);
        ProbePart(ctx, name, closableId, i, tabId);
        std::snprintf(name, sizeof(name), "navigation.tabs.close.%d", i);
        ProbePart(ctx, name, closableId, fi::NavigationClosePart(i), ImGui::GetIDWithSeed("close", nullptr, tabId));
    }
    if (s.tabs.empty())
        Note("Every tab is closed.");
    else
        Note("Showing %s. Tabs closed: %d, moves: %d.", s.tabs[static_cast<std::size_t>(std::clamp(s.tab, 0, static_cast<int>(s.tabs.size()) - 1))].label,
             s.closedCount, s.movedCount);
    if (facet::Button("Reopen the tabs"))
        s.ResetTabs();
    ctx.Probe("navigation.tabs.reopen");
    if (ctx.capture && ctx.captureState == kCaptureDragging)
        ctx.RevealInCapture();

    Caption("Underline");
    facet::TabBar("##underline", &s.underlineTab, kTabLabels, {.style = facet::TabStyle::Underline});

    Paragraph("Tabs that do not fit move into the menu at the end; the chosen tab always stays in the row.");
    Caption("Narrow");
    const ImGuiID overflowId = ImGui::GetID("##overflow");
    if (ctx.capture && ctx.captureState == kCaptureTabsMore)
        ctx.ForceNextItemState(ForcedItemState::Open, kOverflowTabs);
    facet::TabBar("##overflow", &s.overflowTab, kOverflowLabels, {.width = facet::demo::DemoWidth(kNarrowTabs)});
    ProbePart(ctx, "navigation.overflow.more", overflowId, fi::kNavigationPartMore, ImGui::GetIDWithSeed("more", nullptr, overflowId));
    for (int i = 0; i < kOverflowTabs; ++i)
    {
        char name[48];
        std::snprintf(name, sizeof(name), "navigation.overflow.row.%d", i);
        ProbeFloatingPart(ctx, name, overflowId, fi::NavigationMenuRowPart(i), 0);
        std::snprintf(name, sizeof(name), "navigation.overflow.tab.%d", i);
        ProbePart(ctx, name, overflowId, i, fi::SelectionEntryId(overflowId, 0, i));
    }
    if (ctx.capture && ctx.captureState == kCaptureTabsMore)
        ctx.RevealInCapture(0.3f);

    Caption("Small");
    facet::TabBar("##small", &s.smallTab, kTabLabels, {.size = facet::ControlSize::Small});
    Caption("Large");
    facet::TabBar("##large", &s.largeTab, kTabLabels, {.size = facet::ControlSize::Large});
    Caption("Disabled");
    ImGui::BeginDisabled();
    int fixed = 1;
    facet::TabBar("##disabled", &fixed, kTabLabels);
    ImGui::EndDisabled();
}

// --- Sidebar --------------------------------------------------------------------------------------

void SidebarSection(facet::demo::PageCtx& ctx, NavigationState& s)
{
    SectionHeader("Sidebar");
    Paragraph("A navigation column: Up and Down move inside it, Enter opens the item. Auto shows icons and labels "
              "in a wide window and the icon rail in a narrow one, where the labels become tooltips.");
    LabeledRow("Mode");
    facet::SegmentedControl("##mode", &s.sidebarMode, kSidebarModes);
    ctx.Probe("navigation.sidebar.mode");
    facet::SidebarMode mode = s.sidebarMode == 1 ? facet::SidebarMode::Expanded
                            : (s.sidebarMode == 2 ? facet::SidebarMode::Rail : facet::SidebarMode::Auto);
    if (ctx.capture && ctx.captureState == kCaptureRail)
        mode = facet::SidebarMode::Rail;
    static const char* const kPages[] = {"Page 1", "Page 2", "Page 3", "Page 4", "Page 5"};
    static const char* const kIcons[] = {"settings", "layers", "list", "grid", "user"};
    static const char* const kProbes[] = {"navigation.sidebar.page1", "navigation.sidebar.page2", "navigation.sidebar.page3",
                                          "navigation.sidebar.page4", "navigation.sidebar.page5"};
    if (facet::BeginSidebar("sidebar", {.mode = mode, .height = kSidebarHeight}))
    {
        if (facet::BeginSidebarSection("Section 1"))
        {
            for (int i = 0; i < 3; ++i)
            {
                // The badge counts are placeholders.
                facet::SidebarItemOptions item{.selected = s.page == i};
                if (i == 1)
                    item.badge = 3;
                if (i == 2)
                {
                    item.badge = 12;
                    item.badgeTone = facet::Tone::Accent;
                }
                if (ctx.capture && ctx.captureState == kCaptureFocus && i == 1)
                    ctx.ForceNextItemState(ForcedItemState::Focused);
                if (facet::SidebarItem(kPages[i], facet::Icon(kIcons[i]), item))
                    s.page = i;
                ctx.Probe(kProbes[i]);
            }
            facet::EndSidebarSection();
        }
        const bool open = facet::BeginSidebarSection("Section 2", {.collapsible = true});
        ctx.Probe("navigation.sidebar.section2");
        if (open)
        {
            for (int i = 3; i < 5; ++i)
            {
                if (facet::SidebarItem(kPages[i], facet::Icon(kIcons[i]), {.selected = s.page == i}))
                    s.page = i;
                ctx.Probe(kProbes[i]);
            }
            facet::EndSidebarSection();
        }
        facet::EndSidebar();
    }
    ctx.Probe("navigation.sidebar");
    ImGui::SameLine();
    ImGui::BeginGroup();
    Note("Showing %s.", kPages[s.page]);
    Note("The badges are placeholder counts.");
    ImGui::EndGroup();
    if (ctx.capture && ctx.captureState == kCaptureRail)
        ctx.RevealInCapture();
}

// --- Menus ----------------------------------------------------------------------------------------

void FileMenu(facet::demo::PageCtx& ctx, NavigationState& s, bool forceSubmenu)
{
    if (facet::MenuItem("New", {.icon = facet::Icon("plus")}))
    {
        ++s.menuPresses;
        s.lastMenu = "New";
    }
    ctx.Probe("navigation.menu.new");
    if (facet::MenuItem("Run the sample action", {.command = SampleShortcut()}))
        s.lastMenu = "Run the sample action";
    ctx.Probe("navigation.menu.sample");
    if (forceSubmenu)
        ctx.ForceNextItemState(ForcedItemState::Open);
    if (facet::BeginMenu("Recent", {.icon = facet::Icon("clock")}))
    {
        for (int i = 0; i < 3; ++i)
        {
            char label[48];
            std::snprintf(label, sizeof(label), "Placeholder entry %d", i + 1);
            if (facet::MenuItem(label))
            {
                ++s.menuPresses;
                s.lastMenu = "a recent entry";
            }
            if (i == 0)
                ctx.Probe("navigation.menu.recent1");
        }
        facet::EndMenu();
    }
    ctx.Probe("navigation.menu.recent");
    ImGui::BeginDisabled();
    facet::MenuItem("Unavailable", {.icon = facet::Icon("lock")});
    ImGui::EndDisabled();
    if (facet::MenuItem("Delete", {.icon = facet::Icon("trash"), .destructive = true}))
    {
        ++s.menuPresses;
        s.lastMenu = "Delete";
    }
    ctx.Probe("navigation.menu.delete");
}

void ViewMenu(facet::demo::PageCtx& ctx, NavigationState& s)
{
    facet::MenuCheckItem("Show grid", &s.grid, {.shortcut = ImGuiMod_Ctrl | ImGuiKey_G});
    ctx.Probe("navigation.menu.grid");
    for (int i = 0; i < 3; ++i)
    {
        facet::MenuRadioItem(kDensities[i], &s.density, i);
        if (i == 2)
            ctx.Probe("navigation.menu.spacious");
    }
}

void Menus(facet::demo::PageCtx& ctx, NavigationState& s)
{
    SectionHeader("MenuBar, Menu and menu items");
    Paragraph("Once a menu is open, moving to another title opens it instead. Alt moves the focus to the bar; "
              "Left and Right move between titles and between open menus, Down opens one. In a menu, typing "
              "jumps to the next row with that start; a submenu stays open while the pointer travels toward it.");
    const facet::ShortcutId sample = SampleShortcut();
    if (facet::ShortcutPressed(sample))
        ++s.sampleRuns;
    const bool forceMenu = ctx.capture && (ctx.captureState == kCaptureMenu || ctx.captureState == kCaptureSubmenu);
    const ImGuiID barId = ImGui::GetID("menubar");
    if (facet::BeginMenuBar("menubar"))
    {
        if (forceMenu)
            ctx.ForceNextItemState(ForcedItemState::Open);
        if (facet::BeginMenu("File"))
        {
            FileMenu(ctx, s, ctx.capture && ctx.captureState == kCaptureSubmenu);
            facet::EndMenu();
        }
        if (facet::BeginMenu("View"))
        {
            ViewMenu(ctx, s);
            facet::EndMenu();
        }
        if (facet::BeginMenu("Help", {.icon = facet::Icon("help")}))
        {
            if (facet::MenuItem("Shortcuts", {.icon = facet::Icon("keyboard")}))
                s.shortcutSheet = true;
            ctx.Probe("navigation.menu.shortcuts");
            facet::EndMenu();
        }
        facet::EndMenuBar();
    }
    ctx.Probe("navigation.menubar");
    ProbePart(ctx, "navigation.menu.file", barId, 0, ImGui::GetIDWithSeed("File", nullptr, barId));
    ProbePart(ctx, "navigation.menu.view", barId, 1, ImGui::GetIDWithSeed("View", nullptr, barId));
    if (forceMenu)
        ctx.RevealInCapture(0.25f);
    char chord[64];
    facet::ChordToText(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_K, chord, sizeof(chord));
    Note("Last row: %s. Grid %s, density %s. The sample action ran %d times (its menu row, or %s with the menus "
         "closed).",
         s.lastMenu, s.grid ? "shown" : "hidden", kDensities[s.density], s.sampleRuns, chord);

    Paragraph("Menus that do not fit the bar move into its menu at the end; a row there opens the menu.");
    Caption("Narrow bar");
    const ImGuiID narrowId = ImGui::GetID("##narrowbar");
    if (facet::BeginMenuBar("##narrowbar", {.width = facet::demo::DemoWidth(kNarrowMenuBar)}))
    {
        if (facet::BeginMenu("File"))
        {
            if (facet::MenuItem("New"))
                s.lastMenu = "New";
            facet::EndMenu();
        }
        if (facet::BeginMenu("View"))
        {
            ViewMenu(ctx, s);
            facet::EndMenu();
        }
        if (facet::BeginMenu("Help"))
        {
            if (facet::MenuItem("Shortcuts"))
                s.shortcutSheet = true;
            ctx.Probe("navigation.narrowbar.shortcuts");
            facet::EndMenu();
        }
        facet::EndMenuBar();
    }
    ctx.Probe("navigation.narrowbar");
    ProbePart(ctx, "navigation.narrowbar.more", narrowId, fi::kNavigationPartMore, ImGui::GetIDWithSeed("##more", nullptr, narrowId));
    ProbeFloatingPart(ctx, "navigation.narrowbar.row.2", narrowId, fi::NavigationMenuRowPart(2), 0);
}

// --- Breadcrumbs and pagination --------------------------------------------------------------------

void Trails(facet::demo::PageCtx& ctx, NavigationState& s)
{
    SectionHeader("Breadcrumbs");
    Paragraph("A segment goes back to that level; the last one is the current place. Left and Right move between "
              "the segments. Middle segments that do not fit collapse into a menu.");
    const ImGuiID pathId = ImGui::GetID("##path");
    Caption("Path");
    if (ctx.capture && ctx.captureState == kCaptureFocus)
        ctx.ForceNextItemState(ForcedItemState::Focused, 1);
    const int crumb = facet::Breadcrumbs("##path", std::span<const char* const>(kPathLabels, static_cast<std::size_t>(s.depth)));
    if (crumb >= 0)
    {
        s.depth = crumb + 1;
        s.crumbsActivated = crumb;
    }
    for (int i = 0; i < kPathDepth; ++i)
    {
        char name[48];
        std::snprintf(name, sizeof(name), "navigation.path.%d", i);
        ProbePart(ctx, name, pathId, i, fi::SelectionEntryId(pathId, 0, i));
    }
    if (facet::Button("One level deeper") && s.depth < kPathDepth)
        ++s.depth;
    ctx.Probe("navigation.path.deeper");
    ImGui::SameLine();
    Note("Current place: %s.", kPathLabels[s.depth - 1]);
    Caption("Narrow");
    const ImGuiID narrowId = ImGui::GetID("##narrowpath");
    const int narrow = facet::Breadcrumbs("##narrowpath", kPathLabels, {.width = facet::demo::DemoWidth(kNarrowPath)});
    if (narrow >= 0)
        s.crumbsActivated = narrow;
    ProbePart(ctx, "navigation.narrowpath.more", narrowId, fi::kNavigationPartMore, ImGui::GetIDWithSeed("more", nullptr, narrowId));
    for (int i = 1; i < kPathDepth - 1; ++i)
    {
        char name[48];
        std::snprintf(name, sizeof(name), "navigation.narrowpath.row.%d", i);
        ProbeFloatingPart(ctx, name, narrowId, fi::NavigationMenuRowPart(i), 0);
    }
    Caption("Small");
    (void)facet::Breadcrumbs("##smallpath", std::span<const char* const>(kPathLabels, 3), {.size = facet::ControlSize::Small});

    SectionHeader("Pagination");
    Paragraph("Previous and next around the pages; a gap opens a field to type a page. The row is one Tab stop; "
              "Left and Right move the focus, Enter goes to the page.");
    Caption("20 pages");
    if (ctx.capture && ctx.captureState == kCaptureFocus)
        ctx.ForceNextItemState(ForcedItemState::Focused, 2);
    facet::Pagination("##regular", &s.pageIndex, 20);
    // The same pages at the small size, which keeps the full form in narrower rows.
    Caption("20 pages, small");
    const ImGuiID pagesId = ImGui::GetID("##pages");
    facet::Pagination("##pages", &s.pageIndex, 20, {.size = facet::ControlSize::Small});
    ProbePart(ctx, "navigation.pages.previous", pagesId, fi::kNavigationPartPrevious, ImGui::GetIDWithSeed("previous", nullptr, pagesId));
    ProbePart(ctx, "navigation.pages.next", pagesId, fi::kNavigationPartNext, ImGui::GetIDWithSeed("next", nullptr, pagesId));
    ProbePart(ctx, "navigation.pages.gap", pagesId, fi::kNavigationPartGapBase + 1, ImGui::GetIDWithSeed("gap after", nullptr, pagesId));
    for (const int p : {1, 4, 19})
    {
        char name[48];
        std::snprintf(name, sizeof(name), "navigation.pages.%d", p);
        ProbePart(ctx, name, pagesId, p, ImHashData(&p, sizeof(p), pagesId));
    }
    ProbePart(ctx, "navigation.pages.field", pagesId, fi::kNavigationPartFilter, 0);
    Note("Showing page %d of 20.", s.pageIndex + 1);
    Caption("Compact");
    const ImGuiID compactId = ImGui::GetID("##compact");
    facet::Pagination("##compact", &s.compactPage, 12, {.compact = true});
    ProbePart(ctx, "navigation.compact.next", compactId, fi::kNavigationPartNext, ImGui::GetIDWithSeed("next", nullptr, compactId));
    Caption("5 pages, large");
    facet::Pagination("##few", &s.fewPage, 5, {.size = facet::ControlSize::Large});
    Caption("No pages");
    int none = 0;
    facet::Pagination("##none", &none, 0);
}

// --- Steps --------------------------------------------------------------------------------------

void Steps(facet::demo::PageCtx& ctx, NavigationState& s)
{
    SectionHeader("StepIndicator");
    Paragraph("Done steps carry a check mark and can be chosen to go back; the current step is ringed; later steps "
              "cannot be chosen. A Compact width turns the row vertical.");
    const ImGuiID stepsId = ImGui::GetID("##steps");
    Caption("Horizontal");
    facet::StepIndicator("##steps", &s.indicatorStep, kSteps);
    for (int i = 0; i < 4; ++i)
    {
        char name[48];
        std::snprintf(name, sizeof(name), "navigation.steps.%d", i);
        ProbePart(ctx, name, stepsId, i, fi::SelectionEntryId(stepsId, 0, i));
    }
    Caption("Vertical");
    facet::StepIndicator("##vertical", s.indicatorStep, kSteps, {.orientation = facet::Orientation::Vertical});
    if (facet::Button("Next step") && s.indicatorStep < 3)
        ++s.indicatorStep;
    ctx.Probe("navigation.steps.next");

    SectionHeader("Wizard");
    Paragraph("Back and Next move through the steps; Next stays disabled while the step is not valid. A step that "
              "does not apply is skipped and shown dimmed.");
    LabeledRow("Skip step 2");
    facet::Toggle("##skip", &s.skipSecond);
    ctx.Probe("navigation.wizard.skip");
    facet::ChoiceItem steps[4] = {kSteps[0], kSteps[1], kSteps[2], kSteps[3]};
    steps[1].disabled = s.skipSecond;
    const ImGuiID wizardId = ImGui::GetID("wizard");
    if (ctx.capture)
    {
        if (ctx.captureState == kCaptureWizardError)
        {
            s.wizardStep = 0;
            s.name.clear();
        }
        else if (ctx.captureState == kCaptureDefault)
        {
            s.wizardStep = 2;
            s.name = "Placeholder name";
        }
    }
    bool valid = true;
    facet::BeginWizard("wizard", &s.wizardStep, steps);
    switch (s.wizardStep)
    {
    case 0:
        valid = !s.name.empty();
        facet::TextField("Name", &s.name, {.placeholder = "Required", .error = valid ? nullptr : "Enter a name to go on."});
        ctx.Probe("navigation.wizard.name");
        break;
    case 1:
        Note("Content of step 2.");
        break;
    case 2:
        facet::Toggle("Option", &s.optionOn);
        ctx.Probe("navigation.wizard.option");
        break;
    default:
        Note("Name: %s. Option %s.", s.name.c_str(), s.optionOn ? "on" : "off");
        break;
    }
    const facet::WizardAction action = facet::WizardFooter({.canAdvance = valid, .cancelButton = true});
    facet::EndWizard();
    if (action == facet::WizardAction::Finish)
        s.wizardResult = "finished";
    else if (action == facet::WizardAction::Cancel)
    {
        s.wizardResult = "canceled";
        s.wizardStep = 0;
    }
    ProbePart(ctx, "navigation.wizard.back", wizardId, fi::kNavigationPartBack, 0);
    ProbePart(ctx, "navigation.wizard.next", wizardId, fi::kNavigationPartAdvance, 0);
    ProbePart(ctx, "navigation.wizard.cancel", wizardId, fi::kNavigationPartCancel, 0);
    Note("Step %d of 4. Last result: %s.", s.wizardStep + 1, s.wizardResult);
    if (ctx.capture && ctx.captureState == kCaptureWizardError)
        ctx.RevealInCapture();
}

void Sheet(facet::demo::PageCtx& ctx, NavigationState& s)
{
    SectionHeader("ShortcutSheet");
    Paragraph("Lists the registered shortcuts by group, with a filter field. Escape, the close button or a click "
              "beside the sheet closes it; the Help menu opens it too.");
    if (ctx.capture)
        s.shortcutSheet = ctx.captureState == kCaptureShortcuts;
    LabeledRow("Shortcut sheet");
    facet::Toggle("##shortcuts", &s.shortcutSheet);
    ctx.Probe("navigation.shortcuts");
    const ImGuiID sheetId = ImGui::GetID("Shortcuts");
    facet::ShortcutSheet("Shortcuts", &s.shortcutSheet);
    ProbeFloatingPart(ctx, "navigation.shortcuts.close", sheetId, fi::kNavigationPartClose, 0);
    ProbeFloatingPart(ctx, "navigation.shortcuts.filter", sheetId, fi::kNavigationPartFilter, 0);
    ProbeFloatingPart(ctx, "navigation.shortcuts.noresults", sheetId, fi::kNavigationPartNoResults, 0);
    if (ctx.capture && ctx.captureState == kCaptureShortcuts)
        ctx.RevealInCapture();
}

} // namespace

FACET_DEMO_PAGE("navigation", "Navigation", "Widgets", 70, "default", "focus", "menu", "submenu", "tabs-more", "dragging",
                "rail", "wizard-error", "shortcuts")
{
    facet::demo::PageHeader("Navigation",
                            "The sidebar, tab bars, menu bars, menus and menu items, breadcrumbs, pagination, step "
                            "indicators, wizards and the shortcut sheet.");
    facet::demo::ReadyFamilyNote("navigation");
    NavigationState& s = State();
    TabBars(ctx, s);
    SidebarSection(ctx, s);
    Menus(ctx, s);
    Trails(ctx, s);
    Steps(ctx, s);
    Sheet(ctx, s);
}

// Operates every widget of the page by mouse and keyboard and checks what it wrote.
FACET_DEMO_SELFTEST("navigation")
{
    using T = facet::demo::Target;
    test.Do([] {
        NavigationState& s = State();
        s.ResetTabs();
        s.closedCount = 0;
        s.movedCount = 0;
        s.iconTab = 0;
        s.overflowTab = 0;
        s.page = 0;
        s.sidebarMode = 1;
        s.grid = true;
        s.density = 1;
        s.sampleRuns = 0;
        s.menuPresses = 0;
        s.depth = 3;  // short enough for every segment to show in a narrow window
        s.pageIndex = 0;
        s.compactPage = 0;
        s.indicatorStep = 2;
        s.wizardStep = 0;
        s.name.clear();
        s.skipSecond = false;
        s.wizardResult = "none";
        s.shortcutSheet = false;
    });

    // TabBar: a click chooses, Right moves on, Ctrl+Page Down switches.
    test.Click("navigation.tabs.1");
    test.Expect("a click chooses the second tab", [] { return State().tab == 1; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("navigation.tabs.1");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Expect("Left chooses the first tab", [] { return State().tab == 0; });
    test.KeyPress(ImGuiKey_PageDown | ImGuiMod_Ctrl);
    test.Expect("Ctrl+Page Down switches to the next tab", [] { return State().tab == 1; });
    // Reorder by drag: the first tab dragged past the second.
    test.Drag({T::Item("navigation.tabs.0"), T::Item("navigation.tabs.1", 0.9f)}, 0.4f);
    test.Wait(2);
    test.Expect("dragging a tab moves it", [] {
        const NavigationState& s = State();
        return s.movedCount == 1 && std::string(s.tabs[1].label) == "Tab 1";
    });
    test.Expect("the chosen tab stays chosen after the move", [] { return std::string(State().tabs[static_cast<std::size_t>(State().tab)].label) == "Tab 2"; });
    // Reorder by keyboard: Ctrl+Space, Right, Space.
    test.WaitSeconds(0.5f);
    test.Click("navigation.tabs.0");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Space);
    test.Wait(2);
    test.Expect("Ctrl+Space, Right and Space move the focused tab", [] {
        const NavigationState& s = State();
        return s.movedCount == 2 && std::string(s.tabs[0].label) == "Tab 1";
    });
    // Close by the close button, a middle click and Delete.
    test.Do([] { State().ResetTabs(); });
    test.Wait(2);
    test.Click("navigation.tabs.close.0");
    test.Expect("the close button closes the chosen tab and its neighbour takes over", [] {
        const NavigationState& s = State();
        return s.closedCount == 1 && s.tabs.size() == 3 && std::string(s.tabs[static_cast<std::size_t>(s.tab)].label) == "Tab 2";
    });
    test.MouseMove(T::Item("navigation.tabs.1"));
    test.MouseDown(ImGuiMouseButton_Middle);
    test.MouseUp(ImGuiMouseButton_Middle);
    test.Wait(1);
    test.Expect("a middle click closes a tab", [] { return State().closedCount == 2 && State().tabs.size() == 2; });
    test.Click("navigation.tabs.0");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Delete);
    test.Wait(1);
    test.Expect("Delete closes the focused tab", [] { return State().closedCount == 3 && State().tabs.size() == 1; });
    test.Click("navigation.tabs.reopen");
    test.Expect("the button reopens the tabs", [] { return State().tabs.size() == static_cast<std::size_t>(kSampleTabs); });
    // Overflow: the menu chooses a tab that did not fit.
    test.Reveal("navigation.overflow.more");
    test.WaitSeconds(0.4f);
    test.Click("navigation.overflow.more");
    test.WaitSeconds(0.4f);
    test.Click("navigation.overflow.row.5");
    test.Wait(2);
    test.Expect("a row of the more menu chooses its tab", [] { return State().overflowTab == 5; });
    test.ExpectVisible("navigation.overflow.tab.5");

    // Sidebar: a click opens an item; Up, Down and Enter by keyboard; the section header toggles.
    test.Reveal("navigation.sidebar");
    test.WaitSeconds(0.4f);
    test.Click("navigation.sidebar.page2");
    test.Expect("a click opens Page 2", [] { return State().page == 1; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("navigation.sidebar.page2");
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectFocused("navigation.sidebar.page3");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter opens the focused item", [] { return State().page == 2; });
    test.Click("navigation.sidebar.section2");
    test.Wait(2);
    test.ExpectNotVisible("navigation.sidebar.page4");
    test.Click("navigation.sidebar.section2");
    test.Wait(2);
    test.Click("navigation.sidebar.page4");
    test.Expect("opened again, its items work", [] { return State().page == 3; });

    // Menus: a click opens File, a row runs; a check and a radio row write.
    test.Reveal("navigation.menubar");
    test.WaitSeconds(0.4f);
    test.Click("navigation.menu.file");
    test.WaitSeconds(0.4f);
    test.Click("navigation.menu.new");
    test.Expect("a menu row runs its action", [] { return State().menuPresses == 1; });
    test.Click("navigation.menu.view");
    test.WaitSeconds(0.4f);
    test.Click("navigation.menu.grid");
    test.Expect("a check row flips its value", [] { return !State().grid; });
    test.Click("navigation.menu.view");
    test.WaitSeconds(0.4f);
    test.Click("navigation.menu.spacious");
    test.Expect("a radio row writes its value", [] { return State().density == 2; });
    // Submenu: resting on its row opens it.
    test.Click("navigation.menu.file");
    test.WaitSeconds(0.4f);
    test.Hover(T::Item("navigation.menu.recent"), 0.5f);
    test.Hover(T::Item("navigation.menu.recent", 0.6f), 0.3f);
    test.Click("navigation.menu.recent1");
    test.Expect("a row of the submenu runs its action", [] { return State().menuPresses == 2; });
    // The command row triggers the sample shortcut; the chord runs it with the menus closed.
    test.Click("navigation.menu.file");
    test.WaitSeconds(0.4f);
    test.Click("navigation.menu.sample");
    test.Wait(2);
    test.Expect("the command row runs the sample action", [] { return State().sampleRuns == 1; });
    test.KeyPress(ImGuiKey_K | ImGuiMod_Ctrl | ImGuiMod_Shift);
    test.Wait(2);
    test.Expect("the chord runs it with every menu closed", [] { return State().sampleRuns == 2; });
    // Keyboard: Alt focuses the bar, Down opens File, typing d jumps to Delete, Enter runs it.
    test.KeyPress(ImGuiKey_LeftAlt);
    test.ExpectFocused("navigation.menu.file");
    test.KeyPress(ImGuiKey_DownArrow);
    test.WaitSeconds(0.4f);
    test.Type("d");
    test.Wait(2);
    test.ExpectFocused("navigation.menu.delete");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter runs the row typeahead reached", [] { return std::string(State().lastMenu) == "Delete"; });
    // Right in an open menu switches to the next menu; Escape closes it.
    test.KeyPress(ImGuiKey_LeftAlt);
    test.KeyPress(ImGuiKey_DownArrow);
    test.WaitSeconds(0.4f);
    test.KeyPress(ImGuiKey_RightArrow);
    test.WaitSeconds(0.4f);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Right opened View, whose first row Enter flipped", [] { return State().grid; });
    // The narrow bar: its more menu opens a menu that did not fit, whose row works.
    test.Reveal("navigation.narrowbar");
    test.WaitSeconds(0.4f);
    test.Click("navigation.narrowbar.more");
    test.WaitSeconds(0.4f);
    test.Click("navigation.narrowbar.row.2");
    test.WaitSeconds(0.4f);
    test.Click("navigation.narrowbar.shortcuts");
    test.WaitSeconds(0.4f);
    test.Expect("the menu from the more menu opens the shortcut sheet", [] { return State().shortcutSheet; });
    for (int i = 0; i < 3; ++i)
        test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.3f);
    test.Expect("Escape closes the sheet again", [] { return !State().shortcutSheet; });

    // Breadcrumbs: a segment goes back to its level; keyboard; the collapsed menu.
    test.Reveal("navigation.path.deeper");
    test.WaitSeconds(0.4f);
    test.Click("navigation.path.1");
    test.Expect("a segment goes back to its level", [] { return State().depth == 2; });
    test.Click("navigation.path.deeper");
    test.Expect("the button goes one level deeper", [] { return State().depth == 3; });
    // Keyboard: Shift+Tab from the button reaches the trail's one Tab stop, Right and Enter go back.
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("navigation.path.0");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectFocused("navigation.path.1");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on a segment goes back to its level", [] { return State().depth == 2; });
    test.Reveal("navigation.narrowpath.more");
    test.WaitSeconds(0.4f);
    test.Click("navigation.narrowpath.more");
    test.WaitSeconds(0.4f);
    test.Click("navigation.narrowpath.row.1");
    test.Expect("a row of the collapsed menu activates its segment", [] { return State().crumbsActivated == 1; });

    // Pagination: a page, next and previous, keyboard, and the gap's field.
    test.Click("navigation.pages.4");
    test.Expect("a click goes to page 5", [] { return State().pageIndex == 4; });
    test.Click("navigation.pages.next");
    test.Expect("next goes to page 6", [] { return State().pageIndex == 5; });
    test.Click("navigation.pages.previous");
    test.Expect("previous goes back to page 5", [] { return State().pageIndex == 4; });
    test.KeyPress(ImGuiKey_Tab);  // the focus shows on the previous button
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Right moves the focus to the first page and Enter goes there", [] { return State().pageIndex == 0; });
    test.Click("navigation.pages.gap");
    test.WaitSeconds(0.4f);
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.Type("18");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.Expect("the gap's field goes to the typed page", [] { return State().pageIndex == 17; });
    test.Click("navigation.compact.next");
    test.Expect("the compact form steps too", [] { return State().compactPage == 1; });

    // StepIndicator: a done step can be chosen, a later one cannot.
    test.Click("navigation.steps.3");
    test.Expect("a later step cannot be chosen", [] { return State().indicatorStep == 2; });
    test.Click("navigation.steps.0");
    test.Expect("a done step is chosen", [] { return State().indicatorStep == 0; });
    test.Click("navigation.steps.next");
    test.Click("navigation.steps.next");
    test.Click("navigation.steps.1");
    test.Expect("after two steps, the second is done and chosen again", [] { return State().indicatorStep == 1; });

    // Wizard: Next waits for a name, Back goes back, a skipped step is left out, Finish ends.
    test.Reveal("navigation.wizard.next");
    test.WaitSeconds(0.3f);
    test.Click("navigation.wizard.next");
    test.Expect("Next stays disabled without a name", [] { return State().wizardStep == 0; });
    test.Click("navigation.wizard.name");
    test.Type("Placeholder");
    test.KeyPress(ImGuiKey_Enter);
    test.Click("navigation.wizard.next");
    test.Expect("with a name, Next goes to step 2", [] { return State().wizardStep == 1; });
    test.WaitSeconds(0.3f);  // the shorter step lets the page settle at its new scroll limit
    test.Click("navigation.wizard.back");
    test.Expect("Back goes to step 1", [] { return State().wizardStep == 0; });
    test.Click("navigation.wizard.skip");
    test.Click("navigation.wizard.next");
    test.Expect("a skipped step is left out", [] { return State().wizardStep == 2; });
    test.Click("navigation.wizard.next");
    test.Click("navigation.wizard.next");
    test.ExpectText("Finish ends the flow", [] { return std::string(State().wizardResult); }, "finished");
    test.Click("navigation.wizard.cancel");
    test.Expect("Cancel goes back to the start", [] { return State().wizardStep == 0; });

    // Shortcut sheet: the switch opens it, Escape and the close button close it, the filter filters.
    test.Expect("the shortcut sheet starts closed", [] { return !NavigationState{}.shortcutSheet; });
    test.Click("navigation.shortcuts");
    test.WaitSeconds(0.4f);
    test.Expect("the switch opens the sheet", [] { return State().shortcutSheet; });
    test.Type("zzzz");
    test.Wait(2);
    test.ExpectVisible("navigation.shortcuts.noresults");
    test.KeyPress(ImGuiKey_Escape);
    test.Wait(2);
    test.Expect("Escape first clears the filter", [] { return State().shortcutSheet; });
    test.Click("navigation.shortcuts.close");
    test.Expect("the close button closes the sheet", [] { return !State().shortcutSheet; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("navigation.shortcuts");
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.4f);
    test.Expect("Space on the focused switch opens the sheet", [] { return State().shortcutSheet; });
    for (int i = 0; i < 2; ++i)
        test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.3f);
    test.Expect("Escape closes it", [] { return !State().shortcutSheet; });
}
