#pragma once

// Helpers shared by the text-like inputs of inputs.h (TextField, SecureField, SearchField,
// TextArea, Autocomplete, CopyField, FilterField, TagInput, InlineRename, MaskedInput,
// PinCodeInput, PathField). They sit on the text field core (src/internal/text_field.h); the
// names live in facet::internal::text_inputs so they never meet the helpers of the number-like
// inputs in the same directory.

#include <cstddef>
#include <span>
#include <string>

#include <imgui.h>

#include "facet/core/icons.h"
#include "facet/widgets/common.h"
#include "internal/text_field.h"
#include "internal/text_util.h"

namespace facet::internal::text_inputs
{

// True when a std::string or buffer form has somewhere to write (not null, not empty).
inline bool TextWritable(const std::string* text)
{
    return text != nullptr;
}
inline bool TextWritable(const char* buffer, std::size_t bufferSize)
{
    return buffer != nullptr && bufferSize > 0;
}

// Replaces the caller's text with `value` (a std::string when `text` is set, otherwise the
// buffer, cut at a whole code point to fit with its terminator). Returns true when the text changed.
// Allocation failures keep the old text.
bool WriteFieldText(std::string* text, char* buffer, std::size_t bufferSize, const char* value);
// The same for [value, valueEnd).
bool WriteFieldText(std::string* text, char* buffer, std::size_t bufferSize, const char* value, const char* valueEnd);

// The Dear ImGui callback events a widget forwards (inputs.h "Text callbacks"); every other bit
// of `events` is dropped.
ImGuiInputTextFlags TextCallbackEvents(ImGuiInputTextFlags events);

// Whether Caps Lock is on, as far as the input Dear ImGui received tells (Facet reads input only
// from ImGuiIO, which has no lock state): a letter typed in upper case without Shift, or in lower
// case with Shift, means on; the opposite means off; a press of the Caps Lock key flips what is
// known. Unknown, and so false, until the first letter. Updated once per frame by the text inputs'
// frame hook (text_inputs_frame.cpp).
bool CapsLockLikelyOn();
void UpdateCapsLockGuess();
void ResetCapsLockGuess();

// ---- Completion list ---------------------------------------------------------------------------
//
// The open list under a field that keeps the keyboard (the recent entries of SearchField,
// Autocomplete, the suggestions of TagInput), on a completion level of the popup host and a driven
// list view:
//
//     CompletionList list(ImGui::GetID(label));
//     list.BeforeField();                                   // Enter or Tab with no active row go to the field
//     const TextFieldResult field = TextFieldBehavior(label, text, {.completionPopup = list.Level()});
//     const int accepted = list.Run(field, {.count = n, .entry = &EntryOf, .user = &data,
//                                           .wanted = field.active && n > 0});
//     if (accepted >= 0) ... write the entry's label ...
//
// While the list is open the field keeps the keyboard: Up, Down, Page Up and Page Down move the
// active row, Enter and Tab accept it (with no active row they close the list and reach the field),
// Escape closes the list (the next Escape cancels the edit). The pointer over a row makes it the
// active row; a click accepts it and gives the keyboard back to the field with the caret at the end.
// After Escape, Enter, Tab or a choice the list stays closed until the text is edited again. The
// level follows the field when it scrolls or the window resizes.

// One row of a completion list.
struct CompletionEntry
{
    const char* label = nullptr;   // shown; "##" hides the rest
    const char* detail = nullptr;  // secondary text at the end of the row; null for none
    Icon icon;                     // leading icon; none by default
    bool disabled = false;
    std::span<const TextRange> matches;  // byte ranges of the label drawn as matched; empty for none
};

// Fills `out` (default-initialized) for row `row`. Called during CompletionList::Run only; the
// strings and ranges must stay valid until the next call.
using CompletionEntryFn = void (*)(void* user, int row, CompletionEntry* out);

struct CompletionDesc
{
    int count = 0;                       // rows
    CompletionEntryFn entry = nullptr;
    void* user = nullptr;
    bool wanted = false;                 // the list should be open (the field has the keyboard and there is something to show)
    DataState state = DataState::Ready;  // Loading and Error show in the open list
    const char* emptyText = nullptr;     // shown with no rows in Ready; null uses Strings::noResults
    const char* errorText = nullptr;     // null uses Strings::loadFailed
    int maxVisibleRows = 0;              // 0 for list.popupRows
    ControlSize size = ControlSize::Inherit;
    bool forcedOpen = false;             // a capture's Open state: shown for this frame without input
    bool holdTab = false;                // with no active row Tab stays with the list (an inline completion takes it)
};

class CompletionList
{
public:
    // `fieldId`: the text item's id (ImGui::GetID of the label, as the core computes it).
    explicit CompletionList(ImGuiID fieldId);
    // The popup level, for TextFieldParams::completionPopup.
    ImGuiID Level() const { return level_; }
    // Before the field call: Enter or Tab while no row is active close the list, so they reach the
    // field.
    void BeforeField();
    // Opens, places, draws and closes the list. Returns the row accepted this frame, or -1.
    int Run(const TextFieldResult& field, const CompletionDesc& desc);
    // Closes the list after the widget took an entry itself (an inline completion), with the caret
    // moved to the end of the field; it stays closed until the next edit.
    void CloseAccepted();
    // The active row, -1 for none.
    int Highlight() const;
    // True while the list is open.
    bool IsOpen() const;

private:
    ImGuiID field_ = 0;
    ImGuiID level_ = 0;
};

} // namespace facet::internal::text_inputs
