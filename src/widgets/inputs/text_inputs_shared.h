#pragma once

// Helpers shared by the text-like inputs of inputs.h (TextField, SecureField, SearchField,
// TextArea, Autocomplete, CopyField, FilterField, TagInput, InlineRename, MaskedInput,
// PinCodeInput, PathField). They sit on the text field core (src/internal/text_field.h); the
// names live in facet::internal::text_inputs so they never meet the helpers of the number-like
// inputs in the same directory.

#include <cstddef>
#include <string>

#include <imgui.h>

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

} // namespace facet::internal::text_inputs
