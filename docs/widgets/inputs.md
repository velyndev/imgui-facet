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

A field for a filter query the caller keeps, on the text field core: the filter icon in front, the
placeholder `Strings::search` by default, a clear button while there is a query, and Escape that
first empties a non-empty query (an edit) and then ends the session. With non-null `settings` two
toggle buttons sit at the end of the frame: "Aa" for match case and an underlined "ab" for whole
word, with `Strings::matchCase` and `Strings::wholeWord` as their tooltips. Each is a Tab stop after
the text; a click, Space or Enter flips its member of `*settings` at once and the on state shows a
fill and the label color; a click gives the keyboard back to the query. Returns true when the query
or `*settings` was written. `readOnly` shows the query and the toggles and changes neither. Width,
sizes, disabled and the buffer form are as for TextField.

`PassesFilter` is true when every space-separated word of the query occurs in the text: ASCII
case-insensitive unless `matchCase`; with `wholeWord` an occurrence must not touch a letter, digit,
'_' or non-ASCII character on either side (a word that itself starts or ends with punctuation may
touch them there). Any occurrence counts, not only the first. An empty query, or one of spaces only,
lets every text pass. It allocates nothing. `FilterSettings` and `PassesFilter` are declared in
`common.h`, because the property grid, keybind list and settings search take the same settings
through `filterSettings`. Status: ready.

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

A field frame holding the tags as chips (`facet::Chip` from `selection.h`, one size smaller than the
field, with their remove button unless read-only or disabled) followed by a frameless text part for
the next entry. The chips flow into rows and wrap; the text part takes the rest of the last row, or
a row of its own when less than `spacing.xxxl` (or the width of the typed text, when that is wider)
is left, and the frame grows with the rows
(one row is one control height). The label sits in front and the width fills the row by default.

The text part keeps the keyboard and is the widget's only Tab stop; the chips are not Tab stops.
Enter adds the typed text (trimmed) and keeps the keyboard in the field; Enter with nothing typed
ends the session. Every comma, typed or pasted, ends an entry, so pasting "a, b, c" adds three tags.
Backspace in the empty text selects the last chip and a second Backspace removes it; Left at the
start of the text selects the last chip, Left and Right move the selection, Right past the last chip
returns to the text, and Delete or Backspace removes the selected chip; typing or Escape clears the
selection. A click on a chip selects it, its remove button removes it, and a click on the frame
outside the chips gives the text the keyboard. `suggestions` that match the typed text (fuzzy,
best first, without the tags already there unless `allowDuplicates`) open in a completion list under
the frame, operated as in SearchField. With `allowCreate` off only a suggestion becomes a tag, in
the suggestion's spelling; without `allowDuplicates` a tag equal to an existing one (ASCII case
aside) is refused; at `maxTags` nothing more is added. A refused entry stays in the text. Returns
true when `*tags` changed (several tags added in one frame are one write). `readOnly`, a null `tags`
or a pushed `ImGuiItemFlags_ReadOnly` show the chips without remove buttons and add nothing; the
error or help line sits under the frame. A tag containing "##" shows only the part before it,
because the chip treats its text as a label. The chips are the selection family's `Chip`.
Status: ready.

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

While idle, the text as one item in the control text style with spacing.sm on each side: as wide as
the text by default (or `width` units, then cut with an ellipsis and shown in full on hover and
keyboard focus), a hover fill, the focus ring after keyboard input, and a Tab stop. A double click
(`startOnDoubleClick`), F2 while it has the focus (`startOnF2`), or the caller setting `*editing`
to true opens a text field on the text field core in its place, filling the row (or `width` units),
with the keyboard and the whole name selected; `selectStem` selects the name up to its last '.'
instead (a leading dot, as in ".config", counts as part of the name). The field opens one frame
after the double click or F2.

The edit works on a copy kept under the id: typing never changes `*text`. Enter, Tab or a click
elsewhere ends the edit and writes the copy to `*text` once if it differs (the function returns
true on that frame); Escape drops the copy. `*editing` (or the flag kept under the id when it is
null) is true while the field is open and goes back to false when the edit ends; the caller
setting it to false drops the copy. `maxLength` limits the copy; the buffer form never reads past
`bufferSize` and writes the copy cut to fit. `error` draws the error line under the text or the
field. `readOnly`, a null `text`, an empty buffer and `ImGui::BeginDisabled` never start an edit.
Status: ready.

### NumberInput

```cpp
bool NumberInput(const char* label, int* value, const NumberInputOptions& options = {});
bool NumberInput(const char* label, float* value, const NumberInputOptions& options = {});
bool NumberInput(const char* label, double* value, const NumberInputOptions& options = {});
```

A number field with a unit suffix inside the frame and - and + steppers at its end (hidden with
`step = 0`; not Tab stops, held they repeat). The field takes the available width with its label in
front. Typed text is written on Enter, Tab or focus loss and clamped to `min` and `max`; the int
overload also stays inside the int range and the float one inside the float range, so a typed
"99999999999" or a step past the end never overflows. The text accepts a sign, a dot or a single
comma as the decimal separator ("2,5") and, for float and double, an exponent ("1e-3"). Text that is
not a number (empty, "-", "1e") writes nothing; a committed text that cannot be read shows
`Strings::invalidValue` as the error until the field is activated again. While the field has the
keyboard, Up and Down step by `step` (Shift a tenth of it for float and double, Ctrl and Page Up or
Down by `stepFast`) and so does the mouse wheel over it, unless the area under the pointer is still
scrolling; an unfocused field leaves the wheel to the page. Escape restores the value from the start
of the edit. `mixed` shows an empty field with `Strings::mixed` until a value is written. Sizes,
read-only, disabled, error and help text come from the text field core. Status: ready.

### DragNumber

```cpp
bool DragNumber(const char* label, int* value, const DragNumberOptions& options = {});
bool DragNumber(const char* label, float* value, const DragNumberOptions& options = {});
bool DragNumber(const char* label, double* value, const DragNumberOptions& options = {});
```

A number field that changes when it or its label is dragged sideways, after the pointer moved
`drag.startThreshold`; the left-right resize cursor shows over both. The value follows the pointer
travel since the press, so dragging back to the start ends exactly on the start value. `speed` 0
spreads a finite range over the field width (an open int range moves 1 per unit, an open float
range one keyboard step per `spacing.xs`); Shift drags a tenth as far, Ctrl ten times. With
`scale = Logarithmic` over a finite range above zero, equal travel multiplies the value by equal
factors. While focused, Left and Right step by a hundredth of a finite range (an open range: ten
units of travel; int: whole numbers), Ctrl ten steps. A click without travel, Enter or Space types a
value, which writes on Enter, Tab or focus loss, as in NumberInput. `wrap` makes a finite range go
on from the other end. Escape restores the value from the start of the edit. Status: ready.

### UnitInput

```cpp
struct UnitItem { const char* label; double factor; };
bool UnitInput(const char* label, double* value, int* unit, std::span<const UnitItem> units,
               const UnitInputOptions& options = {});
```

A number field over `*value` in base units, shown in the unit `*unit` names. The unit sits at the
end of the frame as a menu button (the unit label and a chevron, a Tab stop after the text): a
click, Enter or Space opens a menu of the units with the shown one checked, and choosing one writes
`*unit` at once, keeping the quantity (`convert`, the default) or the number (`convert = false`,
which writes `*value` too, clamped to the range). Typed text is a number in the shown unit, or a
number with a unit label ("250ms", "1.5 s", "2,5 min"; the label matched exactly, else ignoring
ASCII case); it writes on Enter, Tab or focus loss and switches the shown unit to the typed one. The
field also reads back what its own `format` or `formatter` shows, so a format with the unit in it
("%.1f km") round-trips. Text that is not a number or names no unit writes nothing and shows
`Strings::invalidValue`. `min` and `max` are base units; `step` and `stepFast` are in the shown
unit, for Up, Down, Page Up, Page Down and the wheel while typing, as in NumberInput. A null `unit`
shows the first unit as a suffix without a menu, and typed suffixes are converted into it; an empty
`units` span shows the plain number. Status: ready.

### VectorInput

```cpp
bool VectorInput(const char* label, std::span<float> components, const VectorInputOptions& options = {});
bool VectorInput(const char* label, std::span<int> components, const VectorInputOptions& options = {});
bool VectorInput(const char* label, ImVec2* value, const VectorInputOptions& options = {});
bool VectorInput(const char* label, ImVec4* value, const VectorInputOptions& options = {});
```

One to four drag fields (as DragNumber) side by side after the label, sharing the width with
`spacing.sm` between them; each has its axis label in front as a drag handle (`axisLabels`, else X,
Y, Z, W) and is a Tab stop, in order. When a field would get narrower than
`metrics.numberFieldWidth`, the fields stack under each other at the full width. An edit writes
its component (and returns true once); with `linkToggle` a link button (`Strings::linkValues`, a Tab
stop after the last field) locks the vector, and while locked an edit scales every component by
the same factor, measured from the values at the start of the edit so a long drag does not drift
(a component that started at 0 scales nothing). `normalize` rescales a float vector to length 1
after an edit. More than four components show the first four read-only. `speed` 0 moves an int one
per unit and spreads a finite float range over the field; Left and Right step a hundredth of a
finite range (0.1 for an open one, 1 for int). `error` outlines every field and shows its message
once under them. The link state is kept under the id. Status: ready.

### SizeInput

```cpp
bool SizeInput(const char* label, ImVec2* value, const SizeInputOptions& options = {});
bool SizeInput(const char* label, int* x, int* y, const SizeInputOptions& options = {});
```

Width and height as two typed number fields (as NumberInput, without steppers) side by side after
the label, each half of the rest of the width; when a field would get narrower than
`metrics.numberFieldWidth` they stack. Typed values write on Enter, Tab or focus loss, clamped to
`min` and `max` per axis (the int overload also to the int range); Up and Down step by `step`
while typing. With `aspectLock` (the default) a lock button (`Strings::aspectLock`) sits at the end
of the width field, between the two values; it starts unlocked, and while locked an edit of either
value writes the other in the same frame from the ratio taken when it was locked (or from the
values before the edit when there was none), so the call returns true once. `swapButton` adds a
button (`Strings::swap`) at the end of the height field that exchanges the values; `presets` adds a
menu button (`Strings::presets`) there whose rows read "1920 × 1080" in the field format, with the
current size checked, and choosing one writes both values. Every button is a Tab stop after its
field, so Tab goes width, lock, height, swap, presets. `error` outlines both fields and shows its
message once under them. The lock state and ratio are kept under the id. Status: ready.

### HexInput

```cpp
bool HexInput(const char* label, std::uint32_t* value, const HexInputOptions& options = {});
bool HexInput(const char* label, std::uint64_t* value, const HexInputOptions& options = {});
```

A hexadecimal field of `digits` digits (default 8 for `std::uint32_t`, 16 for `std::uint64_t`; a
value that needs more shows them all) in overtype mode, built on the text field core because the
number core's double cannot hold 64 bits exactly. While it is not typing it shows the digits with
"0x" in front (`prefix`) and a gap after every two (`groupBytes`); while typing the text is the
digits alone. Only hex digits can be typed, in the field's case (`uppercase`); a typed digit
replaces the one after the caret and the text keeps its width, deleted digits become zeros, so
selecting everything and typing "F" gives "F0000000". Up and Down change the digit after the caret
by one with the carry, wrapping within the field's digits, and write at once. Typed digits write on
Enter, Tab or focus loss, masked to the field's digits. Escape restores the value from the start of
the edit, steps included. Status: ready.

### MaskedInput

```cpp
bool MaskedInput(const char* label, std::string* text, const char* mask, const MaskedInputOptions& options = {});
bool MaskedInput(const char* label, char* buffer, std::size_t bufferSize, const char* mask,
                 const MaskedInputOptions& options = {});
```

A TextField whose text follows `mask`: "9" takes an ASCII digit, "A" a letter (ASCII, or any
non-ASCII character), "*" any printable character, "\" makes the next mask character a literal,
and every other character is a literal. The field edits the formatted text, kept under the id:
after every edit, typed or pasted, the text is rebuilt from the slot characters, so a character
its slot does not take is dropped, characters past the last slot are dropped, a literal appears as
soon as a slot character follows it, and the caret stays behind the same slot. Deleting a literal
changes nothing; Backspace after it removes the slot character before it on the next press. The
rest of the mask is drawn after the text in labelSecondary as a hint, each empty slot as the first
character of `placeholder` ("_" by default); it is left out when it would not fit, and while the
mixed placeholder shows.

`*text` gets the slot characters only, or the formatted text (literals up to the last filled
slot) with `keepLiterals`; text the caller sets is read through the mask the same way. Writes per
keystroke and returns true when the text changed. Escape restores the text from activation. A
null or empty mask makes it a plain TextField. Width, sizes, `readOnly`, `mixed`, the error and help
line, disabled and the buffer form are as for TextField. Status: ready.

### PinCodeInput

```cpp
bool PinCodeInput(const char* label, std::string* code, const PinCodeInputOptions& options = {});
bool PinCodeInput(const char* label, char* buffer, std::size_t bufferSize, const PinCodeInputOptions& options = {});
```

A code of `length` cells (1 to 16) with the label in front. One frameless text field on the text
field core takes the keyboard for the whole code, so it is one Tab stop and typing, Backspace,
paste, undo and IME are Dear ImGui's; the field draws no text and the cells are drawn from it. Each
cell is a field frame one control height square with spacing.xs between cells and spacing.md after
every `groupSize` cells; it shows its character centered, or a dot (statusDotSize) when `masked`.
While the field has the keyboard the next empty cell (the last one when the code is full) has the
active outline and a caret line. The caret stays at the end of the code, so typing fills the next
cell and Backspace empties the last one; Ctrl+A then Backspace clears it. `numericOnly` keeps digits
only (also from a paste); otherwise any visible character but a space is taken; a paste fills as
many cells as there are characters. A click anywhere on the cells gives the field the keyboard.
`masked` also turns copying off.

Writes per keystroke and returns true when the code was written; the code is complete when its
length equals `length`. `error` draws the error outline on every cell and the message under the
cells, and shakes the cells sideways once (three swings over motion.durationSlow, spacing.xs wide)
each time the error appears, unless motion is reduced. Default width: the label and the cells, at
most the available width; in a narrower width the cells shrink down to the small control height and
then wrap onto further rows. `readOnly`, disabled and the buffer form are as for TextField.
Status: ready.

### DurationInput

```cpp
bool DurationInput(const char* label, double* seconds, const DurationInputOptions& options = {});
```

A field over `*seconds` that shows "hh:mm:ss" (`showHours = false`: "mm:ss" with the minutes
counting on past 59; `showMilliseconds` adds ".mmm"; a negative value starts with "-"), digits in
`label` and the separators in `labelSecondary`. The field is one Tab stop. While it is focused,
Left and Right pick a segment, shown with `accentSubtle` behind it once the keyboard is in use (the
seconds at first), and Up and Down step that segment by one of its units with the carry into the
next one, because the edit is on the total (59 seconds and Up is 1:00); Page Up and Down and Ctrl
step ten units, the wheel steps while focused. A vertical drag on a segment (the up-down resize
cursor shows) moves that segment in whole units, upward to more. Steps and drags write at once.
Enter, Space, a click without a drag or a typed digit open the text: the shown text all selected
(or the typed digit), and the text is read on Enter, Tab or focus loss: "01:30:00", "1:30" (two
parts are minutes and seconds), "0:00:01.5", "1h2m3s", "1m30" (a number after a unit is in the next
smaller unit), "2,5 min", "250ms" or a plain number of seconds ("90"). What the field shows always
reads back. Text that is not a duration writes nothing and shows `Strings::invalidValue`; Escape
leaves the text without writing. Values are clamped to `min` and `max`; the default `min` of 0
keeps durations positive. Status: ready.

### PathField

```cpp
bool PathField(const char* label, std::string* path, const PathFieldOptions& options = {});
bool PathField(const char* label, char* buffer, std::size_t bufferSize, const PathFieldOptions& options = {});
bool PathFieldBrowsePressed();
```

A TextField for a file or folder path. While the field does not have the keyboard, a path wider
than the text area is drawn shortened in the middle with an ellipsis, so the file name at its end
stays in view, and the whole path shows as a tooltip on hover and keyboard focus; while editing,
the whole path is edited and scrolls as in any field. The shortened form follows the text area
width of the frame before, so it adapts one frame after a resize. `browseButton` (on by default)
adds a folder button at the end of the frame with `Strings::browse` as its tooltip: a Tab stop after
the text, activated by a click, Space or Enter, and off while read-only. `PathFieldBrowsePressed()`
is true on the frame it was activated, for the last PathField call of the frame; Facet opens no
dialog itself, the caller opens its own (a system dialog or a FileBrowser in a modal) and writes the
result. Writes per keystroke and returns true when the path was written. The width, sizes,
`readOnly`, the error and help line, disabled and the buffer form are as for TextField.
Status: ready.

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

Records a key bind (`Keybind` from `core/input.h`). The label is drawn in front; then the primary
slot and, with `showAlternate`, the alternate slot share the width, each a field-like Tab stop that
shows its chord as key caps (the chord as cut text with a tooltip when the caps do not fit) or the
placeholder (`Strings::unbound`). A click, Enter or Space on a slot starts listening: the slot shows
`Strings::listening`, then the held keys as pressed caps, and takes the chord when its last key is
released (the modifiers held when the main key went down, plus that key). While it listens the slot
is the active item, `io.WantCaptureKeyboard` is true and every key is locked to it, so no shortcut,
key bind, `ImGui::IsKeyPressed` or keyboard navigation sees the keys. Escape cancels, so Escape cannot
be bound; a left click anywhere cancels and still reaches what it lands on. Modifiers alone are taken
only with `allowModifierOnly`; the right, middle and side mouse buttons only with `allowMouse` (the
left button never binds); gamepad buttons only with `allowGamepad`. A slot that holds a chord shows a
clear button at its end (not a Tab stop); Backspace or Delete on the focused slot clears it as well.
`showMode` adds a `KeybindModeSelector` (selection.h) beside the slots, or under them when the slots
would get narrower than `metrics.numberFieldWidth` each. Facet does not look for conflicts itself
(validation belongs to the caller): call `FindChordConflict` with the chords and pass the result as
`error`; the demo page shows this. Every write returns true once and ends one edit session
(`IsItemEditCommitted`). Status: ready.

### Rating

```cpp
bool Rating(const char* label, float* value, const RatingOptions& options = {});
bool Rating(const char* label, int* value, const RatingOptions& options = {});
```

A row of `count` icons (1 to 10) after the label, filled up to the value: the built-in "star" and
"star-filled" icons, or `icon` for both. Filled icons are drawn in `focusRing`, which reaches 3:1
on every surface, empty ones in `labelSecondary`; a half step fills the left half of an icon. The
row is one item and one Tab stop. The pointer previews the value under it, a click writes it (the
icon, or with `halfSteps` on the float overload the half under the pointer), and with `clearable` a
click on the current value writes 0. Left and Right step, Home and End go to 0 and `count`, Page
Up and Page Down a tenth (at least one step); a key press is one edit session, so
`IsItemEditCommitted` reports it. Default width: the label and one control height per icon, at
most the available width; in a narrower row the icons shrink. `readOnly` shows the value without
hover preview and is the way to display a rating; `mixed` shows the icons empty until a value is
written. Status: ready.

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
