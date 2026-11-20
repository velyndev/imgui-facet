// Demo page: Text inputs. Calls every text-like widget of facet/widgets/inputs.h once (TextField,
// SecureField, SearchField, TextArea, Autocomplete, CopyField, FilterField, TagInput,
// InlineRename, MaskedInput, PinCodeInput, PathField, FieldValidation, ValidationSummary),
// grouped by purpose. The number-like inputs have their own page (page_w_inputs_value.cpp). A
// planned widget draws its placeholder at the widget's default size; implementing it changes
// nothing in this page's layout.

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

struct TextInputsUi
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
    std::string masked;
    std::string pin;
    std::string path;
    bool stockCheck = false;
};

TextInputsUi ui;

const char* const kOptions[] = {"Option 1", "Option 2", "Option 3"};

} // namespace

FACET_DEMO_PAGE("inputs-text", "Text inputs", "Widgets", 40)
{
    facet::demo::PageHeader("Text inputs",
                            "Text, secure and multi-line fields, search, filter and suggestion fields, tags, "
                            "formatted fields, and validation messages.");
    facet::demo::PlannedFamilyNote("inputs");

    SectionHeader("Text");
    LabeledRow("Text field");
    facet::TextField("##text", &ui.text, {.placeholder = "Placeholder", .clearButton = true});
    ctx.Probe("inputs.text");
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

    SectionHeader("Formatted fields");
    LabeledRow("Masked");
    facet::MaskedInput("##masked", &ui.masked, "9999-9999", {.placeholder = "0"});
    LabeledRow("Code");
    facet::PinCodeInput("##pin", &ui.pin, {.groupSize = 3});
    LabeledRow("Path");
    facet::PathField("##path", &ui.path, {.placeholder = "Placeholder path"});
    LabeledRow("Copy field");
    facet::CopyField("##copy", "Sample text");

    SectionHeader("Validation");
    Paragraph("Messages for items without an error option, and a summary of the errors in a window.");
    facet::ValidationSummary("Errors");
    // A stock Dear ImGui checkbox (it works), with the planned message under it.
    LabeledRow("Stock control");
    ImGui::Checkbox("Option 1##stock", &ui.stockCheck);
    ctx.Probe("inputs.stock");
    facet::FieldValidation("Placeholder message", {.tone = facet::Tone::Warning});
}

FACET_DEMO_SELFTEST("inputs-text")
{
    test.Do([] { ui = {}; });
    // The planned fields are placeholders: typing into one writes nothing.
    test.Click("inputs.text");
    test.Type("abc");
    test.ExpectText("the text field placeholder writes nothing", [] { return ui.text; }, "");
    // The stock checkbox with the message under it works.
    test.Click("inputs.stock");
    test.Expect("the stock checkbox under a message turns on", [] { return ui.stockCheck; });
}
