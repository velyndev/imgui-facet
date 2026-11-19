#pragma once

// Text field core: the one call every text-editing widget makes (TextField, SecureField,
// SearchField, TextArea, NumberInput, the masked, path, tag and filter fields, Autocomplete, the
// search fields of dropdowns and the command palette). It wraps Dear ImGui's own text editing,
// so IME, undo, the clipboard, selection, word moves and key repeat stay Dear ImGui's, and draws
// the Facet field around it: frame, placeholder, leading icon, suffix, clear button, loading
// spinner, error outline and the message line.
//
// This header holds what the widgets and the other blocks call; the popup host places
// completion lists and pickers with TextFieldPopupAnchor. The text field core owns the rest and
// adds it to this file and to further text_field_* files.
//
// Writes follow docs/api-conventions.md rule 3: per keystroke unless the caller pushed
// ImGuiItemFlags_LiveEditOnInputText off; Escape restores the text from activation, writes it
// and ends the edit (the active edit takes Escape first, rule 8). While `completionPopup` names
// an open popup level, Up, Down, Enter, Tab and Escape go to that level first, so a completion
// list can be operated while typing goes on in the field.
//
//     const TextFieldResult field = TextFieldBehavior(label, text, {.placeholder = options.placeholder,
//                                                                  .completionPopup = listId,
//                                                                  .width = options.width});
//     if (field.edited)
//         OpenPopupLevel(listId, TextFieldPopupAnchor(field, TextFieldAnchor::Field), {.takeFocus = false});
//
// After the call the field is Dear ImGui's last item (rule 11), and IsItemEditCommitted and
// IsItemEditCanceled (common.h) report its edit session.

#include <cstddef>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "facet/widgets/common.h"
#include "internal/popup_host.h"

namespace facet::internal
{

// Everything a field widget passes in. Lengths are design units like the option structs they
// come from; members not set keep the plain single-line field.
struct TextFieldParams
{
    const char* placeholder = nullptr;   // labelSecondary while the text is empty; null for none
    Icon icon;                           // leading icon; none by default
    const char* suffix = nullptr;        // text after the value inside the frame (a unit); null for none
    bool clearButton = false;            // a clear button while the text is not empty
    bool password = false;               // characters shown as dots; copying is off
    bool multiline = false;              // several lines; Enter inserts a line break
    int maxLength = 0;                   // code points; 0 unlimited
    bool selectAllOnFocus = false;       // keyboard focus selects the whole text
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_None;  // extra Dear ImGui flags (character filters, callbacks)
    ImGuiInputTextCallback callback = nullptr;             // Dear ImGui's callback with `flags`; null for none
    void* callbackUser = nullptr;
    ImGuiID completionPopup = 0;         // an open popup level that takes the list keys first (see the top); 0 for none
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                  // units; the width precedence of rule 6, the visible label included
    float height = 0.0f;                 // units; multiline only; 0 for three lines
    bool readOnly = false;               // shown, selectable and copyable, never written (rule 7)
    bool loading = false;                // a trailing spinner; the text stays editable
    bool mixed = false;                  // shows Strings::mixed while empty, until the first edit
    const char* error = nullptr;         // error outline and message under the field; replaces helpText
    const char* helpText = nullptr;
};

// What a field did this frame, and where it is.
struct TextFieldResult
{
    bool edited = false;        // the text was written this frame (typing, paste, clear, Escape restoring it)
    bool submitted = false;     // Enter was pressed in a single-line field
    bool clearPressed = false;  // the clear button emptied the text (edited is true as well)
    bool active = false;        // the field has the keyboard
    ImRect frame;               // the field frame in screen pixels, without the label and the message line
    ImRect caret;               // the caret line in screen pixels while active; a zero-width rect at the text start otherwise
};

// The field: the visible label part in front of the frame (rule 6), the frame and the text.
// `text` is resized as needed (null: the field is drawn read-only and returns nothing edited).
TextFieldResult TextFieldBehavior(const char* label, std::string* text, const TextFieldParams& params = {});
// The same for a fixed buffer of `bufferSize` bytes, zero terminated.
TextFieldResult TextFieldBehavior(const char* label, char* buffer, std::size_t bufferSize,
                                  const TextFieldParams& params = {});

// What a popup that belongs to a field lines up with.
enum class TextFieldAnchor
{
    Field,  // below the frame, at least as wide as it (completion lists, pickers)
    Caret,  // below the caret line, at the caret (completion of the word being typed)
};

// The popup anchor (popup_host.h) for a level that belongs to the field `field` came from.
PopupAnchor TextFieldPopupAnchor(const TextFieldResult& field, TextFieldAnchor at);

} // namespace facet::internal
