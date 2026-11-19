// Demo page: Inputs. Calls every widget of facet/widgets/inputs.h once, grouped by purpose.
// The widgets are planned, so each call draws its placeholder at the widget's default size;
// Phase 2 replaces them one by one without touching this page's layout.

#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct InputsUi
{
    std::string text;
    std::string secret;
    std::string query;
    std::string filter;
    facet::FilterSettings filterSettings;
    std::string completion;
    std::vector<std::string> tags = {"Option 1"};
    std::string notes;
    std::string name = "Option 1";
    bool renaming = false;
    int count = 3;
    float amount = 0.5f;
    double duration = 0.25;
    int durationUnit = 0;
    float vector[3] = {0.0f, 1.0f, 0.0f};
    ImVec2 size = {640.0f, 480.0f};
    std::uint32_t hex = 0;
    std::string masked;
    std::string pin;
    double seconds = 90.0;
    std::string path;
    facet::Keybind bind = {.primary = ImGuiKey_F5, .mode = facet::BindMode::Toggle};
    int rating = 3;
    bool stockCheck = false;
};

InputsUi ui;

const char* const kOptions[] = {"Option 1", "Option 2", "Option 3"};
const facet::UnitItem kTimeUnits[] = {{"ms", 0.001}, {"s", 1.0}, {"min", 60.0}};

} // namespace

FACET_DEMO_PAGE("inputs", "Inputs", "Widgets", 40)
{
    facet::demo::PageHeader("Inputs",
                            "Text, search and number fields, drag and vector fields, special-purpose fields, key "
                            "bind capture and ratings.");
    facet::demo::PlannedFamilyNote("inputs");

    SectionHeader("Text");
    LabeledRow("Text field");
    facet::TextField("##text", &ui.text, {.placeholder = "Placeholder", .clearButton = true});
    LabeledRow("Secure field");
    facet::SecureField("##secret", &ui.secret);
    LabeledRow("Multi-line");
    facet::TextArea("##notes", &ui.notes, {.counter = true, .maxLength = 200});
    LabeledRow("Rename in place");
    facet::InlineRename("##rename", &ui.name, &ui.renaming);

    SectionHeader("Search and suggestions");
    LabeledRow("Search field");
    facet::SearchField("##search", &ui.query, {.recent = kOptions});
    LabeledRow("Filter field");
    facet::FilterField("##filter", &ui.filter, &ui.filterSettings);
    LabeledRow("Autocomplete");
    facet::Autocomplete("##complete", &ui.completion, kOptions);
    LabeledRow("Tags");
    facet::TagInput("##tags", &ui.tags, {.suggestions = kOptions});

    SectionHeader("Numbers");
    Paragraph("Typed numbers are written on Enter, Tab or focus loss; steppers and drags write at once.");
    LabeledRow("Number");
    facet::NumberInput("##count", &ui.count, {.min = 0, .max = 10});
    LabeledRow("Drag number");
    facet::DragNumber("##amount", &ui.amount, {.min = 0.0, .max = 1.0, .speed = 0.005});
    LabeledRow("With units");
    facet::UnitInput("##duration", &ui.duration, &ui.durationUnit, kTimeUnits);
    LabeledRow("Vector");
    facet::VectorInput("##vector", std::span<float>(ui.vector), {.linkToggle = true});
    LabeledRow("Size");
    facet::SizeInput("##size", &ui.size);
    LabeledRow("Duration");
    facet::DurationInput("##seconds", &ui.seconds);

    SectionHeader("Formatted fields");
    LabeledRow("Hexadecimal");
    facet::HexInput("##hex", &ui.hex, {.prefix = true});
    LabeledRow("Masked");
    facet::MaskedInput("##masked", &ui.masked, "9999-9999", {.placeholder = "0"});
    LabeledRow("Code");
    facet::PinCodeInput("##pin", &ui.pin, {.groupSize = 3});
    LabeledRow("Path");
    facet::PathField("##path", &ui.path, {.placeholder = "Placeholder path"});
    LabeledRow("Copy field");
    facet::CopyField("##copy", "Sample text");

    SectionHeader("Keys and ratings");
    LabeledRow("Key bind");
    facet::KeybindCapture("##bind", &ui.bind, {.showMode = true});
    LabeledRow("Rating");
    facet::Rating("##rating", &ui.rating);

    SectionHeader("Validation");
    Paragraph("Messages for items without an error option, and a summary of the errors in a window.");
    facet::ValidationSummary("Errors");
    // A stock Dear ImGui checkbox (it works), with the planned message under it.
    LabeledRow("Stock control");
    ImGui::Checkbox("Option 1##stock", &ui.stockCheck);
    facet::FieldValidation("Placeholder message", {.tone = facet::Tone::Warning});
}
