// Demo page: Selection. Each implemented widget of facet/widgets/selection.h has a section with its
// variants, sizes and states, capture states for its important looks and selftest steps that
// operate it by mouse and keyboard. A planned widget is called once and draws its placeholder.

#include <cstdio>
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

    // RadioGroup
    int radio = 0;
    int radioRow = 1;
    int radioDisabled = 0;
    int radioError = -1;
    int radioWrites = 0;

    // SegmentedControl
    int segment = 1;
    int segmentIcons = 0;
    int segmentIconOnly = 2;
    int segmentFill = 0;
    int segmentSizes[3] = {0, 1, 2};
    int segmentDisabled = 0;
    int segmentMixed = 0;
    int segmentNarrow = 1;
    int segmentWrites = 0;

    // Chip
    bool tags[4] = {true, true, true, true};  // which removable tags are still there
    bool filterChip = false;
    int chipPresses = 0;
    int chipRemoves = 0;

    // ChipGroup
    bool chips[3] = {true, false, false};
    int chipChoice = 0;
    bool groupTags[5] = {false, true, false, false, true};
    bool groupPresent[5] = {true, true, true, true, true};
    bool singleLine[6] = {false, false, true, false, false, false};
    int chipGroupWrites = 0;
    int chipGroupRemoved = -1;

    // CheckboxGroup
    bool group[3] = {true, false, true};
    bool columns[6] = {true, false, false, true, false, false};
    bool groupError[3] = {false, false, false};
    int checkGroupWrites = 0;

    // Planned widgets below.
    int cycle = 0;
    facet::BindMode mode = facet::BindMode::Hold;
    int card = 0;
    bool cards[2] = {false, true};
};

SelectionUi ui;

// The capture states, in the order of the FACET_DEMO_PAGE line below. The first four show the top
// of the page (the toggles) at rest, hovered, pressed and focused; each later one scrolls its section
// to the top of the image and shows its main widget focused and another one hovered.
enum CaptureState
{
    kDefault,
    kHovered,
    kPressed,
    kFocused,
    kCheckboxes,
    kRadios,
    kSegments,
    kChips,
    kChipGroups,
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

// Asks the next widget for the look of the page's capture state: in the top states for the toggle
// (`section` kDefault), and in a section's own state the focused look for its main widget.
void ForceState(facet::demo::PageCtx& ctx, int section, int part = -1)
{
    if (!ctx.capture)
        return;
    if (section == kDefault && ctx.captureState == kHovered)
        ctx.ForceNextItemState(ForcedItemState::Hovered, part);
    else if (section == kDefault && ctx.captureState == kPressed)
        ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Pressed, part);
    else if (section == kDefault && ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused, part);
    else if (section != kDefault && ctx.captureState == section)
        ctx.ForceNextItemState(ForcedItemState::Focused, part);
}

// The hovered look for a second widget of a section, in the section's capture state.
void ForceHover(facet::demo::PageCtx& ctx, int section, int part = -1)
{
    if (ctx.capture && ctx.captureState == section)
        ctx.ForceNextItemState(ForcedItemState::Hovered, part);
}

// In a section's capture state, scrolls the section heading to the top of the image.
void RevealSection(facet::demo::PageCtx& ctx, int section)
{
    if (ctx.capture && ctx.captureState == section)
        ctx.RevealInCapture(0.04f);
}

// Records "<probe>.<i>" for the first `count` entries of the group widget `label` just submitted,
// by the id the widget gives entry i (its index hashed with the widget id), so a selftest can check
// which entry has the focus. The rectangle is the whole widget's.
void ProbeEntries(facet::demo::PageCtx& ctx, const char* label, const char* probe, int count)
{
    if (ctx.probes == nullptr)
        return;
    const ImGuiID widget = ImGui::GetID(label);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    char name[64];
    for (int i = 0; i < count; ++i)
    {
        std::snprintf(name, sizeof(name), "%s.%d", probe, i);
        facet::demo::ProbeRect(ctx, name, ImHashData(&i, sizeof(i), widget), min, max);
    }
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
        ForceState(ctx, kDefault);
        if (facet::Toggle("Wi-Fi", &ui.wifi))
            ++ui.toggleWrites;
        ctx.Probe("sel.toggle.wifi");
        row.Next();
        if (facet::Toggle("Bluetooth", &ui.bluetooth))
            ++ui.toggleWrites;
        ctx.Probe("sel.toggle.bluetooth");
    }

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
    RevealSection(ctx, kCheckboxes);
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
                ForceState(ctx, kCheckboxes);
            if (i == 2)
                ForceHover(ctx, kCheckboxes);
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
        facet::Checkbox("Small##check", &ui.checkSizes[0], {.size = facet::ControlSize::Small});
        row.Next();
        facet::Checkbox("Regular##check", &ui.checkSizes[1]);
        row.Next();
        facet::Checkbox("Large##check", &ui.checkSizes[2], {.size = facet::ControlSize::Large});
    }

    LabeledRow("States");
    {
        const ControlRow row;
        bool on = true;
        bool off = false;
        facet::CheckState partly = facet::CheckState::Mixed;
        ImGui::BeginDisabled();
        facet::Checkbox("Disabled##check", &on);
        ctx.Probe("sel.check.disabled");
        row.Next();
        facet::Checkbox("Disabled##checkOff", &off);
        row.Next();
        facet::Checkbox("Disabled##checkMixed", &partly);
        ImGui::EndDisabled();
        row.Next();
        facet::Checkbox("Read-only##check", &on, {.readOnly = true});
        row.Next();
        facet::Checkbox("Read-only##checkMixed", &partly, {.readOnly = true});
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
    const int checked = ui.checks[0] + ui.checks[1] + ui.checks[2] + ui.checks[3];
    Note("Options checked: %d of 4. Tri-state: %s. Writes on this section: %d.", checked,
         ui.triState == facet::CheckState::On ? "on" : ui.triState == facet::CheckState::Off ? "off" : "mixed",
         ui.checkWrites);
}

void CheckboxGroupSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("CheckboxGroup");
    Paragraph("A labelled list of checkboxes, each a Tab stop. The select-all box above the list shows the mixed "
              "mark while only some items are checked and sets every enabled item.");

    static const char* const kSix[] = {"Option 1", "Option 2", "Option 3", "Option 4", "Option 5", "Option 6"};
    static const facet::ChoiceItem kWithDisabled[] = {{"Option 1"}, {"Option 2 (disabled)", {}, nullptr, nullptr, true}, {"Option 3"}};
    const auto count = [](bool written) {
        if (written)
            ++ui.checkGroupWrites;
    };

    LabeledRow("Select all");
    count(facet::CheckboxGroup("Placeholder list", ui.group, kWithDisabled, {.selectAll = true}));
    ctx.Probe("sel.checks.all");
    LabeledRow("Columns");
    count(facet::CheckboxGroup("##columns", ui.columns, kSix, {.columns = 3}));
    ctx.Probe("sel.checks.columns");
    LabeledRow("Messages");
    {
        const bool any = ui.groupError[0] || ui.groupError[1] || ui.groupError[2];
        count(facet::CheckboxGroup("##errors", ui.groupError, kOptions,
                                   {.error = any ? nullptr : "Placeholder error: check at least one option.",
                                    .helpText = "Placeholder help text under the list."}));
    }
    {
        bool fixed[3] = {true, false, true};
        LabeledRow("Read-only");
        facet::CheckboxGroup("##checksReadOnly", fixed, kOptions, {.readOnly = true});
        LabeledRow("Disabled");
        ImGui::BeginDisabled();
        facet::CheckboxGroup("##checksDisabled", fixed, kOptions);
        ImGui::EndDisabled();
    }
    Note("Placeholder list: %d of 2 enabled options checked. Writes on this section: %d.", ui.group[0] + ui.group[2],
         ui.checkGroupWrites);
}

void RadioSection(facet::demo::PageCtx& ctx)
{
    RevealSection(ctx, kRadios);
    SectionHeader("RadioGroup");
    Paragraph("One choice out of a few that all stay visible. The group is one Tab stop; the arrow keys move the "
              "selection and wrap at the ends, Home and End jump to the first and last option.");

    static const facet::ChoiceItem kWithDisabled[] = {
        {"Option 1"},
        {"Option 2 (disabled)", {}, nullptr, nullptr, true},
        {"Option 3"},
    };

    LabeledRow("Vertical");
    ForceState(ctx, kRadios, 1);
    if (facet::RadioGroup("Placeholder choice", &ui.radio, kChoices))
        ++ui.radioWrites;
    ctx.Probe("sel.radio");
    ProbeEntries(ctx, "Placeholder choice", "sel.radio", 3);
    LabeledRow("Horizontal");
    ForceHover(ctx, kRadios, 2);
    if (facet::RadioGroup("##row", &ui.radioRow, kOptions, {.orientation = facet::Orientation::Horizontal}))
        ++ui.radioWrites;
    ctx.Probe("sel.radio.row");
    ProbeEntries(ctx, "##row", "sel.radio.row", 3);
    LabeledRow("Disabled option");
    if (facet::RadioGroup("##disabled", &ui.radioDisabled, kWithDisabled, {.orientation = facet::Orientation::Horizontal}))
        ++ui.radioWrites;
    ctx.Probe("sel.radio.disabled");
    ProbeEntries(ctx, "##disabled", "sel.radio.disabled", 3);
    {
        int fixed = 2;
        LabeledRow("Read-only");
        facet::RadioGroup("##radioReadOnly", &fixed, kOptions,
                          {.orientation = facet::Orientation::Horizontal, .readOnly = true});
        LabeledRow("Disabled");
        ImGui::BeginDisabled();
        facet::RadioGroup("##radioDisabled", &fixed, kOptions, {.orientation = facet::Orientation::Horizontal});
        ImGui::EndDisabled();
    }
    LabeledRow("Messages");
    if (facet::RadioGroup("##error", &ui.radioError, kOptions,
                          {.orientation = facet::Orientation::Horizontal,
                           .error = ui.radioError < 0 ? "Placeholder error: choose one option." : nullptr,
                           .helpText = "Placeholder help text under the group."}))
        ++ui.radioWrites;
    ctx.Probe("sel.radio.error");
    Note("Vertical: %s. Horizontal: %s. Writes on this section: %d.", kChoices[ui.radio].label, kOptions[ui.radioRow],
         ui.radioWrites);
}

void SegmentedSection(facet::demo::PageCtx& ctx)
{
    RevealSection(ctx, kSegments);
    SectionHeader("SegmentedControl");
    Paragraph("A compact one-of-several choice. The selection pill slides to the chosen segment; a press, a drag "
              "across the segments or Left and Right move it, and the control is one Tab stop.");

    static const facet::ChoiceItem kWithDisabled[] = {
        {"Option 1"},
        {"Option 2 (disabled)", {}, nullptr, nullptr, true},
        {"Option 3"},
    };
    const auto count = [](bool written) {
        if (written)
            ++ui.segmentWrites;
    };

    LabeledRow("Labels");
    ForceState(ctx, kSegments, 2);
    count(facet::SegmentedControl("##labels", &ui.segment, kOptions));
    ctx.Probe("sel.seg");
    ProbeEntries(ctx, "##labels", "sel.seg", 3);
    LabeledRow("Icons and labels");
    ForceHover(ctx, kSegments, 1);
    count(facet::SegmentedControl("##icons", &ui.segmentIcons, kChoices));
    ctx.Probe("sel.seg.icons");
    ProbeEntries(ctx, "##icons", "sel.seg.icons", 3);
    LabeledRow("Icons only");
    count(facet::SegmentedControl("##iconOnly", &ui.segmentIconOnly, kChoices, {.iconOnly = true}));
    ctx.Probe("sel.seg.iconOnly");
    LabeledRow("Full width");
    count(facet::SegmentedControl("##fill", &ui.segmentFill, kOptions, {.width = facet::kFill}));
    ctx.Probe("sel.seg.fill");
    LabeledRow("Sizes");
    {
        const ControlRow row;
        count(facet::SegmentedControl("##small", &ui.segmentSizes[0], kOptions, {.size = facet::ControlSize::Small}));
        row.Next();
        count(facet::SegmentedControl("##regular", &ui.segmentSizes[1], kOptions));
        row.Next();
        count(facet::SegmentedControl("##large", &ui.segmentSizes[2], kOptions, {.size = facet::ControlSize::Large}));
    }
    LabeledRow("With a label");
    count(facet::SegmentedControl("Placeholder setting", &ui.segmentDisabled, kWithDisabled));
    ctx.Probe("sel.seg.disabled");
    ProbeEntries(ctx, "Placeholder setting", "sel.seg.disabled", 3);
    LabeledRow("States");
    {
        const ControlRow row;
        int fixed = 1;
        facet::SegmentedControl("##readOnly", &fixed, kOptions, {.readOnly = true});
        row.Next();
        ImGui::BeginDisabled();
        facet::SegmentedControl("##disabledAll", &fixed, kOptions);
        ImGui::EndDisabled();
        row.Next();
        count(facet::SegmentedControl("##mixed", &ui.segmentMixed, kOptions, {.mixed = true}));
        ctx.Probe("sel.seg.mixed");
    }
    LabeledRow("Narrow");
    {
        const ControlRow row;
        // 150 units hold neither set of labels: icons replace them where every item has one,
        // and labels are cut where they do not.
        count(facet::SegmentedControl("##narrowIcons", &ui.segmentNarrow, kChoices, {.width = 150.0f}));
        row.Next();
        count(facet::SegmentedControl("##narrowLabels", &ui.segmentNarrow, kOptions, {.width = 150.0f}));
    }
    Note("Labels: %s. Writes on this section: %d.", kOptions[ui.segment], ui.segmentWrites);
}

void ChipSection(facet::demo::PageCtx& ctx)
{
    RevealSection(ctx, kChips);
    SectionHeader("Chip");
    Paragraph("A small pill for a tag or a filter. Activating it flips a filter; the remove button, or Delete and "
              "Backspace on the focused chip, asks the caller to remove it.");

    LabeledRow("Removable");
    {
        static const char* const kTags[] = {"Tag 1", "Tag 2", "Tag 3", "Tag 4"};
        static const char* const kProbes[] = {"sel.chip.tag0", "sel.chip.tag1", "sel.chip.tag2", "sel.chip.tag3"};
        const float right = facet::demo::ContentRight();
        bool first = true;
        for (int i = 0; i < 4; ++i)
        {
            if (!ui.tags[i])
                continue;
            if (!first)
                facet::demo::SameLineIfFits(right, facet::Px(80.0f), facet::Px(facet::T().spacing.sm));
            first = false;
            ImGui::PushID(i);
            if (facet::Chip(kTags[i], {.removable = true}))
                ++ui.chipPresses;
            ctx.Probe(kProbes[i]);
            const bool removed = facet::ChipRemovePressed();
            ImGui::PopID();
            if (removed)
            {
                ui.tags[i] = false;
                ++ui.chipRemoves;
            }
        }
        if (!first)
            ImGui::SameLine(0.0f, facet::Px(facet::T().spacing.lg));
        if (facet::Button("Restore tags", {.variant = facet::ButtonVariant::Plain, .size = facet::ControlSize::Small}))
            for (bool& tag : ui.tags)
                tag = true;
        ctx.Probe("sel.chip.restore");
    }
    LabeledRow("Filter");
    {
        const ControlRow row;
        ForceState(ctx, kChips);
        if (facet::Chip("Placeholder filter", {.selected = ui.filterChip, .icon = facet::Icon("filter")}))
        {
            ui.filterChip = !ui.filterChip;
            ++ui.chipPresses;
        }
        ctx.Probe("sel.chip.filter");
        row.Next();
        ForceHover(ctx, kChips);
        facet::Chip("Series 1", {.color = facet::ToU32(facet::T().colors.chart[0])});
        row.Next();
        facet::Chip("Series 2", {.color = facet::ToU32(facet::T().colors.chart[1])});
    }
    LabeledRow("Sizes");
    {
        const ControlRow row;
        facet::Chip("Small##chip", {.removable = true, .size = facet::ControlSize::Small});
        row.Next();
        facet::Chip("Regular##chip", {.removable = true});
        row.Next();
        facet::Chip("Large##chip", {.removable = true, .size = facet::ControlSize::Large});
    }
    LabeledRow("States");
    {
        const ControlRow row;
        ImGui::BeginDisabled();
        facet::Chip("Disabled##chip", {.removable = true});
        ctx.Probe("sel.chip.disabled");
        row.Next();
        facet::Chip("Disabled and selected", {.selected = true});
        ImGui::EndDisabled();
    }
    Note("Filter %s. Chips activated: %d, removed: %d.", ui.filterChip ? "on" : "off", ui.chipPresses, ui.chipRemoves);
}

void ChipGroupSection(facet::demo::PageCtx& ctx)
{
    RevealSection(ctx, kChipGroups);
    SectionHeader("ChipGroup");
    Paragraph("Chips that wrap into rows: filters that each flip a flag, or one choice that the selection pill marks. "
              "Each chip is a Tab stop.");

    static const char* const kSeries[] = {"Series 1", "Series 2", "Series 3", "Series 4", "Series 5", "Series 6"};
    const auto count = [](bool written) {
        if (written)
            ++ui.chipGroupWrites;
    };

    LabeledRow("Filters");
    ForceState(ctx, kChipGroups, 1);
    count(facet::ChipGroup("Placeholder filters", ui.chips, kChoices, {.clearAll = true}));
    ctx.Probe("sel.chips.filters");
    ProbeEntries(ctx, "Placeholder filters", "sel.chips.filters", 3);
    LabeledRow("One choice");
    ForceHover(ctx, kChipGroups, 2);
    count(facet::ChipGroup("##choice", &ui.chipChoice, kOptions));
    ctx.Probe("sel.chips.choice");
    ProbeEntries(ctx, "##choice", "sel.chips.choice", 3);
    LabeledRow("Removable");
    {
        // The removable group lists only the series still present; the indices map back.
        const char* present[5];
        bool flags[5];
        int map[5];
        int shown = 0;
        for (int i = 0; i < 5; ++i)
            if (ui.groupPresent[i])
            {
                present[shown] = kSeries[i];
                flags[shown] = ui.groupTags[i];
                map[shown] = i;
                ++shown;
            }
        const float column = ImGui::GetCursorPosX();
        count(facet::ChipGroup("##removable", std::span<bool>(flags, static_cast<std::size_t>(shown)),
                               std::span<const char* const>(present, static_cast<std::size_t>(shown)), {.removable = true}));
        ctx.Probe("sel.chips.removable");
        ProbeEntries(ctx, "##removable", "sel.chips.removable", shown);
        const int removed = facet::ChipGroupRemovedItem();
        for (int k = 0; k < shown; ++k)
            ui.groupTags[map[k]] = flags[k];
        if (removed >= 0)
        {
            ui.groupPresent[map[removed]] = false;
            ui.chipGroupRemoved = map[removed];
        }
        ImGui::SetCursorPosX(column);
        if (facet::Button("Restore series", {.variant = facet::ButtonVariant::Plain, .size = facet::ControlSize::Small}))
            for (bool& p : ui.groupPresent)
                p = true;
        ctx.Probe("sel.chips.restore");
    }
    LabeledRow("Single line");
    count(facet::ChipGroup("##single", ui.singleLine, kSeries, {.singleLine = true, .width = 300.0f}));
    ctx.Probe("sel.chips.single");
    {
        bool fixed[3] = {true, false, true};
        LabeledRow("Read-only");
        facet::ChipGroup("##chipsReadOnly", fixed, kOptions, {.readOnly = true});
        LabeledRow("Disabled");
        ImGui::BeginDisabled();
        facet::ChipGroup("##chipsDisabled", fixed, kOptions);
        ImGui::EndDisabled();
    }
    Note("Filters on: %d of 3. Choice: %s. Writes on this section: %d.", ui.chips[0] + ui.chips[1] + ui.chips[2],
         kOptions[ui.chipChoice], ui.chipGroupWrites);
}

} // namespace

FACET_DEMO_PAGE("selection", "Selection", "Widgets", 20, "default", "hovered", "pressed", "focused", "checkboxes",
                "radios", "segments", "chips", "chipgroups")
{
    facet::demo::PageHeader("Selection",
                            "Toggles, checkboxes and checkbox groups, radio groups, segmented controls, chips, "
                            "cycle selectors, option cards and the key bind mode selector.");
    facet::demo::PlannedFamilyNote("selection");

    ToggleSection(ctx);

    CheckboxSection(ctx);

    CheckboxGroupSection(ctx);

    RadioSection(ctx);

    SegmentedSection(ctx);

    SectionHeader("One of several");
    Paragraph("Exclusive choices, from a few labelled options to a compact strip.");
    LabeledRow("Cycle selector");
    facet::CycleSelector("##cycle", &ui.cycle, kOptions, {.pageDots = true});
    LabeledRow("Bind mode");
    facet::KeybindModeSelector("##mode", &ui.mode);

    ChipSection(ctx);

    ChipGroupSection(ctx);

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

    // CheckboxGroup: the select-all box, an item by keyboard, an item in the second row of columns.
    test.MouseMove(T::Item("sel.checks.all", 0.0f, 0.0f).Offset(8.0f, 31.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("select-all clears both enabled options", [] { return !ui.group[0] && !ui.group[2]; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space checks the first option", [] { return ui.group[0] && !ui.group[2]; });
    // On through the third option to the columns: the first box of their second row is the fourth stop.
    for (int i = 0; i < 5; ++i)
        test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the first box of the second row unchecks it", [] { return !ui.columns[3]; });
    test.ExpectNear("each change wrote once", [] { return ui.checkGroupWrites; }, 3.0, 0.0);

    // RadioGroup: a click on the middle option, then the arrows inside one Tab stop.
    test.MouseMove(T::Item("sel.radio", 0.01f, 0.56f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a click selects the middle option", [] { return ui.radio; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectNear("Down selects the next option", [] { return ui.radio; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectNear("and wraps to the first", [] { return ui.radio; }, 0.0, 0.0);
    test.ExpectFocused("sel.radio.0");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("sel.radio.row.1");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right selects the next option of a row", [] { return ui.radioRow; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("sel.radio.disabled.0");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("the disabled option is skipped", [] { return ui.radioDisabled; }, 2.0, 0.0);
    test.ExpectFocused("sel.radio.disabled.2");
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("sel.radio.row.2");
    test.ExpectNear("each move wrote once", [] { return ui.radioWrites; }, 5.0, 0.0);

    // SegmentedControl: press, drag, arrows inside one Tab stop, the disabled segment.
    test.MouseMove(T::Item("sel.seg", 0.9f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a press selects the last segment", [] { return ui.segment; }, 2.0, 0.0);
    test.Drag({T::Item("sel.seg", 0.85f), T::Item("sel.seg", 0.5f), T::Item("sel.seg", 0.1f)}, 0.3f);
    test.ExpectNear("a drag across moves the selection to the first", [] { return ui.segment; }, 0.0, 0.0);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right selects the next segment", [] { return ui.segment; }, 1.0, 0.0);
    test.ExpectFocused("sel.seg.1");
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End selects the last", [] { return ui.segment; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("and Right stops there", [] { return ui.segment; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("sel.seg.icons.0");
    test.Do([] { ui.segmentDisabled = 0; });
    test.MouseMove(T::Item("sel.seg.disabled", 0.98f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a press on the last segment of the labelled control", [] { return ui.segmentDisabled; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("Left skips the disabled segment", [] { return ui.segmentDisabled; }, 0.0, 0.0);
    test.ExpectFocused("sel.seg.disabled.0");
    test.MouseMove(T::Item("sel.seg.mixed", 0.15f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("the first press on a mixed control writes", [] { return ui.segmentWrites; }, 8.0, 0.0);

    // Chip: the body flips a filter, the remove button and Delete remove a tag.
    test.Click("sel.chip.filter");
    test.Expect("a click on the filter chip turns it on", [] { return ui.filterChip; });
    test.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus on the chip
    test.ExpectFocused("sel.chip.filter");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the focused chip turns it off", [] { return !ui.filterChip; });
    test.MouseMove(T::Item("sel.chip.tag1", 1.0f).Offset(-14.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("the remove button removes the second tag", [] { return !ui.tags[1] && ui.tags[0]; });
    test.MouseMove(T::Item("sel.chip.tag0", 0.25f));
    test.MouseDown();
    test.MouseUp();
    test.KeyPress(ImGuiKey_Delete);
    test.Expect("Delete on the focused chip removes the first tag", [] { return !ui.tags[0]; });
    test.ExpectNear("two chips were removed and three activated", [] { return ui.chipRemoves * 10 + ui.chipPresses; }, 23.0,
                    0.0);
    test.Click("sel.chip.disabled");
    test.Click("sel.chip.restore");
    test.Expect("the plain button restores the tags", [] { return ui.tags[0] && ui.tags[1]; });

    // ChipGroup: a click and the keys on filters, the clear chip, a choice and a removal.
    // The first chip: 10 units in from the left, in the middle of the first row under the label line.
    test.MouseMove(T::Item("sel.chips.filters", 0.0f, 0.0f).Offset(10.0f, 31.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click flips the first filter off", [] { return !ui.chips[0]; });
    test.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus
    test.ExpectFocused("sel.chips.filters.0");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("sel.chips.filters.1");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space flips the second filter on", [] { return ui.chips[1]; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("the clear chip turns every filter off", [] { return !ui.chips[0] && !ui.chips[1] && !ui.chips[2]; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("sel.chips.choice.0");
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.ExpectNear("Space chooses the second chip", [] { return ui.chipChoice; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("sel.chips.removable.0");
    test.KeyPress(ImGuiKey_Delete);
    test.Expect("Delete removes the focused series", [] { return !ui.groupPresent[0] && ui.chipGroupRemoved == 0; });
    test.ExpectNear("each change wrote once", [] { return ui.chipGroupWrites; }, 4.0, 0.0);
    test.Click("sel.chips.restore");
    test.Expect("the plain button restores the series", [] { return ui.groupPresent[0]; });
}
