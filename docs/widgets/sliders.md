# Sliders

Header: `facet/widgets/sliders.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Every widget here is
**planned**: the function exists, draws a placeholder of the widget's default size, returns false and
writes nothing. Phase 2 implements the widgets and changes their status to ready. See
[the widget index](../widgets.md).

Every widget here returns true on the frame a value was written. Drags, clicks and keys write at
once; typed values (Ctrl+click or Enter on a handle) write on Enter, Tab or focus loss; Escape
restores the values from activation. `facet::IsItemEditCommitted()` after the call reports one
finished drag, which is the moment to save or to push an undo step.

## The multi-handle model

RangeSlider, RangeInput, RangeSliderHistogram, MultiSlider, ArcRangeSlider, BoxRangePad and
AllocationBar share one model, so a caller who knows RangeSlider can guess the others:

- The positional parameters are the label, the values the widget writes (two pointers, a span or a
  vector), the range bounds, then the item data (histogram bins, segment items).
- Each handle is its own item under the widget id, with its own Tab stop and focus ring. While a
  handle is active, the last item id is that handle's, so `ImGui::IsItemActive()` and the
  deactivation queries work after the call.
- Handles keep their order and `minGap` between neighbours. `collision` decides what happens at a
  neighbour: `Clamp` stops the handle, `Push` moves the neighbour along until it reaches the end of
  the range.
- `dragSpan` lets a drag of the filled part between two handles move both. A click on the empty
  track moves the nearest handle there.
- Keys on a focused handle: arrows step (Shift fine, Ctrl coarse), Page Up and Down move ten steps,
  Home and End go as far as the handle can.
- The option members have the same names and order everywhere: `format`, `formatter`,
  `formatterUser`, `scale`, `step`, `minGap`, `collision`, `dragSpan`, the widget's own members,
  `orientation`, then the common block.

## Widgets

### Slider

```cpp
bool Slider(const char* label, float* value, float min, float max, const SliderOptions& options = {});
bool Slider(const char* label, int* value, int min, int max, const SliderOptions& options = {});
```

A track with a fill and a knob; logarithmic, stepped, center fill, vertical and a value bubble come
from the options. Status: planned.

### SliderStepped

```cpp
bool SliderStepped(const char* label, float* value, std::span<const float> stops,
                   const SliderSteppedOptions& options = {});
```

A slider over non-uniform stops with tick marks and optional stop labels. Status: planned.

### SliderWithInput

```cpp
bool SliderWithInput(const char* label, float* value, float min, float max,
                     const SliderWithInputOptions& options = {});
bool SliderWithInput(const char* label, int* value, int min, int max, const SliderWithInputOptions& options = {});
```

A slider and a number field bound to the same value; the field may accept values outside the range.
Status: planned.

### RangeSlider

```cpp
bool RangeSlider(const char* label, float* low, float* high, float min, float max,
                 const RangeSliderOptions& options = {});
bool RangeSlider(const char* label, int* low, int* high, int min, int max, const RangeSliderOptions& options = {});
```

Two handles selecting a range, with minimum gap, clamp or push, span drag, mirror, fixed span,
logarithmic, stepped and vertical variants. Status: planned.

### RangeInput

```cpp
bool RangeInput(const char* label, float* low, float* high, float min, float max,
                const RangeInputOptions& options = {});
bool RangeInput(const char* label, int* low, int* high, int min, int max, const RangeInputOptions& options = {});
```

A range slider between a low and a high number field; typed values follow the same handle rules.
Status: planned.

### RangeSliderHistogram

```cpp
bool RangeSliderHistogram(const char* label, float* low, float* high, float min, float max,
                          std::span<const float> bins, const RangeSliderHistogramOptions& options = {});
bool RangeSliderHistogram(const char* label, int* low, int* high, int min, int max, std::span<const float> bins,
                          const RangeSliderHistogramOptions& options = {});
```

A range slider under a bar chart of the distribution; bars outside the range are dimmed.
Status: planned.

### MultiSlider

```cpp
bool MultiSlider(const char* label, std::span<float> values, float min, float max,
                 const MultiSliderOptions& options = {});
bool MultiSlider(const char* label, std::span<int> values, int min, int max, const MultiSliderOptions& options = {});
bool MultiSlider(const char* label, std::vector<float>* values, float min, float max,
                 const MultiSliderOptions& options = {});
int MultiSliderAddedHandle();
int MultiSliderRemovedHandle();
```

N ordered handles on one track dividing it into colored segments; the vector form can add and remove
handles. Status: planned.

### Knob

```cpp
bool Knob(const char* label, float* value, float min, float max, const KnobOptions& options = {});
bool Knob(const char* label, int* value, int min, int max, const KnobOptions& options = {});
```

A rotary control with an arc, dot or wiper, bipolar fill and vertical or circular drag.
Status: planned.

### XYPad

```cpp
bool XYPad(const char* label, ImVec2* value, ImVec2 min, ImVec2 max, const XYPadOptions& options = {});
```

A two-dimensional area with one draggable point, a crosshair and optional grid. Status: planned.

### JoystickPad

```cpp
bool JoystickPad(const char* label, ImVec2* value, const JoystickPadOptions& options = {});
```

A round stick returning -1 to 1 per axis, with a deadzone and a spring back to the center.
Status: planned.

### ArcRangeSlider

```cpp
bool ArcRangeSlider(const char* label, float* low, float* high, float min, float max,
                    const ArcRangeSliderOptions& options = {});
```

A range slider bent into an arc, for angular sectors and cyclic bands. Status: planned.

### BoxRangePad

```cpp
bool BoxRangePad(const char* label, ImVec2* low, ImVec2* high, ImVec2 min, ImVec2 max,
                 const BoxRangePadOptions& options = {});
```

A two-dimensional range with four edge and four corner handles and a draggable body.
Status: planned.

### AllocationBar

```cpp
bool AllocationBar(const char* label, std::span<float> shares, std::span<const ChoiceItem> items,
                   const AllocationBarOptions& options = {});
bool AllocationBar(const char* label, std::span<float> shares, std::span<const char* const> items,
                   const AllocationBarOptions& options = {});
```

One bar split into segments whose shares always add up to 1; a divider moves share between its two
neighbours. Status: planned.

## Usage

A price filter: two handles at least 10 apart that push each other, saved once per finished drag.

```cpp
static float low = 20.0f, high = 80.0f;
if (facet::RangeSlider("Price", &low, &high, 0.0f, 100.0f,
                       {.format = "%.0f", .step = 1.0, .minGap = 10.0, .collision = facet::HandleCollision::Push}))
    ApplyFilter(low, high);
if (facet::IsItemEditCommitted())
    SaveSettings();
```

The same range with typed fields, and over a histogram of the data the caller counted. The calls take
the same arguments; only the item data differs.

```cpp
facet::RangeInput("Price##fields", &low, &high, 0.0f, 100.0f, {.minGap = 10.0});
facet::RangeSliderHistogram("Price##bars", &low, &high, 0.0f, 100.0f, binCounts, {.minGap = 10.0});
```

A gradient with stops the user can add and remove. The color of each stop lives in a second vector
that the caller keeps in step through the two queries, read right after the call.

```cpp
static std::vector<float> stops = {0.0f, 0.5f, 1.0f};
static std::vector<ImVec4> colors = {first, middle, last};  // one per stop
facet::MultiSlider("Stops", &stops, 0.0f, 1.0f, {.allowAdd = true, .allowRemove = true, .minHandles = 2});
if (const int added = facet::MultiSliderAddedHandle(); added >= 0)
    colors.insert(colors.begin() + added, colors[added > 0 ? added - 1 : 0]);
if (const int removed = facet::MultiSliderRemovedHandle(); removed >= 0)
    colors.erase(colors.begin() + removed);
```

Fixed split points need no vector: a span over an array keeps the count.

```cpp
static float splits[2] = {0.33f, 0.66f};
facet::MultiSlider("Splits", std::span<float>(splits), 0.0f, 1.0f, {.minGap = 0.05});
```

A time-of-day band that may wrap past midnight, and a crop rectangle in normalized coordinates.

```cpp
static float from = 22.0f, to = 6.0f;  // 22:00 to 06:00
facet::ArcRangeSlider("Quiet hours", &from, &to, 0.0f, 24.0f, {.step = 0.25, .wrap = true, .sweep = 360.0f});

static ImVec2 cropLow = {0.1f, 0.1f}, cropHigh = {0.9f, 0.9f};
facet::BoxRangePad("Crop", &cropLow, &cropHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                   {.minGap = {0.05f, 0.05f}, .yDown = true});
```

Shares of a budget across three parts, each at least 10 percent.

```cpp
static float shares[3] = {0.5f, 0.3f, 0.2f};
static const char* const kParts[] = {"Option 1", "Option 2", "Option 3"};
facet::AllocationBar("Split", shares, kParts, {.step = 0.01, .minGap = 0.1});
```

## Planned, not declared yet

These widgets of the later tier are not declared yet. They will reuse the multi-handle model.

- **ThresholdSlider**: a slider that edits a threshold over an animated live value and highlights
  the part above it.
- **EndlessDial**: an unbounded rotary encoder for jog and fine adjustment.
- **MultiRangeSlider**: several separate ranges on one track, added by a click on empty track and
  merged when they touch.
