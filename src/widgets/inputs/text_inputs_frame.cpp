// Per-frame and per-device work of the text-like inputs of inputs.h (TextField, SecureField,
// SearchField, TextArea, Autocomplete, CopyField, FilterField, TagInput, InlineRename,
// MaskedInput, PinCodeInput, PathField, FieldValidation). The entry exists in the subsystem table
// (src/core/subsystems.cpp). Each frame it updates the Caps Lock guess of SecureField from the
// input Dear ImGui received.

#include "internal/subsystem.h"
#include "widgets/inputs/text_inputs_shared.h"

namespace facet::internal
{

Subsystem WidgetsTextInputsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_text_inputs";
    subsystem.init = [] {
        text_inputs::ResetCapsLockGuess();
        return true;
    };
    subsystem.shutdown = [] { text_inputs::ResetCapsLockGuess(); };
    subsystem.newFrame = [] { text_inputs::UpdateCapsLockGuess(); };
    return subsystem;
}

} // namespace facet::internal
