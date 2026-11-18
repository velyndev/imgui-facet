# API conventions

These rules apply to every public declaration under `include/facet/widgets/`. They exist so that about 180 widgets, written by different people, read like one library and like the core headers in `include/facet/core/`. When a draft signature disagrees with this document, this document wins. Each rule has a one-line reason. The checklist near the end is what a reviewer runs over a new signature. The last section shows the rules applied to a set of reference widgets.

The users are C++ developers who already write Dear ImGui code, often inside a host application they do not control, and who mix Facet widgets with stock Dear ImGui calls in the same window. Several rules follow from that.

## 1. Names and files

- Everything public lives in `namespace facet`. Internal helpers live in `facet::internal`, under `src/internal/` or next to their widget. Functions, types and enum members use PascalCase. Struct members and parameters use camelCase. Constants use kPascalCase, and macros use `FACET_UPPER_CASE`. *Reason: this matches the core headers.*
- A widget is a free function named with a noun (`Slider`, `TextField`). Its types take its name: `SliderOptions`, `ButtonVariant`. Value types are separated by overloads (`Slider(float*)`, `Slider(int*)`), never by suffixes such as `SliderFloat`. *Reason: an engineer can guess every name from the widget name.*
- A container is `BeginX` / `EndX`, and its RAII form is `XScope`. `Push` / `Pop` is used only for style stacks (`PushAccent`, `PushControlSize`). *Reason: one spelling per concept.*
- There is one header per family, in lower case: `buttons.h`, `selection.h`, `lists.h`, `sliders.h`, `inputs.h`, `pickers.h`, `navigation.h`, `containers.h`, `layout.h`, `overlays.h`, `display.h`, `charts.h`, `media.h`, `composites.h`. Shared types live in `widgets/common.h`; a type that widgets of two or more families take goes there, not into one of the families. Key chords, `Keybind`, `BindMode` and the shortcut registry live in the core header `core/input.h`, and width classes and the labelled row layout in `core/layout.h`. Every header compiles on its own, and a test includes each one alone in a separate translation unit (`tests/headers/`). A family header includes only `imgui.h`, `common.h` and the core headers it needs; `composites.h` alone may also include the family headers whose types its widgets take, because composites are built from the other families. `facet/facet.h` includes every core and widget header, and no Facet header includes `facet.h`. *Reason: one engineer owns one family file, and apps need one include.*
- A Facet name may equal a Dear ImGui name (`Button`, `Checkbox`, `MenuItem`). Calls are always qualified (`facet::`, `ImGui::`). With both using-directives in effect, such a call is ambiguous and fails to compile; it never binds to the wrong function silently. Where the two libraries pair functions differently, Facet uses its own name (`ItemTooltip`, `BeginRichTooltip` / `EndRichTooltip`; Dear ImGui's `BeginItemTooltip` pairs with `EndTooltip`). *Reason: the obvious name wins, and a Facet Begin can never be closed by a stock End.*

## 2. Labels and ids

- The first parameter of every widget that keeps state across frames is `const char* label` or `const char* id`, and it follows Dear ImGui exactly. The whole string is hashed on the current id stack. Text after `##` is hidden. `###` makes only the suffix the id. *Reason: users already know these rules, so Facet adds no second id system.*
- The parameter is called `label` when its visible part is drawn or used as a tooltip or alt text. It is called `id` when it is never shown (`BeginCard`, `TabBar`, `BeginDataTable`). Being an accessible name alone does not make it a `label`: Dear ImGui has no accessibility output, so text that is only a name would never be seen. *Reason: the parameter name tells the caller whether the text appears.*
- A label whose visible text changes from frame to frame must use `###` to keep a stable id. In loops, the caller uses `ImGui::PushID(i)` exactly as with stock widgets. Facet never makes ids unique on its own. *Reason: the id must not depend on display text.*
- Icon-only controls take a label whose visible part is required. It is the tooltip and the accessible name. An empty visible part asserts. The one exception is `CopyButton`, whose empty visible part falls back to `Strings::copy`, because its purpose is the same everywhere. *Reason: every control has a name.*
- Parts and entries inside a widget (handles, segments, tabs, rows) get ids under the widget id. The id is `item.id` when that is non-zero, and the index otherwise. Item labels in a list do not need to be unique. *Reason: one id per widget for the caller, and stable state for entries that can be reordered.*
- `BeginSettingsRow` pushes its label on the id stack until `EndSettingsRow`, so the control in every row can use the same `"##value"`. Two rows with the same visible text need `###` like any other duplicate label. A control decides whether to draw a label only from its own label string; there is no implicit row context. *Reason: explicit `##` is the only switch, so there is nothing hidden to learn.*

## 3. Values and results

- Edited values are non-const pointers (`bool*`, `int*`, `float*`, `std::string*`). Display state that the caller owns and Facet never writes is passed by value. *Reason: the call site shows what can change, as in Dear ImGui.*
- The `bool` result means:
  - for action widgets: activated this frame (click released inside, Enter, Space, gamepad activate);
  - for value and choice widgets: the value or selection was written this frame;
  - for `BeginX`: the content must be submitted.
  
  Dialogs that end with a choice return an enum (`DialogResult`). *Reason: `dirty |= facet::Slider(...)` keeps working.*
- Writes follow Dear ImGui 1.93. Drags, steppers, arrow keys and clicks write at once. Typed text writes per keystroke in text fields (`ImGuiItemFlags_LiveEditOnInputText` is on by default). Typed numbers write on Enter, Tab or focus loss (`ImGuiItemFlags_LiveEditOnInputScalar` is off by default). Callers change either default with `ImGui::PushItemFlag`. *Reason: a Facet field and a stock field next to it behave the same way.*
- Escape during an edit restores the value from activation, writes it, and returns true on that frame, as stock widgets do. *Reason: the stock queries keep their meaning.*
- Edit sessions are read through item queries after the call. `ImGui::IsItemActivated`, `IsItemDeactivated` and `IsItemDeactivatedAfterEdit` work on every Facet widget. `common.h` adds two queries:
  - `facet::IsItemEditCommitted()`: the session ended this frame, the value was edited during it, and it was not canceled;
  - `facet::IsItemEditCanceled()`: Escape ended the session and restored the starting value.
  
  *Reason: commit-on-release and undo need no result struct and no callback.*

## 4. Optional parameters

- Positional parameters are the label or id, every pointer the widget writes through or keeps across frames (value, `bool* open`, selection storage), range bounds, and the item data. Everything else goes into one trailing `const XOptions& options = {}`. There are no other default arguments and no flags parameter. *Reason: one test decides every signature.*
- Option structs are aggregates: no constructors, no base classes, a default member initializer on every member. Callers use designated initializers (`{.variant = facet::ButtonVariant::Primary}`). Positional brace initialization of an options struct is not supported. *Reason: named members can be added without breaking callers.*
- Options may hold pointers that the widget only reads during the call: strings, spans, `const` pointers, and a function pointer with its `void* user`.
- An on/off feature is a named `bool`. A one-of choice is an enum class member. A flags enum exists only when the set itself is a value the caller stores. *Reason: no engineer has to choose between a bool and a bit.*
- Member order is: widget-specific members, most used first; then the data block (`state`, `emptyText`, `errorText`) where it applies; then the common block. The common block always uses these names, types and order, and each member appears only where it applies: `ControlSize size`, `float width`, `float height`, `bool readOnly`, `bool loading`, `bool mixed`, `const char* error`, `const char* helpText`. The bare name `size` always means `ControlSize`. Two-dimensional lengths use compound names (`minSize`, `initialSize`). A compile-time test checks the common block in every options struct. *Reason: the same option is spelled the same way in 180 structs.*
- A new member is appended at the end of the struct. Its default keeps the old behavior, and its trailing comment ends with `(since x.y)`. Never add an overload that differs only in the options type, because `{}` would become ambiguous. *Reason: existing calls keep compiling, and readers know which release has the member.*

## 5. Strings and collections

- Labels, ids, formats and short texts are `const char*`: UTF-8, zero terminated, and read only during the call. Long text with a known length (logs, code) is `std::string_view`. Facet copies any text it keeps past the call (toasts). *Reason: the id stack hashes zero-terminated strings, and literals cost nothing.*
- Editable text is `std::string*`, which Facet resizes. Every such widget also has an overload taking `char* buffer, std::size_t bufferSize` for hosts that avoid heap use.
- Fixed lists are `std::span<const XItem>`, where the item struct puts the label first so `{"Low"}` works. Choice widgets share `ChoiceItem`. A widget whose items need only a label also has a `std::span<const char* const>` overload. A span cannot bind to a braced list in C++20, so callers declare an array.
- Generated or large lists take a count plus a function pointer that fills a whole item, and a `void* user`. `std::function` is never used, because it can allocate.
- Rows with arbitrary content use `BeginX`, a `NextVisibleRows` loop, and `EndX`.
- Numbers use printf formats in the Dear ImGui dialect, and `nullptr` picks the widget default. `FormatFn` covers formats that printf cannot express.

## 6. Size and layout

- Every length in a `widgets/` header is in design units, and Facet multiplies it by `Px()`. Core headers state their unit per function: core `Image`, `IconGlyph`, draw and effects functions take pixels, as their Dear ImGui counterparts do. Stock calls such as `ImGui::SetNextItemWidth` stay in pixels; wrap units with `facet::Px`. *Reason: widgets stay correct at every scale, and the low-level layer keeps Dear ImGui's convention.*
- Host-facing coordinates (hit tests, input regions, the window placement position) are pixels of the host's coordinate space, stated per declaration. *Reason: the host compares them with window messages and OS rectangles, which know nothing of the UI scale.*
- The width is taken from the first of these that is set:
  1. `options.width`: a value > 0 is units, `kFill` (`-FLT_MIN`) is the available width, and any other value < 0 is the available width minus that many units;
  2. `ImGui::SetNextItemWidth`;
  3. `ImGui::PushItemWidth`, when the width stack is not empty;
  4. the family default.
  
  Field widgets (sliders, text, numbers, dropdowns, pickers, progress bars) default to the available content width. Compact controls (buttons, toggles, checkboxes, segmented controls) default to their content.
- The visible label of a field widget is drawn in front of the control and inside the widget width; it takes its measured text width plus `spacing.sm`. Unlike stock widgets, `SetNextItemWidth(-FLT_MIN)` therefore gives a widget that ends at the right edge with its label included. Aligned label columns come from `SettingsRow` (and `LayoutLabeledRow` in `core/layout.h`), not from field widgets. Toggle, checkbox and radio labels follow the mark. In table cells and settings rows, callers pass `"##name"`. *Reason: a field with its label fits a table cell at one control height.*
- Every other value widget draws its visible label by its shape, so widgets of one shape agree:
  - one-line compact controls (`SegmentedControl`, `ToggleButtonGroup`, `KeybindModeSelector`, `CycleSelector`, `ThemeModeControl`, `AccentPicker`) draw it in front, inside the width, like fields;
  - groups that wrap into rows (`RadioGroup`, `CheckboxGroup`, `ChipGroup`, `OptionCardGroup`) draw it above;
  - canvases and panels (pickers such as `ColorPicker` and `FileBrowser`, the curve, easing and gradient editors, `ChartRangeBrush`, the media strips, viewers and rulers) do not draw it beside them, so they keep the whole width, and show it as their tooltip on hover and keyboard focus.
  
  *Reason: a label that is never drawn tells the user nothing, and the same call reads the same way across widgets of one shape.*
- `options.size` is `ControlSize { Inherit, Small, Regular, Large }`, mapped to the controlHeight tokens (`ControlHeight` in `common.h`). `Inherit` takes the innermost `PushControlSize`, or `Regular` (`ResolveControlSize`). `Small` draws its text in Footnote, `Regular` and `Large` in Body (`ControlTextStyle`).
- Every widget advances the cursor like one item, so `SameLine`, `Indent`, groups and tables work unchanged. `SettingsRow` lays out within the available width (also inside a table cell) and never creates table columns.

## 7. States

- **Disabled:** there is no option for it; callers use `ImGui::BeginDisabled`. Facet reads the item flags and draws the disabled tokens. A disabled widget shows the not-allowed cursor, is not a Tab stop, returns false, and still answers `IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)`. *Reason: one channel, so stock and Facet items agree.*
- **Read-only:** `readOnly` exists on every value widget. The value is shown, can be focused and copied, is never written, and the widget returns false. A caller-pushed `ImGuiItemFlags_ReadOnly` (`imgui_internal.h`) counts the same as `readOnly`.
- **Validation** belongs to the caller. A non-null `error` draws the error outline and the message under the control in the destructive token, and replaces `helpText`. Facet validates nothing beyond clamping ranges.
- **Loading:** `loading` exists on actions and on fields that wait (async fields). It keeps the widget size, shows the spinner, and ignores input. Widgets whose items can be fetched (data views and the popup lists `Dropdown`, `MultiDropdown` and `Autocomplete`) take the data block below instead, because their items can also fail to load.
- **Mixed:** `mixed` shows the indeterminate look until the first edit writes a definite value. It is equivalent to a caller-pushed `ImGuiItemFlags_MixedValue`. Checkboxes use the `CheckState` overload instead.
- **Data views and popup lists** take `DataState { Ready, Loading, Error }` plus `emptyText` and `errorText`. In a popup list the state belongs to the items: the open list shows it, the field stays usable, and `error` remains the validation message of the chosen value. A count of 0 in `Ready` shows the empty state, so there is one source for "empty". Built-in English strings come from the `Strings` table, which `SetStrings` replaces.
- **Built-in text** that a widget shows or uses as a name (a button, a segment, a heading) comes from a `Strings` member, never from a literal in the widget, and the header names the member in full (`Strings::minimize`, not "minimize"). A member with numbers in it is a template with named tokens (`"Page {page} of {count}"`), not a printf format: a translation can reorder the tokens, and nothing it contains can break the formatting. A count that needs a singular form has a second member (`unsavedChange`, `unsavedChanges`). A test checks that every `Strings::` name in the headers and docs exists.

## 8. Containers and popups

- A Begin that can skip its content returns `[[nodiscard]] bool`, and its End is called only after true. This covers windows, popups, modals, collapsible parts, lists and tables. Facet closes any Dear ImGui window it opened before returning false.
- A Begin whose content is always laid out (`Card`, `SettingsRow`, `Stack`, `Grid`) returns `void`, and its End is always called. *Reason: layout containers never skip content, so keyboard navigation reaches rows scrolled out of view and a row needs no if block.*
- Every pair has a non-copyable, non-movable `[[nodiscard]] XScope` in the same header. The scope of a bool Begin has `explicit operator bool` and ends only when Begin returned true.
- Layers the caller opens (Shell, Modal, ConfirmDialog, Popover, Drawer) take `bool* open`. The caller writes true to open; Facet writes false on dismissal. Layers opened by input (dropdown lists, context menus, tooltips) open themselves. Results are polled from return values or query functions, never delivered by callback.
- Escape acts on one thing at a time, in this order: it cancels the active edit, then closes the topmost popup, then the modal, then a shell that set `closeOnEscape`. Facet routes Escape itself through key ownership, so this does not depend on `ImGuiConfigFlags_NavEnableKeyboard`.

## 9. Icons and images

- Icons are `facet::Icon` by value, and `Icon{}` means none. An option or item member is named `icon`. Icon size and color follow the widget size and state.
- A widget that can show its icon without its label spells that choice `bool iconOnly`, whatever its default; the visible label part then becomes the tooltip. A reduced form that is not about icons (an avatar without the name) is `compact`.
- Images are `ImageHandle` by value. The media widget `ImageView` takes design units and a `Radius` token, and has an `ID3D11ShaderResourceView*` overload for app textures, as core `Image` does. *Reason: radii stay on the token scale, and app textures need no extra registration step.*

## 10. Per-call styling

- A call may choose `variant`, `size`, `tone`, and `width`, plus colors that are the data itself (picked colors, chart series, image tint). A subtree may use `PushAccent` and `PushControlSize`.
- Data colors that a widget only displays are `ImU32` (packed sRGB, straight alpha); colors the user edits are `ImVec4` (pickers, `GradientStop`, `AccentPicker`). The value 0 is automatic, with one meaning per kind: for indexed data (series, slices, legend entries, the segments of `AllocationBar` and `MultiSlider`) it takes the chart palette color of the index, so the same shares get the same colors in every widget; for markers and adornments (a chip dot, an inventory tint) it means none. Two documented exceptions use the accent instead: `Sparkline`, a single series next to a number, and `MultiSlider` with no segment colors, a control whose fill shows its value. *Reason: one type and one default rule, so data passed to two widgets looks the same in both.*
- Radii, spacing, fonts, the focus ring, motion, shadows and blur are fixed by the theme. Facet widgets read tokens and ignore `ImGui::PushStyleColor`, `PushStyleVar` and `PushFont`. Fonts come from `SetFontSet`, and text size follows the style font scale (see `scale.h`), which a host may manage itself. *Reason: the dose caps in DESIGN.md hold only if a call cannot break them.*

## 11. Interplay with Dear ImGui

- After any widget call, the last item covers the whole widget. `IsItemHovered`, `IsItemActive`, `IsItemEdited`, the deactivation queries, `GetItemRect*`, `SetItemTooltip`, drag and drop and `SetItemDefaultFocus` apply to it. A widget made of several items ends with `EndGroup` followed by one `SetLastItemData`, so that:
  - the last item id is the widget id while no part is active or just deactivated;
  - while a part is active or was just deactivated, the last item id is that part's id, so the activity queries stay true.
- A widget snapshots `SetNextItemWidth`, `SetNextItemAllowOverlap`, `SetNextItemSelectionUserData`, `SetKeyboardFocusHere` and pushed item flags at entry, before its first sub-item, because `ItemAdd` clears them.
- A widget leaves the id stack, style stack, item width and font as it found them.
- Moving focus between items is Dear ImGui navigation and needs the host to enable `NavEnableKeyboard`. Keys inside a focused widget (arrows, Home, End, Escape) are read by Facet through key routing. Facet draws its own focus ring after keyboard input and never the stock highlight.
- After a mouse click, Dear ImGui shows the keyboard focus on the first Tab instead of moving it; Facet keeps this.
- A hit target grown to `minHitTarget` around a small visual never takes a click that lands on a neighbour's visible rectangle.
- A label or title that a widget cuts with an ellipsis shows its full text on keyboard focus as well as on hover.
- Facet never changes io flags and reads input only from `ImGuiIO`, so scripted input drives every widget.
- While `KeybindCapture` listens, it owns every key and stays active, so `io.WantCaptureKeyboard` is true.
- Where Dear ImGui has a type, Facet uses it: `ImGuiKeyChord`; `ImGuiSelectionBasicStorage*` plus `BeginMultiSelect` for every list and table; `ImGui::TableGetSortSpecs` inside `BeginDataTable`.

## 12. Flags and enums

- Enums are always `enum class` with PascalCase members and no prefix. Choice enums have no explicit underlying type, as in the core headers. Their zero member is the default and has a meaningful name (`Inherit`, `Secondary`, `Linear`). *Reason: a value-initialized options struct gives default behavior.*
- Flags enums use `std::uint32_t`, declare `None = 0` and bits `1u << n`, and get `|`, `&`, `^`, `~`, `|=`, `&=` and `Has(set, bit)` from `FACET_FLAGS(Type)`. `Has` is false for `None`. Enum members are only appended.

## 13. Stability

- Frozen means source compatible within a major version. That covers names, parameter order and types, return types and their meaning, option member names, types, defaults and order, enum member names and values, and documented behavior.
- Allowed changes: new functions, members appended per rule 4, enum members appended, and overloads that make no existing call ambiguous. ABI is not promised; Facet is built from source.
- Deprecated declarations carry `FACET_DEPRECATED("use X")`. They stay for at least one minor release and are removed in the next major. `FACET_DISABLE_DEPRECATED` hides them early.
- `facet/core/version.h` defines `FACET_VERSION_MAJOR`, `_MINOR`, `_PATCH`, `FACET_VERSION` ("major.minor.patch", built from the three numbers) and `FACET_VERSION_NUM` (major * 10000 + minor * 100 + patch). The three numbers are the only place the version is written: CMake reads them for `project(VERSION)`. `facet::Version()` returns `FACET_VERSION`.

## 14. Errors

- No exception leaves a public function. Allocation failures are caught inside, and the widget keeps the old value.
- Contract violations use `IM_ASSERT_USER_ERROR`, so a host with Dear ImGui error recovery gets a log line instead of a crash. In release builds:
  - a null value pointer draws the widget read-only and returns false;
  - a null label is treated as `""`;
  - `min > max` is swapped;
  - `min == max` shows the value read-only;
  - `minGap` is clamped to the range.
- Out-of-range, NaN and index -1 values are shown as they are (clamped in the drawing, a placeholder for -1) and are written only by a user edit.
- A call without a current context, or outside `facet::NewFrame` / `facet::EndFrame`, returns the neutral result: false, `None`, or an invalid id.
- `facet::EndFrame` reports an unbalanced Facet scope by name and resets Facet's own stacks (`PushAccent`, `PushControlSize` and every Begin / End pair). It does not repair Dear ImGui windows.

## 15. Threads

- Every widget function, including `ShowToast`, runs on the thread that runs the Dear ImGui frame, as `context.h` states.
- Getters and formatters are called synchronously during the call and are never stored.
- Options structs, items, `Icon` and `Keybind` may be built on any thread. Background work hands its results to the frame thread itself.

## 16. Documentation comments

- A `//` block sits directly above each declaration, in full sentences and in the style of the core headers. It states:
  - what the widget draws and does;
  - what the result means;
  - when it writes (per keystroke, on commit, while dragging);
  - the unit, ownership and lifetime of every parameter;
  - what the label or id is used for;
  - the default width;
  - the keys that operate it;
  - which item queries report the edit;
  - behavior on invalid input;
  - the state kept under its id;
  - for a Begin, whether End is always called.
- Every option member has a trailing comment giving its unit and what its default means. A comment does not repeat this document; it states only what departs from it or completes it. Comments contain no em or en dash.

## Signature checklist

1. Is the first parameter `label` (shown) or `id` (never shown), and does it follow Dear ImGui `##` / `###` rules?
2. Is every positional parameter either written through, kept across frames, a range bound, or the item data? Is everything else in one trailing `const XOptions& options = {}`?
3. Is the options struct an aggregate with defaults on every member, ordered as widget members, then data block, then common block, with the common names and types unchanged?
4. Is every on/off feature a `bool`, every choice an enum class whose zero member is the default, and is there no flags enum unless the set is stored?
5. Is every length in design units with the unit in a comment, and does the width follow the precedence in rule 6?
6. Does the bool result mean pressed, written, or content-to-submit, and do the stock item queries describe the whole widget?
7. Is there no disabled option, and do `readOnly`, `error`, `helpText`, `mixed` and `loading` appear wherever they apply, and `state`, `emptyText` and `errorText` on every widget whose items can be fetched?
8. For a container: does Begin return `[[nodiscard]] bool` only when content can be skipped, is End documented, and is there an `XScope`?
9. Are strings `const char*` read during the call, lists spans of label-first items, and lazy lists function pointers with `void* user`?
10. Does nothing throw, is null and range handling documented, and does the comment carry every item from rule 16?
11. Does the header compile alone and include only `imgui.h`, `common.h` and the core headers it needs (for `composites.h` also the family headers whose types it takes), with no `windows.h` or `d3d11.h`? Does a type that two or more families need live in `common.h`?
12. Is any name the same as a Dear ImGui function with a different pairing (Begin/End, tooltip)? Then rename it.

## Reference signatures

The shared pieces exist as real headers, which are authoritative:

- `include/facet/core/version.h`: the version macros and `FACET_DEPRECATED` (rule 13).
- `include/facet/core/theme.h`: `PushAccent` and `PopAccent`, the scoped accent of rule 10, with `kAccentStackDepth`.
- `include/facet/core/input.h`: chords, `KeyNameText`, `ChordToText` and `ChordFromText`, `BindMode`, `Keybind`, `IsBindActive`, `IsBindPressed` and the shortcut registry.
- `include/facet/core/layout.h`: `WidthClass` and its queries, and `LayoutLabeledRow`.
- `include/facet/widgets/common.h`: `FACET_FLAGS`, `kFill`, `kIndeterminate`, `ControlSize`, `Tone`, `Orientation`, `Radius`, `DataState`, `CheckState`, `ValueScale`, `ChoiceItem`, `ChoiceGetter`, `FormatFn`, `RowRange`, `NextVisibleRows`, the control size stack with `ResolveControlSize`, `ControlHeight` and `ControlTextStyle`, `IsItemEditCommitted`, `IsItemEditCanceled`, and the `Strings` table with `SetStrings` and `GetStrings`.

The widget signatures below show the rules applied. They are the starting point of the family headers, not frozen declarations.

```cpp
// ---- include/facet/widgets/buttons.h ----
namespace facet
{

enum class ButtonVariant
{
    Secondary,    // neutral fill; the default
    Primary,      // accent fill; one primary action per view
    Plain,        // text only, fill on hover
    Destructive,  // destructive tone
};

struct ButtonOptions
{
    ButtonVariant variant = ButtonVariant::Secondary;
    Icon icon;                         // leading icon; none by default
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fits the label, kFill fills
    bool loading = false;              // spinner, width kept, not activatable
};

// A push button. Returns true on the frame it is activated (click released inside, Enter,
// Space, gamepad activate). Default width: its content.
bool Button(const char* label, const ButtonOptions& options = {});

struct IconButtonOptions
{
    ButtonVariant variant = ButtonVariant::Plain;
    bool selected = false;             // shows the on look; the caller flips it on activation
    ControlSize size = ControlSize::Inherit;
    bool loading = false;
};

// A square button showing only `icon`. The visible label part is required: it is the tooltip
// (on hover and keyboard focus) and the name. Returns true on activation.
bool IconButton(const char* label, Icon icon, const IconButtonOptions& options = {});

} // namespace facet

// ---- include/facet/widgets/selection.h ----
namespace facet
{

struct ToggleOptions
{
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fits label and switch; wider puts the switch at the end
    bool readOnly = false;
    bool loading = false;              // the knob shows a spinner; input ignored
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// An on/off switch with the label in front. Space, Enter or a click flips *value. Returns true
// when *value was written.
bool Toggle(const char* label, bool* value, const ToggleOptions& options = {});

// CheckState { Off, On, Mixed } is in common.h: CheckboxTree (lists.h) takes it too.

struct CheckboxOptions
{
    ControlSize size = ControlSize::Inherit;
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Box then label; the label is part of the hit area. Returns true when *value was written.
bool Checkbox(const char* label, bool* value, const CheckboxOptions& options = {});
// Tri-state: activating Off or Mixed writes On, On writes Off. Only the caller sets Mixed.
bool Checkbox(const char* label, CheckState* state, const CheckboxOptions& options = {});

struct RadioGroupOptions
{
    Orientation orientation = Orientation::Vertical;
    ControlSize size = ControlSize::Inherit;
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// One Tab stop; arrow keys move the selection. *index -1 means none. The visible label part
// is drawn above the group. Returns true when *index was written.
bool RadioGroup(const char* label, int* index, std::span<const ChoiceItem> items,
                const RadioGroupOptions& options = {});
bool RadioGroup(const char* label, int* index, std::span<const char* const> items,
                const RadioGroupOptions& options = {});

struct SegmentedControlOptions
{
    bool iconOnly = false;             // segments show item icons; labels become tooltips
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fits the segments, kFill gives equal segments
    bool readOnly = false;
    bool mixed = false;                // no segment highlighted
};

// Segments with the sliding selection pill; the visible label part is drawn in front (rule 6).
// Arrow keys move the selection. Returns true when *index was written.
bool SegmentedControl(const char* label, int* index, std::span<const ChoiceItem> items,
                      const SegmentedControlOptions& options = {});
bool SegmentedControl(const char* label, int* index, std::span<const char* const> items,
                      const SegmentedControlOptions& options = {});

} // namespace facet

// ---- include/facet/widgets/sliders.h ----
namespace facet
{

// ValueScale { Linear, Logarithmic } is in common.h: drag fields (inputs.h) take it too.
enum class HandleCollision { Clamp, Push };

struct SliderOptions
{
    const char* format = nullptr;      // printf; null: "%.3g" (float) or "%d" (int)
    FormatFn formatter = nullptr;      // used instead of format when set
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;                 // value units; > 0 snaps to min + n * step; 0 continuous (int: 1)
    Orientation orientation = Orientation::Horizontal;
    bool fillFromCenter = false;       // fill from the middle of the range, for bipolar values
    bool valueBubble = false;          // value shown above the knob while dragging
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units, horizontal; 0 fills the available width
    float height = 0.0f;               // units, vertical; 0 for the token default
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Drag or click the track; arrows step (Shift fine, Ctrl coarse), Page Up and Down, Home and
// End; Ctrl+click or Enter types a value. Writes while dragging; Escape restores the value
// from activation. A value outside the range is shown clamped and written only when edited.
bool Slider(const char* label, float* value, float min, float max, const SliderOptions& options = {});
bool Slider(const char* label, int* value, int min, int max, const SliderOptions& options = {});

struct RangeSliderOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    ValueScale scale = ValueScale::Linear;
    double step = 0.0;
    double minGap = 0.0;               // value units; smallest allowed high - low, clamped to the range
    HandleCollision collision = HandleCollision::Clamp;
    bool dragSpan = true;              // dragging the filled span moves both handles
    Orientation orientation = Orientation::Horizontal;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    float height = 0.0f;
    bool readOnly = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Two handles, each its own item, Tab stop and focus ring. Handles never cross and keep
// minGap; a track click moves the nearer handle. The item queries describe the whole widget.
// Returns true when *low or *high was written.
bool RangeSlider(const char* label, float* low, float* high, float min, float max,
                 const RangeSliderOptions& options = {});
bool RangeSlider(const char* label, int* low, int* high, int min, int max,
                 const RangeSliderOptions& options = {});

} // namespace facet

// ---- include/facet/widgets/inputs.h ----
#include <limits>
#include <string>
namespace facet
{

struct TextFieldOptions
{
    const char* placeholder = nullptr; // labelSecondary while empty
    Icon icon;                         // leading icon
    bool clearButton = false;          // clear button while the text is not empty
    int maxLength = 0;                 // code points; 0 unlimited
    bool selectAllOnFocus = false;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fills the available width
    bool readOnly = false;
    bool loading = false;              // trailing spinner; the text stays editable
    bool mixed = false;
    const char* error = nullptr;       // replaces helpText
    const char* helpText = nullptr;
};

// Single-line text. Editing, IME, undo and clipboard are Dear ImGui's. Writes per keystroke
// unless the caller pushed ImGuiItemFlags_LiveEditOnInputText off. Escape restores the text
// from activation. Returns true when the text was written.
bool TextField(const char* label, std::string* text, const TextFieldOptions& options = {});
bool TextField(const char* label, char* buffer, std::size_t bufferSize, const TextFieldOptions& options = {});

struct NumberInputOptions
{
    const char* format = nullptr;
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    double min = -std::numeric_limits<double>::infinity();
    double max = std::numeric_limits<double>::infinity();
    double step = 1.0;                 // steppers and arrow keys; 0 hides the steppers
    double stepFast = 10.0;            // Ctrl or Page Up and Down
    const char* unit = nullptr;        // suffix text such as "ms"
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;                // empty field until edited
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// Typed text is written on Enter, Tab or focus loss (Dear ImGui's default for scalars);
// steppers and arrow keys write at once.
bool NumberInput(const char* label, int* value, const NumberInputOptions& options = {});
bool NumberInput(const char* label, float* value, const NumberInputOptions& options = {});
bool NumberInput(const char* label, double* value, const NumberInputOptions& options = {});

// BindMode, Keybind (primary and alternate ImGuiKeyChord: keyboard, mouse or gamepad keys with
// ImGuiMod_ modifiers), IsBindActive and IsBindPressed are in core/input.h: the selection and
// composites families take them too, and apps query binds without any widget.

struct KeybindCaptureOptions
{
    bool allowMouse = true;
    bool allowGamepad = false;
    bool allowModifierOnly = false;
    bool showMode = false;             // adds the BindMode selector
    bool showAlternate = false;        // adds the second slot
    const char* placeholder = nullptr; // shown while unbound; null uses Strings::unbound
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    const char* error = nullptr;       // for example a conflict the caller found
    const char* helpText = nullptr;
};

// Click or Enter starts listening; the next chord is written. While listening the widget owns
// every key and stays active. Escape cancels (so Escape cannot be bound); Backspace or Delete
// while focused clears. Returns true when *bind was written.
bool KeybindCapture(const char* label, Keybind* bind, const KeybindCaptureOptions& options = {});

} // namespace facet

// ---- include/facet/widgets/lists.h ----
namespace facet
{

struct DropdownOptions
{
    const char* placeholder = nullptr; // shown while *index is -1; null uses Strings::select
    bool searchable = false;           // filter field at the top of the list, fuzzy matched
    int maxVisibleRows = 0;            // popup height in rows at most; fewer items shrink it; 0 for the token default
    DataState state = DataState::Ready; // the items: Loading and Error show in the open list
    const char* emptyText = nullptr;   // null uses Strings::empty or Strings::noResults
    const char* errorText = nullptr;   // null uses Strings::loadFailed
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;
    bool readOnly = false;
    bool mixed = false;
    const char* error = nullptr;
    const char* helpText = nullptr;
};

// A field that opens a list. ChoiceItem::group adds group headers, icons show in the field
// and the list. Click, Enter, Space or Alt+Down opens; arrows move; typing jumps (filters when
// searchable); Escape closes without writing. Returns true when *index was written.
bool Dropdown(const char* label, int* index, std::span<const ChoiceItem> items,
              const DropdownOptions& options = {});
bool Dropdown(const char* label, int* index, std::span<const char* const> items,
              const DropdownOptions& options = {});
bool Dropdown(const char* label, int* index, int itemCount, ChoiceGetter getter, void* user,
              const DropdownOptions& options = {});

} // namespace facet

// ---- include/facet/widgets/navigation.h ----
namespace facet
{

struct TabBarOptions
{
    bool closable = false;             // a close button per tab; read with TabBarClosedTab
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fills; tabs that do not fit move into a menu
};

// The tab strip with the selection pill; the caller draws the content of *index. Arrow keys
// and Ctrl+Tab move. Returns true when *index was written.
bool TabBar(const char* id, int* index, std::span<const ChoiceItem> tabs, const TabBarOptions& options = {});
// The tab whose close button was activated in the last TabBar call this frame, or -1.
int TabBarClosedTab();

struct MenuItemOptions
{
    Icon icon;
    ImGuiKeyChord shortcut = 0;        // displayed only; routing is the caller's
    bool destructive = false;
};

bool MenuItem(const char* label, const MenuItemOptions& options = {});
bool MenuItem(const char* label, bool* checked, const MenuItemOptions& options = {});

} // namespace facet

// ---- include/facet/widgets/containers.h ----
namespace facet
{

struct ShellOptions
{
    Icon icon;
    ImVec2 initialSize = {0.0f, 0.0f}; // units; used until a saved size exists; 0 token default
    ImVec2 minSize = {0.0f, 0.0f};
    ImVec2 maxSize = {0.0f, 0.0f};     // 0 for unlimited
    bool resizable = true;
    bool movable = true;
    bool closeOnEscape = false;
    bool glass = true;                 // backdrop blur, opaque tint where blur is unavailable
    bool persist = true;               // position and size saved in units in the ini file
};

// A floating, resizable menu window; the visible label part is the title. `open` may be null
// (no close button). Returns false while closed or collapsed; call EndShell only after true.
[[nodiscard]] bool BeginShell(const char* label, bool* open, const ShellOptions& options = {});
void EndShell();

class [[nodiscard]] ShellScope
{
public:
    ShellScope(const char* label, bool* open, const ShellOptions& options = {})
        : open_(BeginShell(label, open, options)) {}
    ~ShellScope() { if (open_) EndShell(); }
    ShellScope(const ShellScope&) = delete;
    ShellScope& operator=(const ShellScope&) = delete;
    explicit operator bool() const { return open_; }
private:
    bool open_;
};

struct CardOptions
{
    const char* title = nullptr;       // header line; null for none
    Icon icon;
    float width = 0.0f;                // units; 0 fills
    float height = 0.0f;               // units; 0 fits the content
};

// A grouped surface, not interactive. Its content is always submitted; EndCard is always called.
void BeginCard(const char* id, const CardOptions& options = {});
void EndCard();

class [[nodiscard]] CardScope
{
public:
    explicit CardScope(const char* id, const CardOptions& options = {}) { BeginCard(id, options); }
    ~CardScope() { EndCard(); }
    CardScope(const CardScope&) = delete;
    CardScope& operator=(const CardScope&) = delete;
};

struct VirtualListOptions
{
    float rowHeight = 0.0f;            // units; 0 for the control height
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;
    const char* errorText = nullptr;
    float height = 0.0f;               // units; 0 fills the available height
};

// A scrolling list of rowCount rows of equal height. `selection` null: rows are not
// selectable; otherwise Ctrl, Shift and box select through ImGui::BeginMultiSelect. Returns
// false while hidden, not Ready, or empty (the state is drawn instead); call EndVirtualList
// only after true. Loop NextVisibleRows and start each row with VirtualListRow.
[[nodiscard]] bool BeginVirtualList(const char* id, int rowCount, ImGuiSelectionBasicStorage* selection,
                                    const VirtualListOptions& options = {});
// Starts row `index`: a full-width row item (selection, pill, activation) followed on the same
// line by the caller's content. Returns true when the row was activated (double click, Enter).
bool VirtualListRow(int index);
void EndVirtualList();

} // namespace facet

// ---- include/facet/widgets/layout.h ----
namespace facet
{

struct SettingsRowOptions
{
    const char* description = nullptr; // second line under the label, labelSecondary
    Icon icon;
    bool modified = false;             // marks a value that differs from its default
    float labelWidth = 0.0f;           // units; 0 for the token default; stacks when narrow
};

// A label column and a control column within the available width (also in a table cell).
// Pushes the label on the id stack until EndSettingsRow, so the control can use "##value".
// EndSettingsRow is always called.
void BeginSettingsRow(const char* label, const SettingsRowOptions& options = {});
void EndSettingsRow();

} // namespace facet

// ---- include/facet/widgets/overlays.h ----
namespace facet
{

struct ModalOptions
{
    bool dismissible = true;           // Escape and a click on the scrim close it
    bool closeButton = true;
    float width = 0.0f;                // units; 0 for the token default
};

// A modal dialog over a scrim, shown while *open is true; the visible label part is the
// title. Focus is trapped inside and restored on close. Returns false while closed; call
// EndModal only after true.
[[nodiscard]] bool BeginModal(const char* label, bool* open, const ModalOptions& options = {});
void EndModal();

enum class DialogResult { None, Confirmed, Canceled };

struct ConfirmDialogOptions
{
    const char* confirmLabel = nullptr; // null uses Strings::confirm
    const char* cancelLabel = nullptr;  // null uses Strings::cancel
    bool destructive = false;           // destructive confirm button; focus starts on Cancel
};

// The visible label part is the title. Returns Confirmed or Canceled on the deciding frame and
// writes *open = false; None otherwise. Escape cancels.
DialogResult ConfirmDialog(const char* label, bool* open, const char* message,
                           const ConfirmDialogOptions& options = {});

struct ToastId
{
    std::uint32_t value = 0;
    constexpr bool IsValid() const { return value != 0; }
    friend constexpr bool operator==(const ToastId&, const ToastId&) = default;
};

struct ToastOptions
{
    Tone tone = Tone::Neutral;
    Icon icon;
    const char* detail = nullptr;
    const char* actionLabel = nullptr; // one action button
    float duration = 0.0f;             // seconds; 0 token default; < 0 until dismissed
};

// Queues a toast; texts are copied into a fixed queue, the oldest toast is dropped when it is
// full. facet::EndFrame draws the stack.
ToastId ShowToast(const char* text, const ToastOptions& options = {});
bool ToastActionPressed(ToastId toast);  // true once, on the frame after the press
bool IsToastVisible(ToastId toast);
void DismissToast(ToastId toast);

struct TooltipOptions
{
    float maxWidth = 0.0f;             // units; 0 for the token default
};

// Themed tooltip for the last item, after the hover delay and on keyboard focus.
void ItemTooltip(const char* fmt, ...) IM_FMTARGS(1);
// Rich content for the last item's tooltip. Not BeginItemTooltip: Dear ImGui's function of that
// name pairs with ImGui::EndTooltip (rule 1).
[[nodiscard]] bool BeginRichTooltip(const TooltipOptions& options = {});
void EndRichTooltip();                 // only after BeginRichTooltip returned true

enum class ContextTarget { LastItem, Window };

struct ContextMenuOptions
{
    ContextTarget target = ContextTarget::LastItem;
};

// Opens on right click, the Menu key or Shift+F10 over the target. `id` names the popup, so
// it works after composite widgets as well. Call EndContextMenu only after true.
[[nodiscard]] bool BeginContextMenu(const char* id, const ContextMenuOptions& options = {});
void EndContextMenu();

} // namespace facet

// ---- include/facet/widgets/display.h ----
namespace facet
{

// kIndeterminate (progress unknown) is in common.h: overlays take it too.

struct ProgressBarOptions
{
    Tone tone = Tone::Accent;
    bool showPercent = false;
    ControlSize size = ControlSize::Inherit;
    float width = 0.0f;                // units; 0 fills
};

// Not interactive. The visible label part is drawn above the bar; the id keys the fill
// animation. fraction 0 to 1, or kIndeterminate.
void ProgressBar(const char* label, float fraction, const ProgressBarOptions& options = {});

enum class ColumnAlign { Start, Center, End };

struct DataTableColumn
{
    const char* label = nullptr;
    float width = 0.0f;                // units; 0 stretches
    ColumnAlign align = ColumnAlign::Start;
    ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;  // stock flags: DefaultSort, NoHide
};

struct DataTableOptions
{
    bool sortable = false;             // read ImGui::TableGetSortSpecs between Begin and End
    bool multiSort = false;
    bool resizable = true;
    bool reorderable = false;
    bool hideable = false;
    int frozenColumns = 0;
    float rowHeight = 0.0f;            // units; 0 for the control height
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;
    const char* errorText = nullptr;
    float height = 0.0f;               // units; 0 fills the available height
};

// A Dear ImGui table with Facet headers. `selection` as in BeginVirtualList. Returns false
// while hidden, not Ready, or empty (the state is drawn instead); call EndDataTable only after
// true. Loop NextVisibleRows, start rows with DataTableRow, cells with ImGui::TableNextColumn.
[[nodiscard]] bool BeginDataTable(const char* id, std::span<const DataTableColumn> columns, int rowCount,
                                  ImGuiSelectionBasicStorage* selection, const DataTableOptions& options = {});
// Starts row `index`. Returns true when the row was activated (double click, Enter).
bool DataTableRow(int index);
void EndDataTable();

} // namespace facet

// ---- include/facet/widgets/pickers.h ----
namespace facet
{

enum class ColorPickerMode { Square, Wheel };

struct ColorPickerOptions
{
    bool alpha = false;
    ColorPickerMode mode = ColorPickerMode::Square;
    bool inputs = true;                 // hex and channel fields under the picker
    const ImVec4* original = nullptr;   // read during the call; shows a compare swatch that restores it
    std::span<const ImVec4> swatches = {};  // read during the call
    float width = 0.0f;                 // units; 0 fills
    bool readOnly = false;
    bool mixed = false;
};

// An inline picker for *color (sRGB, straight alpha). Writes while dragging; the keyboard
// moves the focused handle. Returns true when *color was written.
bool ColorPicker(const char* label, ImVec4* color, const ColorPickerOptions& options = {});

} // namespace facet

// ---- include/facet/widgets/media.h ----
#include "facet/core/image.h"
namespace facet
{

struct ImageViewOptions
{
    ImageFit fit = ImageFit::Contain;
    Radius corners = Radius::None;
    ImU32 tint = IM_COL32_WHITE;       // image data color, not a theme color
    bool showStates = true;            // skeleton while loading, failed marker on error
    float width = 0.0f;                // units; 0 from the image, keeping its aspect ratio
    float height = 0.0f;
};

// An image as an item, sized in design units. The visible label part is the alt text, shown
// in the failed state and as tooltip. Core Image remains the pixel-sized low-level item.
void ImageView(const char* label, ImageHandle image, const ImageViewOptions& options = {});
// An app-owned texture; Facet reads its size and keeps no reference.
void ImageView(const char* label, ID3D11ShaderResourceView* texture, const ImageViewOptions& options = {});

} // namespace facet
```

### Settings panel example

```cpp
// App state; SaveSettings and ResetSettings are application functions.
static bool open = true, vsync = true, confirmReset = false;
static int quality = 1, volume = 80;
static facet::Keybind menuKey{.primary = ImGuiKey_Insert, .mode = facet::BindMode::Toggle};
static const char* const kQuality[] = {"Low", "Medium", "High"};

if (facet::BeginShell("Settings###settings", &open, {.initialSize = {420.0f, 520.0f}}))
{
    ImGui::SeparatorText("Display");                         // stock widget
    facet::BeginSettingsRow("Vertical sync");
    facet::Toggle("##value", &vsync);
    facet::EndSettingsRow();
    ImGui::BeginDisabled(!vsync);                            // stock disabled scope
    facet::BeginSettingsRow("Quality");
    facet::SegmentedControl("##value", &quality, kQuality);
    facet::EndSettingsRow();
    ImGui::EndDisabled();
    facet::BeginSettingsRow("Volume");
    facet::Slider("##value", &volume, 0, 100, {.format = "%d %%"});
    if (facet::IsItemEditCommitted())                        // once per drag or typed value
        SaveSettings();
    ImGui::SetItemTooltip("Master output level");           // stock call on a Facet item
    facet::EndSettingsRow();
    facet::BeginSettingsRow("Menu key");
    facet::KeybindCapture("##value", &menuKey, {.showMode = true});
    facet::EndSettingsRow();
    ImGui::Spacing();                                        // stock layout between Facet items
    if (facet::Button("Reset all", {.variant = facet::ButtonVariant::Destructive}))
        confirmReset = true;
    ImGui::SameLine();
    if (facet::Button("Save", {.variant = facet::ButtonVariant::Primary}))
    {
        SaveSettings();
        facet::ShowToast("Settings saved", {.tone = facet::Tone::Success});
    }
    if (facet::ConfirmDialog("Reset all settings?", &confirmReset, "Every setting returns to its default.",
                             {.confirmLabel = "Reset", .destructive = true}) == facet::DialogResult::Confirmed)
        ResetSettings();
    facet::EndShell();
}
```
