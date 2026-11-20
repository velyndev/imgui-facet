// Demo page: Buttons. Button is implemented: its variants, sizes, icons, states and width rules,
// with capture states and a selftest. Every other widget of facet/widgets/buttons.h is called
// once below it; a planned widget draws its placeholder at its default size.

#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/popup_host.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
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

    // The planned widgets below.
    bool pinned = false;
    bool latches[3] = {true, false, false};
    int view = 0;
    int textView = 1;
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

    LabeledRow("Sizes");
    {
        const ButtonRow row;
        if (facet::Button("Small", {.size = facet::ControlSize::Small}))
            Pressed(&ui.sized, "Small");
        ctx.Probe("button.small");
        row.Next();
        if (facet::Button("Regular"))
            Pressed(&ui.sized, "Regular");
        ctx.Probe("button.regular");
        row.Next();
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
        ImGui::Checkbox("Loading", &ui.loading);
        ctx.Probe("button.loadingToggle");
    }
    ctx.RevealInCapture(0.5f);

    LabeledRow("Widths");
    {
        const ButtonRow row;
        // 120 units cannot hold this label: it ends in an ellipsis and shows in full on hover and focus.
        if (facet::Button("Export the selected layers", {.width = 120.0f}))
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

} // namespace

FACET_DEMO_PAGE("buttons", "Buttons", "Widgets", 10, "default", "hovered", "pressed", "focused", "loading")
{
    facet::demo::PageHeader("Buttons",
                            "Push buttons with variants, icon buttons, copy buttons, links and toggle button "
                            "groups.");
    facet::demo::PlannedFamilyNote("buttons");

    ButtonSection(ctx);

    SectionHeader("Icon buttons");
    LabeledRow("Icon button");
    facet::IconButton("Pin", facet::Icon("pin"), {.selected = ui.pinned});

    SectionHeader("Copy and links");
    LabeledRow("Copy button");
    facet::CopyButton("Copy sample text", "Sample text", {.iconOnly = false});
    LabeledRow("Link");
    facet::Link("Placeholder link", {.url = "about:blank"});

    SectionHeader("Toggle groups");
    Paragraph("Latched tool buttons: independent flags, or one choice at a time.");
    LabeledRow("Independent");
    facet::ToggleButtonGroup("Views", ui.latches, kTools);
    LabeledRow("Exclusive");
    facet::ToggleButtonGroup("View", &ui.view, kTools, {.iconOnly = false});
    LabeledRow("Text only");
    facet::ToggleButtonGroup("Text view", &ui.textView, kToolNames);
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
    test.Expect("the checkbox turns loading on", [] { return ui.loading; });
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
}
