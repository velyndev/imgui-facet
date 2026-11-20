# Sliders

Header: `facet/widgets/sliders.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Each entry below ends with
the widget's status. A **planned** widget exists as a function that draws a placeholder of the
widget's default size, returns the neutral result and writes nothing; a **ready** one works as its
header describes. See [the widget index](../widgets.md).

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
  `orientation`, then the common block. Every widget of the model has `readOnly`, `mixed`, `error`
  and `helpText`; `mixed` hides the handles until the first edit writes every value.
- The two-handle linear widgets (RangeSlider, RangeInput, RangeSliderHistogram) also share
  `allowOutOfRange` (a typed value outside the range is written and drawn clamped) and
  `doubleClickReset` (a double click moves a handle to its end of the range, which clears that
  bound of a filter).

## Widgets

### Slider

```cpp
bool Slider(const char* label, float* value, float min, float max, const SliderOptions& options = {});
bool Slider(const char* label, int* value, int min, int max, const SliderOptions& options = {});
```

A track with an accent fill and a round knob, the visible label in front and the formatted value at
the end, inside the widget width (the available width by default). The value column is as wide as
the widest of the two range ends and the values shown so far, so it does not jitter; it gives way
when the track would get shorter than the `layout.minRowControl` token, and the label is cut at
half the width with its full text on hover and keyboard focus. A track shorter than its knob gets
a smaller knob, so nothing leaves the row. Ticks closer than three tick sizes are thinned. The knob follows the control size (Small, Regular, Large) and sits on whole
pixels.

- **Input:** a press on the knob grabs it without moving the value; a press elsewhere on the track
  moves the value under the pointer first. Shift drags ten times finer, Ctrl snaps to a tenth of
  the range (or ten steps). A drag that leaves the window keeps going and returns exactly to the
  value it passed on the way out. Keys on the focused slider: Left and Right (Down and Up when
  vertical) step a hundredth of the range or one step, Shift a tenth of that, Ctrl ten times;
  Page Up and Down a tenth of the range; Home and End the ends. The wheel steps while the slider
  is focused or Alt is held. Escape during a drag or a held key restores the value from the start
  of the edit.
- **Typed values:** Ctrl+click or Enter replaces the track with a number field (the number core)
  holding the value; Enter, Tab or a click elsewhere writes the typed number, clamped to the range
  and snapped to the step, and Escape leaves the value as it was. Enter and Escape return the focus
  to the slider. A vertical slider shows the field centered over the fader, wider than the fader.
- **Options:** `scale` Logarithmic needs `min > 0` and falls back to Linear otherwise; `step`
  snaps to `min + n * step` and draws a tick per step when there are at most 21; the int overload
  writes whole numbers only; `fillFromCenter` fills from the middle of the track; `valueBubble`
  shows the value above the knob (beside a vertical one) while it is held or has keyboard focus;
  `orientation` Vertical draws a fader one control height wide and `height` units high
  (faderLength by default) without the label, which becomes its tooltip, and without the value
  column.
- **States:** disabled (`ImGui::BeginDisabled`) draws the disabled tokens, is not a Tab stop and
  writes nothing; `readOnly` (or a pushed `ImGuiItemFlags_ReadOnly`) draws the fill in
  labelSecondary, keeps the focus and writes nothing; `mixed` hides the knob and the fill until
  the first edit; `error` draws the unfilled track in the destructive color and the message under
  the control, replacing `helpText`. `min == max` and a null value pointer show the slider
  read-only.
- **Item queries:** the slider is one item under the label's id; it is active during a drag and
  during a key press with its repeats, so `facet::IsItemEditCommitted()` reports each finished
  drag and key press once.

Status: ready.

### SliderStepped

```cpp
bool SliderStepped(const char* label, float* value, std::span<const float> stops,
                   const SliderSteppedOptions& options = {});
```

A slider over a list of ascending stop values (at most 64 take part), drawn like Slider: label in
front, value at the end, a tick per stop. The stops are evenly spaced on the track whatever their
values, so a list such as 0.25, 0.5, 1, 2, 4, 8 reads as six equal choices; a value between two
stops is drawn at the same share of the way between their ticks.

- Without `magnetic` every drag, click, key and typed value lands on a stop: the arrows go to the
  next stop, Page Up and Down a tenth of the stops (at least one), Home and End the first and last,
  and a typed value goes to the nearest stop. With `magnetic` a drag moves freely and snaps only
  within spacing.sm of a stop, a typed value is written clamped to the first and last stop, and
  the arrow keys still go to the next stop.
- `stopLabels` draws one Caption label per stop under its tick, which adds a line to the widget's
  height; a label that would touch the one before it is left out, and the last label is always
  kept. Vertical sliders draw no stop labels.
- States, typed values, the vertical form and the item queries are as Slider. Fewer than two
  stops show the value read-only.

Status: ready.

### SliderWithInput

```cpp
bool SliderWithInput(const char* label, float* value, float min, float max,
                     const SliderWithInputOptions& options = {});
bool SliderWithInput(const char* label, int* value, int min, int max, const SliderWithInputOptions& options = {});
```

A Slider without its value column, then a number field (the number core) `fieldWidth` units wide
(numberFieldWidth by default), both on the same value; the label is drawn in front of the track.
When the track beside the field would get shorter than `layout.minRowControl`, the field drops
under the track, aligned with it. Two Tab stops: the slider, then the field.

- The slider writes while dragging, with the keys and typed values of Slider. The field writes a
  typed number on Enter, Tab or focus loss (Dear ImGui's default for scalars); Up and Down in the
  field step like the slider's arrow keys (one step, or a hundredth of the range), Shift finer and
  Ctrl or Page Up and Down ten times; Escape restores the value from the start of the edit.
- The field clamps to the range, unless `allowOutOfRange`: then it writes any number, and the
  slider shows it at the end of the track and leaves it alone until it is dragged.
- `error` draws the track's error look and the message under the whole widget; `mixed` hides the
  knob and empties the field until the first edit; `readOnly` and disabled apply to both parts.

Status: ready.

### RangeSlider

```cpp
bool RangeSlider(const char* label, float* low, float* high, float min, float max,
                 const RangeSliderOptions& options = {});
bool RangeSlider(const char* label, int* low, int* high, int min, int max, const RangeSliderOptions& options = {});
```

Two handles selecting `[*low, *high]` on the shared slider track, with the label in front and both
values at the end (low, a short bar, high; each in a column as wide as the widest value, so the
row does not jitter). Handle 0 is `*low`, handle 1 `*high`; each is its own item under the widget
id with its own Tab stop and round focus ring.

- **Pointer:** a press on a knob grabs it without moving it; a press on the filled span grabs
  the span (with `dragSpan`), which moves both handles and keeps their distance in value units; a
  press on the empty track moves the nearer handle there. Handles at one value separate in the
  direction of the first movement. A drag that leaves the window keeps going and returns exactly
  to the values it passed on the way out. The span thickens while a press would grab it.
- **Keys** on a focused handle: as Slider; Home and End go as far as the handle can (the other
  handle and the gap stop it, unless it pushes). Escape during a drag or a key press restores both
  values from where the edit began.
- **Rules:** handles never cross; `minGap` holds under every input and is clamped to the range
  (a gap as large as the range pins the handles to the ends, where nothing can move them);
  `collision` Push lets a handle move the other along until that one reaches the end of the range;
  `mirror` moves the other handle the same distance the other way around the middle of the range
  (values that are not symmetric are drawn symmetric, kept from the high handle, and written on
  the first edit); `fixedSpan` moves both handles with every drag and key and stops them as a
  whole at the ends. Steps, the logarithmic scale (with `min > 0`, else linear), the int overload
  and vertical all combine with these.
- **Typed values:** Ctrl+click or Enter on a handle replaces the track with a number field for
  that handle. The typed value obeys the same rules (order, gap, collision) and is clamped to the
  range and snapped to the step, unless `allowOutOfRange`, which writes it as typed while the
  track draws it clamped; an out-of-range value stays until its own handle is edited.
- **`doubleClickReset`:** a double click on a handle moves it to its end of the range (low to
  `min`, high to `max`), as far as the rules allow.
- **States:** as Slider. `mixed` hides both knobs, the fill and the values until the first edit
  writes both values; the handles stay Tab stops and show their focus ring. Values passed in out
  of range, out of order or closer than the gap are drawn repaired and written only for the
  handles an edit moves.
- **Item queries:** while a handle (or the span) is active or was just released, the last item
  id is that part's, so `ImGui::IsItemActive()` and `facet::IsItemEditCommitted()` report the
  drag; otherwise it is the widget's.

Status: ready.

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
bool MultiSlider(const char* label, std::span<float> storage, int* count, float min, float max,
                 const MultiSliderOptions& options = {});
bool MultiSlider(const char* label, std::vector<float>* values, float min, float max,
                 const MultiSliderOptions& options = {});
int MultiSliderAddedHandle();
int MultiSliderRemovedHandle();
```

N ordered handles on one track dividing it into colored segments; the storage and count form and the
vector form can add and remove handles, while the fixed span forms report `allowAdd` and
`allowRemove` as a usage error. Status: planned.

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
neighbours. The shares are 0 to 1; `format` and `formatter` both get them in percent.
Status: planned.

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
