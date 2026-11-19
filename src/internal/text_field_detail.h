#pragma once

// Text field core, module internals: the one entry point behind both TextFieldBehavior
// overloads, with the hooks the number layer (text_field_number.cpp) needs. Widgets call the
// functions of text_field.h and text_field_number.h, never these.

#include <cstddef>
#include <string>

#include <imgui.h>

#include "internal/item.h"
#include "internal/text_field.h"

namespace facet::internal
{

struct TextFieldHooks
{
    // Writes per keystroke: -1 as the caller's item flags say (ImGuiItemFlags_LiveEditOnInputText),
    // 0 only on Enter, Tab or focus loss, 1 per keystroke.
    int liveEdit = -1;
    // Reload the edit text when the caller changed the buffer while the field has the keyboard.
    // Off for buffers the module owns itself (the number text).
    bool trackExternalChanges = true;
    // Take the keyboard in this call, as a click would (a drag field entering typing).
    bool activate = false;
    // No text input this frame: the text area is a drag part with the field's id, and the shown
    // text (displayText, else the buffer) is drawn by the core. *dragItem receives its state.
    bool dragMode = false;
    ItemState* dragItem = nullptr;
    // The visible label in front of the frame is a drag part too (id "label" under the field);
    // *labelItem receives its state. Without a visible label it stays empty.
    bool dragLabel = false;
    ItemState* labelItem = nullptr;
};

// Both TextFieldBehavior overloads end here: `text` for the string form, otherwise `buffer`
// and `bufferSize`.
TextFieldResult TextFieldCall(const char* label, std::string* text, char* buffer, std::size_t bufferSize,
                              const TextFieldParams& params, const TextFieldHooks& hooks);

} // namespace facet::internal
