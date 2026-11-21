// Demo page: Text inputs. One section per text-like widget of facet/widgets/inputs.h (TextField,
// SecureField, SearchField, TextArea, Autocomplete, CopyField, FilterField, TagInput,
// InlineRename, MaskedInput, PinCodeInput, PathField, FieldValidation, ValidationSummary). A
// ready widget shows its variants, sizes and states with capture states and a selftest; a planned
// one draws its placeholder at the widget's default size. The number-like inputs have their own
// page (page_w_inputs_value.cpp). Texts are labelled samples and placeholders, not data.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/control.h"
#include "internal/list_view.h"
#include "internal/popup_host.h"
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
    kOpen,
    kSuggestions,
    kFormatted,  // the TagInput and FilterField sections
    kLater,      // InlineRename and MaskedInput
    kCodes,      // PinCodeInput and the sections after it
};

constexpr const char* kSample = "Sample text";
constexpr const char* kSampleLong = "A longer sample line that runs past the end of the field";
constexpr const char* kSampleLongCopy =
    "A longer sample value that wraps over several lines in a multi-line copy field, so the whole of it stays "
    "visible and can be selected line by line or copied at once with the button at the end.";

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

    // SecureField
    std::string secret;
    std::string secretHold;
    std::string secretNone;
    std::string secretStrength;
    std::string secretDisabled = kSample;

    // SearchField
    std::string query;
    std::string queryLoading = "Sample query";
    std::string querySmall;

    // TextArea
    std::string notes;
    std::string notesResizable;
    std::string notesValidated;
    std::string notesReadOnly = "Read-only sample text\non two lines";

    // Autocomplete
    std::string completion;
    std::string completionInline;
    std::string completionPrefix;
    std::string completionLoading;
    std::string completionError;
    int completionPicked = -1;

    // The planned widgets below.
    std::string filter;
    facet::FilterSettings filterSettings;
    int filterShown = 0;
    std::string filterSmall;
    std::string filterReadOnly = "Sample query";
    std::vector<std::string> tags = {"Option 1"};
    std::vector<std::string> tagsLimited;
    std::vector<std::string> tagsWrap = {"Sample tag 1", "Sample tag 2", "Sample tag 3", "Sample tag 4"};
    std::vector<std::string> tagsError;
    std::vector<std::string> tagsReadOnly = {"Sample tag 1", "Sample tag 2"};
    std::string name = "Option 1";
    bool renaming = false;
    int renames = 0;
    std::string fileName = "sample-file.txt";
    std::string fileName2 = "second-sample-file.txt";
    bool renamingFile = false;
    std::string nameError = "Sample name";
    std::string nameReadOnly = "Read-only sample name";
    std::string nameNarrow = "A sample name longer than its width";
    std::string masked;
    std::string maskedLiterals;
    std::string maskedError;
    std::string maskedReadOnly = "12345678";
    std::string pin;
    std::string pinMasked;
    std::string pinLetters;
    std::string pinError;
    std::string pinReadOnly = "42";
    std::string pinNarrow = "1234";
    std::string path;

    // CopyField
    int copies = 0;

    // FieldValidation and ValidationSummary
    std::string validName;
    int validPort = 0;
    bool stockCheck = false;
    int summaryActivations = 0;
};

TextInputsUi ui;

const char* const kOptions[] = {"Option 1", "Option 2", "Option 3"};
const char* const kRecent[] = {"Sample query 1", "Sample query 2", "Sample query 3"};

// Records the frame of the last field (its first control height, without the meter and message
// lines under it) for the selftest, so a click lands in the text at every size.
void ProbeFrame(facet::demo::PageCtx& ctx, const char* name, facet::ControlSize size = facet::ControlSize::Regular)
{
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    facet::demo::ProbeRect(ctx, name, ImGui::GetItemID(), min, ImVec2(max.x, min.y + facet::ControlHeight(size)));
}

// Records the first `count` rows of the open completion list of field `fieldId` as
// "<prefix>.<row>", where the text inputs place them: list rows inside the level's padding.
void ProbeCompletionRows(facet::demo::PageCtx& ctx, ImGuiID fieldId, const char* prefix, int count)
{
    const ImGuiID level = ImGui::GetIDWithSeed("##completion", nullptr, fieldId);
    if (!facet::internal::IsPopupLevelOpen(level))
        return;
    const ImRect rect = facet::internal::PopupLevelRect(level);
    const float pad = facet::Px(facet::T().spacing.xs);
    const float row = facet::internal::ListRowHeight(facet::ControlSize::Regular);
    for (int i = 0; i < count; ++i)
    {
        char name[64];
        std::snprintf(name, sizeof(name), "%s.%d", prefix, i);
        const float top = rect.Min.y + pad + row * static_cast<float>(i);
        facet::demo::ProbeRect(ctx, name, 0, ImVec2(rect.Min.x + pad, top), ImVec2(rect.Max.x - pad, top + row));
    }
}

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
    ui.secret = kSample;
    ui.secretHold = kSample;
    ui.secretNone = kSample;
    ui.secretStrength = "Sample";
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
    ProbeFrame(ctx, "inputs.text.limited");

    LabeledRow("Validation");
    const bool tooShort = !ui.validated.empty() && ui.validated.size() < 3;
    facet::TextField("##validated", &ui.validated,
                     {.placeholder = "At least three characters",
                      .error = tooShort ? "Use at least three characters" : nullptr,
                      .helpText = "The caller checks the text and passes the message"});
    ProbeFrame(ctx, "inputs.text.validated");

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

void SecureFieldSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("SecureField");
    Paragraph("Masked text. The eye shows the text until the focus leaves the field; copy and cut stay off either "
              "way. A hint appears under the focused field while letters arrive in upper case without Shift.");

    LabeledRow("Show and hide");
    // The capture of the hovered state rests the pointer on the eye button (part 1).
    if (ctx.capture && ctx.captureState == kHovered)
        ctx.ForceNextItemState(ForcedItemState::Hovered, 1);
    const ImGuiID secretId = ImGui::GetID("##secret");
    facet::SecureField("##secret", &ui.secret, {.placeholder = "Password placeholder"});
    ctx.Probe("inputs.secure");
    {
        // The eye button: the rightmost slot button, as the text field core places it.
        const float gap = facet::Px(facet::T().spacing.xs);
        const float side = facet::ControlHeight(facet::ControlSize::Regular) - 2.0f * gap;
        const ImVec2 max = ImGui::GetItemRectMax();
        const float centerY = (ImGui::GetItemRectMin().y + max.y) * 0.5f;
        facet::demo::ProbeRect(ctx, "inputs.secure.reveal", ImGui::GetIDWithSeed("reveal", nullptr, secretId),
                               ImVec2(max.x - gap - side, centerY - side * 0.5f), ImVec2(max.x - gap, centerY + side * 0.5f));
    }

    LabeledRow("Hold to show");
    facet::SecureField("##secretHold", &ui.secretHold,
                       {.placeholder = "Password placeholder", .reveal = facet::SecureReveal::Hold});
    ctx.Probe("inputs.secure.hold");

    LabeledRow("Always masked");
    facet::SecureField("##secretNone", &ui.secretNone,
                       {.placeholder = "Password placeholder", .reveal = facet::SecureReveal::None});
    ctx.Probe("inputs.secure.none");

    LabeledRow("Strength");
    // Placeholder rule for the demo: the meter follows the length up to 16 characters.
    const float strength = static_cast<float>(ui.secretStrength.size()) / 16.0f;
    const bool tooShort = !ui.secretStrength.empty() && ui.secretStrength.size() < 6;
    facet::SecureField("##secretStrength", &ui.secretStrength,
                       {.placeholder = "Password placeholder",
                        .strength = strength,
                        .error = tooShort ? "Use at least six characters" : nullptr,
                        .helpText = "Placeholder rule: the meter follows the length"});
    ProbeFrame(ctx, "inputs.secure.strength");

    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    facet::SecureField("##secretDisabled", &ui.secretDisabled);
    ImGui::EndDisabled();
}

// The shortcut that focuses the search field, registered once like an app would.
facet::ShortcutId SearchFocusShortcut()
{
    static const facet::ShortcutId id = facet::RegisterShortcut(
        "inputs.search.focus", ImGuiMod_Ctrl | ImGuiKey_F,
        {.label = "Focus the search field", .group = "Text inputs page", .scope = facet::ShortcutScope::Window});
    return id;
}

void SearchFieldSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("SearchField");
    Paragraph("A query with a clear button. Escape empties a query, a second Escape leaves the field. The empty "
              "focused field lists recent entries: Up and Down pick one, Enter or a click takes it.");

    LabeledRow("Recent entries");
    if (ctx.capture && ctx.captureState == kOpen)
        ctx.ForceNextItemState(ForcedItemState::Open);
    const ImGuiID searchId = ImGui::GetID("##search");
    facet::SearchField("##search", &ui.query, {.recent = kRecent, .focusShortcut = SearchFocusShortcut()});
    ProbeFrame(ctx, "inputs.search");
    ProbeCompletionRows(ctx, searchId, "inputs.search.recent", 3);
    if (ctx.capture && ctx.captureState == kOpen)
        ctx.RevealInCapture(0.3f);
    Note("Ctrl+F focuses this field. Query: \"%s\".", ui.query.c_str());

    LabeledRow("Loading");
    facet::SearchField("##searchLoading", &ui.queryLoading, {.loading = true});

    LabeledRow("Small");
    facet::SearchField("##searchSmall", &ui.querySmall, {.size = facet::ControlSize::Small});
    ProbeFrame(ctx, "inputs.search.small", facet::ControlSize::Small);
}

void TextAreaSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("TextArea");
    Paragraph("Several lines with the label above. Enter starts a line, Ctrl+Enter ends the edit. The area grows with "
              "its text between a minimum and a maximum number of lines, then scrolls.");

    if (ctx.capture && ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    facet::TextArea("Notes", &ui.notes,
                    {.placeholder = "Placeholder: several lines", .minLines = 3, .maxLines = 6, .counter = true,
                     .maxLength = 200});
    ctx.Probe("inputs.area");

    facet::TextArea("Resizable##resizable", &ui.notesResizable,
                    {.placeholder = "Drag the bar under the area to change its height", .minLines = 2,
                     .resizable = true, .helpText = "Arrow keys reach the bar; Up and Down change the height"});
    ctx.Probe("inputs.area.resizable");

    const bool tooShort = !ui.notesValidated.empty() && ui.notesValidated.size() < 10;
    facet::TextArea("Validated##validated", &ui.notesValidated,
                    {.minLines = 2, .error = tooShort ? "Write at least ten characters" : nullptr});
    ctx.Probe("inputs.area.validated");

    facet::TextArea("Read-only##readonly", &ui.notesReadOnly, {.minLines = 2, .readOnly = true});
}

// Suggestions named after the built-in icons they show, so the list has icons and details that
// mean something without inventing data.
const facet::ChoiceItem kIconSuggestions[] = {
    {"Calendar", facet::Icon("calendar"), "Icon"}, {"Clock", facet::Icon("clock"), "Icon"},
    {"Download", facet::Icon("download"), "Icon"}, {"Folder", facet::Icon("folder"), "Icon"},
    {"Keyboard", facet::Icon("keyboard"), "Icon"}, {"Layers", facet::Icon("layers"), "Icon"},
    {"Palette", facet::Icon("palette"), "Icon"},   {"Settings", facet::Icon("settings"), "Icon"},
    {"Sliders", facet::Icon("sliders"), "Icon"},   {"Upload", facet::Icon("upload"), "Icon"},
};

void AutocompleteSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Autocomplete");
    Paragraph("Suggestions under the field while it has the keyboard. Up, Down, Page Up and Page Down move, Enter, Tab "
              "or a click takes a suggestion, Escape closes the list and a second Escape restores the text.");

    LabeledRow("Fuzzy");
    if (ctx.capture && ctx.captureState == kSuggestions)
        ctx.ForceNextItemState(ForcedItemState::Open);
    const ImGuiID fuzzyId = ImGui::GetID("##complete");
    facet::Autocomplete("##complete", &ui.completion, kIconSuggestions, {.placeholder = "Type an icon name"});
    if (facet::AutocompleteAcceptedItem() >= 0)
        ui.completionPicked = facet::AutocompleteAcceptedItem();
    ProbeFrame(ctx, "inputs.complete");
    ProbeCompletionRows(ctx, fuzzyId, "inputs.complete.row", 3);
    if (ctx.capture && ctx.captureState == kSuggestions)
        ctx.RevealInCapture(0.2f);
    if (ui.completionPicked >= 0)
        Note("Last suggestion taken: %s.", kIconSuggestions[ui.completionPicked].label);
    else
        Note("No suggestion taken yet.");

    LabeledRow("Inline, strict");
    facet::Autocomplete("##completeInline", &ui.completionInline, kOptions,
                        {.placeholder = "Option 1, 2 or 3", .inlineCompletion = true, .strict = true,
                         .helpText = "Tab or Right takes the grey rest; other text is undone"});
    ProbeFrame(ctx, "inputs.complete.inline");

    LabeledRow("Prefix");
    facet::Autocomplete("##completePrefix", &ui.completionPrefix, kOptions, {.fuzzy = false});
    ProbeFrame(ctx, "inputs.complete.prefix");

    LabeledRow("Loading");
    facet::Autocomplete("##completeLoading", &ui.completionLoading, kOptions, {.state = facet::DataState::Loading});
    ProbeFrame(ctx, "inputs.complete.loading");

    LabeledRow("Load failed");
    facet::Autocomplete("##completeError", &ui.completionError, kOptions, {.state = facet::DataState::Error});
}

void PlannedSections(facet::demo::PageCtx&)
{


    SectionHeader("Formatted fields");
    LabeledRow("Path");
    facet::PathField("##path", &ui.path, {.placeholder = "Placeholder path"});

}

// Records the slot button `index` (0 the rightmost) of the field just drawn at the Regular size,
// as the text field core places it, under `name` with id `id`.
void ProbeSlotButton(facet::demo::PageCtx& ctx, const char* name, ImGuiID id, int index)
{
    const float gap = facet::Px(facet::T().spacing.xs);
    const float side = facet::ControlHeight(facet::ControlSize::Regular) - 2.0f * gap;
    const ImVec2 min = ImGui::GetItemRectMin();
    const float right = ImGui::GetItemRectMax().x - gap - (side + gap) * static_cast<float>(index);
    const float centerY = min.y + facet::ControlHeight(facet::ControlSize::Regular) * 0.5f;
    facet::demo::ProbeRect(ctx, name, id, ImVec2(right - side, centerY - side * 0.5f), ImVec2(right, centerY + side * 0.5f));
}

void CopyFieldSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("CopyField");
    Paragraph("A value to select and copy. The button copies the whole value and shows a check for a moment; Ctrl+C "
              "with nothing selected does the same.");

    LabeledRow("Plain");
    if (ctx.capture && ctx.captureState == kHovered)
        ctx.ForceNextItemState(ForcedItemState::Hovered, 1);
    const ImGuiID copyId = ImGui::GetID("##copy");
    if (facet::CopyField("##copy", "Sample text to copy"))
        ++ui.copies;
    ProbeFrame(ctx, "inputs.copy");
    ProbeSlotButton(ctx, "inputs.copy.button", ImGui::GetIDWithSeed("copy", nullptr, copyId), 0);

    LabeledRow("Masked");
    if (facet::CopyField("Key##copyMasked", "Placeholder secret value",
                         {.masked = true, .helpText = "The copy is the real text, the field shows dots"}))
        ++ui.copies;
    ProbeFrame(ctx, "inputs.copy.masked");

    LabeledRow("Several lines");
    if (facet::CopyField("##copyLines", kSampleLongCopy, {.multiline = true}))
        ++ui.copies;

    LabeledRow("Loading");
    facet::CopyField("##copyLoading", "Sample text to copy", {.loading = true});

    LabeledRow("Error");
    facet::CopyField("##copyError", "Sample text to copy", {.error = "Placeholder error: the value is out of date"});

    LabeledRow("Narrow");
    facet::CopyField("A label longer than its share##copyNarrow", kSampleLong, {.width = 160.0f});
    Note("Copies: %d.", ui.copies);
}

void FilterFieldSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("FilterField");
    Paragraph("A query that filters a list the caller draws, here the names of the built-in icons from the "
              "Autocomplete section. Aa matches case, ab matches whole words; Escape empties the query.");

    LabeledRow("With toggles");
    if (ctx.capture && ctx.captureState == kFormatted)
        ctx.ForceNextItemState(ForcedItemState::Hovered, 2);
    facet::FilterField("##filter", &ui.filter, &ui.filterSettings);
    ProbeFrame(ctx, "inputs.filter");
    ProbeSlotButton(ctx, "inputs.filter.case", ImGui::GetIDWithSeed("matchCase", nullptr, ImGui::GetItemID()), 1);
    // The matching names on one line, joined into a fixed buffer.
    char names[256] = {};
    int length = 0;
    int shown = 0;
    for (const facet::ChoiceItem& item : kIconSuggestions)
    {
        if (!facet::PassesFilter(ui.filter, ui.filterSettings, item.label))
            continue;
        const int written = std::snprintf(names + length, sizeof(names) - static_cast<std::size_t>(length), "%s%s",
                                          shown > 0 ? ", " : "", item.label);
        length = std::min(static_cast<int>(sizeof(names)) - 1, length + std::max(0, written));
        ++shown;
    }
    ui.filterShown = shown;
    LabeledRow("Matches");
    if (shown == 0)
        Note("No icon name matches the query.");
    else
        facet::Text(facet::TextStyle::Body, "%s", names);

    LabeledRow("Small, no toggles");
    facet::FilterField("##filterSmall", &ui.filterSmall, nullptr, {.size = facet::ControlSize::Small});

    LabeledRow("Read-only");
    facet::FilterField("##filterReadOnly", &ui.filterReadOnly, &ui.filterSettings, {.readOnly = true});
}

void InlineRenameSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("InlineRename");
    Paragraph("A name that turns into a field in place: double click it, or focus it and press F2. The edit works on a "
              "copy: Enter or a click elsewhere writes it once, Escape drops it.");

    LabeledRow("Double click or F2");
    if (ctx.capture && ctx.captureState == kLater)
        ctx.ForceNextItemState(ForcedItemState::Hovered);
    if (facet::InlineRename("rename", &ui.name, &ui.renaming))
        ++ui.renames;
    ctx.Probe("inputs.rename");
    if (ctx.capture && ctx.captureState == kLater)
        ctx.RevealInCapture(0.15f);

    LabeledRow("Name and extension");
    facet::InlineRename("renameFile", &ui.fileName, nullptr, {.selectStem = true});
    ctx.Probe("inputs.rename.file");
    LabeledRow("");
    if (facet::Button("Rename the file", {.size = facet::ControlSize::Small}))
        ui.renamingFile = true;
    LabeledRow("");
    facet::InlineRename("renameFromButton", &ui.fileName2, &ui.renamingFile, {.selectStem = true, .width = 240.0f});
    ctx.Probe("inputs.rename.button");

    LabeledRow("Error");
    facet::InlineRename("renameError", &ui.nameError, nullptr,
                        {.error = ui.nameError.empty() ? "A name cannot be empty" : nullptr});

    LabeledRow("Read-only");
    facet::InlineRename("renameReadOnly", &ui.nameReadOnly, nullptr, {.readOnly = true});

    LabeledRow("Narrow");
    facet::InlineRename("renameNarrow", &ui.nameNarrow, nullptr, {.width = 120.0f});
    Note("Names written: %d. Current: \"%s\".", ui.renames, ui.name.c_str());
}

void MaskedInputSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("MaskedInput");
    Paragraph("Text that follows a mask: 9 takes a digit, A a letter, * any character, and every other character is "
              "a literal the field adds by itself. The rest of the mask shows after the text.");

    LabeledRow("Digits");
    facet::MaskedInput("##masked", &ui.masked, "9999-9999", {.placeholder = "0", .helpText = "Mask 9999-9999"});
    ProbeFrame(ctx, "inputs.masked");

    LabeledRow("With literals");
    facet::MaskedInput("##maskedLiterals", &ui.maskedLiterals, "AA-999-*",
                       {.keepLiterals = true, .helpText = "Mask AA-999-*; the text keeps the dashes"});
    ProbeFrame(ctx, "inputs.masked.literals");

    LabeledRow("Error");
    const bool incomplete = !ui.maskedError.empty() && ui.maskedError.size() < 4;
    facet::MaskedInput("##maskedError", &ui.maskedError, "99:99",
                       {.error = incomplete ? "Fill in all four digits" : nullptr});

    LabeledRow("Read-only");
    facet::MaskedInput("##maskedReadOnly", &ui.maskedReadOnly, "9999-9999", {.readOnly = true});

    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    facet::MaskedInput("##maskedDisabled", &ui.maskedReadOnly, "9999-9999");
    ImGui::EndDisabled();
    Note("Digits field: \"%s\". Literals field: \"%s\".", ui.masked.c_str(), ui.maskedLiterals.c_str());
}

void PinCodeSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("PinCodeInput");
    Paragraph("A code in separate cells. Typing fills a cell and moves on, Backspace goes back, a paste fills every "
              "cell. The whole code is one Tab stop.");

    LabeledRow("Six digits");
    if (ctx.capture && ctx.captureState == kCodes)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    facet::PinCodeInput("##pin", &ui.pin, {.groupSize = 3, .helpText = "Placeholder: any six digits"});
    ProbeFrame(ctx, "inputs.pin");
    if (ctx.capture && ctx.captureState == kCodes)
        ctx.RevealInCapture(0.2f);

    LabeledRow("Masked");
    facet::PinCodeInput("##pinMasked", &ui.pinMasked, {.length = 4, .masked = true});
    ProbeFrame(ctx, "inputs.pin.masked");

    LabeledRow("Letters");
    facet::PinCodeInput("##pinLetters", &ui.pinLetters, {.length = 5, .numericOnly = false, .size = facet::ControlSize::Large});

    LabeledRow("Error");
    const bool wrong = ui.pinError.size() == 4 && ui.pinError != "1234";
    facet::PinCodeInput("##pinError", &ui.pinError,
                        {.length = 4, .error = wrong ? "Placeholder rule: only 1234 is accepted" : nullptr,
                         .helpText = "Type four digits other than 1234 to see the error"});
    ProbeFrame(ctx, "inputs.pin.error");

    LabeledRow("Read-only");
    facet::PinCodeInput("##pinReadOnly", &ui.pinReadOnly, {.length = 4, .readOnly = true});

    LabeledRow("Narrow");
    facet::PinCodeInput("##pinNarrow", &ui.pinNarrow, {.length = 8, .width = 120.0f});
    Note("Code: \"%s\" (%s).", ui.pin.c_str(), ui.pin.size() == 6 ? "complete" : "incomplete");
}

void TagInputSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("TagInput");
    Paragraph("Entries become chips. Enter or a comma adds the typed text, Backspace in the empty field selects and then "
              "removes the last chip, Left and Right move between chips, Delete removes the selected one. Matching "
              "suggestions open under the field while typing.");

    LabeledRow("Suggestions");
    if (ctx.capture && ctx.captureState == kFormatted)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    facet::TagInput("##tags", &ui.tags, {.placeholder = "Type and press Enter", .suggestions = kOptions});
    ProbeFrame(ctx, "inputs.tags");
    if (ctx.capture && ctx.captureState == kFormatted)
        ctx.RevealInCapture(0.15f);

    LabeledRow("Limited");
    facet::TagInput("##tagsLimited", &ui.tagsLimited,
                    {.suggestions = kOptions, .allowCreate = false, .maxTags = 2,
                     .helpText = "Suggestions only, two at most"});
    ProbeFrame(ctx, "inputs.tags.limited");

    LabeledRow("Wrapping");
    facet::TagInput("##tagsWrap", &ui.tagsWrap, {.width = 240.0f});

    LabeledRow("Error");
    facet::TagInput("##tagsError", &ui.tagsError,
                    {.error = ui.tagsError.empty() ? "Add at least one tag" : nullptr});

    LabeledRow("Read-only");
    facet::TagInput("##tagsReadOnly", &ui.tagsReadOnly, {.readOnly = true});

    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    facet::TagInput("##tagsDisabled", &ui.tagsReadOnly);
    ImGui::EndDisabled();
    Note("Tags: %d in the first field.", static_cast<int>(ui.tags.size()));
}

// Records the rows of the ValidationSummary just drawn as "inputs.summary.<row>", where the
// summary places them: below its padding and Headline heading, one Small control height or a Body
// line with spacing.xs above and below each.
void ProbeSummaryRows(facet::demo::PageCtx& ctx, ImGuiID summaryId, int count)
{
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const facet::Theme& t = facet::T();
    const float pad = facet::Px(t.spacing.sm);
    const float gap = facet::Px(t.spacing.xs);
    const float row = std::max(facet::ControlHeight(facet::ControlSize::Small),
                               facet::MeasureText(facet::TextStyle::Body, "Ag").y + 2.0f * gap);
    const float top = min.y + pad + facet::MeasureText(facet::TextStyle::Headline, "Ag").y + gap;
    for (int i = 0; i < count; ++i)
    {
        char name[64];
        std::snprintf(name, sizeof(name), "inputs.summary.%d", i);
        const float y = top + row * static_cast<float>(i);
        facet::demo::ProbeRect(ctx, name, ImGui::GetIDWithSeed(i, summaryId), ImVec2(min.x + pad, y),
                               ImVec2(max.x - pad, y + row));
    }
}

void ValidationSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("FieldValidation and ValidationSummary");
    Paragraph("A message under items that have no error option, such as stock Dear ImGui controls, and a summary of "
              "the errors the fields of this window showed. A row of the summary takes the keyboard focus to its "
              "field.");

    if (ctx.capture && ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused, 0);
    const ImGuiID summaryId = ImGui::GetID("Errors in this window");
    if (facet::ValidationSummary("Errors in this window", {.width = 420.0f}))
        ++ui.summaryActivations;
    if (ImGui::GetItemID() == summaryId)
    {
        ctx.Probe("inputs.summary");
        ProbeSummaryRows(ctx, summaryId, 3);
    }

    LabeledRow("Facet field");
    facet::TextField("##validName", &ui.validName,
                     {.placeholder = "Name placeholder", .error = ui.validName.empty() ? "Enter a name" : nullptr});
    ProbeFrame(ctx, "inputs.valid.name");

    LabeledRow("Stock field");
    ImGui::SetNextItemWidth(facet::Px(200.0f));
    ImGui::InputInt("##validPort", &ui.validPort);
    ctx.Probe("inputs.valid.port");
    if (ui.validPort < 1 || ui.validPort > 65535)
        facet::FieldValidation("Use a number from 1 to 65535");
    else
        facet::FieldValidation("In range", {.tone = facet::Tone::Success});

    LabeledRow("Stock checkbox");
    ImGui::Checkbox("Option 1##stock", &ui.stockCheck);
    ctx.Probe("inputs.stock");
    if (ui.stockCheck)
        facet::FieldValidation("Turned on", {.tone = facet::Tone::Neutral});
    else
        facet::FieldValidation("Turned off: a warning, which the summary does not list", {.tone = facet::Tone::Warning});
    if (ctx.capture && ctx.captureState == kFocused)
        ctx.RevealInCapture(0.5f);
    Note("Summary rows activated: %d.", ui.summaryActivations);
}

} // namespace

FACET_DEMO_PAGE("inputs-text", "Text inputs", "Widgets", 40, "default", "hovered", "focused", "filled", "open", "suggestions", "formatted", "later", "codes")
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
    SecureFieldSection(ctx);
    SearchFieldSection(ctx);
    TextAreaSection(ctx);
    AutocompleteSection(ctx);
    ValidationSection(ctx);
    CopyFieldSection(ctx);
    TagInputSection(ctx);
    FilterFieldSection(ctx);
    InlineRenameSection(ctx);
    MaskedInputSection(ctx);
    PinCodeSection(ctx);
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

    // SecureField: typing, no copy, the eye by mouse and by keyboard.
    test.Click("inputs.secure");
    test.Type("secret");
    test.ExpectText("the secure field takes typed text", [] { return ui.secret; }, "secret");
    test.SetClipboard("unchanged");
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("masked text is not copied", "unchanged");
    test.Click("inputs.secure.reveal");
    test.Wait(2);
    test.KeyPress(ImGuiKey_End);
    test.Type("2");
    test.ExpectText("after a click on the eye typing goes on in the field", [] { return ui.secret; }, "secret2");
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("shown text is not copied either", "unchanged");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.secure.reveal");
    test.KeyPress(ImGuiKey_Space);
    test.ExpectFocused("inputs.secure.reveal");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.secure.hold");
    test.Type("abc");
    test.ExpectText("Tab reaches the next secure field", [] { return ui.secretHold; }, "abc");
    test.Click("inputs.secure.strength");
    test.Type("abc");
    test.ExpectText("the strength field takes text", [] { return ui.secretStrength; }, "abc");
    test.KeyPress(ImGuiKey_Enter);

    // SearchField: recent entries by keyboard and by mouse, Escape in steps, the shortcut.
    test.Click("inputs.search");
    test.Wait(2);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectText("Down twice and Enter take the second recent entry", [] { return ui.query; }, "Sample query 2");
    test.ExpectFocused("inputs.search");
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectText("Escape empties the query", [] { return ui.query; }, "");
    test.ExpectFocused("inputs.search");
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectFocused("inputs.search");
    test.Type("q");
    test.ExpectText("after Escape closed the list the field still takes text", [] { return ui.query; }, "q");
    test.KeyPress(ImGuiKey_Backspace);
    test.Wait(2);
    test.Click("inputs.search.recent.0");
    test.ExpectText("a click on a recent entry takes it", [] { return ui.query; }, "Sample query 1");
    test.Wait(2);
    test.Type("!");
    test.ExpectText("and the keyboard goes back to the field", [] { return ui.query; }, "Sample query 1!");
    test.KeyPress(ImGuiKey_Enter);
    test.Click("inputs.search.small");
    test.Type("abc");
    test.ExpectText("the small search field takes text", [] { return ui.querySmall; }, "abc");
    test.KeyPress(ImGuiKey_Escape);
    test.KeyPress(ImGuiKey_Escape);
    test.KeyPress(ImGuiKey_F | ImGuiMod_Ctrl);
    test.Wait(2);
    test.ExpectFocused("inputs.search");
    test.Type("x");
    test.ExpectText("Ctrl+F focuses the search field with its query selected", [] { return ui.query; }, "x");
    test.KeyPress(ImGuiKey_Enter);

    // TextArea: lines, Ctrl+Enter, Tab to the next area.
    test.Click("inputs.area");
    test.Type("one");
    test.KeyPress(ImGuiKey_Enter);
    test.Type("two");
    test.ExpectText("Enter starts a new line in the area", [] { return ui.notes; }, "one\ntwo");
    test.KeyPress(ImGuiKey_Enter | ImGuiMod_Ctrl);
    test.Type("x");
    test.ExpectText("Ctrl+Enter ends the edit", [] { return ui.notes; }, "one\ntwo");
    test.Click("inputs.area");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.area.resizable");
    test.Type("resized");
    test.ExpectText("Tab moves to the next area", [] { return ui.notesResizable; }, "resized");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.area.validated");
    test.Type("short");
    test.ExpectText("the validated area takes text", [] { return ui.notesValidated; }, "short");
    test.KeyPress(ImGuiKey_Enter | ImGuiMod_Ctrl);

    // Autocomplete: keyboard choice, pointer choice, Escape in steps, inline and strict.
    test.Click("inputs.complete");
    test.Type("fol");
    test.Wait(2);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectText("Down and Enter take the suggestion", [] { return ui.completion; }, "Folder");
    test.ExpectFocused("inputs.complete");
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.Type("s");
    test.Wait(2);
    test.Click("inputs.complete.row.0");
    test.Expect("a click on a suggestion takes it", [] { return ui.completion.size() > 1 && ui.completionPicked >= 0; });
    test.Wait(2);
    test.ExpectFocused("inputs.complete");
    test.KeyPress(ImGuiKey_Enter);
    test.Click("inputs.complete");
    test.Do([] { ui.completionPicked = -1; });
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.Type("zz");
    test.Wait(2);
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectText("Escape closes the list and keeps the text", [] { return ui.completion; }, "zz");
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("the second Escape restores the text from activation",
                [] { return ui.completion != "zz" && !ui.completion.empty(); });
    test.Click("inputs.complete.inline");
    test.Type("Opt");
    test.Wait(2);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectText("Tab takes the inline completion", [] { return ui.completionInline; }, "Option 1");
    test.ExpectFocused("inputs.complete.inline");
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.Type("nope");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.ExpectText("strict mode undoes text that matches no suggestion", [] { return ui.completionInline; }, "Option 1");
    test.Click("inputs.complete.prefix");
    test.Type("2");
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectText("prefix matching finds nothing for a middle part", [] { return ui.completionPrefix; }, "2");
    test.Click("inputs.complete.loading");
    test.Type("x");
    test.ExpectText("a loading list leaves the field editable", [] { return ui.completionLoading; }, "x");
    test.KeyPress(ImGuiKey_Enter);

    // ValidationSummary and FieldValidation: from a fresh page the name and the port have errors.
    test.Do([] { ui = {}; });
    test.Wait(3);
    test.ExpectNear("the summary lists the name, the port and the CopyField and TagInput error examples",
                    [] { return static_cast<double>(facet::internal::PreviousFrameFieldErrors().size()); }, 4.0, 0.0);
    test.Click("inputs.summary.0");
    test.Wait(2);
    test.ExpectFocused("inputs.valid.name");
    test.Type("Typed name");
    test.ExpectText("a click on the first summary row gives its field the keyboard", [] { return ui.validName; },
                    "Typed name");
    test.ExpectNear("the click counts as one activation", [] { return ui.summaryActivations; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    // The name is fixed, so three rows are left: the port and the error examples of CopyField and
    // TagInput further down. Shift+Tab from the name reaches the last row.
    test.Click("inputs.valid.name");
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.ExpectFocused("inputs.summary.2");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Enter on a summary row activates it", [] { return ui.summaryActivations; }, 2.0, 0.0);
    test.Do([] { ui.validPort = 80; });
    test.Wait(3);
    test.ExpectNear("a field back in range leaves the summary",
                    [] { return static_cast<double>(facet::internal::PreviousFrameFieldErrors().size()); }, 2.0, 0.0);
    test.Click("inputs.stock");
    test.Expect("the stock checkbox under a message turns on", [] { return ui.stockCheck; });

    // CopyField: the button by mouse and by keyboard, Ctrl+C, the masked value.
    test.SetClipboard("unchanged");
    test.Click("inputs.copy.button");
    test.ExpectClipboard("the copy button copies the whole value", "Sample text to copy");
    test.ExpectNear("and reports one copy", [] { return ui.copies; }, 1.0, 0.0);
    test.SetClipboard("unchanged");
    test.Click("inputs.copy");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.copy.button");
    test.KeyPress(ImGuiKey_Space);
    test.ExpectClipboard("Space on the focused copy button copies", "Sample text to copy");
    test.SetClipboard("unchanged");
    test.Click("inputs.copy.masked");
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("Ctrl+C in the masked field copies the real value", "Placeholder secret value");
    test.ExpectNear("three copies in all", [] { return ui.copies; }, 3.0, 0.0);
    test.KeyPress(ImGuiKey_Escape);

    // TagInput: Enter and a comma add, a suggestion by keyboard, Backspace selects and removes.
    // The first click scrolls the field into view, the second lands in its text part at the end.
    test.Click("inputs.tags");
    test.MouseMove(T::Item("inputs.tags", 1.0f, 0.5f).Offset(-12.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.Type("Sample tag");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.Type("Second tag,");
    test.Expect("Enter and a comma add tags", [] {
        return ui.tags.size() == 3 && ui.tags[1] == "Sample tag" && ui.tags[2] == "Second tag";
    });
    test.Type("ion 2");
    test.Wait(2);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Down and Enter take the suggestion", [] { return ui.tags.size() == 4 && ui.tags[3] == "Option 2"; });
    test.KeyPress(ImGuiKey_Backspace);
    test.KeyPress(ImGuiKey_Backspace);
    test.Expect("Backspace twice removes the last tag", [] { return ui.tags.size() == 3; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.tags.limited");
    test.Type("free,");
    test.Expect("the limited field refuses text that is no suggestion", [] { return ui.tagsLimited.empty(); });
    test.KeyPress(ImGuiKey_Escape);

    // FilterField: the query filters the list, the toggles by pointer and keyboard, Escape empties.
    test.Click("inputs.filter");
    test.Type("l");
    test.ExpectNear("the letter l keeps the eight names that contain it", [] { return ui.filterShown; }, 8.0, 0.0);
    test.Click("inputs.filter.case");
    test.Expect("a click on Aa turns match case on", [] { return ui.filterSettings.matchCase; });
    test.ExpectNear("match case drops Layers, which has only an upper-case L", [] { return ui.filterShown; }, 7.0, 0.0);
    test.Wait(2);
    test.Type("x");
    test.ExpectText("the keyboard went back to the query", [] { return ui.filter; }, "lx");
    test.ExpectNear("nothing matches: the empty state", [] { return ui.filterShown; }, 0.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.filter.case");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on Aa turns match case off", [] { return !ui.filterSettings.matchCase; });
    test.Click("inputs.filter");
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectText("Escape empties the query", [] { return ui.filter; }, "");
    test.KeyPress(ImGuiKey_Escape);

    // InlineRename: double click, F2, the stem, Escape.
    test.DoubleClick("inputs.rename");
    test.Wait(3);
    test.Type("Renamed");
    test.ExpectText("typing changes the copy, not the name", [] { return ui.name; }, "Option 1");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectText("Enter writes the new name", [] { return ui.name; }, "Renamed");
    test.ExpectNear("once", [] { return ui.renames; }, 1.0, 0.0);
    test.Wait(2);
    // The edit started with the pointer, so the first Tab shows the focus on the name, the second moves.
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.rename.file");
    test.KeyPress(ImGuiKey_F2);
    test.Wait(3);
    test.Type("renamed-file");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectText("F2 selects the name without its extension", [] { return ui.fileName; }, "renamed-file.txt");
    test.Wait(2);
    test.DoubleClick("inputs.rename");
    test.Wait(3);
    test.Type("dropped");
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectText("Escape drops the copy", [] { return ui.name; }, "Renamed");

    // MaskedInput: typed digits, a dropped letter, the literals, a paste.
    test.Click("inputs.masked");
    test.Type("12");
    test.Type("x");
    test.Type("345");
    test.ExpectText("the digits field keeps only the digits", [] { return ui.masked; }, "12345");
    test.KeyPress(ImGuiKey_Backspace);
    test.ExpectText("Backspace removes the last digit", [] { return ui.masked; }, "1234");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.masked.literals");
    test.SetClipboard("ab123z");
    test.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    test.ExpectText("a paste fills the slots and keeps the literals", [] { return ui.maskedLiterals; }, "ab-123-z");
    test.KeyPress(ImGuiKey_Enter);

    // PinCodeInput: typing fills the cells, a letter is refused, Backspace goes back, Tab moves on.
    test.Click("inputs.pin");
    test.Type("12");
    test.Type("a");
    test.Type("3456");
    test.ExpectText("typing fills the six cells with digits only", [] { return ui.pin; }, "123456");
    test.Type("7");
    test.ExpectText("a full code takes nothing more", [] { return ui.pin; }, "123456");
    test.KeyPress(ImGuiKey_Backspace);
    test.ExpectText("Backspace empties the last cell", [] { return ui.pin; }, "12345");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("inputs.pin.masked");
    test.SetClipboard("9 8 7 6");
    test.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    test.ExpectText("a paste fills every masked cell", [] { return ui.pinMasked; }, "9876");
    test.Click("inputs.pin.error");
    test.Type("5555");
    test.ExpectText("the error example takes the code", [] { return ui.pinError; }, "5555");
    test.KeyPress(ImGuiKey_Enter);
    test.Do([] { ui.pinError.clear(); });
}
