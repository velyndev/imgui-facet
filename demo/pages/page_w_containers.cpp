// Demo page: Containers. Calls the v1 widgets of facet/widgets/containers.h, grouped by purpose.
// The widgets are planned, so each call draws its placeholder; Phase 2 replaces them one by one
// on this page. The shell is a layer: it starts closed and a checkbox opens it, so a finished
// shell never covers the gallery uninvited; the capture state "shell" shows it open. The app
// frame, the overlay controller and mini windows are root containers that a host calls outside
// every window, so they are not called inside this page; the page says so and the family doc
// shows their host code.

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

// Capture states of the page, in the order of the FACET_DEMO_PAGE registration.
enum CaptureState
{
    kCaptureDefault = 0,
    kCaptureShell = 1,
};

struct ContainersState
{
    bool shellOpen = false;
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

void Windows(facet::demo::PageCtx& ctx, ContainersState& s)
{
    SectionHeader("Windows and frames");
    Paragraph("The floating shell is a window of its own that a page opens; the checkbox sets its "
              "open flag, and its close button or Escape will clear it. A planned shell shows one "
              "placeholder line while the flag is on.");
    if (ctx.capture)
        s.shellOpen = ctx.captureState == kCaptureShell;
    LabeledRow("Shell");
    ImGui::Checkbox("##shell", &s.shellOpen);
    facet::ItemCursor();
    ctx.Probe("containers.shell");
    if (facet::BeginShell("Shell###shell", &s.shellOpen, {.icon = facet::Icon("settings"), .closeOnEscape = true}))
    {
        Note("Shell content.");
        facet::EndShell();
    }
    if (ctx.capture && ctx.captureState == kCaptureShell)
        ctx.RevealInCapture();
    Paragraph("A title row with an icon and window buttons, as the top of an app frame or a shell.");
    facet::BeginTitleBar("Title", {.icon = facet::Icon("monitor"), .windowButtons = true});
    facet::EndTitleBar();
    Note("The app frame, the overlay controller and mini windows are root containers: the host "
         "calls them once per frame outside every window, and the app frame fills the whole "
         "viewport. They cannot sit inside a page, so this page does not call them; "
         "docs/widgets/containers.md shows the host code for each.");
}

void Bars(ContainersState&)
{
    SectionHeader("Bars");
    facet::BeginToolbar("toolbar");
    facet::ToolbarButton("Add", facet::Icon("plus"), {.priority = 2});
    facet::ToolbarButton("Remove", facet::Icon("trash"));
    facet::ToolbarSeparator();
    facet::ToolbarSpacer();
    facet::ToolbarButton("Settings", facet::Icon("settings"), {.iconOnly = false});
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
    facet::BeginCard("Card title", {.icon = facet::Icon("layers")});
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

FACET_DEMO_PAGE("containers", "Containers", "Widgets", 80, "default", "shell")
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
    Windows(ctx, s);
    Bars(s);
    Grouping(s);
    Scrolling(s);
}

// The shell starts closed, so a finished shell never opens over the gallery by itself, and the
// checkbox opens and closes it.
FACET_DEMO_SELFTEST("containers")
{
    test.Expect("the shell starts closed", [] { return !ContainersState{}.shellOpen; });
    test.Do([] { State().shellOpen = false; });
    test.Click("containers.shell");
    test.Expect("the checkbox opens the shell", [] { return State().shellOpen; });
    test.Click("containers.shell");
    test.Expect("the checkbox closes the shell", [] { return !State().shellOpen; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("containers.shell");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the focused checkbox opens the shell", [] { return State().shellOpen; });
    test.KeyPress(ImGuiKey_Space);
    test.Expect("and closes it again", [] { return !State().shellOpen; });
}
