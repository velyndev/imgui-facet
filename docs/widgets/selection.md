# Selection

Header: `facet/widgets/selection.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Each entry below ends with
the widget's status. A **planned** widget exists as a function that draws a placeholder of the
widget's default size, returns the neutral result and writes nothing; a **ready** one works as its
header describes. See [the widget index](../widgets.md).

Every widget here returns true on the frame it wrote its value or selection. Item lists are spans of
`ChoiceItem` (label first, so `{"Option 1"}` works) or of `const char* const` for label-only lists.
Dropdowns, list boxes and trees are in [lists](lists.md).

The visible label part is drawn by the widget's shape (rule 6): in front of one-line controls
(segmented control, cycle selector, key bind mode selector), above groups that wrap into rows
(checkbox and radio groups, chip groups, option cards), and after the mark for toggles and
checkboxes. `"##name"` hides it, as in settings rows.

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
- **Look.** Off: the neutral fill with the 3:1 boundary hairline and a labelSecondary knob. On: the
  accent track with an onAccent knob and a focusRing hairline. The knob springs between the ends
  (the quick spring) and the colors follow over `motion.durationFast`; with reduced motion both
  jump. The focus ring goes around the track after keyboard input.
- **States.** Disabled (`ImGui::BeginDisabled`): fill[3], labelTertiary knob and label, the
  not-allowed cursor, no Tab stop. `readOnly` (or a null `value`): labelSecondary instead of the
  accent, focusable, never written, and the arrow cursor. `loading`: a spinner in the knob and no
  input, but it stays a Tab stop. `mixed`: the knob in the middle of a neutral track until the
  first edit, which writes on; after that the toggle shows its value until `mixed` is switched off
  and on again. `error` replaces `helpText` under the toggle, in destructive with the error icon.
- **Sizes and width.** `size` sets the item height (the control height) and the label text; the
  switch keeps the toggle tokens at every size. The default width is the label, spacing.sm and the
  switch. A wider width puts the switch at the end of the row; a narrower one cuts the label with an
  ellipsis and shows it in full as a tooltip on hover and keyboard focus.

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
- **Look.** Unchecked: no fill at rest (fill[1] on hover, fill[0] while pressed) and the 3:1
  boundary hairline. Checked: the accent with an onAccent check and a focusRing hairline. Mixed:
  the same fill with a minus, so it differs from both other states by its shape and not only by
  its color. The corners are radii.element, at most a quarter of the box, so a box never looks like
  a radio mark. The fill fades over `motion.durationFast` (at once with reduced motion). The focus
  ring goes around the box after keyboard input.
- **States.** Disabled: fill[3] with a labelTertiary glyph, no Tab stop. `readOnly` (or a null
  value pointer): checked and mixed boxes are labelSecondary instead of the accent, focusable,
  never written. `error` replaces `helpText` under the box.
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

A labelled list of checkboxes in one or more columns, with an optional select-all box. Status: planned.

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
- **Look.** Unselected: the boundary hairline circle, fill[1] on hover. Selected: an accent disc
  with an onAccent dot and a focusRing hairline; the dot grows in over `motion.durationFast`. The
  focus ring is round, around the mark.
- **Layout.** `Orientation::Vertical` (the default) puts one option under the other, each one
  control height tall plus its detail line. `Horizontal` places them side by side, spacing.xl
  apart, and starts a new row when the next option does not fit. Default width: the available
  width (rule 6); an option is as wide as its mark, label and detail, at most the group width, and
  a label or detail that does not fit is cut with an ellipsis and shown in full on hover and
  keyboard focus.
- **States.** `ImGui::BeginDisabled` disables every option. `readOnly` (or a null `index`): the
  selection shows in labelSecondary, the arrows still move the focus, nothing is written.
  `error` replaces `helpText` under the group.

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
- **Look.** The track is fill[2] with the 3:1 boundary hairline. The pill is accentSubtle,
  metrics.segmentInset inside the track with concentric corners, and the selected label is
  semibold, so the choice does not depend on the tint alone. Hover and press tint a segment inside
  the same inset; hairline dividers stand between segments where the pill is not. The pill springs
  to a new segment with the standard spring (its leading edge a little faster, so it stretches on
  the way), comes to rest on whole pixels, follows a change of width or scale with the same
  spring, and moves at once with reduced motion. The focus ring goes around the focused segment.
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

A pill-shaped tag that can be selected and can carry a remove button; `ChipRemovePressed` reports
the remove button of the last chip. Status: planned.

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

Wrapping chips as filters (flags) or as one choice (index), with optional remove buttons and a
clear-all chip. Status: planned.

### CycleSelector

```cpp
bool CycleSelector(const char* label, int* index, std::span<const ChoiceItem> items,
                   const CycleSelectorOptions& options = {});
bool CycleSelector(const char* label, int* index, std::span<const char* const> items,
                   const CycleSelectorOptions& options = {});
```

A value between previous and next arrows, stepped with Left and Right. Status: planned.

### KeybindModeSelector

```cpp
bool KeybindModeSelector(const char* label, BindMode* mode, const KeybindModeSelectorOptions& options = {});
```

Chooses how a key bind activates (`BindMode` from `core/input.h`): Hold, Toggle, Always or Off.
Status: planned.

### OptionCardGroup

```cpp
bool OptionCardGroup(const char* label, int* index, std::span<const OptionCard> cards,
                     const OptionCardGroupOptions& options = {});
bool OptionCardGroup(const char* label, std::span<bool> selected, std::span<const OptionCard> cards,
                     const OptionCardGroupOptions& options = {});
```

Large selectable cards with a title, a description, an icon or picture and a badge, for one or
several choices. Status: planned.

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
