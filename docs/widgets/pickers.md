# Pickers

Header: `facet/widgets/pickers.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Every widget here is
**planned**: the function exists, draws a placeholder of the widget's default size, returns the neutral
result and writes nothing. Phase 2 implements the widgets and changes their status to ready. See
[the widget index](../widgets.md).

Colors are `ImVec4` in gamma-encoded sRGB with straight alpha everywhere (see `core/color.h`): the
edited value, swatches, history and the compare color. Without `alpha` the alpha channel is never
written; without `hdr` channels are clamped to 0 to 1. Fields that open a popover open on a click,
Enter, Space or Alt+Down and close on Escape or a click outside. Every widget here returns true on
the frame it wrote its value.

## Widgets

### ColorWell

```cpp
bool ColorWell(const char* label, ImVec4* color, const ColorWellOptions& options = {});
```

A swatch button that opens a ColorPicker in a popover, with hex copy and paste and drag and drop.
Status: planned.

### ColorPicker

```cpp
bool ColorPicker(const char* label, ImVec4* color, const ColorPickerOptions& options = {});
```

An inline picker: square or wheel, hue and alpha bars, inputs, swatches and a compare swatch.
Status: planned.

### ColorInputs

```cpp
bool ColorInputs(const char* label, ImVec4* color, const ColorInputsOptions& options = {});
```

Channel fields in RGB, HSV or HSL, or one hex field, that also parse pasted color text.
Status: planned.

### SwatchPalette

```cpp
struct ColorHistory { static constexpr int kCapacity = 8; ImVec4 colors[kCapacity]; int count; };
bool SwatchPalette(const char* label, ImVec4* color, std::span<const ImVec4> swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});
bool SwatchPalette(const char* label, ImVec4* color, std::vector<ImVec4>* swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options = {});
```

A grid of swatches with a row of recent colors; the vector form lets the user add, remove and
reorder swatches. Status: planned.

### SatValSquare

```cpp
bool SatValSquare(const char* label, ImVec4* color, const SatValSquareOptions& options = {});
```

The saturation and value square of ColorPicker on its own. Status: planned.

### ColorChannelSlider

```cpp
bool ColorChannelSlider(const char* label, ImVec4* color, const ColorChannelSliderOptions& options = {});
```

A slider over the gradient of one channel (hue, saturation, value, red, green, blue or alpha) of the
color. Status: planned.

### ColorModeEditor

```cpp
struct AnimatedColor { ColorAnimation animation; ImVec4 color; ImVec4 second; float period; };
ImVec4 SampleAnimatedColor(const AnimatedColor& animated, double seconds);
bool ColorModeEditor(const char* label, AnimatedColor* animated, const ColorModeEditorOptions& options = {});
```

Edits a color that is static, pulses, cycles through the hues or sweeps a gradient;
`SampleAnimatedColor` evaluates it at a time. Status: planned (until then `SampleAnimatedColor`
returns the base color).

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
bool ThumbnailPicker(const char* label, std::span<bool> selected, std::span<const ThumbnailItem> items,
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

A file browser drawn inside the app, for overlays and hosts that cannot show a system dialog. Its
label is the accessible name and is not drawn. Status: planned.

## Usage

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
