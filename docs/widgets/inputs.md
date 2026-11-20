# Inputs

Header: `facet/widgets/inputs.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Each entry below ends with
the widget's status. A **planned** widget exists as a function that draws a placeholder of the
widget's default size, returns the neutral result and writes nothing; a **ready** one works as its
header describes. See [the widget index](../widgets.md). The demo gallery shows the text-like
inputs on the page Text inputs and the number-like ones on the page Value inputs.

Text editing, IME, undo, selection and the clipboard are Dear ImGui's own (`InputTextEx`); Facet
draws the frame, the placeholder, icons and messages around it. Editable text comes as
`std::string*`, which Facet resizes, and as a `char*` buffer with its size for hosts that avoid the
heap. Where an options struct has `callback`, `callbackUser` and `callbackEvents`, Dear ImGui's text
callback runs for the `ImGuiInputTextFlags_Callback*` events named there (Completion, History,
Always, CharFilter, Edit).

Typed text is written per keystroke; typed numbers on Enter, Tab or focus loss; steppers, drags and
list choices at once. Escape restores the value from activation. `facet::IsItemEditCommitted()`
after the call reports one finished edit.

## Widgets

### TextField

```cpp
bool TextField(const char* label, std::string* text, const TextFieldOptions& options = {});
bool TextField(const char* label, char* buffer, std::size_t bufferSize, const TextFieldOptions& options = {});
```

Single-line text on the text field core: Dear ImGui edits it (caret, selection with the mouse and
with Shift and the arrows, word moves with Ctrl, double click, undo and redo, the clipboard, key
repeat, IME), Facet draws the frame around it. The visible label part sits in front of the frame
and takes at most half the width; a longer label ends in an ellipsis and shows in full on hover
and keyboard focus. Options add a placeholder (labelSecondary), a leading icon, a clear button
while there is text (not a Tab stop; it empties the text, counts as one committed edit and gives
the keyboard back to the field), a limit in code points that also cuts a paste, select-all on
focus, a trailing spinner while `loading` (the text stays editable), the mixed placeholder until
the first edit, and the error or help line under the frame. The width follows rule 6 and fills the
row by default; the three control sizes set the height and the text style.

Typing writes per keystroke and returns true on each write (with
`ImGuiItemFlags_LiveEditOnInputText` pushed off, on Enter, Tab or focus loss only). Enter, Tab or a
click elsewhere ends the edit: `IsItemEditCommitted()` is true once if the text changed. Escape
restores the text from activation, writes it, returns true and reports `IsItemEditCanceled()`.
`readOnly` (or a pushed `ImGuiItemFlags_ReadOnly`) lets the text be focused, selected and copied but
never changed; a null `text` or an empty buffer shows an empty read-only field and returns false.
The buffer form cuts the text to fit and keeps it zero terminated. Inside `ImGui::BeginDisabled` the
field is not a Tab stop and takes no input. `callback` receives only the events named in
`callbackEvents`. Status: ready.

### SecureField

```cpp
bool SecureField(const char* label, std::string* text, const SecureFieldOptions& options = {});
bool SecureField(const char* label, char* buffer, std::size_t bufferSize, const SecureFieldOptions& options = {});
```

Masked text with a reveal button, a Caps Lock hint and a strength meter the caller computes.
Status: planned.

### SearchField

```cpp
bool SearchField(const char* label, std::string* query, const SearchFieldOptions& options = {});
bool SearchField(const char* label, char* buffer, std::size_t bufferSize, const SearchFieldOptions& options = {});
```

A search box with a clear button, recent entries, a busy spinner and an optional focus shortcut.
Status: planned.

### FilterField

```cpp
struct FilterSettings { bool matchCase; bool wholeWord; };
bool FilterField(const char* label, std::string* query, FilterSettings* settings,
                 const FilterFieldOptions& options = {});
bool FilterField(const char* label, char* buffer, std::size_t bufferSize, FilterSettings* settings,
                 const FilterFieldOptions& options = {});
bool PassesFilter(std::string_view query, const FilterSettings& settings, std::string_view text);
```

A compact field for a filter query the caller keeps, with match case and whole word toggles when
`settings` is not null; `PassesFilter` tests a text against the query. Status: planned (until then
`PassesFilter` lets every text pass). `FilterSettings` and `PassesFilter` are declared in `common.h`, because the
property grid, keybind list and settings search take the same settings through `filterSettings`.

### Autocomplete

```cpp
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
int AutocompleteAcceptedItem();
```

A text field with a list of matching suggestions, fuzzy or by prefix, with an optional inline
completion. Status: planned.

### TagInput

```cpp
bool TagInput(const char* label, std::vector<std::string>* tags, const TagInputOptions& options = {});
```

A field that turns entries into removable chips, with suggestions. Status: planned.

### TextArea

```cpp
bool TextArea(const char* label, std::string* text, const TextAreaOptions& options = {});
bool TextArea(const char* label, char* buffer, std::size_t bufferSize, const TextAreaOptions& options = {});
```

Multi-line text that wraps, grows between line limits and can show a counter. Status: planned.

### InlineRename

```cpp
bool InlineRename(const char* id, std::string* text, bool* editing, const InlineRenameOptions& options = {});
bool InlineRename(const char* id, char* buffer, std::size_t bufferSize, bool* editing,
                  const InlineRenameOptions& options = {});
```

Text that becomes a field on double click or F2 and writes once on Enter or focus loss.
Status: planned.

### NumberInput

```cpp
bool NumberInput(const char* label, int* value, const NumberInputOptions& options = {});
bool NumberInput(const char* label, float* value, const NumberInputOptions& options = {});
bool NumberInput(const char* label, double* value, const NumberInputOptions& options = {});
```

A number field with steppers, limits and a unit suffix. Status: planned.

### DragNumber

```cpp
bool DragNumber(const char* label, int* value, const DragNumberOptions& options = {});
bool DragNumber(const char* label, float* value, const DragNumberOptions& options = {});
bool DragNumber(const char* label, double* value, const DragNumberOptions& options = {});
```

A number that changes when dragged sideways; a click types a value. Status: planned.

### UnitInput

```cpp
struct UnitItem { const char* label; double factor; };
bool UnitInput(const char* label, double* value, int* unit, std::span<const UnitItem> units,
               const UnitInputOptions& options = {});
```

A number in base units shown in a chosen unit, with a unit menu and typed suffixes such as "250ms".
Status: planned.

### VectorInput

```cpp
bool VectorInput(const char* label, std::span<float> components, const VectorInputOptions& options = {});
bool VectorInput(const char* label, std::span<int> components, const VectorInputOptions& options = {});
bool VectorInput(const char* label, ImVec2* value, const VectorInputOptions& options = {});
bool VectorInput(const char* label, ImVec4* value, const VectorInputOptions& options = {});
```

One to four drag fields side by side with axis labels and an optional proportional lock.
Status: planned.

### SizeInput

```cpp
bool SizeInput(const char* label, ImVec2* value, const SizeInputOptions& options = {});
bool SizeInput(const char* label, int* x, int* y, const SizeInputOptions& options = {});
```

Width and height fields with an aspect lock, a swap button and size presets. Status: planned.

### HexInput

```cpp
bool HexInput(const char* label, std::uint32_t* value, const HexInputOptions& options = {});
bool HexInput(const char* label, std::uint64_t* value, const HexInputOptions& options = {});
```

A fixed-width hexadecimal field in overtype mode. Status: planned.

### MaskedInput

```cpp
bool MaskedInput(const char* label, std::string* text, const char* mask, const MaskedInputOptions& options = {});
bool MaskedInput(const char* label, char* buffer, std::size_t bufferSize, const char* mask,
                 const MaskedInputOptions& options = {});
```

A field constrained by a mask of digit, letter and literal slots. Status: planned.

### PinCodeInput

```cpp
bool PinCodeInput(const char* label, std::string* code, const PinCodeInputOptions& options = {});
bool PinCodeInput(const char* label, char* buffer, std::size_t bufferSize, const PinCodeInputOptions& options = {});
```

A one-time code as separate cells that fill as the user types or pastes. Status: planned.

### DurationInput

```cpp
bool DurationInput(const char* label, double* seconds, const DurationInputOptions& options = {});
```

A segmented hours, minutes, seconds (and milliseconds) field over a number of seconds.
Status: planned.

### PathField

```cpp
bool PathField(const char* label, std::string* path, const PathFieldOptions& options = {});
bool PathField(const char* label, char* buffer, std::size_t bufferSize, const PathFieldOptions& options = {});
bool PathFieldBrowsePressed();
```

A path field that shortens long paths in the middle and has a browse button; the caller opens the
dialog. Status: planned.

### CopyField

```cpp
bool CopyField(const char* label, const char* text, const CopyFieldOptions& options = {});
```

A read-only value with a copy button, optionally masked. Status: planned.

### KeybindCapture

```cpp
bool KeybindCapture(const char* label, Keybind* bind, const KeybindCaptureOptions& options = {});
```

Records a key bind (`Keybind` from `core/input.h`): chords, mouse and gamepad buttons, a primary and
an alternate slot, and the bind mode. Status: planned.

### Rating

```cpp
bool Rating(const char* label, float* value, const RatingOptions& options = {});
bool Rating(const char* label, int* value, const RatingOptions& options = {});
```

A row of icons filled up to the value, with half steps and a read-only display. Status: planned.

### FieldValidation

```cpp
void FieldValidation(const char* message, const FieldValidationOptions& options = {});
bool ValidationSummary(const char* label, const ValidationSummaryOptions& options = {});
```

A message under the last item, for items without an `error` option, and a summary of the errors
in a window whose entries focus their fields. The message is not an item, so item queries after
`FieldValidation` still describe the validated field. Status: planned.

## Usage

Text with a callback. The `std::string` form needs no buffer management; the callback receives only
the events named in `callbackEvents`.

```cpp
static std::string name;
facet::TextField("Name", &name, {.placeholder = "Placeholder", .maxLength = 64});

static std::string command;
facet::TextField("Command", &command, {.callback = CompleteCommand, .callbackUser = &commands,
                                       .callbackEvents = ImGuiInputTextFlags_CallbackCompletion});
```

Suggestions from a large source. The getter fills one `ChoiceItem` per index during the call;
`state` shows the list's loading state while the source is still fetching and its error state when
fetching failed; the text stays editable in both, and `error` stays the validation message of the
typed text.

```cpp
static std::string city;
facet::Autocomplete("Place", &city, static_cast<int>(results.size()),
                    [](void* user, int index, facet::ChoiceItem* out) {
                        out->label = (*static_cast<std::vector<std::string>*>(user))[index].c_str();
                        return true;
                    },
                    &results, {.state = searchState});  // facet::DataState of the search
if (const int picked = facet::AutocompleteAcceptedItem(); picked >= 0)
    Select(picked);
```

A filter over a list. The query and its settings are the caller's; a host without heap use passes
a `char` buffer instead of the `std::string`.

```cpp
static std::string query;
static facet::FilterSettings match;
facet::FilterField("##filter", &query, &match);
for (const Entry& entry : entries)
    if (facet::PassesFilter(query, match, entry.name))
        DrawEntry(entry);
```

Rename in place, started from a context menu by setting the flag.

```cpp
static std::string layerName = "Option 1";
static bool renaming = false;
facet::InlineRename("layer", &layerName, &renaming);
if (facet::BeginContextMenu("layer menu"))
{
    if (facet::MenuItem("Rename"))
        renaming = true;
    facet::EndContextMenu();
}
```

A duration stored in seconds, shown in the unit the user picks.

```cpp
static const facet::UnitItem kTime[] = {{"ms", 0.001}, {"s", 1.0}, {"min", 60.0}};
static double timeout = 0.25;  // seconds
static int timeoutUnit = 0;    // shown in ms
facet::UnitInput("Timeout", &timeout, &timeoutUnit, kTime, {.min = 0.0});
```

A path with the caller's own dialog, here Facet's in-app browser in a modal.

```cpp
static std::string path;
static bool browsing = false;
facet::PathField("Folder", &path);
if (facet::PathFieldBrowsePressed())
    browsing = true;
if (facet::BeginModal("Choose a folder", &browsing))
{
    if (facet::FileBrowser("##browser", &path, {.mode = facet::FileBrowserMode::Folder}))
        browsing = false;
    facet::EndModal();
}
```

A stock Dear ImGui field with a Facet message, and the summary at the top of the form.

```cpp
facet::ValidationSummary("Problems");
ImGui::InputInt("Port", &port);
const bool valid = port >= 1 && port <= 65535;
if (!valid)
    facet::FieldValidation("Use a port from 1 to 65535");
if (valid && ImGui::IsItemDeactivatedAfterEdit())  // still the port field
    SavePort(port);
```

## Planned, not declared yet

These widgets of the later tier are not declared yet.

- **MentionInput**: a text field where a trigger character opens a picker that inserts chips.
- **EditableHeading**: a large title edited in place that wraps instead of scrolling.
- **IPAddressInput**: four octet segments and an optional port with auto-advance and paste.
- **CodeEditor**: a monospace editor with a gutter, indentation and bracket matching.
- **FilterField regular expressions**: a regex toggle for FilterField; it needs a matcher that
  neither allocates per frame nor throws.
