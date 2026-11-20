# Buttons

Header: `facet/widgets/buttons.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Each entry below ends with
the widget's status. A **planned** widget exists as a function that draws a placeholder of the
widget's default size, returns the neutral result and writes nothing; a **ready** one works as its
header describes. See [the widget index](../widgets.md).

Activation means a click released inside, Enter or Space while focused, or the gamepad activate
button. Disable a button with `ImGui::BeginDisabled`.

## Widgets

### Button

```cpp
bool Button(const char* label, const ButtonOptions& options = {});
```

A push button with a variant, an optional leading icon and a loading state; returns true on the
frame it is activated (a click released inside, or Space or Enter while it has keyboard focus).

- **Variants.** `Secondary` (the default) has the neutral fill with a 3:1 outline; `Primary` the
  accent fill with `onAccent` text, for the one primary action of a view; `Plain` is text only with
  a fill on hover and press; `Destructive` the solid destructive fill with `onDestructive` text.
  Hover and press deepen the fill over `motion.durationFast`. A disabled button
  (`ImGui::BeginDisabled`) of any variant shows the same neutral disabled look, shows the
  not-allowed cursor, is not a Tab stop and never returns true.
- **Sizes.** `size` picks the control height (22, 28 or 36 units) and the padding (8, 12 or 16
  units on each side); Small draws its label in Footnote, Regular and Large in Body.
  `facet::PushControlSize` sets the size of every button that leaves it at `Inherit`.
- **Icons.** `icon` sits in front of the label, as large as the label text. A label whose visible
  part is empty (`"##save"`) with an icon gives a square icon-only button; `IconButton` is the
  widget for that, because it also shows its name as a tooltip.
- **Width.** Its content by default. `width` in units, `facet::kFill`, a negative width (the
  available width minus that much), `ImGui::SetNextItemWidth` and `ImGui::PushItemWidth` apply in
  that order. In a width narrower than its content the label ends in an ellipsis, and the whole
  label shows as a tooltip on hover and on keyboard focus.
- **Loading.** `loading` puts the spinner in the icon's place (in the label's place without an
  icon) and keeps the width; hover and press show nothing and activation returns false, but the
  button keeps its Tab stop so the focus does not jump while it waits.
- **Item queries.** After the call the button is Dear ImGui's last item: `ImGui::IsItemHovered`,
  `IsItemActive`, `SetItemTooltip` and `ImGui::SameLine` work as after a stock button, and stock
  text after `SameLine` sits on the label's line.

The demo page Buttons shows every variant, size and state; `facet_demo --capture` renders it
hovered, pressed, focused and loading. Status: ready.

### IconButton

```cpp
bool IconButton(const char* label, Icon icon, const IconButtonOptions& options = {});
```

A square button one control height wide that shows only `icon`, in the icon size of the control
text; returns true on the frame it is activated.

- **Name.** The visible label part is required: it shows as a tooltip on hover and on keyboard
  focus and is the button's name. An empty visible part is a usage error (`IM_ASSERT_USER_ERROR`).
  A label whose text changes (Pin and Unpin) keeps its id with `###`.
- **Variants.** The `ButtonVariant` colors of Button; the default is `Plain` (no fill at rest), the
  usual look in toolbars.
- **Selected.** `selected` shows the latched look: accentSubtle with a focusRing hairline, so the
  latch shows by its edge as well as its tint; hover and press tint it further. The caller flips its
  flag when the result is true.
- **States.** Disabled: the shared disabled look (fill[1] while also selected), the not-allowed
  cursor, no Tab stop. `loading`: the spinner in place of the icon, no hover or press feedback, no
  activation, but it stays a Tab stop.
- **Sizes.** `size` picks the control height, and the square follows it. `ImGui::SetNextItemWidth`
  and `PushItemWidth` change the width as rule 6 says.

Status: ready.

### CopyButton

```cpp
bool CopyButton(const char* label, const char* text, const CopyButtonOptions& options = {});
```

Copies `text` to the clipboard and shows a check for a moment; returns true on the frame it copied.
Status: planned.

### Link

```cpp
bool Link(const char* label, const LinkOptions& options = {});
```

The visible label in the accentText token as one item the width of its text, one text line tall
(the hit area grows to the minimum hit target). Returns true on activation (a click, Enter or Space
while focused).

- **Address.** With `url`, activation also calls `ImGui::GetPlatformIO().Platform_OpenInShellFn`
  (as `ImGui::TextLinkOpenURL` does) when the host has one, a trailing external-link icon marks
  the link, and the address shows as its tooltip on hover and keyboard focus.
- **Look.** A hairline underline just under the baseline while the pointer is on it, while it is
  pressed and while it has the keyboard focus; the focus ring follows after keyboard input.
  `visited` draws accentTextVisited (the caller tracks visits). Both colors reach 4.5:1 on every
  surface.
- **States.** Disabled: labelTertiary, no underline, no Tab stop, never activates or opens.
- **Sizes and width.** `size` picks the text style (Footnote for Small, Body otherwise).
  `ImGui::SetNextItemWidth` narrower than the text cuts it with an ellipsis and shows it in full in
  the tooltip. Keeps no state.

Status: ready.

### ToggleButtonGroup

```cpp
bool ToggleButtonGroup(const char* label, std::span<bool> selected, std::span<const ChoiceItem> items,
                       const ToggleButtonGroupOptions& options = {});
bool ToggleButtonGroup(const char* label, int* index, std::span<const ChoiceItem> items,
                       const ToggleButtonGroupOptions& options = {});
bool ToggleButtonGroup(const char* label, std::span<bool> selected, std::span<const char* const> items,
                       const ToggleButtonGroupOptions& options = {});
bool ToggleButtonGroup(const char* label, int* index, std::span<const char* const> items,
                       const ToggleButtonGroupOptions& options = {});
```

A joined row of latched toolbar buttons: independent flags, or one exclusive choice. The
label-only forms draw text buttons. Status: planned.

## Usage

A dialog footer: one primary action, a plain one, and a destructive one that asks first.

```cpp
if (facet::Button("Save", {.variant = facet::ButtonVariant::Primary}))
    Save();
ImGui::SameLine();
if (facet::Button("Discard", {.variant = facet::ButtonVariant::Plain}))
    Discard();
ImGui::SameLine();
if (facet::Button("Delete", {.variant = facet::ButtonVariant::Destructive, .icon = facet::Icon("trash")}))
    confirmDelete = true;
```

A latched icon button: the caller owns the flag and flips it when the result is true.

```cpp
static bool pinned = false;
if (facet::IconButton("Pin window", facet::Icon("pin"), {.selected = pinned}))
    pinned = !pinned;
```

A toolbar with independent latches and one with an exclusive choice. A span cannot bind to a braced
list in C++20, so the items and flags are arrays. `std::vector<bool>` does not convert to
`std::span<bool>`; use `bool[]` or `std::array<bool, N>`.

```cpp
static const facet::ChoiceItem kViews[] = {
    {"Grid", facet::Icon("grid")},
    {"List", facet::Icon("list")},
};
static bool shown[2] = {true, false};
facet::ToggleButtonGroup("Panels", shown, kViews);

static int view = 0;
facet::ToggleButtonGroup("View", &view, kViews);
```

## Planned, not declared yet

The later tier of this family is empty: every button widget of the catalogue is declared above.
