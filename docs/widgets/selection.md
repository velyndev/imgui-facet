# Selection

Header: `facet/widgets/selection.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Each entry below ends with
the widget's status; every widget of the family is ready and works as its header describes. See [the widget index](../widgets.md).

Every widget here returns true on the frame it wrote its value or selection. Item lists are spans of
`ChoiceItem` (label first, so `{"Option 1"}` works) or of `const char* const` for label-only lists.
Dropdowns, list boxes and trees are in [lists](lists.md).

The visible label part is drawn by the widget's shape (rule 6): in front of one-line controls
(segmented control, cycle selector, key bind mode selector), above groups that wrap into rows
(checkbox and radio groups, chip groups, option cards), after the mark for checkboxes, and in front
of the switch for toggles. A label in front of a control with a set width (and of every field)
takes the label column of a settings row, so the controls of a column start at one x; a compact
control that fits its content keeps its label tight. `"##name"` hides it, as in settings rows.

## Widgets

### Toggle

```cpp
bool Toggle(const char* label, bool* value, const ToggleOptions& options = {});
```

An on/off switch with the visible label in front and the switch (the toggle tokens, 38 by 22 units)
after it; returns true on the frame it wrote `*value`.

- **Input.** A click on the label or the switch, or Space or Enter while it has keyboard focus,
  flips the value. A press on the switch that moves further than the drag threshold drags the knob,
  and letting go sets the side the knob is on (also when the pointer left the toggle).
- **Look.** A capsule track with circular ends and a round solid knob in the `knob` token (white in
  both themes) with one controlBorder hairline and the level 1 shadow, in every size and state.
  Off: fill[0] with the controlBorder hairline (controlBorderHover under the pointer). On: the
  accent mark (focusRing, 3:1 against an off track) without an outline. The knob springs between
  the ends (the quick spring) and the colors follow over `motion.durationFast`; with reduced motion
  both jump. The focus ring goes around the track after keyboard input only, never after a click.
- **States.** Disabled (`ImGui::BeginDisabled`): an off track in fill[3], an on track in the accent
  mark at the disabled picture opacity (so on and off still differ by color), the knob at that
  opacity, a labelTertiary label, the not-allowed cursor, no Tab stop. `readOnly` (or a null
  `value`): an on track in labelSecondary instead of the accent, focusable, never written, and the
  arrow cursor. `loading`: a spinner in the knob and no
  input, but it stays a Tab stop. `mixed`: the knob in the middle of a neutral track until the
  first edit, which writes on; after that the toggle shows its value until `mixed` is switched off
  and on again. `error` replaces `helpText` under the toggle, in destructive with the error icon, and the
  track keeps a destructive hairline in both positions.
- **Sizes and width.** `size` sets the item height (the control height) and the label text; the
  switch keeps the toggle tokens at every size. The default width is the label, spacing.sm and the
  switch. With a set width the label takes the label column (`layout.labelColumn`, at most half the
  width) and the switch follows it, so a toggle lines up with the sliders and fields of a column;
  a label longer than its room ends in an ellipsis and shows in full as a tooltip on hover and
  keyboard focus. Without a visible label the switch starts the item.

Status: ready.

### Checkbox

```cpp
bool Checkbox(const char* label, bool* value, const CheckboxOptions& options = {});
bool Checkbox(const char* label, CheckState* state, const CheckboxOptions& options = {});
```

A box (the checkboxSize token, 16 units) and the visible label after it; the label is part of the
hit area. Returns true on the frame it wrote the value.

- **Input.** A click, or Space or Enter while it has keyboard focus, flips the value. In the
  `CheckState` overload Off and Mixed become On, and On becomes Off; only the caller sets Mixed.
  A mouse press that then crosses other checkboxes of the same window with the button held gives
  each of them the first box's new value, the first box included (one write per box that changes),
  and the release does not flip the first box back.
- **Look.** Unchecked: a quiet box, the neutral control fill (fill[2] at rest, fill[1] on hover,
  fill[0] while pressed) and one controlBorder hairline (controlBorderHover on hover). Checked: the
  accent with an onAccent check and the accent mark as its hairline, at the same width. Mixed:
  the same fill with a minus, so it differs from both other states by its shape and not only by
  its color. The corners are radii.element, at most a quarter of the box, so a box never looks like
  a radio mark. The fill fades over `motion.durationFast` (at once with reduced motion). The focus
  ring goes around the box after keyboard input.
- **States.** Disabled: fill[3] with a labelTertiary glyph, no Tab stop. `readOnly` (or a null
  value pointer): an unfilled quiet box with the glyph in label instead of the accent fill,
  focusable, never written. `error` replaces `helpText` under the box and turns its hairline
  destructive.
- **Sizes and width.** `size` sets the item height and the label text; the box keeps its token
  size. The default width is the box, spacing.sm and the label; `ImGui::SetNextItemWidth` and
  `PushItemWidth` apply as rule 6 says, and a narrower width cuts the label with an ellipsis and
  shows it in full on hover and keyboard focus.

Status: ready.

### CheckboxGroup

```cpp
bool CheckboxGroup(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                   const CheckboxGroupOptions& options = {});
bool CheckboxGroup(const char* label, std::span<bool> checked, std::span<const char* const> items,
                   const CheckboxGroupOptions& options = {});
```

A list of checkboxes (the Checkbox look and input, drag painting included) with the visible label
part above it. Returns true when any flag was written.

- **Items.** One box per item; `checked` holds one flag per item. Extra flags are ignored, items
  without a flag and items with `disabled` are drawn disabled and are no Tab stop. Every enabled box
  is a Tab stop, in reading order.
- **Select all.** `selectAll` adds a box named `Strings::selectAll` above the list, which shows On
  when every enabled item is checked, Off when none is, and the mixed mark otherwise. Activating it
  from Off or Mixed checks every enabled item, from On unchecks them. The items are indented so
  their boxes line up with its label.
- **Columns.** `columns` 0 fits as many columns of the widest item (plus spacing.xl between
  columns) as the width holds, at most one per item; a set number divides the width equally. A
  column is as wide as the widest item, or narrower when the share of the width is less. Items
  fill the columns row by row, so the Tab order follows the reading order. Labels that do not fit a
  column are cut with an ellipsis and shown in full on hover and keyboard focus.
- **States.** `ImGui::BeginDisabled` disables every box. `readOnly`: shown and focusable, never
  written. `error` replaces `helpText` under the list and turns the hairline of every box
  destructive.
- **Width.** The available width by default (rule 6).

Status: ready.

### RadioGroup

```cpp
bool RadioGroup(const char* label, int* index, std::span<const ChoiceItem> items,
                const RadioGroupOptions& options = {});
bool RadioGroup(const char* label, int* index, std::span<const char* const> items,
                const RadioGroupOptions& options = {});
```

Mutually exclusive options, each a radio mark (the checkboxSize token) with its label after it and
`ChoiceItem::detail` as a second line in Footnote labelSecondary. The visible label part is drawn
above the group. Returns true on the frame it wrote `*index`; -1 means none is selected.

- **Input.** A click on an option (mark, label or detail) or Space or Enter on the focused one
  selects it. The group is one Tab stop: the selected option, or the first enabled one when none
  is selected. The four arrow keys move the focus and the selection to the previous or next
  enabled option and wrap at the ends; Home and End go to the first and last enabled option.
  Options with `disabled` are drawn disabled and skipped. Escape does not undo a choice.
- **Look.** Unselected: a quiet circle in the checkbox colors (the neutral fill and one
  controlBorder hairline, both stronger on hover). Selected: an accent disc with an onAccent dot and
  the accent mark as its hairline; the dot grows in over `motion.durationFast`. The focus ring is
  round, around the mark, after keyboard input only.
- **Layout.** `Orientation::Vertical` (the default) puts one option under the other, each one
  control height tall plus its detail line. `Horizontal` places them side by side, spacing.xl
  apart, and starts a new row when the next option does not fit. Default width: the available
  width (rule 6); an option is as wide as its mark, label and detail, at most the group width, and
  a label or detail that does not fit is cut with an ellipsis and shown in full on hover and
  keyboard focus.
- **States.** `ImGui::BeginDisabled` disables every option. `readOnly` (or a null `index`): the
  selection shows as a label dot in an unfilled quiet circle, the arrows still move the focus,
  nothing is written.
  `error` replaces `helpText` under the group and turns the hairline of every mark destructive.

Status: ready.

### SegmentedControl

```cpp
bool SegmentedControl(const char* label, int* index, std::span<const ChoiceItem> items,
                      const SegmentedControlOptions& options = {});
bool SegmentedControl(const char* label, int* index, std::span<const char* const> items,
                      const SegmentedControlOptions& options = {});
```

Joined segments on one track with the sliding selection pill behind the chosen one, and the
visible label part in front (rule 6). Returns true on the frame it wrote `*index`; -1 shows no
segment selected.

- **Input.** Pressing a segment selects it at once, and a drag across the segments with the button
  held moves the selection to the segment under the pointer. The control is one Tab stop (the
  selected segment, else the first enabled one); Left and Right move the focus and the selection
  and stop at the ends, Home and End go to the first and last enabled segment. Items with
  `disabled` are drawn disabled and skipped.
- **Look.** A quiet container: the track is fill[2] with one controlBorder hairline, and the
  segments have no outline of their own. The pill is accentSubtle,
  metrics.segmentInset inside the track with concentric corners, and the selected label is
  semibold, so the choice does not depend on the tint alone. Hover and press tint a segment inside
  the same inset; hairline dividers stand between segments where the pill is not. The pill springs
  to a new segment with the standard spring (its leading edge a little faster, so it stretches on
  the way), comes to rest on whole pixels, follows a change of width or scale with the same
  spring, and moves at once with reduced motion. After keyboard input the focus ring sits inside
  the focused segment, so it never crosses the container's edge.
- **Width.** `width` 0 fits the segments (each its content plus the control padding, measured in
  semibold so the bold label of the selected one fits), at most the available width; `kFill`, a
  set width, `ImGui::SetNextItemWidth` or a row shorter than the content divide the track into
  equal segments. `iconOnly` shows the item icons in square segments with the labels as tooltips.
  When labels do not fit their segments they collapse to icons if every item has one, and are cut
  with an ellipsis otherwise; either way the full label shows on hover and keyboard focus.
- **States.** `ImGui::BeginDisabled`: fill[3], a neutral pill and labelTertiary labels.
  `readOnly` (or a null `index`): no track fill, a neutral pill, focusable, never written.
  `mixed`: no pill until the first press or arrow key, which writes even when it picks the segment
  that was already set.
- **Limits.** At most 64 segments are shown.

Status: ready.

### Chip

```cpp
bool Chip(const char* label, const ChipOptions& options = {});
bool ChipRemovePressed();
```

A pill-shaped tag one control height tall with the visible label, an optional leading dot in a data
color (`color`) and an optional icon. Returns true when the chip body is activated (a click, or
Space or Enter while focused); a chip used as a filter flips its own `selected` flag then.

- **Remove.** `removable` adds a close button inside the end of the pill: a separate mouse target
  named `Strings::remove` (its tooltip). Delete or Backspace on the focused chip press it too.
  `ChipRemovePressed()`, called right after the chip, is true on that frame; the caller removes the
  chip from its data. The button is not a Tab stop of its own, so a row of chips has one stop per
  chip.
- **Look.** A capsule with circular ends. Unselected: the neutral fill with one controlBorder
  hairline (controlBorderHover on hover), like a secondary button. `selected`: the selection tint
  (accentSubtle) with the accent hairline at the same width and a semibold label, faded in over
  `motion.durationFast`; read-only selected chips keep the quiet hairline on fill[0]. The focus
  ring follows the capsule, after keyboard input only.
- **States.** Disabled: fill[3] (fill[1] while selected), labelTertiary, no Tab stop, nothing
  reported. The dot keeps its data color unless disabled.
- **Width.** Its content (measured with the semibold label, so selecting never cuts it);
  `ImGui::SetNextItemWidth` sets another width as rule 6 says, and a narrower one cuts the label
  with an ellipsis and a tooltip.

Status: ready.

### ChipGroup

```cpp
bool ChipGroup(const char* label, std::span<bool> selected, std::span<const ChoiceItem> items,
               const ChipGroupOptions& options = {});
bool ChipGroup(const char* label, std::span<bool> selected, std::span<const char* const> items,
               const ChipGroupOptions& options = {});
bool ChipGroup(const char* label, int* index, std::span<const ChoiceItem> items,
               const ChipGroupOptions& options = {});
bool ChipGroup(const char* label, int* index, std::span<const char* const> items,
               const ChipGroupOptions& options = {});
int ChipGroupRemovedItem();
```

Chips (the Chip look) that wrap into rows inside the width, one per item, with the visible label
part above them; `ChoiceItem::detail` shows as a count after the label and `icon` before it. Each
chip is a Tab stop that a click, Space or Enter activates. Returns true when a flag or the index was
written.

- **Filters.** The `std::span<bool>` forms flip the item's flag on activation. Items without a flag
  are drawn disabled. `clearAll` adds a trailing chip with `Strings::clear` that unsets every flag
  of an enabled item (with nothing set it writes nothing; it stays enabled so the keyboard focus
  is never left on a disabled chip).
- **One choice.** The `int*` forms write the activated chip's index (-1 means none). The chosen chip
  sits on the selection pill, which slides from chip to chip with the same spring as in segmented
  controls, also across rows.
- **Remove.** `removable` gives every chip its remove button (Delete or Backspace on the focused
  chip press it too); `ChipGroupRemovedItem()` returns the item on that frame, or -1. The caller
  removes the item from its data.
- **Single line.** `singleLine` keeps one row: chips that do not fit move into a chip named
  `Strings::more` at the end of the row, which opens a menu of them (checked when set or chosen);
  the selected or set chips stay in the row longest, and the "more" chip shows the selected look
  while one of them is in the menu. Chips in the menu can be chosen or flipped there, not removed.
- **States.** `ImGui::BeginDisabled` disables every chip. `readOnly` (or a null `index`): shown
  and focusable, never written. Disabled items are drawn disabled and are no Tab stop.
- **Width.** The available width by default (rule 6); a chip wider than the group is cut to it.
  A single-line group ranks at most 64 chips; a longer one wraps.

Status: ready.

### CycleSelector

```cpp
bool CycleSelector(const char* label, int* index, std::span<const ChoiceItem> items,
                   const CycleSelectorOptions& options = {});
bool CycleSelector(const char* label, int* index, std::span<const char* const> items,
                   const CycleSelectorOptions& options = {});
```

The visible label in front (inside the width), then a field frame (the text field frame) with the
value centered between a previous and a next arrow button. Returns true on the frame it wrote
`*index`.

- **Input.** The arrow buttons step back and forward (and repeat while held); a click on the value,
  or Space or Enter, steps forward; Left and Right step while it has keyboard focus; the wheel steps
  while the pointer is over it (and does not scroll the window then). Items with `disabled` are
  skipped. The selector is one Tab stop; the arrow buttons are mouse targets only.
- **Ends.** `wrap` (the default) continues at the other end; without it the arrow at an end is
  drawn in labelTertiary and does nothing.
- **Value.** An index out of range shows `Strings::select` in labelSecondary and is written only
  by a step, which goes to the first (or, backwards, the last) enabled item. `mixed` shows
  `Strings::mixed` until the first step, which writes even when it lands on the same item. A value
  that does not fit is cut with an ellipsis and shown in full on hover and keyboard focus.
- **Page dots.** `pageDots` adds a row under the frame with a dot (the pageDotSize token) per item,
  the chosen one in the accent mark ocusRing and the others in controlBorder (separator while
  disabled); it is left out when the dots do not fit.
- **States.** Disabled: the field frame's disabled look, no Tab stop. `readOnly` (or a null
  `index`): shown and focusable, never written; the arrows show they cannot step. `error`
  replaces `helpText` under the frame.
- **Width.** The available width by default (rule 6).

Status: ready.

### KeybindModeSelector

```cpp
bool KeybindModeSelector(const char* label, BindMode* mode, const KeybindModeSelectorOptions& options = {});
```

Chooses how a key bind activates (`BindMode` from `core/input.h`): Hold, Toggle, Always and Off
with the `Strings::bindHold`, `bindToggle`, `bindAlways` and `bindOff` names. Returns true on the
frame it wrote `*mode`.

- **Form.** A SegmentedControl (the same strip, pill, keys and looks): the visible label in front,
  one Tab stop, a press or a drag picks a mode, Left, Right, Home and End move the selection.
- **Offered modes.** `allowAlways` and `allowOff` leave those modes out. A current mode that is
  not offered is shown, as a disabled segment, until another mode is chosen; then it is gone and
  cannot be chosen again.
- **Narrow rows.** When the resolved width is shorter than the label and the segments at their
  content, the selector draws a `Dropdown` (lists.h) with the same modes, options, error and help
  text instead, so it never outgrows the row. The dropdown is the lists family's widget and looks
  and works as that widget does.
- **States.** `readOnly` (or a null `mode`) shows the mode and never writes; `mixed` shows no pill
  until the first choice; `error` replaces `helpText` under the segments and turns the hairline
  of the strip destructive.
- **Width.** `width` 0 fits the segments, at most the available width; a set width divides it
  into equal segments while they fit.

Status: ready.

### OptionCardGroup

```cpp
bool OptionCardGroup(const char* label, int* index, std::span<const OptionCard> cards,
                     const OptionCardGroupOptions& options = {});
bool OptionCardGroup(const char* label, std::span<bool> selected, std::span<const OptionCard> cards,
                     const OptionCardGroupOptions& options = {});
```

Large selectable cards in a grid, for choices that need a picture or a description, with the
visible label part above them. Returns true when the index or a flag was written.

- **Card.** bgSecondary with radii.card corners and one controlBorder hairline (controlBorderHover
  on hover). Inside (spacing.md
  padding): `image` across the top (the thumbnailSize token tall, filling the width, cropped to
  fit), then the title line in Headline with `icon` in front of it when there is no image, `badge`
  as a small pill and the mark at the end of the line, then `description` wrapped in Footnote
  labelSecondary. A title that does not fit is cut with an ellipsis and shown in full on hover and
  keyboard focus. The cards of one row share the height of the tallest.
- **Selected.** The selected tile look: the selection tint (accentSubtle) with the accent hairline
  at the one border width, and the mark: a radio mark for one choice, a checkbox mark for several,
  so the two forms read differently. After keyboard input the focus ring sits inside the card.
- **One choice** (`int*`): one Tab stop (the chosen card, else the first enabled one). Left and
  Right move along the reading order, Up and Down by a row; disabled cards are skipped and the
  movement stops at the edges. A click, Space or Enter chooses the card. -1 means none.
- **Several** (`std::span<bool>`): every enabled card is a Tab stop that a click, Space or Enter
  flips. Cards without a flag are drawn disabled.
- **Grid.** `columns` 0 fits as many cards of `cardWidth` (the cardWidth token, 256 units, by
  default) as the width holds, at least one; the cards then share the width equally. `width` 0
  takes the available width.
- **States.** Disabled cards (`OptionCard::disabled`, or `ImGui::BeginDisabled` for all): fill[3],
  labelTertiary, no Tab stop. `readOnly` (or a null `index`): shown and focusable, never written;
  a selected card takes fill[0] with the quiet hairline and a read-only mark. `error` replaces
  `helpText` under the grid and turns the hairline of every card and mark destructive.

Status: ready.

## Usage

A settings block: a switch, a tri-state select-all over a group, and a segmented choice. The flags
live in a `bool` array because `std::vector<bool>` does not convert to `std::span<bool>`.

```cpp
static bool enabled = true;
static bool channels[3] = {true, false, true};
static const char* const kChannels[] = {"Option 1", "Option 2", "Option 3"};
static int quality = 1;
static const char* const kQuality[] = {"Low", "Medium", "High"};

facet::Toggle("Enabled", &enabled);
facet::CheckboxGroup("Channels", channels, kChannels, {.selectAll = true});
facet::SegmentedControl("Quality", &quality, kQuality);
```

Chips that the user can remove. The remove button is read right after the call, like
`TabBarClosedTab`, and the caller removes the entry from its own data.

```cpp
for (int i = 0; i < static_cast<int>(filters.size()); ++i)
{
    ImGui::PushID(i);
    if (facet::Chip(filters[i].name.c_str(), {.selected = filters[i].on, .removable = true}))
        filters[i].on = !filters[i].on;
    const bool remove = facet::ChipRemovePressed();
    ImGui::PopID();
    if (remove)
    {
        filters.erase(filters.begin() + i);
        --i;
    }
    ImGui::SameLine();
}
ImGui::NewLine();
```

A key bind row: the mode selector edits only the mode of a `Keybind` that `KeybindCapture`
(inputs.h) records.

```cpp
static facet::Keybind menuKey{.primary = ImGuiKey_Insert, .mode = facet::BindMode::Toggle};
facet::KeybindCapture("Menu key", &menuKey);
ImGui::SameLine();
facet::KeybindModeSelector("##mode", &menuKey.mode, {.allowAlways = false});
```

## Planned, not declared yet

The later tier of this family is empty. One extended widget of the selection family is not
declared here:

- **SearchableChecklist**: an inline multi-select over hundreds of items with a filter field, a
  select-all-visible checkbox, a selected count and a show-only-selected toggle. It is a
  virtualized list with checkboxes, so it belongs in `lists.h` on the list and selection core of
  part B; it is planned there and not declared yet.
