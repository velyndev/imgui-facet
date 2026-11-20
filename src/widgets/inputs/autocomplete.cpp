// Autocomplete: a text field with the matching suggestions in a completion list under it
// (text_inputs_shared.h), ranked by the fuzzy matcher (src/internal/fuzzy.h) or by prefix, with an
// optional inline completion after the caret and a strict mode that restores text matching no
// suggestion when the edit ends.

#include "facet/widgets/inputs.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <string_view>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/fuzzy.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet
{
namespace
{

// The suggestion source of one call: one of the three overload shapes.
struct AutocompleteSource
{
    std::span<const ChoiceItem> items;
    std::span<const char* const> labels;
    int count = 0;
    ChoiceGetter getter = nullptr;
    void* user = nullptr;

    // Fills `out` for suggestion `index`; false for an entry to leave out.
    bool Get(int index, ChoiceItem* out) const
    {
        if (!items.empty())
        {
            *out = items[static_cast<std::size_t>(index)];
            return out->label != nullptr;
        }
        if (!labels.empty())
        {
            *out = ChoiceItem{};
            out->label = labels[static_cast<std::size_t>(index)];
            return out->label != nullptr;
        }
        if (getter == nullptr)
            return false;
        *out = ChoiceItem{};
        return getter(user, index, out) && out->label != nullptr;
    }
};

// One shown suggestion: its index in the source and its matched ranges.
struct AutocompleteMatch
{
    int index = 0;
    int score = 0;
    int length = 0;  // bytes of the visible label: shorter names rank first among equal scores
    int rangeStart = 0;
    int rangeCount = 0;
};

// Kept under the field id. The vectors grow to the largest list seen and are reused, so a list of
// the same size allocates nothing in later frames.
struct AutocompleteState
{
    ImVector<AutocompleteMatch> matches;
    ImVector<internal::TextRange> ranges;
    std::string activationText;  // the text when the field took the keyboard (strict mode)
    bool wasActive = false;
};

// AutocompleteAcceptedItem: the suggestion the last call of this frame accepted.
struct AutocompleteAccepted
{
    int frame = -1;
    int index = -1;
};

AutocompleteAccepted& LastAccepted()
{
    static AutocompleteAccepted accepted;
    return accepted;
}

// ASCII case-insensitive comparison of [a, a + length) and [b, b + length).
bool AutocompleteSameIgnoringCase(const char* a, const char* b, std::size_t length)
{
    for (std::size_t i = 0; i < length; ++i)
    {
        const char x = (a[i] >= 'A' && a[i] <= 'Z') ? static_cast<char>(a[i] - 'A' + 'a') : a[i];
        const char y = (b[i] >= 'A' && b[i] <= 'Z') ? static_cast<char>(b[i] - 'A' + 'a') : b[i];
        if (x != y)
            return false;
    }
    return true;
}

// Fills state.matches with the suggestions that match `query`, best first (fuzzy; among equal scores
// the shorter name, then the source order) or in source order (prefix). An empty query keeps every
// suggestion in order.
void FilterSuggestions(AutocompleteState& state, const AutocompleteSource& source, int count, std::string_view query,
                       bool fuzzy)
{
    state.matches.resize(0);
    state.ranges.resize(0);
    internal::TextRange ranges[internal::kFuzzyMaxRanges];
    for (int i = 0; i < count; ++i)
    {
        ChoiceItem item;
        if (!source.Get(i, &item))
            continue;
        const char* end = ImGui::FindRenderedTextEnd(item.label);
        const std::string_view label(item.label, static_cast<std::size_t>(end - item.label));
        AutocompleteMatch match;
        match.index = i;
        match.length = static_cast<int>(label.size());
        match.rangeStart = state.ranges.Size;
        if (query.empty())
        {
            state.matches.push_back(match);
            continue;
        }
        if (fuzzy)
        {
            const internal::FuzzyMatch scored = internal::FuzzyScore(query, label, ranges);
            if (!scored.matched)
                continue;
            match.score = scored.score;
            match.rangeCount = std::min(scored.rangeCount, internal::kFuzzyMaxRanges);
            for (int r = 0; r < match.rangeCount; ++r)
                state.ranges.push_back(ranges[r]);
        }
        else
        {
            if (label.size() < query.size() || !AutocompleteSameIgnoringCase(label.data(), query.data(), query.size()))
                continue;
            match.rangeCount = 1;
            state.ranges.push_back({0, static_cast<int>(query.size())});
        }
        state.matches.push_back(match);
    }
    if (fuzzy && !query.empty())
        std::sort(state.matches.begin(), state.matches.end(), [](const AutocompleteMatch& a, const AutocompleteMatch& b) {
            if (a.score != b.score)
                return a.score > b.score;
            return a.length != b.length ? a.length < b.length : a.index < b.index;
        });
}

// What the completion list's entry callback reads.
struct AutocompleteRows
{
    const AutocompleteSource* source = nullptr;
    const AutocompleteState* state = nullptr;
};

void AutocompleteRowEntry(void* user, int row, internal::text_inputs::CompletionEntry* out)
{
    const AutocompleteRows& rows = *static_cast<const AutocompleteRows*>(user);
    const AutocompleteMatch& match = rows.state->matches[row];
    ChoiceItem item;
    if (!rows.source->Get(match.index, &item))
        return;
    out->label = item.label;
    out->detail = item.detail;
    out->icon = item.icon;
    out->disabled = item.disabled;
    if (match.rangeCount > 0)
        out->matches = std::span<const internal::TextRange>(rows.state->ranges.Data + match.rangeStart,
                                                            static_cast<std::size_t>(match.rangeCount));
}

// The suggestion whose visible label equals `text` (ASCII case-insensitive), or -1.
int FindExactSuggestion(const AutocompleteSource& source, int count, std::string_view text, ChoiceItem* found)
{
    for (int i = 0; i < count; ++i)
    {
        ChoiceItem item;
        if (!source.Get(i, &item) || item.disabled)
            continue;
        const char* end = ImGui::FindRenderedTextEnd(item.label);
        const std::size_t length = static_cast<std::size_t>(end - item.label);
        if (length == text.size() && AutocompleteSameIgnoringCase(item.label, text.data(), length))
        {
            *found = item;
            return i;
        }
    }
    return -1;
}

// The caller's text as it is now.
std::string_view CurrentText(const std::string* text, const char* buffer, std::size_t bufferSize)
{
    if (text != nullptr)
        return *text;
    if (buffer == nullptr || bufferSize == 0)
        return {};
    const void* zero = std::memchr(buffer, 0, bufferSize);
    return std::string_view(buffer, zero != nullptr ? static_cast<std::size_t>(static_cast<const char*>(zero) - buffer)
                                                    : bufferSize);
}

bool AutocompleteCall(const char* label, std::string* text, char* buffer, std::size_t bufferSize,
                      const AutocompleteSource& source, int count, const AutocompleteOptions& options)
{
    AutocompleteAccepted& accepted = LastAccepted();
    accepted.frame = ImGui::GetFrameCount();
    accepted.index = -1;
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    count = std::max(0, count);
    const bool writable = text != nullptr ? true : (buffer != nullptr && bufferSize > 0);
    const bool canWrite = writable && !options.readOnly &&
                          (GImGui->CurrentItemFlags & ImGuiItemFlags_ReadOnly) == 0;
    const ImGuiID id = window->GetID(label);
    AutocompleteState& state = internal::GetState<AutocompleteState>(id);
    internal::text_inputs::CompletionList list(id);
    bool written = false;
    int acceptedIndex = -1;

    // The inline completion of the last frame: Tab, or Right with the caret at the end, takes it.
    if (options.inlineCompletion && canWrite && list.IsOpen() && list.Highlight() < 0 && ImGui::GetActiveID() == id &&
        !state.matches.empty())
    {
        int selStart = 0;
        int selEnd = 0;
        int caret = 0;
        const std::string_view now = CurrentText(text, buffer, bufferSize);
        const bool atEnd = internal::TextFieldGetSelection(id, &selStart, &selEnd, &caret) && selStart == selEnd &&
                           caret == static_cast<int>(now.size());
        ChoiceItem best;
        if (atEnd && !now.empty() && source.Get(state.matches[0].index, &best) && !best.disabled)
        {
            const char* end = ImGui::FindRenderedTextEnd(best.label);
            const std::size_t length = static_cast<std::size_t>(end - best.label);
            const bool continues = length > now.size() && AutocompleteSameIgnoringCase(best.label, now.data(), now.size());
            const bool tab = ImGui::IsKeyPressed(ImGuiKey_Tab, ImGuiInputFlags_None, list.Level());
            const bool right = ImGui::IsKeyPressed(ImGuiKey_RightArrow, false);
            if (continues && (tab || right))
            {
                written |= internal::text_inputs::WriteFieldText(text, buffer, bufferSize, best.label, end);
                acceptedIndex = state.matches[0].index;
                list.CloseAccepted();
            }
        }
    }
    if (acceptedIndex < 0)
        list.BeforeField();

    internal::TextFieldParams params;
    params.placeholder = options.placeholder;
    params.completionPopup = list.Level();
    params.size = options.size;
    params.width = options.width;
    params.readOnly = options.readOnly;
    params.mixed = options.mixed;
    params.error = options.error;
    params.helpText = options.helpText;
    const internal::TextFieldResult field = text != nullptr ? internal::TextFieldBehavior(label, text, params)
                                                            : internal::TextFieldBehavior(label, buffer, bufferSize, params);
    written |= field.edited && canWrite;

    // Strict mode remembers the text the session started from.
    if (field.active && !state.wasActive && options.strict)
    {
        try
        {
            state.activationText.assign(CurrentText(text, buffer, bufferSize));
        }
        catch (...)
        {
            state.activationText.clear();
        }
    }
    state.wasActive = field.active;

    // The list: every suggestion that matches the text, while the field has the keyboard.
    const std::string_view query = CurrentText(text, buffer, bufferSize);
    const bool forcedOpen = Has(field.forced, internal::ForcedItemState::Open);
    if (field.active || forcedOpen || list.IsOpen())
        FilterSuggestions(state, source, count, query, options.fuzzy);
    const int shown = options.state == DataState::Ready ? state.matches.Size : 0;
    AutocompleteRows rows{&source, &state};
    internal::text_inputs::CompletionDesc desc;
    desc.count = shown;
    desc.entry = AutocompleteRowEntry;
    desc.user = &rows;
    desc.wanted = field.active && canWrite && (options.state != DataState::Ready || shown > 0 || !query.empty());
    desc.state = options.state;
    desc.emptyText = options.emptyText;
    desc.errorText = options.errorText;
    desc.maxVisibleRows = options.maxVisibleRows;
    desc.size = options.size;
    desc.forcedOpen = forcedOpen;
    desc.holdTab = options.inlineCompletion;
    const int row = list.Run(field, desc);
    if (row >= 0 && row < state.matches.Size && canWrite)
    {
        ChoiceItem item;
        if (source.Get(state.matches[row].index, &item))
        {
            written |= internal::text_inputs::WriteFieldText(text, buffer, bufferSize, item.label,
                                                             ImGui::FindRenderedTextEnd(item.label));
            acceptedIndex = state.matches[row].index;
        }
    }

    // The inline completion: the rest of the best match after the caret, in labelSecondary.
    if (options.inlineCompletion && field.active && list.IsOpen() && list.Highlight() < 0 && !query.empty() &&
        !state.matches.empty() && options.state == DataState::Ready)
    {
        ChoiceItem best;
        int selStart = 0;
        int selEnd = 0;
        int caret = 0;
        if (source.Get(state.matches[0].index, &best) && !best.disabled &&
            internal::TextFieldGetSelection(id, &selStart, &selEnd, &caret) && selStart == selEnd &&
            caret == static_cast<int>(query.size()))
        {
            const char* end = ImGui::FindRenderedTextEnd(best.label);
            const std::size_t length = static_cast<std::size_t>(end - best.label);
            if (length > query.size() && AutocompleteSameIgnoringCase(best.label, query.data(), query.size()))
            {
                const internal::ControlTextScope style(ControlTextStyle(ResolveControlSize(options.size)));
                ImDrawList* drawList = window->DrawList;
                drawList->PushClipRect(field.text.Min, field.text.Max, true);
                drawList->AddText(ImVec2(field.caret.Max.x, field.caret.Min.y), ToU32(T().colors.labelSecondary),
                                  best.label + query.size(), end);
                drawList->PopClipRect();
            }
        }
    }

    // Strict: a session that ended on text matching no suggestion gets its starting text back; a
    // match that differs only in case takes the suggestion's spelling.
    // A suggestion taken during the session is the text strict mode goes back to.
    if (options.strict && acceptedIndex >= 0)
    {
        try
        {
            state.activationText.assign(CurrentText(text, buffer, bufferSize));
        }
        catch (...)
        {
        }
    }
    // While a row is held the list is still open: the release that accepts it decides.
    if (options.strict && canWrite && field.committed && acceptedIndex < 0 && !list.IsOpen())
    {
        ChoiceItem item;
        const int exact = FindExactSuggestion(source, count, CurrentText(text, buffer, bufferSize), &item);
        if (exact >= 0)
        {
            written |= internal::text_inputs::WriteFieldText(text, buffer, bufferSize, item.label,
                                                             ImGui::FindRenderedTextEnd(item.label));
            acceptedIndex = exact;
        }
        else
        {
            written |= internal::text_inputs::WriteFieldText(text, buffer, bufferSize, state.activationText.c_str());
        }
    }

    accepted.index = acceptedIndex;
    return written;
}

} // namespace

bool Autocomplete(const char* label, std::string* text, std::span<const ChoiceItem> suggestions,
                  const AutocompleteOptions& options)
{
    AutocompleteSource source;
    source.items = suggestions;
    return AutocompleteCall(label, text, nullptr, 0, source, static_cast<int>(suggestions.size()), options);
}

bool Autocomplete(const char* label, std::string* text, std::span<const char* const> suggestions,
                  const AutocompleteOptions& options)
{
    AutocompleteSource source;
    source.labels = suggestions;
    return AutocompleteCall(label, text, nullptr, 0, source, static_cast<int>(suggestions.size()), options);
}

bool Autocomplete(const char* label, std::string* text, int suggestionCount, ChoiceGetter getter, void* user,
                  const AutocompleteOptions& options)
{
    AutocompleteSource source;
    source.getter = getter;
    source.user = user;
    return AutocompleteCall(label, text, nullptr, 0, source, getter != nullptr ? suggestionCount : 0, options);
}

bool Autocomplete(const char* label, char* buffer, std::size_t bufferSize, std::span<const ChoiceItem> suggestions,
                  const AutocompleteOptions& options)
{
    AutocompleteSource source;
    source.items = suggestions;
    return AutocompleteCall(label, nullptr, buffer, bufferSize, source, static_cast<int>(suggestions.size()), options);
}

bool Autocomplete(const char* label, char* buffer, std::size_t bufferSize, std::span<const char* const> suggestions,
                  const AutocompleteOptions& options)
{
    AutocompleteSource source;
    source.labels = suggestions;
    return AutocompleteCall(label, nullptr, buffer, bufferSize, source, static_cast<int>(suggestions.size()), options);
}

bool Autocomplete(const char* label, char* buffer, std::size_t bufferSize, int suggestionCount, ChoiceGetter getter,
                  void* user, const AutocompleteOptions& options)
{
    AutocompleteSource source;
    source.getter = getter;
    source.user = user;
    return AutocompleteCall(label, nullptr, buffer, bufferSize, source, getter != nullptr ? suggestionCount : 0, options);
}

int AutocompleteAcceptedItem()
{
    const AutocompleteAccepted& accepted = LastAccepted();
    if (ImGui::GetCurrentContext() == nullptr || accepted.frame != ImGui::GetFrameCount())
        return -1;
    return accepted.index;
}

} // namespace facet
