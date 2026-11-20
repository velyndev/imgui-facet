// SearchField: a field with the search icon (the spinner while results load), a clear button,
// Escape that first empties the query, the recent entries in a completion list under the empty
// focused field, and an optional registered shortcut that focuses it. Built on the text field core
// and the completion list of the text inputs (text_inputs_shared.h).

#include "facet/widgets/inputs.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/input.h"
#include "internal/control.h"
#include "internal/text_field.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet
{
namespace
{

void SearchRecentEntry(void* user, int row, internal::text_inputs::CompletionEntry* out)
{
    const std::span<const char* const>& recent = *static_cast<const std::span<const char* const>*>(user);
    out->label = recent[static_cast<std::size_t>(row)];
    out->icon = Icon("clock");
}

template <class Call>
bool SearchFieldCall(const char* label, std::string* text, char* buffer, std::size_t bufferSize, bool writable,
                     const SearchFieldOptions& options, Call call)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const ImGuiID id = window->GetID(label);
    const bool canWrite = writable && !options.readOnly;
    const bool hasRecent = !options.recent.empty();
    internal::text_inputs::CompletionList list(id);
    if (hasRecent)
        list.BeforeField();

    // A registered shortcut focuses the field with its query selected, as Tab would.
    if (options.focusShortcut.IsValid() && ShortcutPressed(options.focusShortcut))
        internal::TextFieldRequestFocus(id, internal::TextFieldFocus::SelectAll);

    internal::TextFieldParams params;
    params.placeholder = options.placeholder != nullptr ? options.placeholder : GetStrings().search;
    params.icon = Icon("search");
    params.clearButton = true;
    params.escapeClears = true;
    params.spinnerReplacesIcon = true;
    params.completionPopup = hasRecent ? list.Level() : 0;
    params.size = options.size;
    params.width = options.width;
    params.readOnly = options.readOnly;
    params.loading = options.loading;
    const internal::TextFieldResult field = call(label, params);
    bool written = field.edited && canWrite;

    if (hasRecent)
    {
        std::span<const char* const> recent = options.recent;
        internal::text_inputs::CompletionDesc desc;
        desc.count = static_cast<int>(recent.size());
        desc.entry = SearchRecentEntry;
        desc.user = &recent;
        desc.wanted = field.active && field.length == 0 && canWrite;
        desc.size = options.size;
        desc.forcedOpen = Has(field.forced, internal::ForcedItemState::Open);
        const int accepted = list.Run(field, desc);
        if (accepted >= 0 && canWrite)
            written |= internal::text_inputs::WriteFieldText(text, buffer, bufferSize,
                                                             recent[static_cast<std::size_t>(accepted)]);
    }
    return written;
}

} // namespace

bool SearchField(const char* label, std::string* query, const SearchFieldOptions& options)
{
    return SearchFieldCall(label, query, nullptr, 0, internal::text_inputs::TextWritable(query), options,
                           [&](const char* l, const internal::TextFieldParams& p) {
                               return internal::TextFieldBehavior(l, query, p);
                           });
}

bool SearchField(const char* label, char* buffer, std::size_t bufferSize, const SearchFieldOptions& options)
{
    return SearchFieldCall(label, nullptr, buffer, bufferSize, internal::text_inputs::TextWritable(buffer, bufferSize),
                           options, [&](const char* l, const internal::TextFieldParams& p) {
                               return internal::TextFieldBehavior(l, buffer, bufferSize, p);
                           });
}

} // namespace facet
