// ConsoleView: a LogView over the caller's entries with a TextField for the command under it. The
// log and the field are the finished widgets; this file adds the layout, the history walk, the
// completion and the submit.
//
// History: Up and Down in the field walk `history` (oldest first) through the field's history
// callback. The first Up keeps the text typed so far and shows the newest entry; Up at the oldest
// entry and Down at the typed text stay where they are. Typing or a submit starts the walk again.
//
// Completion: Tab completes the word before the caret from `commands` while the field holds text
// (on an empty field Tab moves the focus on, and Shift+Tab always moves back, so the field is never
// a keyboard trap); the completed text replaces the field's text as a paste would (one undo step).
// The commands are ranked with the fuzzy matcher. No candidate:
// the line above the field says Strings::noResults. One: the word becomes it, followed by a space.
// Several: the word grows to the longest prefix they share (ASCII case ignored) and the line lists
// them; a Tab that cannot grow the word further puts the candidates in, one per press, in their
// ranked order. Typing hides the line.
//
// Submit: Enter in the field with text ends the edit; the function returns true on that frame and
// gives the field the keyboard back in the next one, so the next command can be typed at once. The
// text stays for the caller to read, add to its history and clear.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/display.h"
#include "facet/widgets/inputs.h"
#include "internal/control.h"
#include "internal/fuzzy.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"

namespace facet
{
namespace
{

// Candidates one Tab ranks and lists at most.
constexpr int kConsoleMaxCandidates = 16;
// The text of the field after the last completion, to tell a repeated Tab; longer text never repeats.
constexpr int kConsoleCompletionText = 256;
// The log keeps at least this many of its rows when the console is low.
constexpr float kConsoleMinLogRows = 2.0f;

struct ConsoleViewState
{
    int historyPos = -1;      // the history entry shown; -1 the text typed before the walk
    std::string draft;        // that text
    int candidates[kConsoleMaxCandidates] = {};
    int candidateCount = 0;
    bool noMatch = false;     // the last Tab found no candidate
    int cycle = 0;            // the next candidate a repeated Tab puts in
    char completed[kConsoleCompletionText] = {};  // the field's text after the last completion
    bool fieldActive = false; // the field had the keyboard in the last frame
    bool completing = false;  // a completion waits to be written by the field
};

struct ConsoleCallbackContext
{
    ConsoleViewState* state = nullptr;
    const ConsoleViewOptions* options = nullptr;
    bool walked = false;      // the callback changed the text by the history this frame
};

void ConsoleReplace(ImGuiInputTextCallbackData* data, int start, int end, const char* text)
{
    if (end > start)
        data->DeleteChars(start, end - start);
    data->InsertChars(start, text);
}

void ConsoleHistory(ImGuiInputTextCallbackData* data, ConsoleCallbackContext& context)
{
    ConsoleViewState& state = *context.state;
    const std::span<const char* const> history = context.options->history;
    const int count = static_cast<int>(history.size());
    if (count == 0)
        return;
    int pos = state.historyPos;
    if (data->EventKey == ImGuiKey_UpArrow)
    {
        if (pos < 0)
        {
            try
            {
                state.draft.assign(data->Buf, static_cast<std::size_t>(data->BufTextLen));
            }
            catch (...)
            {
                return;
            }
            pos = count - 1;
        }
        else if (pos > 0)
        {
            --pos;
        }
        else
        {
            return;  // the oldest entry: stay
        }
    }
    else if (data->EventKey == ImGuiKey_DownArrow)
    {
        if (pos < 0)
            return;  // the typed text: stay
        pos = pos + 1 < count ? pos + 1 : -1;
    }
    else
    {
        return;
    }
    state.historyPos = pos;
    const char* text = pos >= 0 ? history[static_cast<std::size_t>(pos)] : state.draft.c_str();
    ConsoleReplace(data, 0, data->BufTextLen, text != nullptr ? text : "");
    state.candidateCount = 0;
    state.noMatch = false;
    context.walked = true;
}

bool ConsoleStartsWith(const char* text, const char* prefix, int length)
{
    for (int i = 0; i < length; ++i)
    {
        if (text[i] == '\0' || ImToUpper(text[i]) != ImToUpper(prefix[i]))
            return false;
    }
    return true;
}

// The command text a Tab completes, as an editable copy with the caret.
struct ConsoleEdit
{
    char buf[kConsoleCompletionText] = {};
    int length = 0;
    int caret = 0;

    void Replace(int start, int end, const char* text)
    {
        const int insert = static_cast<int>(std::strlen(text));
        const int tail = length - end;
        if (start + insert + tail >= kConsoleCompletionText)
            return;
        std::memmove(buf + start + insert, buf + end, static_cast<std::size_t>(tail) + 1);
        std::memcpy(buf + start, text, static_cast<std::size_t>(insert));
        length = start + insert + tail;
        caret = start + insert;
    }
};

// Completes the word before the caret of `edit` from the commands (see the top of the file).
// Returns true when the text changed.
bool ConsoleComplete(ConsoleEdit& edit, ConsoleViewState& state, std::span<const char* const> commands)
{
    if (commands.empty())
        return false;
    int start = edit.caret;
    while (start > 0 && edit.buf[start - 1] != ' ')
        --start;
    const int end = edit.caret;
    const int wordLength = end - start;
    const int before = edit.length;
    char original[kConsoleCompletionText];
    ImStrncpy(original, edit.buf, sizeof(original));

    // A Tab that could not grow the word: the candidates go in one per press.
    if (state.candidateCount > 1 && std::strcmp(state.completed, edit.buf) == 0)
    {
        const char* next = commands[static_cast<std::size_t>(state.candidates[state.cycle % state.candidateCount])];
        state.cycle = (state.cycle + 1) % state.candidateCount;
        edit.Replace(start, end, next != nullptr ? next : "");
        ImStrncpy(state.completed, edit.buf, sizeof(state.completed));
        return true;
    }

    internal::FuzzyResult ranked[kConsoleMaxCandidates];
    int total = 0;
    const int shown = internal::FuzzyFilter(std::string_view(edit.buf + start, static_cast<std::size_t>(wordLength)), commands,
                                            ranked, &total);
    state.candidateCount = 0;
    state.cycle = 0;
    state.noMatch = total == 0;
    for (int i = 0; i < shown; ++i)
    {
        if (commands[static_cast<std::size_t>(ranked[i].index)] != nullptr)
            state.candidates[state.candidateCount++] = ranked[i].index;
    }
    if (state.candidateCount == 1 && wordLength > 0)
    {
        const char* only = commands[static_cast<std::size_t>(state.candidates[0])];
        edit.Replace(start, end, only);
        if (edit.caret >= edit.length || edit.buf[edit.caret] != ' ')
            edit.Replace(edit.caret, edit.caret, " ");
        state.candidateCount = 0;
    }
    else if (state.candidateCount > 1 && wordLength > 0)
    {
        // The longest prefix every candidate that starts with the word shares.
        const char* first = nullptr;
        int prefix = 0;
        bool allStart = true;
        for (int i = 0; i < state.candidateCount; ++i)
        {
            const char* name = commands[static_cast<std::size_t>(state.candidates[i])];
            if (!ConsoleStartsWith(name, edit.buf + start, wordLength))
            {
                allStart = false;
                break;
            }
            if (first == nullptr)
            {
                first = name;
                prefix = static_cast<int>(std::strlen(name));
            }
            else
            {
                int n = 0;
                while (n < prefix && name[n] != '\0' && ImToUpper(name[n]) == ImToUpper(first[n]))
                    ++n;
                prefix = n;
            }
        }
        if (allStart && first != nullptr && prefix > wordLength)
        {
            char grown[kConsoleCompletionText];
            ImStrncpy(grown, first, static_cast<std::size_t>(std::min(prefix + 1, kConsoleCompletionText)));
            edit.Replace(start, end, grown);
        }
    }
    ImStrncpy(state.completed, edit.buf, sizeof(state.completed));
    return edit.length != before || std::strcmp(original, edit.buf) != 0;
}

int ConsoleCallback(ImGuiInputTextCallbackData* data)
{
    ConsoleCallbackContext& context = *static_cast<ConsoleCallbackContext*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
        ConsoleHistory(data, context);
    return 0;
}

// `text()` gives the command text (never null); `log(options)` draws the log and `field(options)`
// the command field, returning its result.
template <class TextOf, class LogCall, class FieldCall>
bool ConsoleViewImpl(const char* id, const ConsoleViewOptions& options, TextOf&& text, LogCall&& log, FieldCall&& field)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const Strings& s = GetStrings();
    internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
    const ImGuiID widgetId = window->GetID(id);
    const bool readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    ConsoleViewState& state = internal::GetState<ConsoleViewState>(widgetId);

    // ---- Layout: the log takes what the field and the candidates line leave ----
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const float available = ImGui::GetContentRegionAvail().y;
    float height = available;
    if (options.height > 0.0f)
        height = Px(options.height);
    else if (options.height < 0.0f)
        height = available + Px(options.height);
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const bool showLine = state.candidateCount > 0 || state.noMatch;
    const float lineHeight = MeasureText(TextStyle::Footnote, "").y;
    const float rowHeight = MeasureText(TextStyle::Body, "").y;
    const float logHeight = std::max(rowHeight * kConsoleMinLogRows,
                                     height - m.height - spacing - (showLine ? lineHeight + spacing : 0.0f));
    next.data.HasFlags &= ~ImGuiNextItemDataFlags_HasWidth;
    next.hasWidth = false;

    ImGui::BeginGroup();
    ImGui::PushOverrideID(widgetId);
    internal::ApplyNextItemData(next);
    // Part 0 (or -1) is the log, part 1 the field.
    if (forced.state != internal::ForcedItemState::None && forced.part <= 0)
        internal::ForceNextItemState(forced.state);
    LogViewOptions logOptions;
    logOptions.timestamps = options.timestamps;
    logOptions.state = options.state;
    logOptions.emptyText = options.emptyText;
    logOptions.errorText = options.errorText;
    logOptions.width = width / Scale();
    logOptions.height = logHeight / Scale();
    log(logOptions);

    // The candidates line, while a Tab found candidates or none.
    if (showLine)
    {
        const ImVec2 at = ImGui::GetCursorScreenPos();
        ImGui::Dummy(ImVec2(width, lineHeight));
        if (ImGui::IsRectVisible(at, ImVec2(at.x + width, at.y + lineHeight)))
        {
            char line[kConsoleCompletionText];
            int length = 0;
            if (state.noMatch)
            {
                ImStrncpy(line, s.noResults, sizeof(line));
            }
            else
            {
                line[0] = '\0';
                for (int i = 0; i < state.candidateCount && i < static_cast<int>(options.commands.size()); ++i)
                {
                    const char* name = options.commands[static_cast<std::size_t>(state.candidates[i])];
                    length += ImFormatString(line + length, sizeof(line) - static_cast<std::size_t>(length), "%s%s",
                                             i > 0 ? "   " : "", name != nullptr ? name : "");
                }
            }
            internal::ControlTextScope footnote(TextStyle::Footnote);
            const internal::FittedText fitted = internal::FitText(line, nullptr, width);
            window->DrawList->AddText(at, ToU32(internal::ControlSecondaryColor(false)), fitted.text);
        }
    }

    // The command field.
    ConsoleCallbackContext context{&state, &options};
    TextFieldOptions command;
    command.placeholder = options.placeholder;
    command.readOnly = readOnly;
    command.callback = &ConsoleCallback;
    command.callbackUser = &context;
    command.callbackEvents = ImGuiInputTextFlags_CallbackHistory;
    if (forced.state != internal::ForcedItemState::None && forced.part == 1)
        internal::ForceNextItemState(forced.state);
    ImGui::SetNextItemWidth(width);
    const bool written = field(command);
    const ImGuiID fieldId = ImGui::GetItemID();
    const bool fieldActive = ImGui::IsItemActive();
    if (written && !context.walked && !state.completing)
    {
        // Typed: the walk and the candidates start again.
        state.historyPos = -1;
        state.candidateCount = 0;
        state.noMatch = false;
    }
    if (written)
        state.completing = false;

    // Tab completes while the field holds text (see the top of the file). Dear ImGui's tabbing
    // reads Tab in the next frame before any item, unless Tab has an owner: the field owns it while
    // completion is on (and Shift is up, so Shift+Tab still moves back), and the completed text
    // replaces the field's text in its next call, as a paste would.
    const ImGuiIO& io = ImGui::GetIO();
    const bool completion = fieldActive && text()[0] != '\0' && !options.commands.empty() && !readOnly;
    if (completion && ImGui::IsKeyPressed(ImGuiKey_Tab, ImGuiInputFlags_None, fieldId) && !io.KeyShift && !io.KeyCtrl)
    {
        ConsoleEdit edit;
        ImStrncpy(edit.buf, text(), sizeof(edit.buf));
        edit.length = static_cast<int>(std::strlen(edit.buf));
        const ImGuiInputTextState* input = ImGui::GetInputTextState(fieldId);
        edit.caret = input != nullptr ? std::clamp(input->GetCursorPos(), 0, edit.length) : edit.length;
        const int before = edit.length;
        if (ConsoleComplete(edit, state, options.commands))
        {
            internal::TextFieldSetSelection(fieldId, 0, before);
            internal::TextFieldInsert(fieldId, edit.buf);
            state.completing = true;
        }
    }
    if (completion && !io.KeyShift)
        ImGui::SetKeyOwner(ImGuiKey_Tab, fieldId);
    // Enter with text ends the edit: the submit, and the field gets the keyboard back.
    const bool enter = ImGui::GetKeyData(ImGuiKey_Enter)->DownDuration == 0.0f ||
                       ImGui::GetKeyData(ImGuiKey_KeypadEnter)->DownDuration == 0.0f;
    const bool submitted = !readOnly && state.fieldActive && !fieldActive && enter && text()[0] != '\0';
    if (submitted)
    {
        state.historyPos = -1;
        state.candidateCount = 0;
        state.noMatch = false;
        internal::TextFieldRequestFocus(fieldId, internal::TextFieldFocus::End);
    }
    state.fieldActive = fieldActive;
    ImGui::PopID();
    ImGui::EndGroup();
    internal::FinishWidgetGroup(widgetId);
    return submitted;
}

// The log over a span or a getter.
struct ConsoleLogSource
{
    std::span<const LogEntry> entries;
    int count = 0;
    LogGetter getter = nullptr;
    void* user = nullptr;

    void Draw(const LogViewOptions& options) const
    {
        if (getter != nullptr)
            LogView("##log", count, getter, user, options);
        else
            LogView("##log", entries, options);
    }
};

bool ConsoleViewString(const char* id, std::string* command, const ConsoleLogSource& source, const ConsoleViewOptions& options)
{
    return ConsoleViewImpl(
        id, options, [&] { return command != nullptr ? command->c_str() : ""; },
        [&](const LogViewOptions& log) { source.Draw(log); },
        [&](const TextFieldOptions& field) { return TextField("##command", command, field); });
}

bool ConsoleViewBuffer(const char* id, char* buffer, std::size_t bufferSize, const ConsoleLogSource& source,
                       const ConsoleViewOptions& options)
{
    return ConsoleViewImpl(
        id, options, [&] { return buffer != nullptr && bufferSize > 0 ? static_cast<const char*>(buffer) : ""; },
        [&](const LogViewOptions& log) { source.Draw(log); },
        [&](const TextFieldOptions& field) { return TextField("##command", buffer, bufferSize, field); });
}

} // namespace

bool ConsoleView(const char* id, std::string* command, std::span<const LogEntry> entries, const ConsoleViewOptions& options)
{
    return ConsoleViewString(id, command, {entries}, options);
}

bool ConsoleView(const char* id, std::string* command, int entryCount, LogGetter getter, void* user,
                 const ConsoleViewOptions& options)
{
    return ConsoleViewString(id, command, {{}, std::max(0, entryCount), getter, user}, options);
}

bool ConsoleView(const char* id, char* buffer, std::size_t bufferSize, std::span<const LogEntry> entries,
                 const ConsoleViewOptions& options)
{
    return ConsoleViewBuffer(id, buffer, bufferSize, {entries}, options);
}

bool ConsoleView(const char* id, char* buffer, std::size_t bufferSize, int entryCount, LogGetter getter, void* user,
                 const ConsoleViewOptions& options)
{
    return ConsoleViewBuffer(id, buffer, bufferSize, {{}, std::max(0, entryCount), getter, user}, options);
}

} // namespace facet
