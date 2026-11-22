# Sliders

Header: `facet/widgets/sliders.h`. The declarations follow [the API conventions](../api-conventions.md);
the comment above each declaration in the header is the full reference. Each entry below ends with
the widget's status; every widget of the family is ready and works as its header describes. See [the widget index](../widgets.md).

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

The two-handle track of RangeSlider between a low and a high number field (`fieldWidth` units,
numberFieldWidth by default), with the label in front. With `slider` false the two fields stand
alone with a short bar between them. When the track beside the fields would get shorter than
`layout.minRowControl`, the fields drop under it: the low field under its start, the high field
under its end. Four Tab stops: low field, low handle, high handle, high field.

- A typed value writes on Enter, Tab or focus loss and obeys the rules of a dragged handle: it
  keeps the order and `minGap`, and `collision` decides whether it stops at the other value or
  pushes it. It is clamped to the range and snapped to the step, unless `allowOutOfRange`, which
  writes it as typed. Up and Down in a field step like the arrows on a handle.
- The handles, the span, `dragSpan`, `doubleClickReset` and the item queries are as RangeSlider.
  `mixed` empties both fields and hides the handles until the first edit; `error` draws the track's
  error look and the message under the whole widget.

Status: ready.

### RangeSliderHistogram

```cpp
bool RangeSliderHistogram(const char* label, float* low, float* high, float min, float max,
                          std::span<const float> bins, const RangeSliderHistogramOptions& options = {});
bool RangeSliderHistogram(const char* label, int* low, int* high, int min, int max, std::span<const float> bins,
                          const RangeSliderHistogramOptions& options = {});
```

The two-handle track of RangeSlider (label in front, both values at the end) under a bar area
`histogramHeight` units high (histogramHeight by default). The bins divide `[min, max]` evenly;
each bar stands where its values lie on the track (narrower toward `max` on a logarithmic track),
with a hairline gap between bars, its height relative to the largest bin, and its top corners
rounded. The parts of the bars between the two handles are drawn in the accent (labelSecondary
read-only, labelTertiary disabled), the rest in fill[0]; while `mixed` every bar is drawn in the
accent. Hovering a bar shows its interval in interval notation (`[a, b)`, the last one closed)
and its weight in a tooltip. Negative, NaN and infinite bins count as 0; no bins, or bins that
are all 0, keep the bar area empty. Only the track row takes the mouse. Handles, keys, typed
values, `allowOutOfRange`, `doubleClickReset`, states and item queries are as RangeSlider.

Status: ready.

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

Up to 16 ordered handles on the shared slider track, with the label in front and no value column;
each handle shows its value in a bubble while it is dragged or has keyboard focus. Handle `i` is
`values[i]`, each its own Tab stop, with the keys, `minGap`, `collision`, the repair of values
passed in and the item queries of RangeSlider. `dragSpan` (off by default) lets a drag between the
first and the last handle move all of them.

- **Segments:** without `segmentColors` one accent fill runs from the first handle to the last
  (from the start of the track with one handle). With `segmentColors`, the n handles make n + 1
  segments, segment k in entry `k % size`, and a 0 entry takes the chart palette color of index k.
- **Growable overloads** (storage and count, vector): with `allowAdd` a double click on the empty
  track (not on a handle or the span) inserts a handle there, where it keeps the order and the gap
  (the first click of the double click, which moved the nearest handle, is undone); with
  `allowRemove` Delete or Backspace on a focused handle removes it, and so does a handle dragged
  more than two control heights away from the track and released there (it takes the disabled look
  while it would go). The focus moves to the handle that takes the removed one's place. Removing
  stops at `minHandles`, adding at `maxHandles` and at the storage capacity. Handles keep their
  identity (item id, focus, animations) through inserts and removals. The vector overload allocates
  only when a handle is added. `MultiSliderAddedHandle` and `MultiSliderRemovedHandle` report the
  index of the last call's insert or removal this frame, -1 otherwise.
- **Fixed overloads** keep the count; `allowAdd` or `allowRemove` there is reported as a usage
  error and ignored.

Status: ready.

### Knob

```cpp
bool Knob(const char* label, float* value, float min, float max, const KnobOptions& options = {});
bool Knob(const char* label, int* value, int min, int max, const KnobOptions& options = {});
```

A dial `diameter` units across (two control heights of `size` by default, at most the available
width) with a track arc along the sweep (`sweep` degrees, dialSweep by default, open at the bottom,
clockwise over the top), a face in the background color with a 3:1 outline, the formatted value
inside the face (under the dial when the dial is smaller than one and a half control heights) and
the label under it in Footnote, cut with its full text on hover and keyboard focus.

- **Variants:** Arc fills the arc from the start (from the middle with `fillFromCenter`) to the
  value in the accent; Dot puts a dot on the face; Wiper draws a line from the centre.
- **Input:** every press grabs without moving the value; dragging up (and back down) moves it,
  one fader length of travel for the whole range, Shift ten times finer, and a drag that leaves the
  window comes back exactly. With `circularDrag` the value follows the pointer angle along the
  sweep (inside the gap at the bottom it goes to the nearer end) and Escape restores it. The
  arrows step while focused, Up and Right toward the maximum, Down and Left toward the minimum
  (a dial has no direction on screen, so all four turn it; Tab moves the focus), Page Up and
  Down a tenth, Home and End the ends; the wheel steps while focused or with Alt. Ctrl+click or
  Enter types a value in a number field over the dial. A double click restores the value from the
  start of its first click.
- **States:** as Slider: disabled, `readOnly`, `mixed` (no value mark and no text until the first
  edit), `error` (the track arc in the destructive color and the message under the knob).

Status: ready.

### XYPad

```cpp
bool XYPad(const char* label, ImVec2* value, ImVec2 min, ImVec2 max, const XYPadOptions& options = {});
```

A two-dimensional area with one draggable point: a field frame (the frame of a text field, with its
hover, read-only, disabled and error looks) `width` by `height` units, the padSize token square by
default and at most the available width (`kFill` fills), with the slider knob as the point. The
knob centre travels in the frame inset by half a knob, so it never leaves the frame; a pad smaller
than two knobs gets a smaller knob. The visible label is not drawn: it is the tooltip on hover and
keyboard focus. One item and one Tab stop under the label's id.

- **Pointer:** a press on the point grabs it without moving it, a press elsewhere moves it there
  first. A drag moves both axes; Shift keeps the axis the drag goes along when it is pressed (the
  one with the larger travel since the press) until it is let go, and Ctrl snaps both to tenths of
  the range (or ten steps). A drag out of the window and back returns
  exactly to the values it passed. There is no fine drag, because Shift is the axis lock.
- **Keys** while focused: Left and Right step x, Up and Down move the point up and down on screen
  (a hundredth of the range or one step, Shift a tenth of that, Ctrl ten times); Page Up and Down
  move it up and down by a tenth of the y range; Home and End send x to `min.x` and `max.x`.
  Escape during a drag or a key press restores both values from the start of the edit. The wheel
  scrolls the window, also over the pad.
- **Options:** `step` snaps each axis to `min + n * step` (0 continuous); `grid` draws separator
  hairlines at every step when there are at most 32 per axis, and at the quarters otherwise;
  `crosshair` (on by default) draws labelTertiary guide lines through the point and the two
  values in Footnote at the edges of the frame, moved to the other side when the point comes
  near, and left out when the pad is too small for both; `format`, `formatter` and
  `formatterUser` format both readouts; `yDown` puts `min.y` at the top.
- **States:** disabled draws the disabled tokens, is not a Tab stop and writes nothing; `readOnly`
  (or a pushed `ImGuiItemFlags_ReadOnly`) keeps the focus and writes nothing; `mixed` hides the
  point, the guides and the readouts until the first edit (the focus ring goes around the frame);
  `error` draws the destructive frame and the message under the pad, replacing `helpText`. An axis
  with `min == max` does not move; with both, and with a null value pointer, the pad is read-only.
  A value outside the range is drawn at the edge and written only by an edit, which writes the
  axes it moved.
- **Not done yet:** typed values (the Ctrl+click and Enter of the other sliders) and the wheel.

Status: ready.

### JoystickPad

```cpp
bool JoystickPad(const char* label, ImVec2* value, const JoystickPadOptions& options = {});
```

A pad with a stick: `*value` is its deflection, -1 to 1 per axis, x to the right and y up. The pad
is `diameter` units across (the padSize token by default, at most the available width), round with
the fill and 3:1 boundary of a field (with `circular` off, the square field frame of XYPad). The
stick is the slider knob, with a line in the accent from the centre to it, so the vector reads at a
glance; a short separator cross marks the centre. The stick centre travels half a knob inside the
pad. The label is the tooltip, as for XYPad; one item and one Tab stop.

- **Pointer:** a press on the stick grabs it, a press elsewhere moves it there first; the drag
  follows the pointer, Shift keeps one axis, Ctrl snaps to tenths. With `circular` the stick stays
  inside the unit circle, moved onto its edge in the pointer's direction; otherwise inside the square.
- **springReturn** (on by default): the end of every drag or key session writes `{0, 0}` (the call
  returns true on that frame), and the drawn stick springs back to the centre with the
  motion.standard spring of the animation store: the same path at any frame rate, and at once with
  reduced motion. The arrows hold the stick at the edge in their direction while they are down (two
  arrows: the diagonal, on the circle or in the corner; Shift: half way), and letting go returns it.
  Escape during a drag restores the value from its start. At rest the reported vector is zero.
- **Without springReturn** the stick stays where it is left, and the keys step it as XYPad's do (a
  hundredth of the range, Shift finer, Ctrl coarser, Page Up and Down y by a tenth, Home and End x
  to -1 and 1); a value the caller writes moves the stick there.
- **deadzone** (0 to 1 of the radius): inside it the value is 0; outside it the distance from the
  deadzone's edge is scaled up to the full radius, so the value grows from 0 without a jump and
  still reaches 1 at the edge. `showDeadzone` draws its ring in labelTertiary.
- **States:** disabled and `readOnly` as XYPad (the line in labelSecondary read-only); `error`
  draws the destructive outline and the message under the pad. A null value pointer shows the pad
  read-only.

Status: ready.

### ArcRangeSlider

```cpp
bool ArcRangeSlider(const char* label, float* low, float* high, float min, float max,
                    const ArcRangeSliderOptions& options = {});
```

The two-handle range of RangeSlider on the dial of a Knob: a dial `diameter` units across (four
control heights of `size` by default, at most the available width), the track arc along the sweep
(`sweep` degrees, dialSweep by default, open at the bottom; 360 is a closed circle starting at the
bottom), the band between the handles in the accent, both values inside the dial (low, a short bar,
high, as RangeSlider's value column; under the dial when they do not fit in it) and the label under
it in Footnote, cut with its full text on hover and keyboard focus. The handles are slider knobs
on the arc, smaller on a dial too small for them; the arc runs half a knob inside the dial, so
nothing leaves it. Handle 0 is `*low`, handle 1 `*high`, each its own item and Tab stop.

- **Pointer:** a press on a handle grabs it without moving it, a press on the band grabs the band
  (with `dragSpan`), a press elsewhere on the ring moves the nearer handle there first; a press in
  the middle of the dial does nothing. Drags follow the pointer angle, with the grab offset kept;
  on an open arc a pointer in the gap at the bottom goes to the nearer end. The band turns both
  handles by the same amount (whole steps with `step`). The band thickens while a press would grab
  it.
- **Keys** on a focused handle: all four arrows step (Up and Right up, Down and Left down, as on a
  knob), Shift finer, Ctrl coarser, Page Up and Down a tenth, Home and End as far as the rules let
  the handle go; Escape during a drag or a key press restores both values. Ctrl+click or Enter
  types the value of a handle in a number field over the dial.
- **Rules:** without `wrap` as RangeSlider: order, `minGap`, `collision`, the band stops as a whole
  at the ends. `wrap` needs `sweep` 360 and is ignored otherwise: the range is then a circle, the
  band runs clockwise from `*low` to `*high` (through `max` to `min` when `*low` is the larger),
  keys and drags pass the end of the range, and the handles never pass each other: both arcs
  between them keep `minGap`, and Push turns the other handle along instead of stopping. Home and
  End go to `min` and `max`, which are the same point there. `scale` Logarithmic needs `min > 0`
  and no `wrap`.
- **States:** as RangeSlider; `mixed` hides the band, the handles and the values until the first
  edit (the handles stay Tab stops with their focus ring); `error` draws the track arc in the
  destructive color and the message under the widget. Values passed in out of range, or closer
  than the gap, are drawn repaired and written only for the handles an edit moves.
- **Not done yet:** an int overload (the header declares the float one only), and tick marks
  for the steps.

Status: ready.

### BoxRangePad

```cpp
bool BoxRangePad(const char* label, ImVec2* low, ImVec2* high, ImVec2 min, ImVec2 max,
                 const BoxRangePadOptions& options = {});
```

A two-dimensional range: the box from `*low` to `*high` on the pad of XYPad (the field frame,
`width` by `height` units, square at the padSize token by default and at most the available width),
drawn as a wash of accentSubtle with an accent outline, like the filled span of a range slider.
Eight handles, each its own item and Tab stop under the widget id, in this order: the left, top,
right and bottom edges (parts 0 to 3), then the corners from the top left clockwise (parts 4 to 7).
The corners are slider knobs; each edge has a knob at its middle, drawn when the edge is at least
three knobs long (a focused edge always). The label is the tooltip, as for XYPad.

- **Rules per axis:** x is one pair of handles of the multi-handle model and y another: the low
  value never passes the high one, `minGap` holds per axis under every input, and `collision`
  Push lets a handle move the other side along until it reaches the end of the range.
- **Pointer:** a press within half a knob (at least half the minimum hit target) of a corner takes
  it, then one near an edge; a press inside the box takes the body (with `dragSpan`), which moves
  the whole box and keeps its size; a press anywhere else moves the nearest handle there first.
  An edge moves one axis, a corner both; Shift during a corner drag keeps one axis, Ctrl snaps to
  tenths. A drag out of the window and back returns exactly.
- **Keys** on a focused handle: an edge moves along its axis (Left and Right for the left and right
  edges, Up and Down for the top and bottom ones, as on screen), with the arrows of the other axis
  left to keyboard navigation; a corner takes all four. Steps as XYPad (Shift finer, Ctrl coarser);
  Page Up and Down move a handle that moves y by a tenth; Home and End send the handle's x (or y,
  for the top and bottom edges) as far as the rules let it. Escape during a drag or a key press
  restores the whole box from where the edit began.
- **States:** as XYPad. `mixed` hides the box and the handles until the first edit writes (the
  handles stay Tab stops and show their focus ring). Values out of range, out of order or closer
  than the gap are drawn repaired and written only for the values an edit moves. An axis with
  `min == max` does not move; with both, and with a null pointer, the pad is read-only.
- **Not done yet:** typed values (the Ctrl+click and Enter of the other sliders).

Status: ready.

### AllocationBar

```cpp
bool AllocationBar(const char* label, std::span<float> shares, std::span<const ChoiceItem> items,
                   const AllocationBarOptions& options = {});
bool AllocationBar(const char* label, std::span<float> shares, std::span<const char* const> items,
                   const AllocationBarOptions& options = {});
```

One bar split into segments whose shares always add up to 1: the slider track (label in front,
the available width by default) with a stretch per segment in its data color, and a slider knob on
each divider. Under the bar, a Caption line shows each segment's label and its share in percent,
centred on the segment and cut to its width; when both do not fit the share is kept and the label
left out, and the whole text shows on hover. At most 17 segments take part; of a `ChoiceItem` the
label is used (icon, detail, group and disabled are not).

- **Rules:** the dividers are the handles of the multi-handle model, each its own Tab stop, so
  dragging divider `i` moves share only between segments `i` and `i + 1`, and the total never
  changes. Every segment keeps at least `minGap` (clamped to an equal split, and rounded up to the
  step), the first and the last included; `collision` Push lets a divider move the ones beyond it
  until their segments are at the minimum. `step` snaps the dividers to multiples of the step, so
  the shares are too.
- **Pointer:** a press on a divider grabs it, a press elsewhere on the track moves the nearest
  divider there. **Keys** on a focused divider: as Slider (a hundredth, or one step; Shift finer,
  Ctrl coarser; Page Up and Down a tenth; Home and End as far as the rules let it); Escape restores
  every divider from where the edit began.
- **Typed shares:** Enter or Ctrl+click on a divider types the share of the segment before it, and
  a click on a segment's caption types that segment's share, in percent in a number field over the
  bar. The divider after the segment moves (the one before it, for the last segment), within the
  same rules. Enter or Escape returns the focus to the divider.
- **Colors:** `segmentColors` repeats over the segments; an empty list or a 0 entry takes the
  chart palette color of the segment index, as charts do. Disabled draws every segment in fill[3].
- **Values:** negative and NaN shares count as 0, and shares that do not add up to 1 are drawn
  normalized (all 0: equal) and written only by an edit, which writes every share, the last one
  closing the total at 1.
- **States:** disabled and `readOnly` as Slider (the segments keep their data colors when
  read-only); `mixed` draws equal segments without dividers and without percentages until the
  first edit writes every share (the dividers stay Tab stops with their focus ring); `error` adds
  the message under the bar. Fewer than two segments show the bar read-only.

Status: ready.

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
