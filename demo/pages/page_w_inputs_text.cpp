// Demo page: Text inputs. One section per text-like widget of facet/widgets/inputs.h (TextField,
// SecureField, SearchField, TextArea, Autocomplete, CopyField, FilterField, TagInput,
// InlineRename, MaskedInput, PinCodeInput, PathField, FieldValidation, ValidationSummary). A
// ready widget shows its variants, sizes and states with capture states and a selftest; a planned
// one draws its placeholder at the widget's default size. The number-like inputs have their own
// page (page_w_inputs_value.cpp). Texts are labelled samples and placeholders, not data.

#include <string>
#include <vector>

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

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kHovered,
    kFocused,
    kFilled,
};

constexpr const char* kSample = "Sample text";
constexpr const char* kSampleLong = "A longer sample line that runs past the end of the field";

// Edit sessions of one field, counted from the item queries after its call.
struct Sessions
{
    int writes = 0;
    int commits = 0;
    int cancels = 0;

    void Count(bool written)
    {
        writes += written ? 1 : 0;
        commits += facet::IsItemEditCommitted() ? 1 : 0;
        cancels += facet::IsItemEditCanceled() ? 1 : 0;
    }
};

struct TextInputsUi
{
    // TextField
    std::string text;
    std::string named;
    std::string file;
    std::string limited;
    std::string validated;
    std::string loading = kSample;
    std::string mixed;
    std::string readOnly = "Read-only sample text: select it and copy it";
    std::string disabled = kSample;
    std::string small;
    std::string regular;
    std::string large;
    std::string narrow;
    Sessions textSessions;

    // The planned widgets below.
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

// The texts a capture of the filled state shows.
void FillSamples()
{
    ui.text = kSample;
    ui.named = kSample;
    ui.file = kSample;
    ui.limited = "Twelve chars";
    ui.validated = "ab";
    ui.mixed.clear();
    ui.small = kSample;
    ui.regular = kSample;
    ui.large = kSample;
    ui.narrow = kSampleLong;
}

void TextFieldSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("TextField");
    Paragraph("Single-line text. Typing writes at once; Enter or leaving the field ends the edit, Escape restores the "
              "text from when the field took the keyboard.");

    LabeledRow("Placeholder");
    if (ctx.capture && ctx.captureState == kHovered)
        ctx.ForceNextItemState(ForcedItemState::Hovered);
    else if (ctx.capture && ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    ui.textSessions.Count(facet::TextField("##text", &ui.text, {.placeholder = "Placeholder", .clearButton = true}));
    ctx.Probe("inputs.text");
    if (ctx.capture && (ctx.captureState == kHovered || ctx.captureState == kFocused))
        ctx.RevealInCapture(0.3f);

    LabeledRow("Label in front");
    facet::TextField("Name##named", &ui.named, {.placeholder = "Placeholder"});
    ctx.Probe("inputs.text.named");

    LabeledRow("Icon");
    facet::TextField("##file", &ui.file, {.placeholder = "File name placeholder", .icon = facet::Icon("file")});
    ctx.Probe("inputs.text.icon");

    LabeledRow("Length limit");
    facet::TextField("##limited", &ui.limited, {.placeholder = "Placeholder", .maxLength = 12,
                                                .helpText = "Up to 12 characters"});
    ctx.Probe("inputs.text.limited");

    LabeledRow("Validation");
    const bool tooShort = !ui.validated.empty() && ui.validated.size() < 3;
    facet::TextField("##validated", &ui.validated,
                     {.placeholder = "At least three characters",
                      .error = tooShort ? "Use at least three characters" : nullptr,
                      .helpText = "The caller checks the text and passes the message"});
    ctx.Probe("inputs.text.validated");

    LabeledRow("Loading");
    facet::TextField("##loading", &ui.loading, {.loading = true});

    LabeledRow("Mixed");
    facet::TextField("##mixed", &ui.mixed, {.mixed = ui.mixed.empty()});
    ctx.Probe("inputs.text.mixed");

    LabeledRow("Read-only");
    facet::TextField("##readonly", &ui.readOnly, {.readOnly = true});
    ctx.Probe("inputs.text.readonly");

    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    facet::TextField("##disabled", &ui.disabled);
    ImGui::EndDisabled();
    ctx.Probe("inputs.text.disabled");

    LabeledRow("Sizes");
    facet::TextField("##small", &ui.small, {.placeholder = "Small", .size = facet::ControlSize::Small});
    LabeledRow("");
    facet::TextField("##regular", &ui.regular, {.placeholder = "Regular"});
    LabeledRow("");
    facet::TextField("##large", &ui.large, {.placeholder = "Large", .size = facet::ControlSize::Large});

    LabeledRow("Narrow");
    // 160 units: the label takes at most half and ends in an ellipsis, the text scrolls inside.
    facet::TextField("A label longer than its share##narrow", &ui.narrow, {.placeholder = "Placeholder", .width = 160.0f});
    ctx.Probe("inputs.text.narrow");
    if (ctx.capture && ctx.captureState == kFilled)
        ctx.RevealInCapture(0.6f);

    Note("Placeholder field: %d writes, %d committed edits, %d canceled.", ui.textSessions.writes,
         ui.textSessions.commits, ui.textSessions.cancels);
}

void PlannedSections(facet::demo::PageCtx& ctx)
{
    SectionHeader("Other text fields");
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

} // namespace

FACET_DEMO_PAGE("inputs-text", "Text inputs", "Widgets", 40, "default", "hovered", "focused", "filled")
{
    if (ctx.capture)
    {
        ui = {};
        if (ctx.captureState == kFilled)
            FillSamples();
    }
    facet::demo::PageHeader("Text inputs",
                            "Text, secure and multi-line fields, search, filter and suggestion fields, tags, "
                            "formatted fields, and validation messages.");
    facet::demo::PlannedFamilyNote("inputs");

    TextFieldSection(ctx);
    PlannedSections(ctx);
}

FACET_DEMO_SELFTEST("inputs-text")
{
    using T = facet::demo::Target;
    test.Do([] { ui = {}; });

    // TextField: typing, selection with Shift and the arrows, the clipboard, undo, Enter, Escape.
    test.Click("inputs.text");
    test.ExpectFocused("inputs.text");
    test.Type("hello world");
    test.ExpectText("typing writes per keystroke", [] { return ui.text; }, "hello world");
    for (int i = 0; i < 5; ++i)
        test.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("Shift+Left five times and Ctrl+C copy the last word", "world");
    test.KeyPress(ImGuiKey_End);
    test.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    test.ExpectText("Ctrl+Left moves a word, Ctrl+V pastes there", [] { return ui.text; }, "hello worldworld");
    test.KeyPress(ImGuiKey_Z | ImGuiMod_Ctrl);
    test.ExpectText("Ctrl+Z undoes the paste", [] { return ui.text; }, "hello world");
    test.KeyPress(ImGuiKey_Y | ImGuiMod_Ctrl);
    test.ExpectText("Ctrl+Y redoes it", [] { return ui.text; }, "hello worldworld");
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectText("Escape restores the text from activation", [] { return ui.text; }, "");
    test.ExpectNear("Escape reports one canceled edit", [] { return ui.textSessions.cancels; }, 1.0, 0.0);
    test.ExpectNear("and no committed one", [] { return ui.textSessions.commits; }, 0.0, 0.0);
    test.Click("inputs.text");
    test.Type("kept");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Enter commits the edit once", [] { return ui.textSessions.commits; }, 1.0, 0.0);
    test.ExpectText("the committed text stays", [] { return ui.text; }, "kept");

    // The clear button at the end of the frame empties it and gives the keyboard back.
    test.MouseMove(T::Item("inputs.text", 1.0f, 0.5f).Offset(-14.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectText("the clear button empties the field", [] { return ui.text; }, "");
    test.ExpectNear("as one more committed edit", [] { return ui.textSessions.commits; }, 2.0, 0.0);
    test.Wait(2);
    test.Type("again");
    test.ExpectText("the keyboard went back to the field", [] { return ui.text; }, "again");

    // Tab moves on: the next field takes the keyboard, focus loss commits.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.text.named");
    test.ExpectNear("focus loss by Tab commits the edit", [] { return ui.textSessions.commits; }, 3.0, 0.0);
    test.Type("Typed name");
    test.ExpectText("the labelled field takes the text", [] { return ui.named; }, "Typed name");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.text.icon");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.text.limited");
    test.Type("0123456789abcdef");
    test.ExpectText("the length limit stops typing at 12 characters", [] { return ui.limited; }, "0123456789ab");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.text.validated");
    test.Type("ab");
    test.ExpectText("the validated field holds the short text", [] { return ui.validated; }, "ab");
    test.KeyPress(ImGuiKey_Enter);

    // Mixed shows the mixed placeholder until the first edit.
    test.Click("inputs.text.mixed");
    test.Type("one");
    test.ExpectText("a mixed field takes a definite text", [] { return ui.mixed; }, "one");
    test.KeyPress(ImGuiKey_Enter);

    // Read-only: select and copy, never written.
    test.Click("inputs.text.readonly");
    test.Type("xyz");
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("read-only text can be copied", "Read-only sample text: select it and copy it");
    test.ExpectText("and is never written", [] { return ui.readOnly; }, "Read-only sample text: select it and copy it");
    test.KeyPress(ImGuiKey_Escape);

    // Disabled: a click does nothing, Tab skips it.
    test.Click("inputs.text.disabled");
    test.Type("x");
    test.ExpectText("the disabled field takes no text", [] { return ui.disabled; }, kSample);

    // A double click selects the word under the pointer (past the text: the last word).
    test.Do([] { ui.file = "first second"; });
    test.DoubleClick("inputs.text.icon");
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("a double click selects a word", "second");
    test.KeyPress(ImGuiKey_Escape);

    // The stock checkbox with the planned message under it works.
    test.Click("inputs.stock");
    test.Expect("the stock checkbox under a message turns on", [] { return ui.stockCheck; });
}
