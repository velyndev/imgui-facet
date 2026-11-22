// Demo page: Buttons. Every widget of facet/widgets/buttons.h in a section with its variants, sizes
// and states, capture states for the important looks and a selftest that operates each by mouse
// and keyboard.

#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/popup_host.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::MixedHeightRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct ButtonsUi
{
    // Button: what was activated, so every press shows on the page and the selftest can read it.
    const char* last = nullptr;
    int presses = 0;
    int save = 0;
    int duplicate = 0;
    int cancel = 0;
    int remove = 0;
    int sized = 0;
    int download = 0;
    int settings = 0;
    int disabled = 0;
    int upload = 0;
    int narrow = 0;
    int wide = 0;
    bool loading = false;
    bool narrowTip = false;  // the cut button's tooltip shows (read in the frame, for the selftest)

    // IconButton
    bool pinned = false;
    bool locked = true;
    int zoom = 0;
    int iconPresses = 0;
    bool iconLoading = false;
    bool pinTip = false;  // the pin's tooltip shows (read in the frame, for the selftest)

    // CopyButton
    int copies = 0;

    // Link
    int links = 0;
    bool linkVisited = false;
    std::string openedUrl;  // selftests: the address the link asked the platform to open

    // ToggleButtonGroup
    bool latches[3] = {true, false, false};
    int view = 0;
    int textView = 1;
    int labelView = 1;
    int narrowView = 0;
    int disabledItem = 0;
    int mixedView = 0;
    int toggleWrites = 0;
};

ButtonsUi ui;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kHovered,
    kPressed,
    kFocused,
    kLoading,
    kToolsState,    // the icon buttons, copy buttons and links, focused, confirming and hovered
    kTogglesState,  // the toggle button groups, a button focused and one hovered
};

// Toolbar items named after the built-in icons they show.
const facet::ChoiceItem kTools[] = {
    {"Grid", facet::Icon("grid")},
    {"List", facet::Icon("list")},
    {"Layers", facet::Icon("layers")},
};
const char* const kToolNames[] = {"Option 1", "Option 2", "Option 3"};

// Counts a press and remembers which button it was.
void Pressed(int* counter, const char* name)
{
    ++*counter;
    ++ui.presses;
    ui.last = name;
}

// Places buttons side by side in a regular row, one under the other in the control column when
// the row is compact.
float Height(facet::ControlSize size)
{
    return facet::ControlHeight(size);
}

struct ButtonRow
{
    bool stack = facet::AvailableWidthClass() == facet::WidthClass::Compact;
    float column = ImGui::GetCursorPosX();
    void Next() const
    {
        if (stack)
            ImGui::SetCursorPosX(column);
        else
            ImGui::SameLine();
    }
};

void ButtonSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Button");
    Paragraph("One primary action per view, neutral and plain actions beside it, and a destructive one. Space or "
              "Enter activates the focused button.");

    LabeledRow("Variants");
    {
        const ButtonRow row;
        // The forced capture states are shown on the primary button.
        if (ctx.capture && ctx.captureState == kHovered)
            ctx.ForceNextItemState(ForcedItemState::Hovered);
        else if (ctx.capture && ctx.captureState == kPressed)
            ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Pressed);
        else if (ctx.capture && ctx.captureState == kFocused)
            ctx.ForceNextItemState(ForcedItemState::Focused);
        if (facet::Button("Save", {.variant = facet::ButtonVariant::Primary}))
            Pressed(&ui.save, "Save");
        ctx.Probe("button.save");
        row.Next();
        if (facet::Button("Duplicate"))
            Pressed(&ui.duplicate, "Duplicate");
        ctx.Probe("button.duplicate");
        row.Next();
        if (facet::Button("Cancel", {.variant = facet::ButtonVariant::Plain}))
            Pressed(&ui.cancel, "Cancel");
        ctx.Probe("button.cancel");
        row.Next();
        if (facet::Button("Delete", {.variant = facet::ButtonVariant::Destructive, .icon = facet::Icon("trash")}))
            Pressed(&ui.remove, "Delete");
        ctx.Probe("button.delete");
    }
    ctx.RevealInCapture(0.3f);

    LabeledRow("Sizes", Height(facet::ControlSize::Large));
    {
        MixedHeightRow row(Height(facet::ControlSize::Large));
        row.Item(Height(facet::ControlSize::Small));
        if (facet::Button("Small", {.size = facet::ControlSize::Small}))
            Pressed(&ui.sized, "Small");
        ctx.Probe("button.small");
        row.Item(Height(facet::ControlSize::Regular));
        if (facet::Button("Regular"))
            Pressed(&ui.sized, "Regular");
        ctx.Probe("button.regular");
        row.Item(Height(facet::ControlSize::Large));
        if (facet::Button("Large", {.size = facet::ControlSize::Large}))
            Pressed(&ui.sized, "Large");
        ctx.Probe("button.large");
    }

    LabeledRow("Icons");
    {
        const ButtonRow row;
        if (facet::Button("Download", {.icon = facet::Icon("download")}))
            Pressed(&ui.download, "Download");
        ctx.Probe("button.download");
        row.Next();
        // Icon only: an empty visible label. IconButton adds the tooltip such a button needs.
        if (facet::Button("##settings", {.icon = facet::Icon("settings")}))
            Pressed(&ui.settings, "Settings (icon only)");
        ctx.Probe("button.settings");
    }

    LabeledRow("States");
    {
        const ButtonRow row;
        ImGui::BeginDisabled();
        if (facet::Button("Disabled", {.variant = facet::ButtonVariant::Primary}))
            Pressed(&ui.disabled, "Disabled");
        ImGui::EndDisabled();
        ctx.Probe("button.disabled");
        row.Next();
        const bool loading = ui.loading || (ctx.capture && ctx.captureState == kLoading);
        if (facet::Button("Upload", {.icon = facet::Icon("upload"), .loading = loading}))
            Pressed(&ui.upload, "Upload");
        ctx.Probe("button.upload");
        row.Next();
        facet::Toggle("Loading", &ui.loading);
        ctx.Probe("button.loadingToggle");
    }
    ctx.RevealInCapture(0.5f);

    LabeledRow("Widths");
    {
        const ButtonRow row;
        // 120 units cannot hold this label: it ends in an ellipsis and shows in full on hover and focus.
        if (facet::Button("Export the selected layers", {.width = facet::demo::DemoWidth(120.0f)}))
            Pressed(&ui.narrow, "Export the selected layers");
        ui.narrowTip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
        ctx.Probe("button.narrow");
        row.Next();
        if (facet::Button("Continue", {.width = facet::kFill}))
            Pressed(&ui.wide, "Continue");
        ctx.Probe("button.wide");
    }

    if (ui.last != nullptr)
        Note("Last activated: %s. Presses on this page: %d.", ui.last, ui.presses);
    else
        Note("No button activated yet.");
}

void IconButtonSection(facet::demo::PageCtx& ctx)
{
    if (ctx.capture && ctx.captureState == kToolsState)
        ctx.RevealInCapture(0.04f);
    SectionHeader("IconButton");
    Paragraph("A square button for a tool whose icon says what it does; its name shows as a tooltip on hover and "
              "on keyboard focus. A latched tool passes selected and flips its own flag.");

    LabeledRow("Tools");
    {
        const ButtonRow row;
        if (ctx.capture && ctx.captureState == kToolsState)
            ctx.ForceNextItemState(ForcedItemState::Focused);
        if (facet::IconButton("Zoom in", facet::Icon("zoom-in")))
            Pressed(&ui.zoom, "Zoom in");
        ctx.Probe("button.icon.zoomIn");
        row.Next();
        if (facet::IconButton("Zoom out", facet::Icon("zoom-out")))
            Pressed(&ui.zoom, "Zoom out");
        ctx.Probe("button.icon.zoomOut");
        row.Next();
        if (facet::IconButton(ui.pinned ? "Unpin###pin" : "Pin###pin", facet::Icon(ui.pinned ? "unpin" : "pin"),
                              {.selected = ui.pinned}))
        {
            ui.pinned = !ui.pinned;
            Pressed(&ui.iconPresses, "Pin");
        }
        ui.pinTip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(ImGui::GetItemID()));
        ctx.Probe("button.icon.pin");
        row.Next();
        if (facet::IconButton("Lock layout", facet::Icon("lock"), {.selected = ui.locked}))
        {
            ui.locked = !ui.locked;
            Pressed(&ui.iconPresses, "Lock layout");
        }
        ctx.Probe("button.icon.lock");
    }
    LabeledRow("Variants");
    {
        const ButtonRow row;
        if (facet::IconButton("Settings", facet::Icon("settings"), {.variant = facet::ButtonVariant::Secondary}))
            Pressed(&ui.iconPresses, "Settings");
        row.Next();
        if (facet::IconButton("Add", facet::Icon("plus"), {.variant = facet::ButtonVariant::Primary}))
            Pressed(&ui.iconPresses, "Add");
        row.Next();
        if (facet::IconButton("Delete item", facet::Icon("trash"), {.variant = facet::ButtonVariant::Destructive}))
            Pressed(&ui.iconPresses, "Delete item");
    }
    LabeledRow("Sizes", Height(facet::ControlSize::Large));
    {
        MixedHeightRow row(Height(facet::ControlSize::Large));
        row.Item(Height(facet::ControlSize::Small));
        facet::IconButton("Search##small", facet::Icon("search"),
                          {.variant = facet::ButtonVariant::Secondary, .size = facet::ControlSize::Small});
        row.Item(Height(facet::ControlSize::Regular));
        facet::IconButton("Search##regular", facet::Icon("search"), {.variant = facet::ButtonVariant::Secondary});
        row.Item(Height(facet::ControlSize::Large));
        facet::IconButton("Search##large", facet::Icon("search"),
                          {.variant = facet::ButtonVariant::Secondary, .size = facet::ControlSize::Large});
    }
    LabeledRow("States");
    {
        MixedHeightRow row(Height(facet::ControlSize::Regular));
        row.Item(Height(facet::ControlSize::Regular));
        ImGui::BeginDisabled();
        if (facet::IconButton("Edit (disabled)", facet::Icon("edit"), {.variant = facet::ButtonVariant::Secondary}))
            Pressed(&ui.iconPresses, "Edit (disabled)");
        ctx.Probe("button.icon.disabled");
        row.Item(Height(facet::ControlSize::Regular));
        facet::IconButton("Disabled and latched", facet::Icon("lock"), {.selected = true});
        ImGui::EndDisabled();
        row.Item(Height(facet::ControlSize::Regular));
        const bool loading = ui.iconLoading || (ctx.capture && (ctx.captureState == kLoading || ctx.captureState == kToolsState));
        if (facet::IconButton("Refresh", facet::Icon("refresh"), {.variant = facet::ButtonVariant::Secondary, .loading = loading}))
        {
            ui.iconLoading = true;
            Pressed(&ui.iconPresses, "Refresh");
        }
        ctx.Probe("button.icon.refresh");
        row.Item(Height(facet::ControlSize::Small));
        if (facet::Button("Stop loading", {.variant = facet::ButtonVariant::Plain, .size = facet::ControlSize::Small}))
            ui.iconLoading = false;
        ctx.Probe("button.icon.stop");
    }
    Note("Pin %s, layout %s. Refresh %s.", ui.pinned ? "on" : "off", ui.locked ? "locked" : "unlocked",
         ui.iconLoading ? "runs until Stop loading is pressed" : "is idle");
}

void CopyButtonSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("CopyButton");
    Paragraph("Copies a text to the clipboard. For a moment afterwards it shows a check and the word Copied, in a "
              "width that holds both looks, so nothing next to it moves.");

    static const char* const kSample = "Placeholder text to copy";
    LabeledRow("Beside a value");
    {
        facet::Text(facet::TextStyle::Body, "%s", kSample);
        ImGui::SameLine(0.0f, facet::Px(facet::T().spacing.sm));
        if (ctx.capture && ctx.captureState == kToolsState)
            ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Pressed);
        if (facet::CopyButton("Copy the placeholder text", kSample))
            Pressed(&ui.copies, "Copy (icon only)");
        ctx.Probe("button.copy.icon");
    }
    LabeledRow("With its label");
    {
        MixedHeightRow row(Height(facet::ControlSize::Regular));
        row.Item(Height(facet::ControlSize::Regular));
        if (facet::CopyButton("Copy##labelled", kSample, {.iconOnly = false}))
            Pressed(&ui.copies, "Copy");
        ctx.Probe("button.copy.label");
        row.Item(Height(facet::ControlSize::Regular));
        if (facet::CopyButton("Copy as one line", "Placeholder line",
                              {.variant = facet::ButtonVariant::Secondary, .iconOnly = false}))
            Pressed(&ui.copies, "Copy as one line");
        ctx.Probe("button.copy.secondary");
        row.Item(Height(facet::ControlSize::Small));
        facet::CopyButton("Copy##small", kSample,
                          {.variant = facet::ButtonVariant::Secondary, .iconOnly = false, .size = facet::ControlSize::Small});
    }
    LabeledRow("States");
    {
        const ButtonRow row;
        // A null text: nothing to copy, so the button is disabled.
        if (facet::CopyButton("Nothing to copy", nullptr, {.variant = facet::ButtonVariant::Secondary, .iconOnly = false}))
            Pressed(&ui.copies, "Nothing to copy");
        ctx.Probe("button.copy.none");
    }
    Note("Copies on this page: %d.", ui.copies);
}

void ToggleGroupSection(facet::demo::PageCtx& ctx)
{
    if (ctx.capture && ctx.captureState == kTogglesState)
        ctx.RevealInCapture(0.04f);
    SectionHeader("ToggleButtonGroup");
    Paragraph("Latched tool buttons joined into one row: independent flags, or one choice at a time with the "
              "selection pill. Every button is a Tab stop; buttons that do not fit move into a menu.");

    static const facet::ChoiceItem kWithDisabled[] = {
        {"Option 1"},
        {"Option 2 (disabled)", {}, nullptr, nullptr, true},
        {"Option 3"},
    };
    const auto count = [](bool written) {
        if (written)
            ++ui.toggleWrites;
    };

    LabeledRow("Independent");
    if (ctx.capture && ctx.captureState == kTogglesState)
        ctx.ForceNextItemState(ForcedItemState::Focused, 1);
    count(facet::ToggleButtonGroup("Panels", ui.latches, kTools));
    ctx.Probe("button.toggles.latches");
    LabeledRow("Exclusive");
    if (ctx.capture && ctx.captureState == kTogglesState)
        ctx.ForceNextItemState(ForcedItemState::Hovered, 2);
    count(facet::ToggleButtonGroup("##view", &ui.view, kTools, {.allowEmpty = true}));
    ctx.Probe("button.toggles.view");
    LabeledRow("Icons and labels");
    count(facet::ToggleButtonGroup("##labels", &ui.labelView, kTools, {.iconOnly = false}));
    LabeledRow("Text only");
    count(facet::ToggleButtonGroup("##text", &ui.textView, kToolNames));
    LabeledRow("Narrow");
    // 150 units hold one or two of the labels; the rest move into the "more" menu.
    count(facet::ToggleButtonGroup("##narrow", &ui.narrowView, kToolNames, {.width = facet::demo::DemoWidth(150.0f)}));
    ctx.Probe("button.toggles.narrow");
    LabeledRow("States");
    {
        const ButtonRow row;
        count(facet::ToggleButtonGroup("##disabledItem", &ui.disabledItem, kWithDisabled));
        ctx.Probe("button.toggles.disabledItem");
        row.Next();
        int fixed = 0;
        facet::ToggleButtonGroup("##readOnly", &fixed, kToolNames, {.readOnly = true});
        row.Next();
        count(facet::ToggleButtonGroup("##mixed", &ui.mixedView, kTools, {.mixed = true}));
    }
    Note("Panels latched: %d of 3. View: %s. Text: %s. Writes on this section: %d.",
         ui.latches[0] + ui.latches[1] + ui.latches[2], ui.view >= 0 ? kTools[ui.view].label : "none",
         ui.textView >= 0 ? kToolNames[ui.textView] : "none", ui.toggleWrites);
}

// Stands in for the platform's shell during selftests: the address is recorded instead of opened,
// so a selftest never starts a browser.
bool RecordOpenedUrl(ImGuiContext*, const char* path)
{
    ui.openedUrl = path != nullptr ? path : "";
    return true;
}

void LinkSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Link");
    Paragraph("Accent text for navigation inside a sentence or a row. It underlines under the pointer and the "
              "keyboard focus; with an address it opens it through the platform and shows it as a tooltip.");
    if (ctx.probes != nullptr)
        ImGui::GetPlatformIO().Platform_OpenInShellFn = RecordOpenedUrl;

    LabeledRow("Links");
    {
        const ButtonRow row;
        if (ctx.capture && ctx.captureState == kToolsState)
            ctx.ForceNextItemState(ForcedItemState::Hovered);
        if (facet::Link("Placeholder link"))
            Pressed(&ui.links, "Placeholder link");
        ctx.Probe("button.link.plain");
        row.Next();
        if (facet::Link("Placeholder address", {.url = "about:blank", .visited = ui.linkVisited}))
        {
            ui.linkVisited = true;
            Pressed(&ui.links, "Placeholder address");
        }
        ctx.Probe("button.link.url");
    }
    LabeledRow("Sizes");
    {
        const ButtonRow row;
        facet::Link("Small link", {.size = facet::ControlSize::Small});
        row.Next();
        facet::Link("Regular link");
        row.Next();
        facet::Link("Large link", {.size = facet::ControlSize::Large});
    }
    LabeledRow("States");
    {
        const ButtonRow row;
        facet::Link("Visited link", {.visited = true});
        row.Next();
        ImGui::BeginDisabled();
        if (facet::Link("Disabled link"))
            Pressed(&ui.links, "Disabled link");
        ctx.Probe("button.link.disabled");
        ImGui::EndDisabled();
    }
    Note("Links activated: %d. The placeholder address %s.", ui.links,
         ui.linkVisited ? "is marked visited" : "has not been visited");
}

} // namespace

FACET_DEMO_PAGE("buttons", "Buttons", "Widgets", 10, "default", "hovered", "pressed", "focused", "loading", "tools",
                "toggles")
{
    facet::demo::PageHeader("Buttons",
                            "Push buttons with variants, icon buttons, copy buttons, links and toggle button "
                            "groups.");
    facet::demo::ReadyFamilyNote("buttons");

    ButtonSection(ctx);

    IconButtonSection(ctx);

    CopyButtonSection(ctx);

    LinkSection(ctx);

    ToggleGroupSection(ctx);
}

FACET_DEMO_SELFTEST("buttons")
{
    using T = facet::demo::Target;
    test.Do([] { ui = {}; });

    // Pointer: a click activates once; a disabled button never does.
    test.Click("button.save");
    test.ExpectNear("a click activates the primary button", [] { return ui.save; }, 1.0, 0.0);
    test.Click("button.delete");
    test.ExpectNear("a click activates the destructive button", [] { return ui.remove; }, 1.0, 0.0);
    test.Click("button.disabled");
    test.ExpectNear("the disabled button does not activate", [] { return ui.disabled; }, 0.0, 0.0);
    test.Click("button.large");
    test.ExpectText("the large button reports itself", [] { return std::string(ui.last != nullptr ? ui.last : ""); },
                    "Large");

    // Pressed on the button, released outside: nothing.
    test.MouseMove(T::Item("button.cancel"));
    test.MouseDown();
    test.MouseMove(T::Item("button.cancel", 0.5f, 4.0f));
    test.MouseUp();
    test.ExpectNear("a press released outside does not activate", [] { return ui.cancel; }, 0.0, 0.0);

    // Keyboard: after a click the first Tab shows the focus, the next moves it; Space and Enter activate.
    test.Click("button.save");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("button.save");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("button.duplicate");
    test.KeyPress(ImGuiKey_Space);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Space and Enter activate the focused button", [] { return ui.duplicate; }, 2.0, 0.0);

    // Tab passes over the disabled button.
    test.Click("button.settings");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("button.settings");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("button.upload");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("the icon slot button activates with Enter", [] { return ui.upload; }, 1.0, 0.0);

    // Loading: the button keeps its place and its focus but ignores activation.
    test.Click("button.loadingToggle");
    test.Expect("the Loading switch turns loading on", [] { return ui.loading; });
    test.Click("button.upload");
    test.ExpectNear("a loading button does not activate", [] { return ui.upload; }, 1.0, 0.0);
    test.Click("button.loadingToggle");
    test.Click("button.upload");
    test.ExpectNear("after loading it activates again", [] { return ui.upload; }, 2.0, 0.0);

    // A cut label shows in full in a tooltip on hover.
    test.Hover(T::Item("button.narrow"), 0.8f);
    test.Expect("the cut label shows its tooltip",
                [] { return ui.narrowTip; });
    test.Click("button.wide");
    test.ExpectNear("the full-width button activates", [] { return ui.wide; }, 1.0, 0.0);

    // IconButton: click, keys, latch, tooltip, loading, disabled.
    test.Click("button.icon.zoomIn");
    test.ExpectNear("a click activates the icon button", [] { return ui.zoom; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("button.icon.zoomIn");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("button.icon.zoomOut");
    test.KeyPress(ImGuiKey_Space);
    test.ExpectNear("Space activates the focused icon button", [] { return ui.zoom; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter latches the pin", [] { return ui.pinned; });
    test.Click("button.icon.lock");
    test.Expect("a click releases the lock", [] { return !ui.locked; });
    test.Hover(T::Item("button.icon.pin"), 0.8f);
    test.Expect("the icon button shows its name as a tooltip", [] { return ui.pinTip; });
    test.Click("button.icon.disabled");
    test.Click("button.icon.refresh");
    test.Expect("Refresh starts loading", [] { return ui.iconLoading; });
    test.Click("button.icon.refresh");
    test.ExpectNear("a loading icon button does not activate again", [] { return ui.iconPresses; }, 3.0, 0.0);
    test.Click("button.icon.stop");
    test.Expect("the plain button stops loading", [] { return !ui.iconLoading; });

    // CopyButton: the clipboard of the run receives the text, by click and by keyboard.
    test.Click("button.copy.icon");
    test.ExpectClipboard("a click copies the text", "Placeholder text to copy");
    test.Click("button.copy.secondary");
    test.ExpectClipboard("the secondary copy button copies its own text", "Placeholder line");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("button.copy.label");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectClipboard("Enter copies the text", "Placeholder text to copy");
    test.Click("button.copy.none");
    test.ExpectNear("the button without text copies nothing", [] { return ui.copies; }, 3.0, 0.0);

    // Link: click and Enter, the address goes to the platform hook (recorded here), disabled.
    test.Click("button.link.plain");
    test.ExpectNear("a click activates the link", [] { return ui.links; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("button.link.url");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectText("Enter opens the address through the platform hook", [] { return ui.openedUrl; }, "about:blank");
    test.Expect("and the caller marks it visited", [] { return ui.linkVisited; });
    test.Click("button.link.disabled");
    test.ExpectNear("the disabled link does not activate", [] { return ui.links; }, 2.0, 0.0);

    // ToggleButtonGroup: latches by click and Space, an exclusive choice, allowEmpty, the disabled item.
    test.MouseMove(T::Item("button.toggles.latches", 1.0f).Offset(-14.0f, 0.0f));  // the last button
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click latches the last tool", [] { return ui.latches[2]; });
    test.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space flips the focused tool", [] { return ui.latches[1]; });
    // The exclusive icon group: its square buttons fit at every width of the selftest.
    test.Do([] { ui.view = 2; });
    test.MouseMove(T::Item("button.toggles.view", 0.0f).Offset(14.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a click chooses the first view", [] { return ui.view; }, 0.0, 0.0);
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("pressing the chosen view again clears the choice (allowEmpty)", [] { return ui.view; }, -1.0, 0.0);
    test.MouseMove(T::Item("button.toggles.disabledItem", 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("the disabled item does not take the choice", [] { return ui.disabledItem; }, 0.0, 0.0);
    test.ExpectNear("each change wrote once", [] { return ui.toggleWrites; }, 4.0, 0.0);
}
