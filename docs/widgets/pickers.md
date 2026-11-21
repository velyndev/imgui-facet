# Pickers

Header: `facet/widgets/pickers.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Each entry below ends with
the widget's status. A **planned** widget exists as a function that draws a placeholder of the
widget's default size, returns the neutral result and writes nothing; a **ready** one works as its
header describes. See [the widget index](../widgets.md). The demo gallery shows the color pickers on
the page Color pickers and the others on the page Other pickers.

Colors are `ImVec4` in gamma-encoded sRGB with straight alpha everywhere (see `core/color.h`): the
edited value, swatches, history and the compare color. Without `alpha` the alpha channel is never
written; without `hdr` channels are clamped to 0 to 1. Fields that open a popover open on a click,
Enter, Space or Alt+Down and close on Escape or a click outside. Every widget here returns true on
the frame it wrote its value.

## Color pickers

The color pickers share one model: HSV and HSL over the gamma-encoded sRGB channels, which is also the
space their gradients are drawn in, so the color under the centre of a handle is the color the
handle stands for. A picker keeps the last hue (and saturation) under its id, so a color dragged
through grey or black keeps its hue. A part writes only what it edits: an edit of one channel leaves
the other channels with their exact float values, and a color that is not touched is never written,
so a color survives a round trip through every picker unchanged. Handles are filled with their
color and carry an inner ring in white or black (whichever reads better on that color) with a
hairline of the other one outside it, so they read on any color in both themes. Disabled pickers
show their colors dimmed and the handles in the disabled label color.

### ColorWell

```cpp
bool ColorWell(const char* label, ImVec4* color, const ColorWellOptions& options = {});
```

A field-like swatch button: the label in front, then a field frame with the swatch (a checkerboard
under translucent colors, the opaque color in its left half) and, with `showHex`, the `#RRGGBB` or
`#RRGGBBAA` text. A click, Enter, Space or Alt+Down opens a ColorPicker in a popover below the well
(flipped above when there is no room), with the color from before the opening as its compare color
and `swatches` under it. The popover follows the well while it scrolls. Escape closes it and
restores the color from before it opened; a click outside closes it and keeps the edit; the focus
goes back to the well. Ctrl+C on the focused well copies the color as hex text, Ctrl+V pastes any
color text that ColorInputs reads. The well is a drag and drop source and target of Dear ImGui's
`IMGUI_PAYLOAD_TYPE_COLOR_3F` and `_4F` payloads. Three control sizes; the default width fits the
label, the swatch and the hex text. `readOnly` shows the color, copies it and opens nothing;
`mixed` shows a striped swatch (and Strings::mixed as the hex text) until the first edit; `error`
and `helpText` go under the frame. Status: ready.

### ColorPicker

```cpp
bool ColorPicker(const char* label, ImVec4* color, const ColorPickerOptions& options = {});
```

An inline panel, from the top: the saturation and value square (or, with `ColorPickerMode::Wheel`,
a hue ring around a saturation and value triangle with the pure hue, white and black at its
corners), the hue bar (Square mode), the alpha bar (with `alpha`), the compare strip (with
`original`: the color now beside the original, whose half restores it when clicked or activated
with Enter or Space, Strings::original as its tooltip), ColorInputs (with `inputs`) and a
SwatchPalette of `swatches`. Every part is a Tab stop. The square and the triangle move on both
axes with the arrow keys (Shift a tenth of a step, Ctrl ten steps, Page Up and Down the value by a
tenth, Home and End the saturation to its ends); the bars and the ring take the keys of a Slider,
and the hue wraps at its ends. A press puts a handle under the pointer and a drag follows it
(Shift finer); Escape during a drag or a held key restores the color from the start of that edit.
The default width is `metrics.pickerWidth`, at most the available width. `hdr` lets the inputs edit
channels above 1; the square, the triangle and the bars cover 0 to 1, so an edit there brings an
HDR color into that range. The visible label part is the panel's tooltip. Status: ready.

### ColorInputs

```cpp
bool ColorInputs(const char* label, ImVec4* color, const ColorInputsOptions& options = {});
```

A Dropdown with the models (Strings::colorModelRgb, Hsv, Hsl and Hex; left out with `modeSwitch`
false, and on a line of its own above the fields when they do not fit beside it), then one DragNumber per channel of the
model, each a Tab stop marked with the first letter of its channel name (Strings::colorRed and the
others), or one HexInput (six digits, eight with `alpha`) marked `#`. RGB channels run from 0 to 255,
hue from 0 to 360 (wrapping), saturation, value, lightness and alpha in HSV and HSL from 0 to 100 %;
with `floats` (always with `hdr`) every channel is a fraction from 0 to 1, with `hdr` without an
upper limit for red, green, blue and value. The fields drag, step and type as DragNumber and
HexInput do; a field writes only its channel, and a hex edit changes only the bytes that changed.
Ctrl+V on a focused field reads color text and writes the whole color: `#RGB`, `#RGBA`, `#RRGGBB`,
`#RRGGBBAA`, six or eight hex digits with a letter among them, `rgb()`, `rgba()`, `hsv()` and `hsl()`
with percentages or fractions, and three or four bare numbers (fractions when all are at most 1,
0 to 255 otherwise); a single number is pasted into the field as a number. The model the user
picks is kept under the id. `error` outlines every field and its message goes under the row.
Status: ready.

### SwatchPalette

```cpp
struct ColorHistory { static constexpr int kCapacity = 8; ImVec4 colors[kCapacity]; int count; };
bool SwatchPalette(const char* label, ImVec4* color, std::span<const ImVec4> swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});
bool SwatchPalette(const char* label, ImVec4* color, std::span<ImVec4> storage, int* count, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});
bool SwatchPalette(const char* label, ImVec4* color, std::vector<ImVec4>* swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});
```

A grid of swatches the size of the control height (smaller when a fixed `columns` count does not
fit the width), with the recent colors of `history` in a row above it. The whole palette is one
Tab stop: Left and Right move the focused swatch, Up and Down move to the swatch above or below
(across the recent row and the grid), Home and End go to the first and last swatch, Enter or Space
picks it; a click picks the swatch under the pointer. Each pick goes to the front of the history.
The swatch equal to the color has a ring in the label color and a check mark in white or black,
whichever reads on it, so the choice does not rest on color alone. Without `alpha` the palette
compares and writes red, green and blue only. The editable forms end with a + swatch (Strings::add
as its tooltip) that adds the color while there is room, Delete or Backspace removes the focused
swatch, Alt with Left or Right moves it, and a drag moves a swatch to where the pointer is (a
focus ring colored line marks the place). Without swatches and history it shows Strings::empty.
The label is the palette's tooltip. Status: ready.

### SatValSquare

```cpp
bool SatValSquare(const char* label, ImVec4* color, const SatValSquareOptions& options = {});
```

The saturation and value square of ColorPicker on its own, for the hue of the color: one item and
one Tab stop with the keys and the drag of the picker's square. Hue and alpha are kept. The default
width is `metrics.pickerWidth`, at most the available width; the square keeps its aspect. The label
is its tooltip. Status: ready.

### ColorChannelSlider

```cpp
bool ColorChannelSlider(const char* label, ImVec4* color, const ColorChannelSliderOptions& options = {});
```

A slider whose track is the gradient of one channel (hue, HSV saturation and value, red, green, blue
or alpha) with the other channels held, `picker.colorBarHeight` thick, with a checkerboard under the
alpha gradient. It lays out like a Slider (the label in front, the fader length when vertical, three
control sizes) and takes its keys: the arrows step (Shift finer, Ctrl coarser), Page Up and Down
move a tenth, Home and End go to the ends; the hue wraps. There is no value text and no typed
value. It writes only its channel. Status: ready.

### ColorModeEditor

```cpp
struct AnimatedColor { ColorAnimation animation; ImVec4 color; ImVec4 second; float period; };
ImVec4 SampleAnimatedColor(const AnimatedColor& animated, double seconds);
bool ColorModeEditor(const char* label, AnimatedColor* animated, const ColorModeEditorOptions& options = {});
```

The editor is a SegmentedControl of the animations (Strings::animationStatic and the others), then
a ColorWell for the base color, a second ColorWell for Pulse and Gradient, a DragNumber for the period
in seconds (Strings::period, 0.1 to 60) for every animation but Static, and a live preview swatch;
the second row wraps when the width is too small. Each is a Tab stop. `SampleAnimatedColor` gives the
color at a time: Pulse eases from `color` to `second` and back (a cosine over the period), Gradient
sweeps evenly there and back, both blended in OKLab; Cycle turns the OKLCH hue of `color` once per
period and keeps its lightness and chroma. While facet::ReduceMotion() is true, for Static and for
a period that is not above 0 it returns `color`. The period field shows no unit text yet: a
built-in string for seconds does not exist. Status: ready.
### Usage of the color pickers

A color setting with a recent-colors row shared by two pickers. The history is caller data, so it
can be saved with the other settings.

```cpp
static ImVec4 tint = facet::T().colors.accent;
static facet::ColorHistory recent;
facet::ColorWell("Tint", &tint, {.alpha = true, .showHex = true});
static const ImVec4 kPresets[] = {first, second, third};
facet::SwatchPalette("##presets", &tint, kPresets, &recent);
```

Undo for a picker: the compare swatch shows the color before the current edit, and each finished
drag pushes one undo step.

```cpp
static ImVec4 color = start, before = start;
facet::ColorPicker("Color", &color, {.original = &before});
if (facet::IsItemEditCommitted())
{
    PushUndo(before, color);  // application function
    before = color;
}
```

An overlay element that pulses, evaluated once per frame.

```cpp
static facet::AnimatedColor marker = {.animation = facet::ColorAnimation::Pulse, .color = base, .second = bright};
facet::ColorModeEditor("Marker", &marker);
const ImU32 now = facet::ToU32(facet::SampleAnimatedColor(marker, ImGui::GetTime()));
```

## Date, time and other pickers

### DatePicker

```cpp
struct Date { int year; int month; int day; };
bool DatePicker(const char* label, Date* date, const DatePickerOptions& options = {});
```

A segmented date field with a calendar popover, limits and day, month or year precision.
Status: planned.

### DateRangePicker

```cpp
bool DateRangePicker(const char* label, Date* start, Date* end, const DateRangePickerOptions& options = {});
```

Two months side by side that pick a start and an end date, with presets. Status: planned.

### TimePicker

```cpp
struct TimeOfDay { int hour; int minute; int second; };
bool TimePicker(const char* label, TimeOfDay* time, const TimePickerOptions& options = {});
```

A segmented time field with scrolling hour and minute columns, 12 or 24 hours. Status: planned.

### AnglePicker

```cpp
bool AnglePicker(const char* label, float* radians, const AnglePickerOptions& options = {});
```

A dial with a needle and a degrees field over an angle in radians. Status: planned.

### IconPicker

```cpp
bool IconPicker(const char* label, Icon* icon, const IconPickerOptions& options = {});
bool IconPicker(const char* label, Icon* icon, std::span<const ChoiceItem> icons, const IconPickerOptions& options = {});
```

A field that opens a searchable grid of icons, every registered icon or a given list in groups.
Status: planned.

### ThumbnailPicker

```cpp
bool ThumbnailPicker(const char* label, int* index, std::span<const ThumbnailItem> items,
                     const ThumbnailPickerOptions& options = {});
bool ThumbnailPicker(const char* label, std::span<bool> checked, std::span<const ThumbnailItem> items,
                     const ThumbnailPickerOptions& options = {});
```

A grid of captioned pictures with a selection ring and lock badges, with empty, loading and error
states. Status: planned.

### FileBrowser

```cpp
struct FileType { const char* label; const char* patterns; };
bool FileBrowser(const char* label, std::string* path, const FileBrowserOptions& options = {});
bool FileBrowser(const char* label, char* buffer, std::size_t bufferSize, const FileBrowserOptions& options = {});
```

A file browser drawn inside the app, for overlays and hosts that cannot show a system dialog. Like
the other panels, it does not draw its label beside it but shows it as its tooltip on hover and
keyboard focus. Status: planned.

### Usage of the other pickers

A date range limited to the past, with one preset.

```cpp
static facet::Date from, to;
const facet::Date today = Today();  // application function
const facet::DateRangePreset presets[] = {{"Option 1", today, today}};
facet::DateRangePicker("Period", &from, &to, {.max = today, .presets = presets});
```

Picking a file in an overlay, where a system dialog cannot be shown: the browser goes into a modal
and writes the path on confirm. A file type without patterns lists every file.

```cpp
static const facet::FileType kTypes[] = {{"Images", "*.png;*.jpg"}, {"All files"}};
if (facet::BeginModal("Open image", &browsing))
{
    if (facet::FileBrowser("##files", &imagePath, {.fileTypes = kTypes}))
        browsing = false;
    facet::EndModal();
}
```

## Planned, not declared yet

These widgets of the later tier are not declared yet.

- **NumberWheel**: a drum-style scrolling picker with inertia and a center selection band.
- **DirectionPicker**: a shaded sphere that picks a normalized 3D direction.
- **FontPicker**: a dropdown of loaded font faces previewed in their own face.
- **CascadePicker**: a hierarchical picker with side-by-side level columns.
- **Screen eyedropper**: sampling colors outside the app's own frame needs a capture path that a
  Direct3D 11 overlay does not have; it waits for the blur copy path to be usable for sampling.
