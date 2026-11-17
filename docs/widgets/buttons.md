# Buttons

Header: `facet/widgets/buttons.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Every widget here is
**planned**: the function exists, draws a placeholder of the widget's default size, returns false and
writes nothing. Phase 2 implements the widgets and changes their status to ready. See
[the widget index](../widgets.md).

Activation means a click released inside, Enter or Space while focused, or the gamepad activate
button. Disable a button with `ImGui::BeginDisabled`.

## Widgets

### Button

```cpp
bool Button(const char* label, const ButtonOptions& options = {});
```

A push button with a variant (`Secondary`, `Primary`, `Plain`, `Destructive`), an optional leading
icon and a loading state; returns true when activated. Status: planned.

### IconButton

```cpp
bool IconButton(const char* label, Icon icon, const IconButtonOptions& options = {});
```

A square button showing only an icon; the visible label part is its tooltip and name, and
`selected` shows a latched look. Status: planned.

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

Accent text that underlines on hover and focus; with `url` it also asks Dear ImGui's platform hook to
open the address. Status: planned.

### ToggleButtonGroup

```cpp
bool ToggleButtonGroup(const char* label, std::span<bool> selected, std::span<const ChoiceItem> items,
                       const ToggleButtonGroupOptions& options = {});
bool ToggleButtonGroup(const char* label, int* index, std::span<const ChoiceItem> items,
                       const ToggleButtonGroupOptions& options = {});
```

A joined row of latched toolbar buttons: independent flags, or one exclusive choice. Status: planned.

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
