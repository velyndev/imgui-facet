#pragma once

// Inputs: text, secure, search and multi-line fields, autocomplete and tag input, number, drag,
// unit and vector fields, masked and special-purpose fields, key bind capture and ratings.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families. The family is planned: its widgets are declared here and implemented in
// Phase 2 (docs/widgets/inputs.md). Until then every function draws a placeholder of the
// widget's default size, returns false and writes nothing.
//
// Common to every widget here:
//   - Text editing, IME, undo, selection and the clipboard are Dear ImGui's own (InputTextEx in
//     imgui_internal.h); Facet draws the frame, the placeholder, icons and messages around it.
//   - The bool result is true on the frame the value was written. Typed text writes per
//     keystroke (ImGuiItemFlags_LiveEditOnInputText, on by default); typed numbers write on
//     Enter, Tab or focus loss (ImGuiItemFlags_LiveEditOnInputScalar, off by default); steppers,
//     drags, arrow keys and list choices write at once. Escape during an edit restores the value
//     from activation, writes it and returns true. facet::IsItemEditCommitted() after the call
//     reports one finished edit session.
//   - Editable text comes as std::string* (resized by Facet) and as a char buffer with its size
//     in bytes (the text is cut to fit, always zero terminated) for hosts that avoid the heap.
//   - The visible label part is drawn in front of the field, inside the widget width (rule 6).
//     Use "##name" in settings rows and table cells.
//   - A null value pointer draws the widget read-only and returns false.
//   - Disabled: ImGui::BeginDisabled. readOnly shows the value, lets it be focused, selected and
//     copied, and never writes.
//   - Lengths are design units (multiplied by Px()).
//
// Text callbacks: where an options struct has `callback`, Dear ImGui calls it during the widget
// call for the ImGuiInputTextFlags_Callback* events named in `callbackEvents` (Completion,
// History, Always, CharFilter, Edit), with `callbackUser` as UserData. Other bits are ignored;
// CallbackResize is Facet's own for std::string.

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "facet/core/icons.h"
#include "facet/core/input.h"
#include "facet/widgets/common.h"

namespace facet
{

// --- TextField --------------------------------------------------------------------------------

struct TextFieldOptions
{
    const char* placeholder = nullptr;  // labelSecondary while empty
    Icon icon;                          // leading icon
    bool clearButton = false;           // clear button while the text is not empty
    int maxLength = 0;                  // code points; 0 unlimited
    bool selectAllOnFocus = false;
    ImGuiInputTextCallback callback = nullptr;  // see "Text callbacks" above
    void* callbackUser = nullptr;
    ImGuiInputTextFlags callbackEvents = ImGuiInputTextFlags_None;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fills the available width
    bool readOnly = false;
    bool loading = false;               // trailing spinner; the text stays editable
    bool mixed = false;                 // empty with Strings::mixed as placeholder until edited
    const char* error = nullptr;        // replaces helpText
    const char* helpText = nullptr;
};

// Single-line text. Writes per keystroke unless the caller pushed
// ImGuiItemFlags_LiveEditOnInputText off. Enter ends the session (IsItemEditCommitted), Escape
// restores the text from activation. Returns true when the text was written. Keeps the edit
// state in Dear ImGui's text input state under its id.
bool TextField(const char* label, std::string* text, const TextFieldOptions& options = {});
bool TextField(const char* label, char* buffer, std::size_t bufferSize, const TextFieldOptions& options = {});

// --- SecureField ------------------------------------------------------------------------------

enum class SecureReveal
{
    Toggle,  // an eye button switches between dots and text; the default
    Hold,    // the text shows while the eye button is held
    None,    // always dots
};

struct SecureFieldOptions
{
    const char* placeholder = nullptr;
    SecureReveal reveal = SecureReveal::Toggle;
    bool capsLockHint = true;           // Strings::capsLock under the field while Caps Lock is on and it is focused
    float strength = -1.0f;             // 0 to 1, computed by the caller; a meter under the field; < 0 hides it
    int maxLength = 0;                  // code points; 0 unlimited
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A text field that shows dots instead of the text (ImGuiInputTextFlags_Password: copy and cut
// are off while masked). Writes per keystroke. Returns true when the text was written. Keeps
// the reveal state under its id; it resets to masked when the field loses focus.
bool SecureField(const char* label, std::string* text, const SecureFieldOptions& options = {});
bool SecureField(const char* label, char* buffer, std::size_t bufferSize, const SecureFieldOptions& options = {});

// --- SearchField ------------------------------------------------------------------------------

struct SearchFieldOptions
{
    const char* placeholder = nullptr;  // null uses Strings::search
    std::span<const char* const> recent = {};  // read during the call; listed under the empty focused field
    ImGuiKeyChord focusShortcut = ImGuiKey_None;  // focuses the field from anywhere in its window; none by default
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool loading = false;               // spinner in place of the search icon while results load
};

// A rounded field with a search icon and a clear button. Writes per keystroke; choosing a
// recent entry (arrows and Enter, or a click) writes it. Escape clears a non-empty query, then
// ends the session. Returns true when the query was written.
bool SearchField(const char* label, std::string* query, const SearchFieldOptions& options = {});
bool SearchField(const char* label, char* buffer, std::size_t bufferSize, const SearchFieldOptions& options = {});

// --- FilterField ------------------------------------------------------------------------------

// A filter the caller keeps: the query and its match settings. Plain data.
struct TextFilter
{
    std::string query;
    bool matchCase = false;
    bool wholeWord = false;
};

struct FilterFieldOptions
{
    const char* placeholder = nullptr;  // null uses Strings::search
    bool toggles = true;                // match case and whole word toggle buttons in the field
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
};

// A compact field that edits a TextFilter: the query per keystroke, the toggles on activation.
// Returns true when any part of *filter was written.
bool FilterField(const char* label, TextFilter* filter, const FilterFieldOptions& options = {});

// Whether `text` matches the filter: every space-separated word of the query occurs in it (as a
// whole word with wholeWord, ASCII case-insensitive unless matchCase). An empty query matches
// everything. No allocation; usable from any thread.
bool PassesFilter(const TextFilter& filter, std::string_view text);

// --- Autocomplete -----------------------------------------------------------------------------

struct AutocompleteOptions
{
    const char* placeholder = nullptr;
    bool fuzzy = true;                  // fuzzy matching with highlights; otherwise prefix matching
    bool inlineCompletion = false;      // the best match continues the text in labelSecondary; Tab or Right accepts it
    bool strict = false;                // on commit, text that matches no suggestion is restored
    int maxVisibleItems = 0;            // list height in rows; 0 for the default
    const char* emptyText = nullptr;    // null uses Strings::noResults
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool loading = false;               // the list shows the loading state (suggestions from a slow source)
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A text field with a list of matching suggestions under it while it is focused. Typing writes
// per keystroke; Down and Up move in the list, Enter or a click accepts a suggestion and writes
// its label, Escape closes the list (a second Escape ends the session). Returns true when the
// text was written. Read AutocompleteAcceptedItem after the call to learn which suggestion was
// taken. Suggestions are spans read during the call, or a count and a getter called during the
// call (for large or changing sources; set `loading` while they are fetched).
bool Autocomplete(const char* label, std::string* text, std::span<const ChoiceItem> suggestions,
                  const AutocompleteOptions& options = {});
bool Autocomplete(const char* label, std::string* text, std::span<const char* const> suggestions,
                  const AutocompleteOptions& options = {});
bool Autocomplete(const char* label, std::string* text, int suggestionCount, ChoiceGetter getter, void* user,
                  const AutocompleteOptions& options = {});
bool Autocomplete(const char* label, char* buffer, std::size_t bufferSize, std::span<const ChoiceItem> suggestions,
                  const AutocompleteOptions& options = {});
bool Autocomplete(const char* label, char* buffer, std::size_t bufferSize, std::span<const char* const> suggestions,
                  const AutocompleteOptions& options = {});
bool Autocomplete(const char* label, char* buffer, std::size_t bufferSize, int suggestionCount, ChoiceGetter getter,
                  void* user, const AutocompleteOptions& options = {});

// The suggestion accepted by the last Autocomplete call this frame, or -1.
int AutocompleteAcceptedItem();

// --- TagInput ---------------------------------------------------------------------------------

struct TagInputOptions
{
    const char* placeholder = nullptr;
    std::span<const char* const> suggestions = {};  // read during the call; listed while typing
    bool allowCreate = true;            // typed text that is no suggestion becomes a tag
    bool allowDuplicates = false;
    int maxTags = 0;                    // 0 unlimited
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fills; chips wrap into more rows
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A field that turns entries into removable chips, followed by a caret for the next one. Enter
// or a comma adds the typed text as a tag; Backspace in the empty field selects, then removes
// the last chip; Left and Right move between chips; Delete removes the selected chip. Returns
// true when *tags was written (a tag added or removed). Allocates when a tag is added.
bool TagInput(const char* label, std::vector<std::string>* tags, const TagInputOptions& options = {});

// --- TextArea ---------------------------------------------------------------------------------

struct TextAreaOptions
{
    const char* placeholder = nullptr;
    int minLines = 3;                   // visible lines while the text is shorter
    int maxLines = 0;                   // > minLines: grows with the text up to this many lines, then scrolls; 0: fixed
    bool wordWrap = true;               // ImGuiInputTextFlags_WordWrap
    bool allowTab = false;              // Tab inserts a tab instead of moving focus
    bool counter = false;               // character count under the field ("n / maxLength" when limited)
    bool resizable = false;             // a grip drags the height; the height is kept under the id
    int maxLength = 0;                  // code points; 0 unlimited
    ImGuiInputTextCallback callback = nullptr;  // see "Text callbacks" above
    void* callbackUser = nullptr;
    ImGuiInputTextFlags callbackEvents = ImGuiInputTextFlags_None;
    float width = 0.0f;                 // units; 0 fills the available width
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Multi-line text in Body style (ImGui::InputTextMultiline underneath). Enter inserts a line
// break; Ctrl+Enter ends the session. Writes per keystroke. Returns true when the text was
// written. The visible label part is drawn above the field.
bool TextArea(const char* label, std::string* text, const TextAreaOptions& options = {});
bool TextArea(const char* label, char* buffer, std::size_t bufferSize, const TextAreaOptions& options = {});

// --- InlineRename -----------------------------------------------------------------------------

struct InlineRenameOptions
{
    bool startOnDoubleClick = true;
    bool startOnF2 = true;              // F2 while the text item is focused
    bool selectStem = false;            // starting selects the name without its extension
    int maxLength = 0;                  // code points; 0 unlimited
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fits the text while not editing, fills while editing
    bool readOnly = false;              // never starts editing
    const char* error = nullptr;
};

// Text that turns into a field in place. The edit works on a copy: Enter or focus loss writes
// the copy to *text once, Escape drops it, so the shown name never changes half typed. This is
// a deliberate departure from per-keystroke writing. `editing` (may be null, then the state is
// kept under the id) reads and writes whether the field is open; the caller may set it to start
// editing, for example from a context menu. Returns true when *text was written.
bool InlineRename(const char* id, std::string* text, bool* editing, const InlineRenameOptions& options = {});
bool InlineRename(const char* id, char* buffer, std::size_t bufferSize, bool* editing,
                  const InlineRenameOptions& options = {});

// --- NumberInput ------------------------------------------------------------------------------

struct NumberInputOptions
{
    const char* format = nullptr;       // printf; null: "%d" (int) or "%.3g"
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    double min = -std::numeric_limits<double>::infinity();
    double max = std::numeric_limits<double>::infinity();
    double step = 1.0;                  // steppers and arrow keys; 0 hides the steppers
    double stepFast = 10.0;             // Ctrl or Page Up and Down
    const char* unit = nullptr;         // suffix text such as "ms"
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;                 // empty field until edited
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A number field with - and + steppers. Typed text is written on Enter, Tab or focus loss and
// clamped to [min, max]; steppers (held: repeating) and Up and Down write at once. Returns true
// when *value was written.
bool NumberInput(const char* label, int* value, const NumberInputOptions& options = {});
bool NumberInput(const char* label, float* value, const NumberInputOptions& options = {});
bool NumberInput(const char* label, double* value, const NumberInputOptions& options = {});

// --- DragNumber -------------------------------------------------------------------------------

struct DragNumberOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    double min = -std::numeric_limits<double>::infinity();
    double max = std::numeric_limits<double>::infinity();
    double speed = 0.0;                 // value units per unit of pointer travel; 0 derives it from the range (int: 1)
    ValueScale scale = ValueScale::Linear;  // Logarithmic needs a finite range with min > 0
    bool wrap = false;                  // a finite range wraps around instead of stopping
    const char* unit = nullptr;         // suffix text
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A number field that changes when dragged sideways (Shift fine, Ctrl coarse); a click without
// a drag, or Enter, types a value. Writes while dragging; typed values on Enter, Tab or focus
// loss. Escape restores the value from activation. Returns true when *value was written.
bool DragNumber(const char* label, int* value, const DragNumberOptions& options = {});
bool DragNumber(const char* label, float* value, const DragNumberOptions& options = {});
bool DragNumber(const char* label, double* value, const DragNumberOptions& options = {});

// --- UnitInput --------------------------------------------------------------------------------

// One unit of a UnitInput: its suffix and how many base units one of it is. Label first, so
// {"ms", 0.001} works.
struct UnitItem
{
    const char* label = nullptr;        // suffix shown and parsed ("ms"); required
    double factor = 1.0;                // base units per one of this unit
};

struct UnitInputOptions
{
    const char* format = nullptr;       // for the number in the selected unit
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    double min = -std::numeric_limits<double>::infinity();  // base units
    double max = std::numeric_limits<double>::infinity();   // base units
    double step = 1.0;                  // in the selected unit
    double stepFast = 10.0;
    bool convert = true;                // switching the unit keeps the quantity; false keeps the number
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A number field with a unit menu at its end. *value is always in base units; *unit is the
// index of the unit shown (may be null: the first unit, no menu). Typing a number with a known
// suffix ("250ms") switches the unit. Numbers write on Enter, Tab or focus loss; the menu
// writes *unit (and with convert off *value) at once. Returns true when *value or *unit was
// written. An empty `units` span shows the plain number.
bool UnitInput(const char* label, double* value, int* unit, std::span<const UnitItem> units,
               const UnitInputOptions& options = {});

// --- VectorInput ------------------------------------------------------------------------------

struct VectorInputOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    double min = -std::numeric_limits<double>::infinity();
    double max = std::numeric_limits<double>::infinity();
    double speed = 0.0;                 // as DragNumberOptions::speed
    std::span<const char* const> axisLabels = {};  // read during the call; empty: X, Y, Z, W
    bool linkToggle = false;            // a lock button; while locked an edit scales every component alike
    bool normalize = false;             // float: an edit rescales the vector to length 1
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                 // units; 0 fills; when narrow the fields stack
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// One to four DragNumber fields side by side, one per component, each with its axis label as a
// drag handle. Each field is a Tab stop. Returns true when a component was written. More than
// four components show the first four read-only. Keeps the link state under its id.
bool VectorInput(const char* label, std::span<float> components, const VectorInputOptions& options = {});
bool VectorInput(const char* label, std::span<int> components, const VectorInputOptions& options = {});

// --- SizeInput --------------------------------------------------------------------------------

struct SizeInputOptions
{
    const char* format = nullptr;
    ImVec2 min = {0.0f, 0.0f};
    ImVec2 max = {FLT_MAX, FLT_MAX};
    double step = 1.0;
    bool aspectLock = true;             // a lock button keeping the ratio while locked
    bool swapButton = false;            // a button that exchanges width and height
    std::span<const ImVec2> presets = {};  // read during the call; offered in a menu
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Width and height fields with the aspect lock between them. Typed values write on Enter, Tab or
// focus loss; the lock writes the other field in the same frame. Returns true when a value was
// written. Keeps the lock state under its id.
bool SizeInput(const char* label, ImVec2* value, const SizeInputOptions& options = {});
bool SizeInput(const char* label, int* x, int* y, const SizeInputOptions& options = {});

// --- HexInput ---------------------------------------------------------------------------------

struct HexInputOptions
{
    int digits = 0;                     // 0: 8 for 32-bit values, 16 for 64-bit values
    bool prefix = false;                // shows "0x" before the digits
    bool uppercase = true;
    bool groupBytes = false;            // a gap after every two digits
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A fixed-width hexadecimal field in overtype mode: typing replaces the digit under the caret,
// Up and Down change it by one. Typed digits write on Enter, Tab or focus loss; Up and Down at
// once. Returns true when *value was written.
bool HexInput(const char* label, std::uint32_t* value, const HexInputOptions& options = {});
bool HexInput(const char* label, std::uint64_t* value, const HexInputOptions& options = {});

// --- MaskedInput ------------------------------------------------------------------------------

struct MaskedInputOptions
{
    const char* placeholder = nullptr;  // shown in empty slots, one character; null uses "_"
    bool keepLiterals = false;          // the written text includes the literal characters of the mask
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A field constrained by `mask` (read during the call): "9" a digit, "A" a letter, "*" any
// character, "\" makes the next character literal, every other character is a literal the
// caret skips. *text holds the slot characters only, unless keepLiterals. Writes per keystroke.
// Returns true when the text was written. A null or empty mask behaves as TextField.
bool MaskedInput(const char* label, std::string* text, const char* mask, const MaskedInputOptions& options = {});
bool MaskedInput(const char* label, char* buffer, std::size_t bufferSize, const char* mask,
                 const MaskedInputOptions& options = {});

// --- PinCodeInput -----------------------------------------------------------------------------

struct PinCodeInputOptions
{
    int length = 6;                     // number of cells, 1 to 16
    bool numericOnly = true;
    bool masked = false;                // cells show dots
    int groupSize = 0;                  // a wider gap after every groupSize cells; 0 none
    ControlSize size = ControlSize::Inherit;
    bool readOnly = false;
    const char* error = nullptr;        // also shakes the cells once (motion permitting)
    const char* helpText = nullptr;
};

// A one-time code as separate cells. Typing fills a cell and moves on, Backspace moves back,
// pasting fills every cell. One Tab stop. Writes per keystroke; the code is complete when its
// length equals `length`. Returns true when the code was written. Default width: its cells.
bool PinCodeInput(const char* label, std::string* code, const PinCodeInputOptions& options = {});
bool PinCodeInput(const char* label, char* buffer, std::size_t bufferSize, const PinCodeInputOptions& options = {});

// --- DurationInput ----------------------------------------------------------------------------

struct DurationInputOptions
{
    bool showHours = true;
    bool showMilliseconds = false;
    double min = 0.0;                   // seconds; a negative min allows negative durations
    double max = std::numeric_limits<double>::infinity();  // seconds
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A segmented hh:mm:ss(.ms) field over *seconds. Each segment is dragged or stepped with Up and
// Down (carrying into the next segment) and writes at once; typed text such as "1m30" or
// "90" writes on Enter, Tab or focus loss. Returns true when *seconds was written.
bool DurationInput(const char* label, double* seconds, const DurationInputOptions& options = {});

// --- PathField --------------------------------------------------------------------------------

struct PathFieldOptions
{
    const char* placeholder = nullptr;
    bool browseButton = true;           // a trailing Strings::browse button; read PathFieldBrowsePressed
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    const char* error = nullptr;        // for example a path the caller found missing
    const char* helpText = nullptr;
};

// A text field for a file or folder path. While not focused, a long path is shortened in the
// middle so the file name stays visible. Writes per keystroke. Returns true when the path was
// written. Facet opens no dialog itself: a modal system dialog would stop the frame loop, and
// overlays cannot show one. When PathFieldBrowsePressed() is true, the caller opens its own
// dialog or a FileBrowser (pickers.h) and writes the result.
bool PathField(const char* label, std::string* path, const PathFieldOptions& options = {});
bool PathField(const char* label, char* buffer, std::size_t bufferSize, const PathFieldOptions& options = {});

// True when the browse button of the last PathField call this frame was activated.
bool PathFieldBrowsePressed();

// --- CopyField --------------------------------------------------------------------------------

struct CopyFieldOptions
{
    bool masked = false;                // shows dots and a reveal button; the copy is still the real text
    bool multiline = false;             // wraps the text over several lines
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool loading = false;               // a skeleton in place of the text; the button is off
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A read-only field showing `text` (read during the call) that can be selected, with a copy
// button at its end that turns into a check for a moment. Returns true on the frame the text
// was copied (button, or Ctrl+C with nothing selected). A null `text` shows an empty field.
bool CopyField(const char* label, const char* text, const CopyFieldOptions& options = {});

// --- KeybindCapture ---------------------------------------------------------------------------

struct KeybindCaptureOptions
{
    bool allowMouse = true;
    bool allowGamepad = false;
    bool allowModifierOnly = false;
    bool showMode = false;              // adds the BindMode selector
    bool showAlternate = false;         // adds the second slot
    const char* placeholder = nullptr;  // shown while unbound; null uses Strings::unbound
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    const char* error = nullptr;        // for example a conflict the caller found (FindChordConflict)
    const char* helpText = nullptr;
};

// Records a key bind (Keybind in core/input.h) and shows its chords as key caps. A click or
// Enter starts listening (Strings::listening); the next chord is written. While listening the
// widget owns every key and stays active, so io.WantCaptureKeyboard is true. Escape cancels (so
// Escape cannot be bound); Backspace or Delete while focused and not listening clears the slot.
// Returns true when *bind was written.
bool KeybindCapture(const char* label, Keybind* bind, const KeybindCaptureOptions& options = {});

// --- Rating -----------------------------------------------------------------------------------

struct RatingOptions
{
    int count = 5;                      // number of icons, 1 to 10
    Icon icon;                          // none: the built-in "star" and "star-filled" icons
    bool halfSteps = false;             // float overload: halves can be chosen
    bool clearable = true;              // choosing the current value again writes 0
    ControlSize size = ControlSize::Inherit;
    bool readOnly = false;              // also the way to display a rating without input
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A row of icons filled up to *value (0 to count). Hovering previews; a click writes. One Tab
// stop; Left and Right step, Home and End go to 0 and count. Returns true when *value was
// written.
bool Rating(const char* label, float* value, const RatingOptions& options = {});
bool Rating(const char* label, int* value, const RatingOptions& options = {});

// --- FieldValidation --------------------------------------------------------------------------

struct FieldValidationOptions
{
    Tone tone = Tone::Destructive;      // Destructive: error, Warning, Success, Neutral: help text
    bool outline = true;                // also outlines the last item in the tone's color
};

// A message under the last item, for items without an `error` option (stock Dear ImGui
// widgets, groups, custom items). Draws `message` (read during the call) in Footnote in the
// tone's color under the item's rect and advances the cursor past it. Destructive messages are
// collected for ValidationSummary. Keeps no state of its own. A null message draws nothing.
void FieldValidation(const char* message, const FieldValidationOptions& options = {});

struct ValidationSummaryOptions
{
    float width = 0.0f;                 // units; 0 fills the available width
};

// A list of the Destructive messages that the fields of the current window reported in the
// previous frame (through `error` options or FieldValidation), each a link that focuses and
// scrolls to its field. Draws nothing while there are none. Returns true when an entry was
// activated.
bool ValidationSummary(const char* label, const ValidationSummaryOptions& options = {});

} // namespace facet
