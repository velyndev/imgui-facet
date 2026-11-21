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
// adds it to this file and to further text_field_* files (number editing: text_field_number.h).
//
// Writes follow docs/api-conventions.md rule 3: per keystroke unless the caller pushed
// ImGuiItemFlags_LiveEditOnInputText off; Escape restores the text from activation, writes it
// and ends the edit (the active edit takes Escape first, rule 8). While `completionPopup` names
// an open popup level, Up, Down, Page Up, Page Down, Enter, Tab and Escape go to that level first
// (owner: the level id), so a completion list can be operated while typing goes on in the field: a
// BeginListView inside the level with ListViewDesc::keyOwner set to the level id and a highlight
// reads them. Home and End stay with the caret.
//
//     const TextFieldResult field = TextFieldBehavior(label, text, {.placeholder = options.placeholder,
//                                                                  .completionPopup = listId,
//                                                                  .width = options.width});
//     if (field.edited)
//         OpenPopupLevel(listId, TextFieldPopupAnchor(field, TextFieldAnchor::Field), {.takeFocus = false});
//
// After the call the field is Dear ImGui's last item (rule 11), and IsItemEditCommitted and
// IsItemEditCanceled (common.h) report its edit session.
//
// How the field is built, so a widget author knows what to expect:
//   - Dear ImGui's InputTextEx runs on the text area with the stock frame made invisible for the
//     call (frame and border colors cleared, the navigation highlight cleared); every style color,
//     style variable, font, clip rectangle and item flag the call pushes is popped before it
//     returns, also on its early returns, so the host's style is unchanged afterwards.
//   - The item id is ImGui::GetID(label) on the id stack of the call (TextFieldResult::id), so
//     ImGui::SetKeyboardFocusHere() before the call, ImGui::GetInputTextState(id) and
//     ImGui::ActivateItemByID(id) work as for a stock field.
//   - Text, selection and caret are Dear ImGui's drawing in token colors: label text,
//     accentSubtle selection, a label colored caret T().field.caretWidth wide (whole pixels).
//     Text that does not fit scrolls inside the text area and fades out over
//     T().scroll.edgeFade at the edge where it continues.
//   - The widget is one group: the visible label part in front (rule 6), the frame, the message
//     line under it (error or helpText). Adornments (clear button, slot buttons) are parts made
//     with PartBehavior (item.h), submitted after the text, so Tab reaches the text first.
//   - The clear button is not a Tab stop (arrows still reach it while the text is not being
//     edited); activating it empties the text, writes it, reports one committed edit (its own
//     press and release), and gives the keyboard back to the text with the caret at the end.
//   - Buffers: the std::string overload grows the string through Dear ImGui's resize callback;
//     the char buffer overload cuts the text to fit and keeps it zero terminated; a zero-size or
//     null buffer shows an empty read-only field. Nothing is allocated per frame while the field
//     is idle. Text the caller changes while the field has the keyboard replaces the edit text on
//     the next call (Dear ImGui's ReloadUserBuf), keeping the selection where it can.
//
// Geometry and control for other blocks (completion popups, tag chips, inline rename) are the
// functions after TextFieldPopupAnchor: caret and selection rectangles in screen pixels read from
// ImGuiInputTextState, setting the selection, inserting at the caret, requesting the keyboard.

#include <cstddef>
#include <span>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/icons.h"
#include "facet/widgets/common.h"
#include "internal/item.h"
#include "internal/popup_host.h"

namespace facet::internal
{

struct TextFieldSlot;

// Submits and draws the items of a field's trailing slot during the field call (see
// TextFieldSlot). Called once per call, after the text and the clear button, while the field's
// group is open; `user` is TextFieldParams::trailingUser.
using TextFieldSlotFn = void (*)(const TextFieldSlot& slot, void* user);

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
    // Members below were added by the text field core.
    // Single-line: the visible label part is drawn inside the frame where the placeholder would
    // be, and moves above the text with a spring while the field has the keyboard or text (the
    // placeholder then shows in its place). The frame is taller to hold both lines: at least
    // spacing.xs + a Caption line + a text line + spacing.sm.
    bool floatingLabel = false;
    bool revealed = false;               // password: the text shows instead of dots (the widget keeps this state)
    bool escapeClears = false;           // Escape first empties a non-empty text (an edit, not a cancel), then ends the session
    bool wordWrap = true;                // multiline: lines wrap at the text area width; false scrolls sideways
    const char* displayText = nullptr;   // shown instead of the text while the field does not have the keyboard; null shows the text
    float trailingWidth = 0.0f;          // units reserved at the end of the frame for the trailing slot; 0 for none
    TextFieldSlotFn trailingSlot = nullptr;  // fills the slot (unit menu, steppers, reveal toggle); null for none
    void* trailingUser = nullptr;
    // No frame: no fill, outline, hover look or focus ring, and no padding at the sides, for a
    // field inside a surface the widget draws itself (tag input, pin code cells, inline rename,
    // the search row of a list popup). The widget draws the focus indication.
    bool frameless = false;
    // The core draws nothing of the text: no text, placeholder, selection, caret or dots. Editing,
    // IME and the item queries stay; the widget draws from the geometry functions below (pin code
    // cells, a skeleton while loading).
    bool hideText = false;
    bool spinnerReplacesIcon = false;    // while loading, the spinner sits where the leading icon is (search fields)
    // The visible label part is not drawn: it only names the field (the item id stays ImGui::GetID
    // of the label). For widgets that draw the label elsewhere, such as above a text area.
    bool labelHidden = false;
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
    // Members below were added by the text field core.
    ImGuiID id = 0;             // the text item: ImGui::GetID(label) on the id stack of the call
    bool committed = false;     // facet::IsItemEditCommitted() after the call
    bool canceled = false;      // facet::IsItemEditCanceled() after the call
    bool hovered = false;       // the pointer is over the frame (adornments included)
    int length = 0;             // code points in the text after the call (for counters next to maxLength)
    ImRect text;                // the visible text area in screen pixels (inside the padding and adornments)
    ImRect clear;               // the clear button while shown; empty otherwise
    ImRect trailing;            // the trailing slot while reserved; empty otherwise
    // The forced state the field took (captures); Open is for widgets that show a popup.
    ForcedItemState forced = ForcedItemState::None;
    int lineCount = 1;          // multiline: visual lines of the text as laid out (wrapping included), for growing areas
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

// ---- The trailing slot ---------------------------------------------------------------------

// What a slot function gets: where to put its items and the state of the field around them.
// The slot is a strip at the end of the frame, TextFieldParams::trailingWidth units wide and as
// tall as the frame; the text never reaches into it.
struct TextFieldSlot
{
    ImRect rect;                // screen pixels
    ImGuiID fieldId = 0;        // the text item; give slot items ids under it (ImGui::GetID inside the call)
    bool fieldActive = false;   // the text has the keyboard
    bool disabled = false;      // inside ImGui::BeginDisabled
    bool readOnly = false;
    ControlSize size = ControlSize::Regular;  // resolved
    const ForcedItemRequest* forced = nullptr;  // the request the field took; pass it on to slot parts
};

struct TextFieldSlotButtonOptions
{
    bool repeat = false;        // pressed again while held (steppers)
    bool tabStop = false;       // a Tab stop after the text; otherwise reached only by the arrows
    bool selected = false;      // the on look (a reveal toggle that is on)
    bool disabled = false;      // disabled in addition to the field (a stepper at its end of the range)
    const char* name = nullptr; // tooltip and accessible name; required (Strings member text)
    // Members below were added for the filter field toggles.
    const char* glyph = nullptr;  // short text ("Aa") drawn in the field's font instead of the icon; null draws the icon
    bool glyphUnderline = false;  // a hairline under the glyph (the whole word toggle)
};

// A square icon button inside the slot: `index` 0 is the rightmost, 1 the one before it, and so
// on, each the control height minus spacing.xs on both sides, spacing.xs apart, centered on the
// frame. Built on PartBehavior with id `id`; for forced states it is part `index` + 1 (part 0 is
// the clear button, -1 the text). Draws a plain fill on hover and press, the icon at the field's
// text size in labelSecondary (label while selected or hovered), the disabled colors and its own
// focus ring, and shows `name` as the tooltip on hover. Returns the item state; act on `pressed`.
ItemState TextFieldSlotButton(const TextFieldSlot& slot, int index, ImGuiID id, Icon icon,
                              const TextFieldSlotButtonOptions& options = {});
// The width in units a slot of `count` slot buttons needs at control size `size`, for
// TextFieldParams::trailingWidth.
float TextFieldSlotButtonsWidth(int count, ControlSize size);

// ---- Geometry and control for other blocks -------------------------------------------------

// The caret line of field `id` in screen pixels while it has the keyboard, read from
// ImGuiInputTextState and the geometry of its last call (horizontal and vertical scroll,
// multiline wrapping, the password advance); false and an empty rect otherwise.
bool TextFieldCaretRect(ImGuiID id, ImRect* out);

// The selected text of field `id` as screen rectangles, one per visual line, in the order of
// the text. Returns how many rectangles the selection has (also when `out` is smaller; only that
// many are written); 0 while the field does not have the keyboard or nothing is selected.
int TextFieldSelectionRects(ImGuiID id, std::span<ImRect> out);

// The selection and caret of field `id` as byte offsets into its text while it has the
// keyboard: start <= end. Returns false (and writes nothing) otherwise.
bool TextFieldGetSelection(ImGuiID id, int* start, int* end, int* caret);

// Asks field `id` to select bytes [start, end) with the caret at `end` (start > end puts the
// caret at the start). Applied in the field's next call while it has the keyboard, clamped to
// the text and moved back to code point boundaries. A request waits at most two frames for the
// field to take the keyboard (pair it with TextFieldRequestFocus), then it is dropped.
void TextFieldSetSelection(ImGuiID id, int start, int end);

// Asks field `id` to replace its selection with `text` (copied), or to insert it at the caret,
// as a paste would: one undo step, maxLength and the buffer size cut it to fit, newlines become
// spaces in a single-line field. Not applied to read-only fields. Character filters
// (TextFieldParams::flags) do not run on it: the caller inserts what it means to insert. Waits
// like TextFieldSetSelection.
void TextFieldInsert(ImGuiID id, const char* text);

// How a field that receives the keyboard by request starts.
enum class TextFieldFocus
{
    SelectAll,  // the whole text selected (as after Tab)
    End,        // the caret after the last character
    Keep,       // the selection and caret of the last edit of this field, if Dear ImGui still has them
};

// Gives field `id` the keyboard in its next call, as a click would, with the selection of
// `select`. Works without ImGuiConfigFlags_NavEnableKeyboard. A request waits like
// TextFieldSetSelection.
void TextFieldRequestFocus(ImGuiID id, TextFieldFocus select = TextFieldFocus::SelectAll);

// Drops every pending selection, insert and focus request (facet::Init and Shutdown).
void ResetTextFieldRequests();

} // namespace facet::internal
