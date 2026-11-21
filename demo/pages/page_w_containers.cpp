// Demo page: Containers. One section per widget of facet/widgets/containers.h. The ready widgets
// show their variants and states and record probes for the selftest; a planned widget draws its
// placeholder. The shell is a layer: it starts closed and a checkbox opens it, so a shell never
// covers the gallery uninvited; the capture state "shell" shows it open. The app frame, the overlay
// controller and mini windows are root containers that a host calls outside every window, so they
// are not called inside this page; the page says so and the family doc shows their host code.

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// Capture states of the page, in the order of the FACET_DEMO_PAGE registration.
enum CaptureState
{
    kCaptureDefault = 0,
    kCaptureShell = 1,
    kCaptureFocused = 2,
    kCaptureHovered = 3,
};

constexpr int kScrollLines = 40;

struct ContainersState
{
    bool shellOpen = false;
    bool inspectorVisible = true;
    ImGuiSelectionBasicStorage selection;

    // Card
    int cardPresses = 0;
    // Group box
    bool groupOpen = false;     // read back from the collapsible box each frame
    int groupPresses = 0;
    // Disclosure
    bool detailsOpen = false;
    bool linkOpen = false;      // read back from the link disclosure each frame
    // Accordion
    bool sectionOpen[3] = {};   // read back each frame
    bool singleOpen[3] = {};
    // Scroll area
    float scrollY = 0.0f;
    float scrollMax = 0.0f;
    int scrollPresses = 0;
    bool resetOpen = false;     // the selftest asks for the default open states
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

// Probes the header a Begin just submitted. A section that is animating has already clipped the
// window to its content when the Begin returns, so the header is probed against the window's
// visible area instead.
void ProbeHeader(const facet::demo::PageCtx& ctx, const char* name)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImGui::PushClipRect(window->InnerClipRect.Min, window->InnerClipRect.Max, false);
    ctx.Probe(name);
    ImGui::PopClipRect();
}

// The selftest's reset: the next frame opens or closes every container that keeps its own open
// state back to its default.
void ResetOpen(const ContainersState& s, bool open)
{
    if (s.resetOpen)
        ImGui::SetNextItemOpen(open, ImGuiCond_Always);
}

bool Focused(const facet::demo::PageCtx& ctx)
{
    return ctx.capture && ctx.captureState == kCaptureFocused;
}

bool Hovered(const facet::demo::PageCtx& ctx)
{
    return ctx.capture && ctx.captureState == kCaptureHovered;
}

void Windows(facet::demo::PageCtx& ctx, ContainersState& s)
{
    SectionHeader("Shell");
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
    SectionHeader("Title bar");
    Paragraph("A title row with an icon and window buttons, as the top of an app frame or a shell.");
    facet::BeginTitleBar("Title", {.icon = facet::Icon("monitor"), .windowButtons = true});
    facet::EndTitleBar();
    SectionHeader("App frame, overlay controller and mini window");
    Note("The app frame, the overlay controller and mini windows are root containers: the host "
         "calls them once per frame outside every window, and the app frame fills the whole "
         "viewport. They cannot sit inside a page, so this page does not call them; "
         "docs/widgets/containers.md shows the host code for each.");
}

void Bars(ContainersState&)
{
    SectionHeader("Toolbar and status bar");
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

void Cards(facet::demo::PageCtx& ctx, ContainersState& s)
{
    SectionHeader("Card");
    Paragraph("A grouped surface with a title line. Items that fill the width stop at the padding; "
              "the second card has no title, the third is a fixed narrow width with a long title "
              "that ends in an ellipsis and shows in full on hover.");
    facet::BeginCard("Card title", {.icon = facet::Icon("layers")});
    Note("Content of the card. The button below fills the inner width.");
    if (facet::Button("Count a press", {.width = facet::kFill}))
        ++s.cardPresses;
    ctx.Probe("containers.card.button");
    Note("Presses: %d", s.cardPresses);
    facet::EndCard();
    ctx.Probe("containers.card");
    ImGui::Spacing();
    facet::BeginCard("##untitled");
    Note("A card without a title line.");
    facet::EndCard();
    ImGui::Spacing();
    facet::BeginCard("A card title that is longer than its narrow card", {.width = 160.0f});
    Note("Narrow card.");
    facet::EndCard();
}

void GroupBoxes(facet::demo::PageCtx& ctx, ContainersState& s)
{
    SectionHeader("Group box");
    if (facet::BeginGroupBox("Group box", {.icon = facet::Icon("folder"), .description = "A description line under the title."}))
    {
        if (facet::Button("Count a group press"))
            ++s.groupPresses;
        ctx.Probe("containers.group.button");
        ImGui::SameLine();
        facet::demo::NoteBeside("Presses: %d", s.groupPresses);
        facet::EndGroupBox();
    }
    ImGui::Spacing();
    if (Focused(ctx))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    ResetOpen(s, false);
    s.groupOpen = facet::BeginGroupBox("Collapsible group box", {.collapsible = true, .defaultOpen = false});
    ProbeHeader(ctx, "containers.group.toggle");
    if (s.groupOpen)
    {
        Note("Content that shows while the box is open.");
        facet::EndGroupBox();
    }
    ImGui::Spacing();
    ImGui::BeginDisabled();
    if (facet::BeginGroupBox("Disabled group box", {.description = "Every control inside is disabled."}))
    {
        (void)facet::Button("Disabled button");
        facet::EndGroupBox();
    }
    ImGui::EndDisabled();
}

void Disclosures(facet::demo::PageCtx& ctx, ContainersState& s)
{
    SectionHeader("Disclosure");
    if (Hovered(ctx))
        ctx.ForceNextItemState(ForcedItemState::Hovered);
    const bool details = facet::BeginDisclosure("Details", &s.detailsOpen, {.summary = "Two lines"});
    ProbeHeader(ctx, "containers.disclosure");
    if (details)
    {
        Note("First line of the disclosed content.");
        Note("Second line of the disclosed content.");
        facet::EndDisclosure();
    }
    if (Focused(ctx))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    ResetOpen(s, false);
    s.linkOpen = facet::BeginDisclosure("Show more", nullptr, {.style = facet::DisclosureStyle::Link});
    ProbeHeader(ctx, "containers.disclosure.link");
    if (s.linkOpen)
    {
        Note("Content behind the link.");
        facet::EndDisclosure();
    }
    ImGui::BeginDisabled();
    if (facet::BeginDisclosure("Disabled disclosure", nullptr))
        facet::EndDisclosure();
    ImGui::EndDisabled();
}

void Accordions(facet::demo::PageCtx& ctx, ContainersState& s)
{
    SectionHeader("Accordion");
    Paragraph("Several sections may be open at once. Up and Down move between the headers.");
    const char* const labels[3] = {"General", "Appearance", "Advanced"};
    const char* const probes[3] = {"containers.accordion.0", "containers.accordion.1", "containers.accordion.2"};
    facet::BeginAccordion("accordion");
    for (int i = 0; i < 3; ++i)
    {
        if (i == 1 && Focused(ctx))
            ctx.ForceNextItemState(ForcedItemState::Focused);
        if (i == 2 && Hovered(ctx))
            ctx.ForceNextItemState(ForcedItemState::Hovered);
        ResetOpen(s, i == 0);
        s.sectionOpen[i] = facet::BeginAccordionSection(
            labels[i], {.icon = facet::Icon(i == 0 ? "settings" : (i == 1 ? "image" : "layers")),
                        .detail = i == 2 ? "Placeholder detail" : nullptr,
                        .defaultOpen = i == 0});
        ProbeHeader(ctx, probes[i]);
        if (s.sectionOpen[i])
        {
            Note("Content of the section %s.", labels[i]);
            facet::EndAccordionSection();
        }
    }
    facet::EndAccordion();
    ImGui::Spacing();
    Paragraph("Single open: opening a section closes the others.");
    const char* const single[3] = {"First", "Second", "Third"};
    const char* const singleProbes[3] = {"containers.single.0", "containers.single.1", "containers.single.2"};
    facet::BeginAccordion("single", {.singleOpen = true});
    for (int i = 0; i < 3; ++i)
    {
        ResetOpen(s, i == 0);
        s.singleOpen[i] = facet::BeginAccordionSection(single[i], {.defaultOpen = i == 0});
        ProbeHeader(ctx, singleProbes[i]);
        if (s.singleOpen[i])
        {
            Note("Content of the section %s.", single[i]);
            facet::EndAccordionSection();
        }
    }
    facet::EndAccordion();
}

void ScrollAreas(facet::demo::PageCtx& ctx, ContainersState& s)
{
    SectionHeader("Scroll area");
    Paragraph("The wheel scrolls smoothly; the bar shows while scrolling or hovered, and the content "
              "fades out at an edge with more behind it.");
    if (facet::BeginScrollArea("scroll", {.height = RegionHeight()}))
    {
        for (int i = 0; i < kScrollLines; ++i)
        {
            ImGui::PushID(i);
            if (i == 0)
            {
                if (facet::Button("Count a scroll press"))
                    ++s.scrollPresses;
                ctx.Probe("containers.scroll.first");
            }
            else if (i == kScrollLines - 1)
            {
                (void)facet::Button("Last line");
                ctx.Probe("containers.scroll.last");
            }
            else
            {
                Note("Line %d", i + 1);
            }
            ImGui::PopID();
        }
        if (s.resetOpen)
            ImGui::SetScrollY(0.0f);
        s.scrollY = ImGui::GetScrollY();
        s.scrollMax = ImGui::GetScrollMaxY();
        facet::EndScrollArea();
    }
    ctx.Probe("containers.scroll");
}

void Lists(ContainersState& s)
{
    SectionHeader("Virtual list");
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
    SectionHeader("Inspector pane");
    facet::BeginInspectorPane("Inspector", &s.inspectorVisible);
    Note("Main content.");
    if (facet::InspectorPaneNext())
        Note("Inspector pane content.");
    facet::EndInspectorPane();
}

} // namespace

FACET_DEMO_PAGE("containers", "Containers", "Widgets", 80, "default", "shell", "focused", "hovered")
{
    facet::demo::PageHeader("Containers",
                            "The floating shell, the app frame and its title bar, the overlay controller, mini "
                            "windows, tool and status bars, accordions, disclosures, cards, group boxes, scroll "
                            "areas, virtual lists and the inspector pane.");
    facet::demo::PlannedFamilyNote("containers");
    ContainersState& s = State();
    Windows(ctx, s);
    Bars(s);
    Cards(ctx, s);
    GroupBoxes(ctx, s);
    Disclosures(ctx, s);
    Accordions(ctx, s);
    if (ctx.capture && ctx.captureState != kCaptureDefault && ctx.captureState != kCaptureShell)
        ctx.RevealInCapture();
    ScrollAreas(ctx, s);
    Lists(s);
    s.resetOpen = false;
}

FACET_DEMO_SELFTEST("containers")
{
    using T = facet::demo::Target;
    test.Do([] {
        ContainersState& s = State();
        s.shellOpen = false;
        s.cardPresses = 0;
        s.groupPresses = 0;
        s.scrollPresses = 0;
        s.detailsOpen = false;
        s.resetOpen = true;
    });

    // Shell: the checkbox opens and closes it, by mouse and keyboard.
    test.Expect("the shell starts closed", [] { return !ContainersState{}.shellOpen; });
    test.Click("containers.shell");
    test.Expect("the checkbox opens the shell", [] { return State().shellOpen; });
    test.Click("containers.shell");
    test.Expect("the checkbox closes the shell", [] { return !State().shellOpen; });

    // Card: the content works and fills the inner width.
    test.Click("containers.card.button");
    test.Expect("a button inside a card works", [] { return State().cardPresses == 1; });

    // Group box: content works; the collapsible one toggles by click, Space and Enter.
    test.Click("containers.group.button");
    test.Expect("a button inside a group box works", [] { return State().groupPresses == 1; });
    test.Expect("the collapsible group box starts closed", [] { return !State().groupOpen; });
    test.Click("containers.group.toggle");
    test.Expect("a click on the title opens the group box", [] { return State().groupOpen; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("containers.group.toggle");
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.5f);  // Begin returns true while the region animates closed
    test.Expect("Space on the focused title closes it", [] { return !State().groupOpen; });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter opens it again", [] { return State().groupOpen; });

    // Disclosure: the caller's flag follows a click, Space and Enter; the link keeps its own state.
    test.Click("containers.disclosure");
    test.Expect("a click opens the disclosure", [] { return State().detailsOpen; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("containers.disclosure");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space closes it", [] { return !State().detailsOpen; });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter opens it", [] { return State().detailsOpen; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("containers.disclosure.link");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on the link toggles its own state", [] { return State().linkOpen; });
    test.Click("containers.disclosure.link");
    test.WaitSeconds(0.5f);
    test.Expect("a click on the link closes it", [] { return !State().linkOpen; });

    // Accordion: clicks toggle sections; the arrows move between the headers.
    test.Click("containers.accordion.1");
    test.Expect("a click opens the second section", [] { return State().sectionOpen[1]; });
    test.Expect("the first section stays open", [] { return State().sectionOpen[0]; });
    test.Click("containers.accordion.1");
    test.WaitSeconds(0.5f);
    test.Expect("a second click closes it", [] { return !State().sectionOpen[1]; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("containers.accordion.1");
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectFocused("containers.accordion.2");
    test.KeyPress(ImGuiKey_Home);
    test.ExpectFocused("containers.accordion.0");
    test.KeyPress(ImGuiKey_End);
    test.ExpectFocused("containers.accordion.2");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter opens the focused section", [] { return State().sectionOpen[2]; });
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectFocused("containers.accordion.1");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space opens the second section", [] { return State().sectionOpen[1]; });

    // Single open: opening one closes the others.
    test.Expect("the first single-open section starts open", [] { return State().singleOpen[0]; });
    test.Click("containers.single.2");
    test.WaitSeconds(0.5f);
    test.Expect("opening the third closes the first", [] {
        const ContainersState& s = State();
        return s.singleOpen[2] && !s.singleOpen[0] && !s.singleOpen[1];
    });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("containers.single.2");
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.5f);
    test.Expect("Enter on the second opens it and closes the third", [] {
        const ContainersState& s = State();
        return s.singleOpen[1] && !s.singleOpen[2];
    });

    // Scroll area: the wheel scrolls it; keyboard focus inside scrolls it to the focused item.
    test.Reveal("containers.scroll");
    test.Click("containers.scroll.first");
    test.Expect("a button inside the scroll area works", [] { return State().scrollPresses == 1; });
    test.Wheel(T::Item("containers.scroll"), -3.0f);
    test.WaitSeconds(0.5f);
    test.Expect("the wheel scrolls the area", [] { return State().scrollY > 0.0f; });
    test.Click("containers.scroll.first");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_End);
    test.WaitSeconds(0.5f);
    test.Expect("End moves the focus to the last line and scrolls there", [] {
        const ContainersState& s = State();
        return s.scrollMax > 0.0f && s.scrollY >= s.scrollMax - 1.0f;
    });
    test.ExpectVisible("containers.scroll.last");
}
