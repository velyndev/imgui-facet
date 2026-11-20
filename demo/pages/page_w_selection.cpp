// Demo page: Selection. Each implemented widget of facet/widgets/selection.h has a section with its
// variants, sizes and states, capture states for its important looks and selftest steps that
// operate it by mouse and keyboard. A planned widget is called once and draws its placeholder.

#include <string>

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

struct SelectionUi
{
    // Toggle
    bool wifi = true;
    bool bluetooth = false;
    bool sizes[3] = {false, true, true};
    bool wide = true;
    bool loading = false;
    bool mixedToggle = false;
    bool errorToggle = false;
    int toggleWrites = 0;

    // Checkbox
    bool checks[4] = {true, false, false, true};
    facet::CheckState triState = facet::CheckState::Mixed;
    bool checkSizes[3] = {true, true, true};
    bool checkError = false;
    int checkWrites = 0;

    // Planned widgets below.
    bool group[3] = {true, false, true};
    int radio = 0;
    int segment = 1;
    bool chips[3] = {true, false, false};
    int chipChoice = 0;
    int cycle = 0;
    facet::BindMode mode = facet::BindMode::Hold;
    int card = 0;
    bool cards[2] = {false, true};
};

SelectionUi ui;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kHovered,
    kPressed,
    kFocused,
};

const char* const kOptions[] = {"Option 1", "Option 2", "Option 3"};
const facet::ChoiceItem kChoices[] = {
    {"Option 1", facet::Icon("sun"), "Placeholder detail"},
    {"Option 2", facet::Icon("moon"), "Placeholder detail"},
    {"Option 3", facet::Icon("monitor"), "Placeholder detail"},
};
const facet::OptionCard kCards[] = {
    {"Option 1", "Placeholder description of the first choice.", facet::Icon("grid")},
    {"Option 2", "Placeholder description of the second choice.", facet::Icon("list")},
};

// Asks the next widget for the look of the page's capture state.
void ForceState(facet::demo::PageCtx& ctx, int part = -1)
{
    if (!ctx.capture)
        return;
    if (ctx.captureState == kHovered)
        ctx.ForceNextItemState(ForcedItemState::Hovered, part);
    else if (ctx.captureState == kPressed)
        ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Pressed, part);
    else if (ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused, part);
}

// Places controls side by side in a regular row, one under the other when the row is compact.
struct ControlRow
{
    bool stack = facet::AvailableWidthClass() == facet::WidthClass::Compact;
    float column = ImGui::GetCursorPosX();
    void Next() const
    {
        if (stack)
            ImGui::SetCursorPosX(column);
        else
            ImGui::SameLine(0.0f, facet::Px(facet::T().spacing.xl));
    }
};

void ToggleSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Toggle");
    Paragraph("An on/off switch for a setting that applies at once. A click on the label or the switch, Space or "
              "Enter flips it; the knob can also be dragged.");

    LabeledRow("On and off");
    {
        const ControlRow row;
        ForceState(ctx);
        if (facet::Toggle("Wi-Fi", &ui.wifi))
            ++ui.toggleWrites;
        ctx.Probe("sel.toggle.wifi");
        row.Next();
        if (facet::Toggle("Bluetooth", &ui.bluetooth))
            ++ui.toggleWrites;
        ctx.Probe("sel.toggle.bluetooth");
    }
    ctx.RevealInCapture(0.3f);

    LabeledRow("Sizes");
    {
        const ControlRow row;
        facet::Toggle("Small", &ui.sizes[0], {.size = facet::ControlSize::Small});
        ctx.Probe("sel.toggle.small");
        row.Next();
        facet::Toggle("Regular", &ui.sizes[1]);
        row.Next();
        facet::Toggle("Large", &ui.sizes[2], {.size = facet::ControlSize::Large});
    }

    LabeledRow("Full width");
    facet::Toggle("The switch sits at the end of a wide row", &ui.wide, {.width = facet::kFill});
    ctx.Probe("sel.toggle.wide");

    LabeledRow("States");
    {
        const ControlRow row;
        ImGui::BeginDisabled();
        bool disabledOn = true;
        bool disabledOff = false;
        facet::Toggle("Disabled", &disabledOn);
        ctx.Probe("sel.toggle.disabled");
        row.Next();
        facet::Toggle("Disabled##off", &disabledOff);
        ImGui::EndDisabled();
        row.Next();
        bool readOnly = true;
        facet::Toggle("Read-only", &readOnly, {.readOnly = true});
        ctx.Probe("sel.toggle.readOnly");
        row.Next();
        facet::Toggle("Loading", &ui.loading, {.loading = true});
        ctx.Probe("sel.toggle.loading");
        row.Next();
        if (facet::Toggle("Mixed", &ui.mixedToggle, {.mixed = true}))
            ++ui.toggleWrites;
        ctx.Probe("sel.toggle.mixed");
    }
    LabeledRow("Messages");
    {
        const ControlRow row;
        facet::Toggle("Placeholder setting", &ui.errorToggle,
                      {.error = ui.errorToggle ? nullptr : "Placeholder error: turn this on to clear it."});
        ctx.Probe("sel.toggle.error");
        row.Next();
        bool help = false;
        facet::Toggle("Another setting", &help, {.helpText = "Placeholder help text under the switch."});
    }
    Note("Wi-Fi %s, Bluetooth %s. Writes on this section: %d.", ui.wifi ? "on" : "off", ui.bluetooth ? "on" : "off",
         ui.toggleWrites);
}

void CheckboxSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Checkbox");
    Paragraph("A box for an option that is on or off. The label is part of the target; dragging from one box "
              "across others gives them all the first box's new value.");

    LabeledRow("Options");
    {
        static const char* const kNames[] = {"Option 1", "Option 2", "Option 3", "Option 4"};
        static const char* const kProbes[] = {"sel.check.0", "sel.check.1", "sel.check.2", "sel.check.3"};
        const ControlRow row;
        for (int i = 0; i < 4; ++i)
        {
            if (i > 0)
                row.Next();
            if (i == 1)
                ForceState(ctx);
            if (facet::Checkbox(kNames[i], &ui.checks[i]))
                ++ui.checkWrites;
            ctx.Probe(kProbes[i]);
        }
    }

    LabeledRow("Tri-state");
    {
        const ControlRow row;
        if (facet::Checkbox("Partly chosen", &ui.triState))
            ++ui.checkWrites;
        ctx.Probe("sel.check.tri");
        row.Next();
        if (facet::Button("Set mixed again", {.variant = facet::ButtonVariant::Plain, .size = facet::ControlSize::Small}))
            ui.triState = facet::CheckState::Mixed;
        ctx.Probe("sel.check.reset");
    }

    LabeledRow("Sizes");
    {
        const ControlRow row;
        facet::Checkbox("Small", &ui.checkSizes[0], {.size = facet::ControlSize::Small});
        row.Next();
        facet::Checkbox("Regular", &ui.checkSizes[1]);
        row.Next();
        facet::Checkbox("Large", &ui.checkSizes[2], {.size = facet::ControlSize::Large});
    }

    LabeledRow("States");
    {
        const ControlRow row;
        bool on = true;
        bool off = false;
        facet::CheckState partly = facet::CheckState::Mixed;
        ImGui::BeginDisabled();
        facet::Checkbox("Disabled", &on);
        ctx.Probe("sel.check.disabled");
        row.Next();
        facet::Checkbox("Disabled##off", &off);
        row.Next();
        facet::Checkbox("Disabled##mixed", &partly);
        ImGui::EndDisabled();
        row.Next();
        facet::Checkbox("Read-only", &on, {.readOnly = true});
        row.Next();
        facet::Checkbox("Read-only##mixed", &partly, {.readOnly = true});
    }
    LabeledRow("Messages");
    {
        const ControlRow row;
        facet::Checkbox("Placeholder agreement", &ui.checkError,
                        {.error = ui.checkError ? nullptr : "Placeholder error: check the box to clear it."});
        ctx.Probe("sel.check.error");
        row.Next();
        bool help = false;
        facet::Checkbox("Another option", &help, {.helpText = "Placeholder help text under the box."});
    }
    ctx.RevealInCapture(0.5f);
    const int checked = ui.checks[0] + ui.checks[1] + ui.checks[2] + ui.checks[3];
    Note("Options checked: %d of 4. Tri-state: %s. Writes on this section: %d.", checked,
         ui.triState == facet::CheckState::On ? "on" : ui.triState == facet::CheckState::Off ? "off" : "mixed",
         ui.checkWrites);
}

} // namespace

FACET_DEMO_PAGE("selection", "Selection", "Widgets", 20, "default", "hovered", "pressed", "focused")
{
    facet::demo::PageHeader("Selection",
                            "Toggles, checkboxes and checkbox groups, radio groups, segmented controls, chips, "
                            "cycle selectors, option cards and the key bind mode selector.");
    facet::demo::PlannedFamilyNote("selection");

    ToggleSection(ctx);

    CheckboxSection(ctx);

    SectionHeader("Checkbox groups");
    LabeledRow("Checkbox group");
    facet::CheckboxGroup("##group", ui.group, kOptions, {.selectAll = true});

    SectionHeader("One of several");
    Paragraph("Exclusive choices, from a few labelled options to a compact strip.");
    LabeledRow("Radio group");
    facet::RadioGroup("##radio", &ui.radio, kChoices);
    LabeledRow("Segmented");
    facet::SegmentedControl("##segment", &ui.segment, kOptions);
    LabeledRow("Cycle selector");
    facet::CycleSelector("##cycle", &ui.cycle, kOptions, {.pageDots = true});
    LabeledRow("Bind mode");
    facet::KeybindModeSelector("##mode", &ui.mode);

    SectionHeader("Chips");
    LabeledRow("Chip");
    facet::Chip("Option 1##chip", {.selected = true, .removable = true});
    LabeledRow("Filter chips");
    facet::ChipGroup("##filters", ui.chips, kChoices, {.clearAll = true});
    LabeledRow("Choice chips");
    facet::ChipGroup("##choice", &ui.chipChoice, kOptions);

    SectionHeader("Option cards");
    Paragraph("Large cards for choices that need a description.");
    facet::OptionCardGroup("Single choice", &ui.card, kCards);
    ImGui::Spacing();
    facet::OptionCardGroup("Several choices", ui.cards, kCards);
}

FACET_DEMO_SELFTEST("selection")
{
    using T = facet::demo::Target;
    test.Do([] { ui = {}; });

    // Toggle: a click flips it, Space and Enter flip the focused one; Tab passes the disabled ones.
    test.Click("sel.toggle.bluetooth");
    test.Expect("a click turns the toggle on", [] { return ui.bluetooth; });
    test.Click("sel.toggle.wifi");
    test.Expect("a click turns the other toggle off", [] { return !ui.wifi; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("sel.toggle.wifi");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space flips the focused toggle", [] { return ui.wifi; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("sel.toggle.bluetooth");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter flips the focused toggle", [] { return !ui.bluetooth; });
    test.ExpectNear("each flip wrote once", [] { return ui.toggleWrites; }, 4.0, 0.0);
    test.Drag({T::Item("sel.toggle.small", 0.75f), T::Item("sel.toggle.small", 1.0f)}, 0.25f);
    test.Expect("dragging the knob to the end turns it on", [] { return ui.sizes[0]; });
    test.Click("sel.toggle.disabled");
    test.Click("sel.toggle.readOnly");
    test.Click("sel.toggle.loading");
    test.Expect("disabled, read-only and loading toggles do not change", [] { return !ui.loading; });
    test.Click("sel.toggle.mixed");
    test.Expect("the first edit of a mixed toggle writes on", [] { return ui.mixedToggle; });
    // The probe covers the message line too; press on the switch row at the top.
    test.MouseMove(T::Item("sel.toggle.error", 0.5f, 0.2f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("turning the toggle on clears its placeholder error", [] { return ui.errorToggle; });

    // Checkbox: click, keys, painting, tri-state, disabled.
    test.Click("sel.check.1");
    test.Expect("a click checks the box", [] { return ui.checks[1]; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("sel.check.1");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space unchecks the focused box", [] { return !ui.checks[1]; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("sel.check.2");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter checks the focused box", [] { return ui.checks[2]; });
    test.Drag({T::Item("sel.check.0", 0.1f), T::Item("sel.check.1", 0.1f), T::Item("sel.check.2", 0.1f),
               T::Item("sel.check.3", 0.1f)},
              0.4f);
    test.Expect("dragging from a checked box unchecks every box it crosses",
                [] { return !ui.checks[0] && !ui.checks[1] && !ui.checks[2] && !ui.checks[3]; });
    test.Click("sel.check.tri");
    test.Expect("a click on a mixed box writes on", [] { return ui.triState == facet::CheckState::On; });
    test.Click("sel.check.tri");
    test.Expect("and the next one off", [] { return ui.triState == facet::CheckState::Off; });
    test.Click("sel.check.reset");
    test.Expect("the plain button sets mixed again", [] { return ui.triState == facet::CheckState::Mixed; });
    const int writes = 8;
    test.Click("sel.check.disabled");
    test.ExpectNear("the disabled box does not write", [] { return ui.checkWrites; }, writes, 0.0);
    test.MouseMove(T::Item("sel.check.error", 0.1f, 0.2f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("checking the box clears its placeholder error", [] { return ui.checkError; });
}
