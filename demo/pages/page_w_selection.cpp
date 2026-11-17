// Demo page: Selection. Calls every widget of facet/widgets/selection.h once, grouped by
// purpose. The widgets are planned, so each call draws its placeholder at the widget's default
// size; Phase 2 replaces them one by one without touching this page's layout.

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct SelectionUi
{
    bool toggle = true;
    bool check = false;
    facet::CheckState mixed = facet::CheckState::Mixed;
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

} // namespace

FACET_DEMO_PAGE("selection", "Selection", "Widgets", 20)
{
    facet::demo::PageHeader("Selection",
                            "Toggles, checkboxes and checkbox groups, radio groups, segmented controls, chips, "
                            "cycle selectors, option cards and the key bind mode selector. Every widget on this "
                            "page is a placeholder until it is implemented: it has the widget's default size, "
                            "cannot be operated and writes nothing.");

    SectionHeader("On and off");
    LabeledRow("Toggle");
    facet::Toggle("##toggle", &ui.toggle);
    LabeledRow("Checkbox");
    facet::Checkbox("Option 1##check", &ui.check);
    LabeledRow("Tri-state");
    facet::Checkbox("Option 2##mixed", &ui.mixed);
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
