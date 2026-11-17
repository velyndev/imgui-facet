// Demo page: Containers. Calls every v1 widget of facet/widgets/containers.h once, grouped by
// purpose. The widgets are planned, so each call draws its placeholder; Phase 2 replaces them
// one by one on this page. Window-like containers (shell, mini window) and the app frame are
// shown in the page flow while they are planned; their finished forms float or fill the host.

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct ContainersState
{
    bool shellOpen = true;
    bool overlayVisible = true;
    bool miniPinned = false;
    bool disclosureOpen = false;
    bool inspectorVisible = true;
    ImGuiSelectionBasicStorage selection;
};

// Height of the sample regions: a few large control heights in design units, so it follows the
// tokens.
constexpr float kRegionControls = 5.0f;

float RegionHeight()
{
    return facet::T().metrics.controlHeightLarge * kRegionControls;
}

ContainersState& State()
{
    static ContainersState state;
    return state;
}

void Windows(ContainersState& s)
{
    SectionHeader("Windows and frames");
    Paragraph("The floating shell for overlay menus and the app frame for desktop apps, with a "
              "title bar. The overlay controller wraps the menu and the mini window stays visible "
              "when pinned.");
    if (facet::BeginShell("Shell###shell", &s.shellOpen, {.icon = facet::Icon("settings")}))
    {
        Note("Shell content.");
        facet::EndShell();
    }
    if (facet::BeginAppFrame("appframe"))
    {
        facet::BeginTitleBar("Title", {.icon = facet::Icon("monitor"), .windowButtons = true});
        facet::EndTitleBar();
        Note("App frame content.");
        facet::EndAppFrame();
    }
    if (facet::BeginOverlay("overlay", &s.overlayVisible, {.toggleKey = {.primary = ImGuiKey_Insert}}))
    {
        Note("Overlay menu content.");
        facet::EndOverlay();
    }
    if (facet::BeginMiniWindow("Mini window", &s.miniPinned))
    {
        Note("Mini window content.");
        facet::EndMiniWindow();
    }
}

void Bars(ContainersState&)
{
    SectionHeader("Bars");
    facet::BeginToolbar("toolbar");
    facet::ToolbarButton("Add", facet::Icon("plus"), {.priority = 2});
    facet::ToolbarButton("Remove", facet::Icon("trash"));
    facet::ToolbarSeparator();
    facet::ToolbarSpacer();
    facet::ToolbarButton("Settings", facet::Icon("settings"), {.showLabel = true});
    facet::EndToolbar();
    facet::BeginStatusBar("statusbar");
    facet::StatusItem("Status 1", {.icon = facet::Icon("info")});
    facet::StatusItem("Status 2", {.tone = facet::Tone::Success});
    facet::EndStatusBar();
}

void Grouping(ContainersState& s)
{
    SectionHeader("Group and reveal");
    facet::BeginAccordion("accordion", {.singleOpen = true});
    if (facet::BeginAccordionSection("Section 1", {.defaultOpen = true}))
    {
        Note("Section 1 content.");
        facet::EndAccordionSection();
    }
    if (facet::BeginAccordionSection("Section 2"))
    {
        Note("Section 2 content.");
        facet::EndAccordionSection();
    }
    facet::EndAccordion();
    if (facet::BeginDisclosure("Details", &s.disclosureOpen, {.summary = "Summary"}))
    {
        Note("Disclosed content.");
        facet::EndDisclosure();
    }
    facet::BeginCard("card", {.title = "Card title", .icon = facet::Icon("layers")});
    Note("Card content.");
    facet::EndCard();
    if (facet::BeginGroupBox("Group box", {.description = "Description"}))
    {
        Note("Group box content.");
        facet::EndGroupBox();
    }
}

void Scrolling(ContainersState& s)
{
    SectionHeader("Scroll");
    if (facet::BeginScrollArea("scroll", {.height = RegionHeight()}))
    {
        Note("Scroll area content.");
        facet::EndScrollArea();
    }
    if (facet::BeginVirtualList("virtual", 10000, &s.selection, {.height = RegionHeight()}))
    {
        facet::RowRange rows;
        while (facet::NextVisibleRows(&rows))
            for (int row = rows.first; row < rows.last; ++row)
            {
                facet::VirtualListRow(row);
                facet::Text(facet::TextStyle::Body, "Row %d", row + 1);
            }
        facet::EndVirtualList();
    }
    facet::BeginInspectorPane("Inspector", &s.inspectorVisible);
    Note("Main content.");
    if (facet::InspectorPaneNext())
        Note("Inspector pane content.");
    facet::EndInspectorPane();
}

} // namespace

FACET_DEMO_PAGE("containers", "Containers", "Widgets", 80)
{
    facet::demo::PageHeader("Containers",
                            "The floating shell, the app frame and its title bar, the overlay "
                            "controller, mini windows, tool and status bars, accordions, "
                            "disclosures, cards, group boxes, scroll areas, virtual lists and the "
                            "inspector pane. Every widget on this page is a placeholder until "
                            "Phase 2 implements it: it shows its name and the word planned, and "
                            "does not react to input.");
    Note("Declared in facet/widgets/containers.h; the declarations and usage examples are in "
         "docs/widgets/containers.md.");
    ContainersState& s = State();
    Windows(s);
    Bars(s);
    Grouping(s);
    Scrolling(s);
}
