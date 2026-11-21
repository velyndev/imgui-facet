// Demo page: Containers. One section per widget of facet/widgets/containers.h, with its variants and
// states and the probes of the selftest at the end of the file. The layers (the shell, the overlay
// menu, the mini window) start closed and a checkbox opens each, so none covers the gallery
// uninvited; the capture states "shell" and "overlay" show them open. The app frame fills the whole
// viewport of a host, so the page only describes it; its unit tests and the family doc cover it.

#include <algorithm>
#include <cmath>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

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
    kCaptureMenu = 4,
    kCaptureOverlay = 5,
};

constexpr int kScrollLines = 40;
constexpr int kVirtualRows = 10000;
// The narrow status bar: three quarters of a card, too narrow for its three segments.
constexpr float kNarrowShare = 0.75f;

struct ContainersState
{
    bool shellOpen = false;
    ImRect shellRect;           // the shell window in the last frame it showed
    int shellPresses = 0;
    // Overlay and mini window
    bool overlayVisible = false;
    bool miniPinned = false;
    int menuPresses = 0;
    ImVec2 miniCenter = {0.0f, 0.0f};
    bool miniTakesInput = false;
    // Title bar
    const char* lastWindowButton = "none";
    bool maximized = false;
    bool inspectorVisible = true;
    ImRect inspectorRect;
    int inspectorMainPresses = 0;
    int inspectorPanePresses = 0;
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
    // Toolbar
    const char* lastTool = "none";
    int toolPresses = 0;
    int view = 0;
    bool narrowShown[4] = {};
    // Virtual list
    int listState = 0;          // 0 rows, 1 loading, 2 error, 3 empty
    int listActivated = -1;
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
              "open flag, and its close button or Escape clears it. Drag the title bar to move it "
              "(it snaps to the display edges) and any edge or corner to resize it; its place and "
              "size are kept in design units.");
    if (ctx.capture)
        s.shellOpen = ctx.captureState == kCaptureShell;
    LabeledRow("Shell");
    ImGui::Checkbox("##shell", &s.shellOpen);
    facet::ItemCursor();
    ctx.Probe("containers.shell");
    facet::ShellOptions shell{.icon = facet::Icon("settings"),
                              .initialSize = {300.0f, 220.0f},
                              .minSize = {240.0f, 160.0f},
                              .closeOnEscape = true};
    // Capture and selftest runs start from the same place every time.
    shell.persist = !ctx.capture && ctx.probes == nullptr;
    if (facet::BeginShell("Shell###containers.shell", &s.shellOpen, shell))
    {
        ImGuiWindow* root = ImGui::GetCurrentWindow()->RootWindow;
        s.shellRect = root->Rect();
        const float title = facet::Px(facet::T().containers.titleBarHeight);
        const ImRect r = s.shellRect;
        // The title bar, the close button and the bottom-right corner, for the selftest. They lie
        // outside the content area, so they are probed against the shell window's own rectangle.
        ImGui::PushClipRect(r.Min, r.Max, false);
        facet::demo::ProbeRect(ctx, "containers.shell.title", root->GetID("##titlebar"), r.Min,
                               ImVec2(r.Max.x - title, r.Min.y + title));
        facet::demo::ProbeRect(ctx, "containers.shell.close", root->GetID("##close"), ImVec2(r.Max.x - title, r.Min.y),
                               ImVec2(r.Max.x, r.Min.y + title));
        const float band = facet::Px(facet::T().containers.resizeBorder);
        facet::demo::ProbeRect(ctx, "containers.shell.corner", 0, ImVec2(r.Max.x - band, r.Max.y - band), r.Max);
        ImGui::PopClipRect();
        Paragraph("Content of the shell. It scrolls when the shell is short.");
        if (facet::Button("Count a shell press"))
            ++s.shellPresses;
        ctx.Probe("containers.shell.button");
        ImGui::SameLine();
        facet::demo::NoteBeside("Presses: %d", s.shellPresses);
        if (facet::BeginGroupBox("Group box in the shell", {.description = "Containers nest inside the shell."}))
        {
            Note("Placeholder content.");
            facet::EndGroupBox();
        }
        facet::EndShell();
    }
    SectionHeader("Title bar");
    Paragraph("A title row with an icon, a detail, the caller's items at the trailing end and the "
              "window buttons, as the top of an app frame. On this page the buttons only report "
              "their press; in an app frame the host minimizes, maximizes or closes its window.");
    facet::BeginTitleBar("Window title", {.icon = facet::Icon("monitor"),
                                          .detail = "Placeholder detail",
                                          .windowButtons = true,
                                          .maximized = s.maximized});
    if (facet::Button("Title bar item"))
        s.lastWindowButton = "item";
    ctx.Probe("containers.titlebar.item");
    facet::EndTitleBar();
    const ImRect bar(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    switch (facet::TitleBarButtonPressed())
    {
    case facet::WindowButton::Minimize: s.lastWindowButton = "minimize"; break;
    case facet::WindowButton::Maximize:
        s.lastWindowButton = "maximize";
        s.maximized = !s.maximized;
        break;
    case facet::WindowButton::Close: s.lastWindowButton = "close"; break;
    default: break;
    }
    {
        // The window buttons, for the selftest: three squares of the control height at the end.
        const float side = facet::ControlHeight(facet::ControlSize::Inherit);
        const float gap = facet::Px(facet::T().spacing.xs);
        const float inset = (bar.GetHeight() - side) * 0.5f;
        const float top = std::floor(bar.GetCenter().y - side * 0.5f);
        const char* const names[3] = {"containers.titlebar.minimize", "containers.titlebar.maximize",
                                      "containers.titlebar.close"};
        float x = bar.Max.x - inset - side * 3.0f - gap * 2.0f;
        for (const char* name : names)
        {
            facet::demo::ProbeRect(ctx, name, 0, ImVec2(x, top), ImVec2(x + side, top + side));
            x += side + gap;
        }
    }
    Note("Last pressed: %s.", s.lastWindowButton);
    SectionHeader("App frame");
    Note("The app frame is a root container: the host calls it once per frame outside every window "
         "and it fills the whole viewport, so this page does not call it. Its hit test, size limits "
         "and placement are checked by the unit tests in tests/test_containers.cpp, and "
         "docs/widgets/containers.md shows the host code.");

    SectionHeader("Overlay controller and mini window");
    Paragraph("An overlay menu as a host would show it over a game: the first checkbox shows or hides "
              "the menu (a small shell) with a fade. The mini window shows with the menu; pinned, it "
              "stays while the menu is hidden and then lets input through. A host calls the controller "
              "outside every window; this page calls it from its own code to show it.");
    if (ctx.capture)
    {
        s.overlayVisible = ctx.captureState == kCaptureOverlay;
        s.miniPinned = ctx.captureState == kCaptureOverlay;
    }
    LabeledRow("Overlay menu");
    ImGui::Checkbox("##overlay", &s.overlayVisible);
    facet::ItemCursor();
    ctx.Probe("containers.overlay");
    LabeledRow("Pin the mini window");
    ImGui::Checkbox("##pinned", &s.miniPinned);
    facet::ItemCursor();
    ctx.Probe("containers.mini.pinned");
    if (facet::BeginOverlay("containers.overlay", &s.overlayVisible))
    {
        facet::ShellOptions menu{.initialSize = {260.0f, 160.0f}, .centered = false,
                                 .corner = facet::ScreenCorner::TopRight};
        menu.persist = false;
        if (facet::BeginShell("Overlay menu###containers.overlay.shell", nullptr, menu))
        {
            Note("Placeholder content of the overlay menu.");
            if (facet::Button("Count a menu press"))
                ++s.menuPresses;
            ctx.Probe("containers.overlay.button");
            facet::EndShell();
        }
        facet::EndOverlay();
    }
    if (facet::BeginMiniWindow("Mini window###containers.mini", &s.miniPinned,
                               {.icon = facet::Icon("info"), .persist = false}))
    {
        ImGuiWindow* mini = ImGui::GetCurrentWindow();
        const ImRect r = mini->Rect();
        s.miniCenter = r.GetCenter();
        Note("Placeholder content.");
        // The pin button sits at the end of the title row, in the window's padding.
        const float pad = facet::Px(facet::T().spacing.md);
        const float side = facet::ControlHeight(facet::ControlSize::Small);
        ImGui::PushClipRect(r.Min, r.Max, false);
        facet::demo::ProbeRect(ctx, "containers.mini.pin", mini->GetID("##pin"),
                               ImVec2(r.Max.x - pad - side, r.Min.y + pad), ImVec2(r.Max.x - pad, r.Min.y + pad + side));
        ImGui::PopClipRect();
        facet::EndMiniWindow();
    }
    s.miniTakesInput = facet::OverlayHitTest(s.miniCenter);
    Note("Menu presses: %d. The mini window takes input: %s.", s.menuPresses, s.miniTakesInput ? "yes" : "no");
}

// One toolbar button that records its activation, probed while it shows. Returns whether it shows.
bool Tool(facet::demo::PageCtx& ctx, ContainersState& s, const char* label, const char* icon, const char* probe,
          const facet::ToolbarButtonOptions& options = {})
{
    if (facet::ToolbarButton(label, facet::Icon(icon), options))
    {
        s.lastTool = label;
        ++s.toolPresses;
    }
    const bool shown = ImGui::GetItemID() == ImGui::GetID(label);
    if (shown)
        ctx.Probe(probe);
    return shown;
}

void Toolbars(facet::demo::PageCtx& ctx, ContainersState& s)
{
    SectionHeader("Toolbar");
    Paragraph("Plain tool buttons with tooltips; the view buttons latch. The second toolbar is narrow: "
              "the buttons that do not fit move into the more menu, lowest priority first.");
    facet::BeginToolbar("toolbar");
    Tool(ctx, s, "Add", "plus", "containers.toolbar.add", {.priority = 2});
    Tool(ctx, s, "Remove", "trash", "containers.toolbar.remove", {.priority = 2});
    facet::ToolbarSeparator();
    Tool(ctx, s, "Undo", "undo", "containers.toolbar.undo");
    Tool(ctx, s, "Redo", "redo", "containers.toolbar.redo");
    facet::ToolbarSeparator();
    if (facet::ToolbarButton("Grid view", facet::Icon("grid"), {.selected = s.view == 0, .priority = 1}))
        s.view = 0;
    if (ImGui::GetItemID() == ImGui::GetID("Grid view"))
        ctx.Probe("containers.toolbar.grid");
    if (facet::ToolbarButton("List view", facet::Icon("list"), {.selected = s.view == 1, .priority = 1}))
        s.view = 1;
    if (ImGui::GetItemID() == ImGui::GetID("List view"))
        ctx.Probe("containers.toolbar.list");
    facet::ToolbarSpacer();
    Tool(ctx, s, "Settings", "settings", "containers.toolbar.settings", {.iconOnly = false, .priority = 1});
    facet::EndToolbar();
    Note("Last tool: %s (%d presses). View: %s.", s.lastTool, s.toolPresses, s.view == 0 ? "grid" : "list");

    ImGui::Spacing();
    // Room for two buttons and the more button.
    const float narrow = facet::T().metrics.controlHeight * 3.0f + facet::T().spacing.xs * 2.0f + facet::T().spacing.sm;
    facet::BeginToolbar("narrow", {.width = narrow});
    const float left = ImGui::GetCursorScreenPos().x;
    s.narrowShown[0] = Tool(ctx, s, "Refresh", "refresh", "containers.narrow.refresh");
    s.narrowShown[1] = Tool(ctx, s, "Copy", "copy", "containers.narrow.copy", {.priority = 1});
    s.narrowShown[2] = Tool(ctx, s, "Paste", "paste", "containers.narrow.paste", {.priority = 2});
    s.narrowShown[3] = Tool(ctx, s, "Download", "download", "containers.narrow.download");
    const ImGuiID more = ImGui::GetID("##more");
    if (ctx.capture && ctx.captureState == kCaptureMenu)
        ctx.ForceNextItemState(ForcedItemState::Open);
    facet::EndToolbar();
    const float side = facet::ControlHeight(facet::ControlSize::Inherit);
    const ImVec2 max = ImGui::GetItemRectMax();
    facet::demo::ProbeRect(ctx, "containers.narrow.more", more, ImVec2(std::max(left, max.x - side), max.y - side), max);
    if (ctx.capture && ctx.captureState == kCaptureMenu)
        ctx.RevealInCapture(0.3f);
}

void Bars(facet::demo::PageCtx& ctx, ContainersState& s)
{
    SectionHeader("Status bar");
    Paragraph("Status segments with a tone dot or icon. The second bar is narrow: the segments that do "
              "not fit are hidden, lowest priority first. The texts are placeholders.");
    facet::BeginStatusBar("statusbar");
    facet::StatusItem("Placeholder status", {.icon = facet::Icon("info"), .priority = 2});
    facet::StatusItem(s.toolPresses > 0 ? "Tool pressed" : "No tool pressed",
                      {.tone = s.toolPresses > 0 ? facet::Tone::Success : facet::Tone::Neutral, .priority = 1});
    facet::StatusItem("Warning placeholder", {.icon = facet::Icon("warning"), .tone = facet::Tone::Warning});
    facet::StatusItem("Error placeholder", {.tone = facet::Tone::Destructive});
    facet::EndStatusBar();
    ctx.Probe("containers.status");
    ImGui::Spacing();
    facet::BeginStatusBar("narrowstatus", {.width = facet::T().metrics.cardWidth * kNarrowShare});
    facet::StatusItem("Kept segment", {.tone = facet::Tone::Accent, .priority = 2});
    facet::StatusItem("Hidden first", {.priority = 0});
    facet::StatusItem("Hidden second", {.priority = 1});
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

void Lists(facet::demo::PageCtx& ctx, ContainersState& s)
{
    SectionHeader("Virtual list");
    Paragraph("Ten thousand placeholder rows; only the visible ones are submitted. Click, Ctrl and "
              "Shift select, the arrows, Page Up and Down, Home and End move, Enter or a double "
              "click activates. The buttons switch the list's data state.");
    const char* const states[4] = {"Rows", "Loading", "Error", "Empty"};
    const char* const stateProbes[4] = {"containers.virtual.rows", "containers.virtual.loading",
                                        "containers.virtual.error", "containers.virtual.empty"};
    const float right = facet::demo::ContentRight();
    for (int i = 0; i < 4; ++i)
    {
        if (i > 0)
            facet::demo::SameLineIfFits(right, facet::demo::ButtonWidth(states[i]));
        if (ImGui::RadioButton(states[i], s.listState == i))
            s.listState = i;
        facet::ItemCursor();
        ctx.Probe(stateProbes[i]);
    }
    const facet::DataState data =
        s.listState == 1 ? facet::DataState::Loading : (s.listState == 2 ? facet::DataState::Error : facet::DataState::Ready);
    const int count = s.listState == 3 ? 0 : kVirtualRows;
    const bool list = facet::BeginVirtualList("virtual", count, &s.selection,
                                              {.state = data,
                                               .emptyText = "No rows in this placeholder list",
                                               .height = RegionHeight()});
    ctx.Probe("containers.virtual");
    if (list)
    {
        facet::RowRange rows;
        while (facet::NextVisibleRows(&rows))
            for (int row = rows.first; row < rows.last; ++row)
            {
                if (facet::VirtualListRow(row))
                    s.listActivated = row;
                if (row == 2)
                    ctx.Probe("containers.virtual.row2");
                if (row == 4)
                    ctx.Probe("containers.virtual.row4");
                if (row == kVirtualRows - 1)
                    ctx.Probe("containers.virtual.last");
                facet::Text(facet::TextStyle::Body, "Row %d", row + 1);
            }
        facet::EndVirtualList();
    }
    if (s.listActivated >= 0)
        Note("Selected rows: %d. Last activated: row %d.", s.selection.Size, s.listActivated + 1);
    else
        Note("Selected rows: %d. No row activated yet.", s.selection.Size);
    SectionHeader("Inspector pane");
    Paragraph("A details pane at the trailing edge: its close button hides it, the checkbox shows it "
              "again, and its leading edge drags its width. In a narrow window it floats over the "
              "main content.");
    LabeledRow("Show the pane");
    ImGui::Checkbox("##inspector", &s.inspectorVisible);
    facet::ItemCursor();
    ctx.Probe("containers.inspector.toggle");
    facet::BeginInspectorPane("Inspector###containers.inspector", &s.inspectorVisible, {.height = RegionHeight()});
    Note("Main content of the region (placeholder).");
    if (facet::Button("Count a main press"))
        ++s.inspectorMainPresses;
    ctx.Probe("containers.inspector.main");
    if (facet::InspectorPaneNext())
    {
        ImGuiWindow* pane = ImGui::GetCurrentWindow();
        s.inspectorRect = pane->Rect();
        const ImRect& r = s.inspectorRect;
        const float pad = facet::Px(facet::T().spacing.lg);
        const float side = facet::ControlHeight(facet::ControlSize::Small);
        const float band = facet::Px(facet::T().containers.resizeBorder);
        ImGui::PushClipRect(r.Min, r.Max, false);
        facet::demo::ProbeRect(ctx, "containers.inspector.close", pane->GetID("##close"),
                               ImVec2(r.Max.x - pad - side, r.Min.y + pad), ImVec2(r.Max.x - pad, r.Min.y + pad + side));
        facet::demo::ProbeRect(ctx, "containers.inspector.edge", pane->GetID("##resize"), r.Min,
                               ImVec2(r.Min.x + band, r.Max.y));
        ImGui::PopClipRect();
        Note("Pane content (placeholder).");
        if (facet::Button("Count a pane press"))
            ++s.inspectorPanePresses;
        ctx.Probe("containers.inspector.button");
    }
    facet::EndInspectorPane();
    Note("Main presses: %d. Pane presses: %d.", s.inspectorMainPresses, s.inspectorPanePresses);
    // The overlay capture shows the lower sections under the menu and the mini window.
    if (ctx.capture && ctx.captureState == kCaptureOverlay)
        ctx.RevealInCapture(0.7f);
}

} // namespace

FACET_DEMO_PAGE("containers", "Containers", "Widgets", 80, "default", "shell", "focused", "hovered", "menu", "overlay")
{
    facet::demo::PageHeader("Containers",
                            "The floating shell, the app frame and its title bar, the overlay controller, mini "
                            "windows, tool and status bars, accordions, disclosures, cards, group boxes, scroll "
                            "areas, virtual lists and the inspector pane.");
    facet::demo::ReadyFamilyNote("containers");
    ContainersState& s = State();
    Windows(ctx, s);
    Toolbars(ctx, s);
    Bars(ctx, s);
    Cards(ctx, s);
    GroupBoxes(ctx, s);
    Disclosures(ctx, s);
    Accordions(ctx, s);
    if (Focused(ctx) || Hovered(ctx))
        ctx.RevealInCapture();
    ScrollAreas(ctx, s);
    Lists(ctx, s);
    s.resetOpen = false;
}

// The shell's rectangle before a drag, read by the expectations after it.
static ImRect shellStart;
// The pane's rectangle before a drag of its edge.
static ImRect paneStart;

FACET_DEMO_SELFTEST("containers")
{
    using T = facet::demo::Target;
    test.Do([] {
        ContainersState& s = State();
        s.shellOpen = false;
        s.shellPresses = 0;
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
    test.Wait(2);
    test.Click("containers.shell.button");
    test.Expect("a button inside the shell works", [] { return State().shellPresses == 1; });
    // Moving: the title bar drags the shell along.
    test.Do([] { State().shellPresses = 0; });
    test.Do([] { shellStart = State().shellRect; });
    test.Drag({T::Item("containers.shell.title", 0.3f), T::Item("containers.shell.title", 0.3f).Offset(-40.0f, -30.0f)},
              0.3f);
    test.Expect("dragging the title bar moves the shell", [] {
        const ImRect& r = State().shellRect;
        return r.Min.x < shellStart.Min.x - 1.0f && r.Min.y < shellStart.Min.y - 1.0f &&
               std::fabs(r.GetWidth() - shellStart.GetWidth()) < 0.5f;
    });
    // Resizing: the bottom-right corner grows it.
    test.Do([] { shellStart = State().shellRect; });
    test.Drag({T::Item("containers.shell.corner"), T::Item("containers.shell.corner").Offset(30.0f, 20.0f)}, 0.3f);
    test.Expect("dragging the corner resizes the shell", [] {
        const ImRect& r = State().shellRect;
        return r.GetWidth() > shellStart.GetWidth() + 1.0f && r.GetHeight() > shellStart.GetHeight() + 1.0f &&
               std::fabs(r.Min.x - shellStart.Min.x) < 0.5f;
    });
    // Keyboard: Tab reaches the content; Escape closes the shell.
    test.Click("containers.shell.button");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("containers.shell.button");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on the focused button inside the shell works", [] { return State().shellPresses == 2; });
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape closes the shell", [] { return !State().shellOpen; });
    // The close button.
    test.Click("containers.shell");
    test.Wait(2);
    test.Click("containers.shell.close");
    test.Expect("the close button closes the shell", [] { return !State().shellOpen; });

    // Title bar: the window buttons report their press, by mouse and keyboard; maximize shows restore.
    test.Click("containers.titlebar.minimize");
    test.ExpectText("the minimize button reports its press", [] { return std::string(State().lastWindowButton); },
                    "minimize");
    test.Click("containers.titlebar.maximize");
    test.Expect("the maximize button reports its press", [] { return State().maximized; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectText("Enter on the focused close button reports it", [] { return std::string(State().lastWindowButton); },
                    "close");
    test.Click("containers.titlebar.item");
    test.ExpectText("the caller's item in the title bar works", [] { return std::string(State().lastWindowButton); },
                    "item");
    test.Do([] { State().maximized = false; });

    // Toolbar: clicks and Space or Enter activate; the arrows move along the row; the view buttons latch.
    test.Click("containers.toolbar.add");
    test.ExpectText("a click activates a tool button", [] { return std::string(State().lastTool); }, "Add");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("containers.toolbar.add");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectFocused("containers.toolbar.remove");
    test.KeyPress(ImGuiKey_Space);
    test.ExpectText("Space activates the focused tool button", [] { return std::string(State().lastTool); }, "Remove");
    test.Click("containers.toolbar.list");
    test.Expect("the list view button latches", [] { return State().view == 1; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectFocused("containers.toolbar.grid");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on the grid view button latches it", [] { return State().view == 0; });
    test.Click("containers.toolbar.settings");
    test.ExpectText("the labelled button at the trailing end works", [] { return std::string(State().lastTool); },
                    "Settings");
    // Narrow toolbar: Copy and Paste show, Refresh and Download wait in the more menu.
    test.ExpectVisible("containers.narrow.paste");
    test.Expect("Refresh and Download wait in the more menu", [] {
        const ContainersState& s = State();
        return !s.narrowShown[0] && s.narrowShown[1] && s.narrowShown[2] && !s.narrowShown[3];
    });
    test.Click("containers.narrow.paste");
    test.ExpectText("a visible button of the narrow toolbar works", [] { return std::string(State().lastTool); },
                    "Paste");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectFocused("containers.narrow.more");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.ExpectText("Enter on the first menu row activates Refresh", [] { return std::string(State().lastTool); },
                    "Refresh");
    test.ExpectFocused("containers.narrow.more");  // the focus came back from the closed menu
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.ExpectText("the second menu row activates Download", [] { return std::string(State().lastTool); },
                    "Download");

    // Virtual list: click, Ctrl and Shift select; End reaches the last row; Enter activates; the states show.
    test.Do([] {
        ContainersState& s = State();
        s.selection.Clear();
        s.listState = 0;
        s.listActivated = -1;
    });
    test.Reveal("containers.virtual");
    test.Wait(2);
    // The page's only virtual list starts at its top; the request needs the list in sight.
    test.Do([] { facet::SetNextVirtualListScrollTo(0); });
    test.Wait(2);
    test.Click("containers.virtual.row2");
    test.Expect("a click selects a row", [] { return State().selection.Size == 1 && State().selection.Contains(2); });
    test.KeyDown(ImGuiMod_Shift);
    test.Click("containers.virtual.row4");
    test.KeyUp(ImGuiMod_Shift);
    test.Expect("Shift+click selects the range", [] { return State().selection.Size == 3; });
    // The clicked row keeps the keyboard; Tab would leave the list (list_view.h, "Tab").
    test.KeyPress(ImGuiKey_End);
    test.Wait(3);
    test.ExpectVisible("containers.virtual.last");
    test.ExpectFocused("containers.virtual.last");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter activates the last row", [] { return State().listActivated == kVirtualRows - 1; });
    test.Do([] { State().listActivated = -1; });
    test.DoubleClick("containers.virtual.last");
    test.Expect("a double click activates a row", [] { return State().listActivated == kVirtualRows - 1; });
    test.Click("containers.virtual.loading");
    test.Expect("the loading state shows", [] { return State().listState == 1; });
    test.Click("containers.virtual.empty");
    test.Click("containers.virtual.rows");
    test.Expect("the rows come back", [] { return State().listState == 0; });

    // Status bar: not interactive; the first segment follows the tool presses.
    test.Reveal("containers.status");
    test.ExpectVisible("containers.status");

    // Overlay: the checkbox shows the menu; the mini window shows with it and pins; pinned and
    // without the menu it lets input through.
    test.Do([] {
        ContainersState& s = State();
        s.overlayVisible = false;
        s.miniPinned = false;
        s.menuPresses = 0;
    });
    test.Click("containers.overlay");
    test.WaitSeconds(0.5f);
    test.Expect("the checkbox shows the overlay menu", [] { return State().overlayVisible; });
    test.Click("containers.overlay.button");
    test.Expect("a button in the overlay menu works", [] { return State().menuPresses == 1; });
    test.Expect("the mini window takes input while the menu shows", [] { return State().miniTakesInput; });
    test.Click("containers.mini.pin");
    test.Expect("the pin button pins the mini window", [] { return State().miniPinned; });
    test.Click("containers.overlay");
    test.WaitSeconds(0.5f);
    test.Expect("the pinned mini window lets input through without the menu", [] {
        return !State().overlayVisible && !State().miniTakesInput;
    });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("containers.overlay");
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.5f);
    test.Expect("Space on the checkbox shows the menu again", [] { return State().overlayVisible; });
    // The menu's shell took the keyboard focus as it appeared; the mouse hides the menu again.
    test.Click("containers.overlay");
    test.WaitSeconds(0.5f);
    test.Expect("the checkbox hides the menu", [] { return !State().overlayVisible; });
    test.Do([] { State().miniPinned = false; });
    test.Wait(2);

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

    // Inspector pane: both parts work; the close button hides it, the checkbox shows it, the edge resizes it.
    test.Do([] {
        ContainersState& s = State();
        s.inspectorVisible = true;
        s.inspectorMainPresses = 0;
        s.inspectorPanePresses = 0;
    });
    test.Reveal("containers.inspector.main");
    test.Wait(2);
    // Near its leading edge: in a narrow window the pane floats over the rest of the main content.
    test.MouseMove(T::Item("containers.inspector.main", 0.05f));
    test.MouseDown();
    test.MouseUp();
    test.Click("containers.inspector.button");
    test.Expect("the main content and the pane both work", [] {
        return State().inspectorMainPresses == 1 && State().inspectorPanePresses == 1;
    });
    test.Do([] { paneStart = State().inspectorRect; });
    // Narrower first (it may already be at its minimum), then wider again.
    test.Drag({T::Item("containers.inspector.edge"), T::Item("containers.inspector.edge").Offset(30.0f, 0.0f)}, 0.3f);
    test.Expect("dragging the leading edge right never widens the pane", [] {
        return State().inspectorRect.GetWidth() <= paneStart.GetWidth() + 0.5f;
    });
    test.Do([] { paneStart = State().inspectorRect; });
    test.Drag({T::Item("containers.inspector.edge"), T::Item("containers.inspector.edge").Offset(-30.0f, 0.0f)}, 0.3f);
    test.Expect("dragging the leading edge left widens the pane", [] {
        return State().inspectorRect.GetWidth() > paneStart.GetWidth() + 1.0f;
    });
    test.Click("containers.inspector.close");
    test.WaitSeconds(0.5f);
    test.Expect("the close button hides the pane", [] { return !State().inspectorVisible; });
    test.Click("containers.inspector.toggle");
    test.WaitSeconds(0.5f);
    test.Expect("the checkbox shows it again", [] { return State().inspectorVisible; });
    test.Click("containers.inspector.button");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("containers.inspector.close");
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.5f);
    test.Expect("Enter on the focused close button hides the pane", [] { return !State().inspectorVisible; });
    test.Do([] { State().inspectorVisible = true; });
}
