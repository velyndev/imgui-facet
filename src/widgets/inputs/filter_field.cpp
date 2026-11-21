// FilterField and PassesFilter. FilterField is a compact search-style field on the text field core
// with the filter icon, a clear button, Escape that first empties the query, and the match case and
// whole word toggles in the trailing slot. PassesFilter is the matching every filtered view shares.

#include "facet/widgets/inputs.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/control.h"
#include "internal/item.h"
#include "internal/text_field.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet
{
namespace
{

// Letters, digits, '_' and every byte of a multi-byte UTF-8 character count as word characters,
// so whole-word matching never splits a non-ASCII word.
bool PassesFilterWordByte(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c >= 0x80;
}

char PassesFilterFold(char c)
{
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Whether `word` occurs in `text` (as a whole word with `wholeWord`).
bool PassesFilterFind(std::string_view word, std::string_view text, const FilterSettings& settings)
{
    if (word.size() > text.size())
        return false;
    for (std::size_t start = 0; start + word.size() <= text.size(); ++start)
    {
        bool same = true;
        for (std::size_t i = 0; i < word.size() && same; ++i)
        {
            const char a = text[start + i];
            const char b = word[i];
            same = settings.matchCase ? a == b : PassesFilterFold(a) == PassesFilterFold(b);
        }
        if (!same)
            continue;
        if (!settings.wholeWord)
            return true;
        const std::size_t end = start + word.size();
        const bool openStart = start == 0 || !PassesFilterWordByte(static_cast<unsigned char>(text[start - 1])) ||
                               !PassesFilterWordByte(static_cast<unsigned char>(word.front()));
        const bool openEnd = end == text.size() || !PassesFilterWordByte(static_cast<unsigned char>(text[end])) ||
                             !PassesFilterWordByte(static_cast<unsigned char>(word.back()));
        if (openStart && openEnd)
            return true;
    }
    return false;
}

// What the trailing slot reports back to the widget.
struct FilterFieldToggles
{
    FilterSettings settings;
    bool off = false;  // read-only: the toggles show the settings and change nothing
    ImGuiID matchCaseId = 0;
    ImGuiID wholeWordId = 0;
    bool matchCasePressed = false;
    bool wholeWordPressed = false;
    bool pointer = false;  // the press came from the pointer
};

void FilterFieldToggleButtons(const internal::TextFieldSlot& slot, void* user)
{
    FilterFieldToggles& t = *static_cast<FilterFieldToggles*>(user);
    const Strings& strings = GetStrings();
    // Submitted left to right, so Tab reaches them in reading order.
    const internal::ItemState matchCase = internal::TextFieldSlotButton(
        slot, 1, t.matchCaseId, Icon(),
        {.tabStop = true, .selected = t.settings.matchCase, .disabled = t.off, .name = strings.matchCase, .glyph = "Aa"});
    const internal::ItemState wholeWord = internal::TextFieldSlotButton(
        slot, 0, t.wholeWordId, Icon(),
        {.tabStop = true, .selected = t.settings.wholeWord, .disabled = t.off, .name = strings.wholeWord, .glyph = "ab",
         .glyphUnderline = true});
    t.matchCasePressed = matchCase.pressed;
    t.wholeWordPressed = wholeWord.pressed;
    t.pointer = (matchCase.pressed && !matchCase.keyboardPressed) || (wholeWord.pressed && !wholeWord.keyboardPressed);
}

template <class Call>
bool FilterFieldCall(const char* label, bool writable, FilterSettings* settings, const FilterFieldOptions& options,
                     Call call)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const ImGuiID id = window->GetID(label);
    const bool readOnly = options.readOnly || (GImGui->CurrentItemFlags & ImGuiItemFlags_ReadOnly) != 0;

    FilterFieldToggles toggles;
    internal::TextFieldParams params;
    params.placeholder = options.placeholder != nullptr ? options.placeholder : GetStrings().search;
    params.icon = Icon("filter");
    params.clearButton = true;
    params.escapeClears = true;
    params.size = options.size;
    params.width = options.width;
    params.readOnly = options.readOnly;
    if (settings != nullptr)
    {
        toggles.settings = *settings;
        toggles.off = readOnly;
        toggles.matchCaseId = ImGui::GetIDWithSeed("matchCase", nullptr, id);
        toggles.wholeWordId = ImGui::GetIDWithSeed("wholeWord", nullptr, id);
        params.trailingWidth = internal::TextFieldSlotButtonsWidth(2, options.size);
        params.trailingSlot = FilterFieldToggleButtons;
        params.trailingUser = &toggles;
    }
    const internal::TextFieldResult field = call(label, params);
    bool written = field.edited && writable && !readOnly;

    if (settings != nullptr && !readOnly && (toggles.matchCasePressed || toggles.wholeWordPressed))
    {
        if (toggles.matchCasePressed)
            settings->matchCase = !settings->matchCase;
        if (toggles.wholeWordPressed)
            settings->wholeWord = !settings->wholeWord;
        written = true;
        // A click gives the keyboard back to the query, so typing goes on.
        if (toggles.pointer)
            internal::TextFieldRequestFocus(id, internal::TextFieldFocus::Keep);
    }
    return written;
}

} // namespace

bool FilterField(const char* label, std::string* query, FilterSettings* settings, const FilterFieldOptions& options)
{
    return FilterFieldCall(label, internal::text_inputs::TextWritable(query), settings, options,
                           [&](const char* l, const internal::TextFieldParams& p) {
                               return internal::TextFieldBehavior(l, query, p);
                           });
}

bool FilterField(const char* label, char* buffer, std::size_t bufferSize, FilterSettings* settings,
                 const FilterFieldOptions& options)
{
    return FilterFieldCall(label, internal::text_inputs::TextWritable(buffer, bufferSize), settings, options,
                           [&](const char* l, const internal::TextFieldParams& p) {
                               return internal::TextFieldBehavior(l, buffer, bufferSize, p);
                           });
}

bool PassesFilter(std::string_view query, const FilterSettings& settings, std::string_view text)
{
    std::size_t start = 0;
    while (start < query.size())
    {
        while (start < query.size() && (query[start] == ' ' || query[start] == '\t'))
            ++start;
        std::size_t end = start;
        while (end < query.size() && query[end] != ' ' && query[end] != '\t')
            ++end;
        if (end > start && !PassesFilterFind(query.substr(start, end - start), text, settings))
            return false;
        start = end;
    }
    return true;
}

} // namespace facet
