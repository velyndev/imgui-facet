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

A TextField that shows the font's bullet for every character. Copy and cut are off, and they stay
off while the text is shown too, so the secret never reaches the clipboard; the text never appears
in a tooltip, and a capture shows only dots. `reveal` picks the eye button at the end of the frame:
`Toggle` (a Tab stop; a click or Space switches between dots and text, a click gives the keyboard
back to the field), `Hold` (the text shows while the button is held) or `None` (no button). The
shown state resets to dots as soon as the focus leaves both the field and its button.

Under the frame: the strength meter when `strength` is 0 or more (a progressHeightSmall track the
frame's width, filled to the value in destructive below a third, warning below two thirds and success
above), then one message line: the error, else the Caps Lock hint, else the help text. Dear ImGui
reports no lock state, so the hint follows the typed letters: an upper-case letter without Shift (or a
lower-case one with Shift) turns it on, the opposite turns it off, and a press of the Caps Lock key
flips it; it shows only while the field has the keyboard, and it adds a line where there was none.
Writing, Escape, `readOnly`, disabled, width and sizes are as for TextField. Status: ready.

### SearchField

```cpp
bool SearchField(const char* label, std::string* query, const SearchFieldOptions& options = {});
bool SearchField(const char* label, char* buffer, std::size_t bufferSize, const SearchFieldOptions& options = {});
```

A TextField with the search icon in front, the placeholder `Strings::search` by default and a clear
button while there is a query. Escape empties a non-empty query (an edit, not a cancel); with the
query empty, Escape ends the session. While `loading`, the spinner takes the place of the search
icon and the query stays editable. The corners are the control radius of every field.

`recent` entries (read during the call) are listed under the field while it has the keyboard and
the query is empty, with the clock icon. The list is a completion list that leaves the keyboard in
the field: Up and Down (Page Up and Page Down by a page) move the active row, Enter or Tab writes it
into the query, a click writes it and gives the keyboard back to the field with the caret at the end,
and the pointer and the keys share one active row. With no active row, Enter and Tab close the list
and act on the field as usual. Escape closes the list; it stays closed until the query is edited. The
list follows the field when the window scrolls or resizes. `focusShortcut` names a shortcut the host
registered (core/input.h); when it fires, the field takes the keyboard with the query selected.
`readOnly` shows no list. Status: ready.

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

A TextField with the matching suggestions in a list under it while it has the keyboard (every
suggestion while the text is empty). `fuzzy` ranks them with the fuzzy matcher and marks the matched
characters on an accentSubtle background; with it off, suggestions that start with the text keep
their order. A suggestion's icon and `detail` show in its row; disabled ones are shown and skipped.
The list keeps the keyboard in the field, exactly as the recent list of SearchField: Up and Down (Page
Up and Page Down by a page) move the active row, Enter, Tab or a click writes the suggestion's visible
label and gives the keyboard back with the caret at the end, the pointer and the keys share one active
row, Enter and Tab with no active row act on the field, and Escape closes the list (the next Escape
restores the text from activation). The list follows the field on scroll and resize; with no match it
shows `emptyText`, and with `state` Loading or Error it shows that state while the text stays
editable. `AutocompleteAcceptedItem()` returns the index of the suggestion taken by the last call of
the frame, or -1.

`inlineCompletion` draws the rest of the best suggestion after the caret in labelSecondary while the
list is open, no row is active and the text is a prefix of it; Tab, or Right with the caret at the end,
takes it. `strict` puts back the text from activation (or the suggestion taken last in the edit)
when an edit ends on text that matches no suggestion, and gives a match that differs only in case the
suggestion's spelling. Writing, `readOnly`
(no list), `mixed`, error and help text, sizes and width are as for TextField. The getter form is
called during the call for every suggestion while the list is open, so a very large source costs a
scan per frame. Status: ready.

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

Multi-line text on the text field core in Body style, with the visible label above the frame (it
takes the whole width; the frame keeps it too). Enter inserts a line break, Ctrl+Enter ends the
edit; `allowTab` makes Tab insert a tab instead of moving the focus. Lines wrap at the area width
unless `wordWrap` is off (then the text scrolls sideways). The frame shows `minLines` lines; with
`maxLines` above it, the area grows with its visual lines (wrapping included) up to that many and
then scrolls. `resizable` adds a short bar centered on the bottom edge: dragging it changes the
height (never below `minLines`), and the arrow keys reach it, where Up and Down change the height by
a line; the height set this way is kept under the id and wins over the growth. Under the frame one
line holds the error or help text at the start and, with `counter`, the character count at the end
("n / maxLength" when limited). `maxLength` also cuts a paste. Writing, Escape, `readOnly`, `mixed`,
disabled and the buffer form are as for TextField; the width fills the row by default. There is no
`size` option: the area uses the Regular paddings. Status: ready.

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

A read-only TextField showing `text` (read during the call; the field shows it in place and copies
nothing per frame). The text can be focused, selected with the mouse and the keys, and copied as in
any field; it is never written. A copy button sits in the trailing slot inside the frame: a Tab
stop after the text, activated by a click, Space or Enter. It copies the whole text through
`ImGui::SetClipboardText` and shows the check icon, with `Strings::copied` as its tooltip, for
`motion.feedbackHold` seconds. Ctrl+C (or Ctrl+Insert) in the field with nothing selected copies the
whole text the same way; with a selection it copies the selection as a stock field does and does not
count as a copy of the field. Returns true on the frame the text was copied.

`masked` shows dots and adds a reveal button (eye, a Tab stop) before the copy button; the text
goes back to dots when the focus leaves the field and its buttons, and the copy button and Ctrl+C
still copy the real text. `multiline` wraps the text and makes the frame as tall as the wrapped
lines (it follows the text one frame later); a masked field stays on one line. `loading` draws a
skeleton bar in place of the text and turns the buttons off. A null or empty `text` shows an empty
field whose buttons are off. The label sits in front, the width fills the row by default, and the
sizes, the error and help line and the disabled state are those of TextField. The copy button is
the text field core's slot button, not `CopyButton` from `buttons.h`, so it lines up with the
other buttons inside fields. Status: ready.

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

`FieldValidation` draws a message under the last item, for items without an `error` option (stock
Dear ImGui widgets, groups, custom items). The message is Footnote text in the tone's color with
the tone's icon (error, warning, success, info for Accent; Neutral is plain help text in
labelSecondary), starting at the item's left edge spacing.xs below it and wrapping at the right edge
of the content region; the cursor moves past it. With `outline` (the default) the item gets a
hairline in the tone's color around its rect, at the control radius; Neutral draws none. The
message is not an item, so item queries after the call still describe the validated field. The
item is found by its rect, so groups and text items (id 0 while idle) work; first thing in a window,
the message starts at the cursor. Destructive messages are reported to the error store that
ValidationSummary reads, with the item's id (0 for a group). A null or empty message draws nothing.

`ValidationSummary` lists the Destructive messages the fields of its window (its root window, so
child windows count) reported in the previous frame: through the `error` option of Facet widgets
and through `FieldValidation`. It is a block on the destructiveSubtle wash with the visible label
part as a Headline heading and one row per message: the error icon and the message as an
underlined link in label color, cut with an ellipsis when narrow (the full text shows on hover and
keyboard focus). Each row is a Tab stop; a click, Enter or Space scrolls the field's window to it
and gives the field the keyboard focus (a text field also takes the keyboard with its text
selected). A message reported without an id is scrolled to but not focused. The summary keeps
nothing under its id and submits no item while there is no message; because it reads the previous
frame, it appears and disappears one frame after the fields change. Returns true when a row was
activated. The width fills the row by default. Status: ready.

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
