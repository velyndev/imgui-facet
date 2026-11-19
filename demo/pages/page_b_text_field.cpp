// Demo page: Text field core. Prototype fields built only from the text field core (Dear ImGui's
// own editing inside a Facet frame), the item behaviour, the focus ring and the number layer: a
// plain field, a search field with icon and clear button, a password with a reveal toggle, a
// field with validation, a floating label, a limited field with a counter, a loading field, a
// number field with steppers and drag, a multi-line area, read-only text, and the three control
// sizes. Each shows its value and its edit sessions.

#include <cstdio>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/row_layout.h"
#include "internal/text_field.h"
#include "internal/text_field_number.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::Note;
using facet::demo::SectionHeader;
using facet::internal::NumberFieldBehavior;
using facet::internal::TextFieldBehavior;
using facet::internal::TextFieldParams;
using facet::internal::TextFieldResult;

enum CaptureState
{
    kDefault,
    kFocused,
    kFilled,
    kError,
    kDisabled,
    kReadOnly,
};

// Sample texts are labelled as samples: the page shows how fields behave, not real data.
constexpr const char* kSampleShort = "Sample text";
constexpr const char* kSampleLong = "A longer sample line that runs past the end of the field";
constexpr int kLimit = 20;

// Edit sessions of one field, counted from the item queries after its call.
struct Sessions
{
    int commits = 0;
    int cancels = 0;

    void Count()
    {
        commits += facet::IsItemEditCommitted() ? 1 : 0;
        cancels += facet::IsItemEditCanceled() ? 1 : 0;
    }
};

struct TextFieldPage
{
    std::string plain;
    std::string narrow;
    std::string search;
    std::string password;
    std::string validated;
    std::string floating;
    std::string limited;
    std::string loading;
    std::string area;
    std::string readOnly = "Read-only sample text: select it and copy it";
    std::string small;
    std::string regular;
    std::string large;
    double number = 12.0;
    bool revealed = false;
    Sessions plainSessions;
    Sessions searchSessions;
    Sessions numberSessions;
    Sessions areaSessions;
    int clears = 0;
    int submits = 0;
    int limitedLength = 0;
    bool numberTyping = false;
    bool initialized = false;
};

TextFieldPage page;

void Reset()
{
    page = {};
    page.initialized = true;
}

// The values the capture states show.
void Fill(bool longText)
{
    const char* text = longText ? kSampleLong : kSampleShort;
    page.plain = text;
    page.narrow = kSampleLong;
    page.search = kSampleShort;
    page.password = kSampleShort;
    page.validated = "ab";
    page.floating = kSampleShort;
    page.limited = kSampleShort;
    page.loading = kSampleShort;
    page.area = "First sample line\nSecond sample line that is long enough to wrap inside the area";
    page.small = kSampleShort;
    page.regular = kSampleShort;
    page.large = kSampleShort;
    page.number = 42.5;
}

// Records the frame of a field (and not its message line) for selftests.
void ProbeField(facet::demo::PageCtx& ctx, const char* name, const TextFieldResult& field)
{
    facet::demo::ProbeRect(ctx, name, field.id, field.frame.Min, field.frame.Max);
}

struct RevealSlot
{
    facet::demo::PageCtx* ctx = nullptr;
    bool pressed = false;
};

// The reveal toggle of the password prototype: a slot button that is a Tab stop.
void RevealToggle(const facet::internal::TextFieldSlot& slot, void* user)
{
    RevealSlot& reveal = *static_cast<RevealSlot*>(user);
    const facet::Strings& strings = facet::GetStrings();
    const ImGuiID id = ImGui::GetIDWithSeed("reveal", nullptr, slot.fieldId);
    const facet::internal::ItemState item = facet::internal::TextFieldSlotButton(
        slot, 0, id, facet::Icon(page.revealed ? "eye-off" : "eye"),
        {.tabStop = true, .selected = page.revealed, .name = page.revealed ? strings.hide : strings.show});
    facet::demo::ProbeRect(*reveal.ctx, "tf.password.reveal", id, item.rect.Min, item.rect.Max);
    reveal.pressed = item.pressed;
}

void SessionNote(const char* what, const Sessions& sessions)
{
    Note("%s: committed %d, canceled %d.", what, sessions.commits, sessions.cancels);
}

} // namespace

FACET_DEMO_PAGE("text-field", "Text field core", "Blocks", 150, "default", "focused", "filled", "error", "disabled",
                "read-only")
{
    if (!page.initialized)
        Reset();
    if (ctx.capture)
    {
        Reset();
        if (ctx.captureState == kFilled || ctx.captureState == kError || ctx.captureState == kDisabled ||
            ctx.captureState == kReadOnly)
            Fill(ctx.captureState == kFilled);
    }
    const bool disabled = ctx.captureState == kDisabled;
    const bool readOnly = ctx.captureState == kReadOnly;

    facet::demo::PageHeader("Text field core",
                            "Dear ImGui edits the text, so typing, the IME, undo, the clipboard, selection and word "
                            "moves work as in every field; Facet draws the frame, the adornments and the messages "
                            "around it, and adds number editing on top.");
    Note("Prototype fields built only from the text field core, the item behaviour and the focus ring; they are not "
         "the Phase 2 widgets. The texts are samples. Tab moves between fields, Escape restores the text from when "
         "the field took the keyboard, Enter ends the edit. In the number field Up and Down step (Shift finer, Ctrl "
         "coarser); drag it or its label sideways, or click it to type.");

    if (disabled)
        ImGui::BeginDisabled();

    SectionHeader("Text");
    facet::internal::BeginRowGroup("text");
    {
        facet::internal::BeginRow("Plain");
        if (ctx.captureState == kFocused)
            ctx.ForceNextItemState(ForcedItemState::Focused);
        const TextFieldResult field =
            TextFieldBehavior("##value", &page.plain, {.placeholder = "Placeholder", .readOnly = readOnly});
        page.plainSessions.Count();
        page.submits += field.submitted ? 1 : 0;
        ProbeField(ctx, "tf.plain", field);
        facet::internal::EndRow();
        if (ctx.captureState == kFocused)
            ctx.RevealInCapture(0.3f);
    }
    {
        // Narrow on purpose (the pad size token): a long text scrolls inside and fades out where
        // it continues.
        facet::internal::BeginRow("Narrow");
        const TextFieldResult field = TextFieldBehavior(
            "##value", &page.narrow,
            {.placeholder = "Placeholder", .width = facet::T().metrics.padSize, .readOnly = readOnly});
        ProbeField(ctx, "tf.narrow", field);
        facet::internal::EndRow();
    }
    {
        facet::internal::BeginRow("Search");
        const TextFieldResult field = TextFieldBehavior(
            "##value", &page.search,
            {.placeholder = "Search placeholder", .icon = facet::Icon("search"), .clearButton = true, .readOnly = readOnly});
        page.searchSessions.Count();
        page.clears += field.clearPressed ? 1 : 0;
        ProbeField(ctx, "tf.search", field);
        if (field.clear.GetWidth() > 0.0f)
            facet::demo::ProbeRect(ctx, "tf.search.clear", ImGui::GetIDWithSeed("clear", nullptr, field.id),
                                   field.clear.Min, field.clear.Max);
        facet::internal::EndRow();
    }
    {
        facet::internal::BeginRow("Password");
        RevealSlot reveal;
        reveal.ctx = &ctx;
        TextFieldParams params;
        params.placeholder = "Password placeholder";
        params.password = true;
        params.revealed = page.revealed;
        params.readOnly = readOnly;
        params.trailingWidth = facet::internal::TextFieldSlotButtonsWidth(1, facet::ControlSize::Inherit);
        params.trailingSlot = RevealToggle;
        params.trailingUser = &reveal;
        const TextFieldResult field = TextFieldBehavior("##value", &page.password, params);
        ProbeField(ctx, "tf.password", field);
        if (reveal.pressed)
            page.revealed = !page.revealed;
        facet::internal::EndRow();
    }
    {
        facet::internal::BeginRow("Validated");
        const bool tooShort = page.validated.size() < 3;
        if (ctx.captureState == kError)
            ctx.ForceNextItemState(ForcedItemState::Focused);
        const TextFieldResult field = TextFieldBehavior(
            "##value", &page.validated,
            {.placeholder = "At least three characters",
             .readOnly = readOnly,
             .error = tooShort && !page.validated.empty() ? "Use at least three characters" : nullptr,
             .helpText = "The caller validates: three characters or more"});
        ProbeField(ctx, "tf.validated", field);
        facet::internal::EndRow();
        if (ctx.captureState == kError)
            ctx.RevealInCapture(0.4f);
    }
    {
        facet::internal::BeginRow("Floating label");
        const TextFieldResult field = TextFieldBehavior(
            "Name", &page.floating, {.placeholder = "Placeholder", .readOnly = readOnly, .floatingLabel = true});
        ProbeField(ctx, "tf.floating", field);
        facet::internal::EndRow();
    }
    {
        char counter[48];
        std::snprintf(counter, sizeof(counter), "%d / %d characters", page.limitedLength, kLimit);
        facet::internal::BeginRow("Limited");
        const TextFieldResult field = TextFieldBehavior(
            "##value", &page.limited, {.placeholder = "Placeholder", .maxLength = kLimit, .readOnly = readOnly, .helpText = counter});
        page.limitedLength = field.length;
        ProbeField(ctx, "tf.limited", field);
        facet::internal::EndRow();
    }
    {
        facet::internal::BeginRow("Loading");
        const TextFieldResult field =
            TextFieldBehavior("##value", &page.loading, {.placeholder = "Placeholder", .readOnly = readOnly, .loading = true});
        ProbeField(ctx, "tf.loading", field);
        facet::internal::EndRow();
    }
    facet::internal::EndRowGroup();
    SessionNote("Plain field", page.plainSessions);
    SessionNote("Search field", page.searchSessions);
    Note("Clear button pressed %d times; Enter pressed in the plain field %d times.", page.clears, page.submits);

    SectionHeader("Numbers");
    facet::internal::BeginRowGroup("numbers");
    {
        facet::internal::BeginRow("Steppers and drag");
        const facet::internal::NumberFieldResult number = NumberFieldBehavior(
            "##value", &page.number,
            {.format = "%.1f", .min = -100.0, .max = 100.0, .step = 0.5, .stepFast = 5.0, .steppers = true, .drag = true,
             .unit = "ms", .readOnly = readOnly});
        page.numberSessions.Count();
        page.numberTyping = number.typing;
        ProbeField(ctx, "tf.number", number.field);
        const ImRect slot = number.field.trailing;
        if (slot.GetWidth() > 0.0f)
        {
            const ImGuiID id = number.field.id;
            facet::demo::ProbeRect(ctx, "tf.number.minus", ImGui::GetIDWithSeed("decrease", nullptr, id), slot.Min,
                                   ImVec2(slot.GetCenter().x, slot.Max.y));
            facet::demo::ProbeRect(ctx, "tf.number.plus", ImGui::GetIDWithSeed("increase", nullptr, id),
                                   ImVec2(slot.GetCenter().x, slot.Min.y), slot.Max);
        }
        facet::internal::EndRow();
    }
    facet::internal::EndRowGroup();
    Note("Value %.2f. Committed %d, canceled %d.", page.number, page.numberSessions.commits, page.numberSessions.cancels);

    SectionHeader("Multiple lines");
    {
        const TextFieldResult field = TextFieldBehavior(
            "##area", &page.area, {.placeholder = "Placeholder: several lines, Enter starts a new one", .multiline = true,
                                   .height = 0.0f, .readOnly = readOnly});
        page.areaSessions.Count();
        ProbeField(ctx, "tf.area", field);
    }
    SessionNote("Area", page.areaSessions);

    SectionHeader("Read-only");
    {
        const TextFieldResult field = TextFieldBehavior("##readonly", &page.readOnly, {.readOnly = true});
        ProbeField(ctx, "tf.readonly", field);
    }

    SectionHeader("Control sizes");
    facet::internal::BeginRowGroup("sizes");
    {
        facet::internal::BeginRow("Small", {.size = facet::ControlSize::Small});
        const TextFieldResult field = TextFieldBehavior(
            "##value", &page.small, {.placeholder = "Placeholder", .clearButton = true, .size = facet::ControlSize::Small,
                                     .readOnly = readOnly});
        ProbeField(ctx, "tf.small", field);
        facet::internal::EndRow();
    }
    {
        facet::internal::BeginRow("Regular", {.size = facet::ControlSize::Regular});
        const TextFieldResult field = TextFieldBehavior(
            "##value", &page.regular, {.placeholder = "Placeholder", .clearButton = true, .size = facet::ControlSize::Regular,
                                       .readOnly = readOnly});
        ProbeField(ctx, "tf.regular", field);
        facet::internal::EndRow();
    }
    {
        facet::internal::BeginRow("Large", {.size = facet::ControlSize::Large});
        const TextFieldResult field = TextFieldBehavior(
            "##value", &page.large, {.placeholder = "Placeholder", .clearButton = true, .size = facet::ControlSize::Large,
                                     .readOnly = readOnly});
        ProbeField(ctx, "tf.large", field);
        facet::internal::EndRow();
    }
    facet::internal::EndRowGroup();
    // The filled state shows the lower half: the area, the read-only text and the sizes.
    if (ctx.captureState == kFilled)
        ctx.RevealInCapture(0.85f);

    if (disabled)
        ImGui::EndDisabled();
}

FACET_DEMO_SELFTEST("text-field")
{
    using T = facet::demo::Target;
    test.Do([] { Reset(); });

    // Typing, selecting with Shift and the arrows, the private clipboard, undo.
    test.Click("tf.plain");
    test.ExpectFocused("tf.plain");
    test.Type("hello world");
    test.ExpectText("typing writes per keystroke", [] { return page.plain; }, "hello world");
    for (int i = 0; i < 5; ++i)
        test.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("Shift+Left five times and Ctrl+C copy the last word", "world");
    test.KeyPress(ImGuiKey_End);
    test.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    test.ExpectText("Ctrl+V pastes it at the caret", [] { return page.plain; }, "hello worldworld");
    test.KeyPress(ImGuiKey_Z | ImGuiMod_Ctrl);
    test.ExpectText("Ctrl+Z undoes the paste", [] { return page.plain; }, "hello world");
    test.SetClipboard("pasted");
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    test.ExpectText("Ctrl+A and Ctrl+V replace everything", [] { return page.plain; }, "pasted");
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectText("Escape restores the text from activation", [] { return page.plain; }, "");
    test.ExpectNear("Escape reports one canceled session", [] { return page.plainSessions.cancels; }, 1.0, 0.0);
    test.ExpectNear("and no committed one", [] { return page.plainSessions.commits; }, 0.0, 0.0);
    test.Click("tf.plain");
    test.Type("kept");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Enter commits the session once", [] { return page.plainSessions.commits; }, 1.0, 0.0);
    test.ExpectNear("and counts as a submit", [] { return page.submits; }, 1.0, 0.0);
    test.ExpectText("the committed text stays", [] { return page.plain; }, "kept");

    // Tab moves on to the next field, which takes the keyboard with its text selected. The field
    // was clicked, so the first Tab only shows where the focus is (Dear ImGui's rule).
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("tf.plain");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("tf.narrow");
    test.Type("a sample line longer than the narrow field");
    test.ExpectText("the narrow field takes text longer than itself", [] { return page.narrow; },
                    "a sample line longer than the narrow field");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("tf.search");
    test.Type("query");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectNear("focus loss by Tab commits the search field", [] { return page.searchSessions.commits; }, 1.0, 0.0);
    // The clear button is no Tab stop: Tab went from the search text to the password text.
    test.ExpectFocused("tf.password");
    test.Click("tf.search.clear");
    test.ExpectText("the clear button empties the field", [] { return page.search; }, "");
    test.ExpectNear("and is counted", [] { return page.clears; }, 1.0, 0.0);
    test.ExpectNear("as one committed edit", [] { return page.searchSessions.commits; }, 2.0, 0.0);
    test.Wait(2);
    test.ExpectFocused("tf.search");
    test.Type("again");
    test.ExpectText("the keyboard went back to the field", [] { return page.search; }, "again");
    test.KeyPress(ImGuiKey_Enter);

    // The password: dots, no copy while masked, the reveal toggle by keyboard and by mouse.
    test.Click("tf.password");
    test.Type("secret");
    test.SetClipboard("unchanged");
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("a masked password is not copied", "unchanged");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("tf.password.reveal");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the reveal toggle shows the text", [] { return page.revealed; });
    test.Click("tf.password.reveal");
    test.Expect("a click hides it again", [] { return !page.revealed; });
    test.ExpectText("the password kept its text", [] { return page.password; }, "secret");

    // Validation is the caller's; the field shows the message.
    test.Click("tf.validated");
    test.Type("ab");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectText("the validated field holds the short text", [] { return page.validated; }, "ab");

    // The limited field stops at its length, also for a paste.
    test.Click("tf.limited");
    test.SetClipboard("abcdefghijklmnopqrstuvwxyz");
    test.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    test.ExpectNear("maxLength cuts the paste at 20 characters", [] { return static_cast<double>(page.limited.size()); },
                    20.0, 0.0);
    test.KeyPress(ImGuiKey_Enter);

    // Numbers: typing commits on Enter and clamps; Up and Down step; steppers step; drag; Escape.
    test.Click("tf.number");
    test.Wait(1);
    test.Expect("a click on the number field types a value", [] { return page.numberTyping; });
    test.Type("250");
    test.ExpectNear("typed numbers wait for Enter", [] { return page.number; }, 12.0, 0.0);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Enter writes the number clamped to the maximum", [] { return page.number; }, 100.0, 0.0);
    test.Click("tf.number");
    test.Wait(1);
    test.Type("4,5");
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectNear("Down steps from the typed number (a comma reads as the separator)", [] { return page.number; }, 4.0,
                    1e-9);
    test.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+Up steps by the fast step", [] { return page.number; }, 9.0, 1e-9);
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectNear("Escape restores the value from activation", [] { return page.number; }, 100.0, 0.0);
    test.Click("tf.number.minus");
    test.ExpectNear("the minus stepper steps down", [] { return page.number; }, 99.5, 1e-9);
    test.Click("tf.number.plus");
    test.ExpectNear("the plus stepper steps up", [] { return page.number; }, 100.0, 1e-9);
    test.Drag({T::Item("tf.number", 0.5f), T::Item("tf.number", 0.5f).Offset(-40.0f, 0.0f)}, 0.3f);
    test.Expect("dragging the field to the left lowers the value", [] { return page.number < 100.0; });

    // The multi-line area takes Enter as a line break and ends with Ctrl+Enter.
    test.Click("tf.area");
    test.Type("one");
    test.KeyPress(ImGuiKey_Enter);
    test.Type("two");
    test.ExpectText("Enter starts a new line", [] { return page.area; }, "one\ntwo");
    test.KeyPress(ImGuiKey_Enter | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+Enter commits the area", [] { return page.areaSessions.commits; }, 1.0, 0.0);

    // Read-only text is selected and copied, never written.
    test.Click("tf.readonly");
    test.Type("xyz");
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("read-only text can be copied", "Read-only sample text: select it and copy it");
    test.ExpectText("and is never written", [] { return page.readOnly; }, "Read-only sample text: select it and copy it");
    test.KeyPress(ImGuiKey_Escape);

    // The double click selects the word under the pointer (past the text: the last word), as in a
    // stock field.
    test.Do([] { page.small = "first second"; });
    test.DoubleClick("tf.small");
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("a double click selects a word", "second");
    test.KeyPress(ImGuiKey_Escape);
    test.Reveal("tf.large");
    test.ExpectVisible("tf.large");
}
