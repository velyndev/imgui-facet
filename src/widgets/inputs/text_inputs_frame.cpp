// Per-frame and per-device work of the text-like inputs of inputs.h (TextField, SecureField,
// SearchField, TextArea, Autocomplete, CopyField, FilterField, TagInput, InlineRename,
// MaskedInput, PinCodeInput, PathField, FieldValidation). The entry exists in the subsystem table
// (src/core/subsystems.cpp) from the start, so these widgets fill their hooks here without
// touching a shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsTextInputsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_text_inputs";
    return subsystem;
}

} // namespace facet::internal
