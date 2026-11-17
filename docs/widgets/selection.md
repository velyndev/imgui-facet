# Selection

Header: `facet/widgets/selection.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Every widget here is
**planned**: the function exists, draws a placeholder of the widget's default size, returns false and
writes nothing. Phase 2 implements the widgets and changes their status to ready. See
[the widget index](../widgets.md).

Every widget here returns true on the frame it wrote its value or selection. Item lists are spans of
`ChoiceItem` (label first, so `{"Option 1"}` works) or of `const char* const` for label-only lists.
Dropdowns, list boxes and trees are in [lists](lists.md).

## Widgets

### Toggle

```cpp
bool Toggle(const char* label, bool* value, const ToggleOptions& options = {});
```

An on/off switch with the label in front; a click, Space or Enter flips the value. Status: planned.

### Checkbox

```cpp
bool Checkbox(const char* label, bool* value, const CheckboxOptions& options = {});
bool Checkbox(const char* label, CheckState* state, const CheckboxOptions& options = {});
```

A box and its label; the `CheckState` overload adds the mixed look that only the caller sets.
Status: planned.

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

Mutually exclusive options, one Tab stop, arrow keys move the selection. Status: planned.

### SegmentedControl

```cpp
bool SegmentedControl(const char* label, int* index, std::span<const ChoiceItem> items,
                      const SegmentedControlOptions& options = {});
bool SegmentedControl(const char* label, int* index, std::span<const char* const> items,
                      const SegmentedControlOptions& options = {});
```

Joined segments with the sliding selection pill. Status: planned.

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

The later tier of this family is empty: every selection widget of the catalogue is declared above.
