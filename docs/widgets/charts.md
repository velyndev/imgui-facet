# Charts

Sparklines, line, bar and donut charts, gauges, level meters, frame time graphs, legends and the
range brush.

Header: `facet/widgets/charts.h`. Every declaration follows [the API conventions](../api-conventions.md),
and the header comments are the full reference. The demo gallery shows every widget on the page
Widgets > Charts.

Each entry ends with its status; every widget of the family is ready and works as its header describes.

## One data form, one axis pattern

- **Series** are `ChartSeries`: `values` (a span of float y values) with an optional `x` span of
  doubles of the same length (double, so time axes in seconds keep their precision), or `count` plus a `ChartGetter` that fills one `ChartPoint` (double x and y)
  at a time and may return false for a gap. The data stays the caller's and is read during the
  call only.
- **Axes** are `ChartAxis`: `min` and `max` (equal values follow the data), `ValueScale`,
  `format` or `formatter`, an axis title and switches for ticks and grid.
- **Colors** are data: `color` on a series, slice or legend entry; 0 picks palette color number
  i for entry i (the theme's chart palette, `T().colors.chart`, see `docs/theming.md`), so a
  `Legend` next to a chart matches it without repeating colors.
- **Visibility** is the caller's: `Legend` writes `LegendItem::hidden`, and the caller copies it
  into `ChartSeries::hidden`.
- **States**: charts that load data take `state`, `emptyText` and `errorText` like every data
  view.

## Widgets

### Sparkline

```cpp
void Sparkline(const char* label, std::span<const float> values, const SparklineOptions& options = {});
void Sparkline(const char* label, int count, ChartGetter getter, void* user, const SparklineOptions& options = {});
```

A small trend without axes, for a table cell or the side of a number. It draws a line (`area`
fills under it at low opacity), or one bar per value with `bars` (bars grow from 0, so an
automatic range includes 0; more values than pixel columns draw each column's extremes).
`markMinMax` puts a dot on the smallest and the largest value. The range is `min` to `max` when
they differ, else the range of the values; a range of zero draws a flat line in the middle.

- **Data:** NaN values (and a getter returning false) break the line; a stretch of one value
  between gaps is a dot. Without values it draws a hairline through the middle of its box.
- **Color:** `color`, or the first color of the chart palette (the accent hue at 3:1 on every
  surface); disabled, labelTertiary.
- **Hover:** the nearest value gets a dot and, after the tooltip delay, a tooltip with the visible
  label part and the value ("Trend: 4.25"). It is not a Tab stop and has no keys: the number
  it stands next to carries the meaning.
- **Size:** `width` 0 fills the available width (rule 6), `height` 0 is the control height of the
  inherited size. Everything is clipped to its box, down to a few pixels.

Status: ready.

### LineChart

```cpp
void LineChart(const char* id, std::span<const ChartSeries> series, const LineChartOptions& options = {});
```

Lines of one or more series over a shared x axis, with a y axis title above the plot, tick labels
on both axes and the x axis title under them.

- **Axes:** the x range is the extent of the visible series, the y range the data padded by 5 %
  and extended to whole tick steps (from 0 with `area` or `stacked`), unless `min` and `max` of
  the `ChartAxis` differ. Tick labels follow `formatter`, `format` or the shortest text the step
  needs, and are measured: along x fewer ticks are asked for until the widest label fits between
  its neighbours, and if it still does not, only every n-th tick is labelled, so labels never
  overlap. `hideTicks` gives the room to the plot, `hideGrid` leaves out the grid lines (`separator`
  hairlines); a linear y axis through 0 draws its zero line stronger (`separatorOpaque`), because it
  is the baseline the bars and areas stand on. A logarithmic axis leaves values at or below 0
  out of its automatic range.
- **Series:** `color` or the palette color of the series index; `hidden` series are not drawn and
  leave the range. NaN values and getter gaps break a line; a stretch of one sample is a dot.
  Long series are decimated to a few points per pixel column. `area` fills under each line at
  low opacity; `stacked` adds the series up (they need equal x) and fills the band between one
  stack and the next, so the fills never overlap; gaps count as 0 in the stacks below.
  `markers` puts a marker on every sample while samples are at least a marker apart, in a shape
  per series index (circle, square, diamond, triangle), so series stay apart without color.
- **Readout:** the chart is one item and one Tab stop. With `crosshair`, the pointer over the plot
  picks the nearest x of the first visible series: a hairline marks it, each visible series gets
  a dot at its sample nearest to that x, and a box beside it (the level 1 elevated surface with the
  `separator` hairline of a floating surface) names the x and every series value
  (eight rows, then "+{count}"). Without `crosshair` the pointer picks the nearest point of any
  series and the box names that series alone. With keyboard focus, Left and Right step through
  the samples of the first visible series, Home and End go to the ends; the keys are taken from
  navigation while the chart is focused. The inspected sample is kept under the id.
- **Legend:** `legend` draws a passive legend under the plot (series labels; with `markers`, the
  marker shapes). Use `Legend` beside the chart to let people toggle series.
- **States:** `Loading` draws a skeleton over the chart's box, `Error` the error state with
  `errorText`, and every series empty the empty state with `emptyText`; each keeps the size and
  is not a Tab stop. Disabled: the disabled colors and no readout.
- **Size:** `width` 0 fills the available width, `height` 0 is `metrics.chartHeight`. Everything is
  clipped to the box, down to a few pixels.

Status: ready.

### BarChart

```cpp
void BarChart(const char* id, std::span<const ChartSeries> series, std::span<const char* const> categories, const BarChartOptions& options = {});
```

Bars per category: value i of each visible series belongs to category i, and there are as many
categories as the longest series has values (at least as many as labels). `Vertical` bars grow
up from the value axis baseline with the category labels under them; `Horizontal` bars grow to
the right with the labels in a column before them (at most 40 % of the width).

- **Grouped** (the default): the visible series side by side in each category, `charts.barGap`
  apart, categories `spacing.sm` apart. **Stacked**: one bar per category, positive values up and
  negative values down from 0, each segment a bar gap before the next, so neighbouring colors
  never touch; only the outer end of a stack is rounded.
- **Value axis:** as on LineChart, from 0, padded and on whole steps; NaN values and getter gaps
  draw no bar. Category labels are cut with an ellipsis to their slot, and when slots are too
  narrow only every n-th label shows.
- **Readout:** one item and one Tab stop. The pointer picks the category under it; Left and Right
  (Up and Down too when horizontal), Home and End step through the categories. The picked
  category gets a wash behind its bars and a box with its label and every visible series value.
- **Legend, states, size:** as on LineChart.

Status: ready.

### DonutChart

```cpp
void DonutChart(const char* id, std::span<const DonutSlice> slices, const DonutChartOptions& options = {});
```

A ring (or with `pie` a full disc) of slices proportional to their values, from the top and
clockwise. Negative and NaN values count as 0. The surface shows through a gap of
`charts.barGap` between slices, so neighbouring colors never touch.

- **Small slices:** with `minSliceFraction` > 0, slices below that share of the total merge into
  one last slice named `otherLabel` (or `Strings::more`), drawn in labelSecondary.
- **Center:** the ring's hole (`charts.donutHole` of the diameter) shows `centerText`, or the
  total in `format` or `formatter` (default "%g"), cut to the hole.
- **Readout:** one item and one Tab stop. The pointer over a slice, or Left, Right, Home and End,
  pick a slice: it grows out by `spacing.xs` (over `motion.durationFast`, at once with reduced
  motion) and the center shows its value and label; a pie names it in a box beside it.
- **Legend:** `legend` puts a passive legend beside the ring when the width allows, else under it.
- **States:** a total of 0 shows the empty state; `Loading` and `Error` as on LineChart.
- **Size:** `diameter` 0 is `metrics.chartHeight`; the width is the ring and the legend, at most the
  available width (`SetNextItemWidth` sets it); the ring shrinks to fit.

Status: ready.

### Gauge

```cpp
void Gauge(const char* label, double value, double min, double max, const GaugeOptions& options = {});
```

A track along `metrics.dialSweep` degrees of a circle, open at the bottom (`Arc`), or along its
upper half (`HalfCircle`), `charts.gaugeWidth` wide. `zones` are drawn as a thin band along the
outer edge in their tone; the filled arc runs from `min` to the value in the tone of the zone the
value is in (else the accent), or with `needle` a needle in the label color points at it. The
value moves with the quick spring (at once with reduced motion).

- **Readout:** the value in `format` or `formatter` (default "%.0f") in the middle, the visible
  label part under it in labelSecondary, cut with an ellipsis and shown whole as a tooltip on hover.
- **Out of range:** the fill stops at the end and a dot in the label color marks that end; the
  readout shows the value as it is. `min > max` is swapped; `min == max` draws the readout alone.
- **Not interactive:** one item, not a Tab stop. Disabled: the disabled colors.
- **Size:** `diameter` 0 is `metrics.chartHeight`, at most the available width.

Status: ready.

### LevelMeter

```cpp
void LevelMeter(const char* label, std::span<const float> channels, double min, double max, const LevelMeterOptions& options = {});
```

One bar per channel, `charts.meterSegmentGap` apart. Each bar follows its value with
exponential smoothing (time constant `motion.durationFast`, at once with reduced motion); a value
outside `min` to `max` is drawn clamped, NaN as `min`. The part of a fill inside a zone takes the
zone's tone, so where a warning or clipping band starts shows on the bar itself.

- **Peak hold:** with `peakHold` a mark in the label color stays at the highest recent value for
  `charts.peakHold` seconds (times `motion.scale`), then falls back over four slow durations; with
  reduced motion it jumps back.
- **Segments:** `segments` draws square cells instead of a solid bar; a cell is lit up to the value.
- **Label:** horizontal meters draw the visible label part in front (inside the width, as fields
  do); vertical meters draw it above their columns, where a narrow column has room for it.
- **Not interactive:** one item, not a Tab stop. Smoothing and peaks of the first 32 channels are
  kept under the id in fixed storage; later channels show their value directly. No allocation per
  frame.
- **Size:** horizontal: `width` 0 fills, `height` 0 is the control height shared by the channels;
  vertical: `width` 0 is one control height per channel, `height` 0 is `metrics.chartHeight`.

Status: ready.

### FrameTimeGraph

```cpp
void FrameTimeGraph(const char* id, const FrameTimeGraphOptions& options = {});
void FrameTimeGraph(const char* id, std::span<const float> frameTimes, const FrameTimeGraphOptions& options = {});
```

One bar per frame, newest at the right, as many recent frames as fit at `charts.barGap` pixels
each. The first overload draws the frame times the charts subsystem records in every
`facet::NewFrame` (`io.DeltaTime`, the last 300 frames); the second draws the caller's times, in
seconds, oldest first.

- **Readout:** the frames per second (from the average of the frames shown) in Headline, and the
  minimum, average and maximum frame time in milliseconds. `compact` shows only the frame rate
  beside a small graph.
- **Target:** a line at `target` seconds (0: 1/60); frames over it are drawn in the warning color
  and reach above the line, so they stand out without color too. The graph's top is twice the
  target or the slowest frame shown.
- **Not interactive:** one item, not a Tab stop. It reads the samples in place: no allocation.
- **Text:** the words of the readout are built-in strings (`Strings::framesPerSecond`, `minimum`,
  `average`, `maximum` and `milliseconds`), so `SetStrings` translates them.
- **Size:** `width` 0 fills, `height` 0 is `metrics.chartHeight` (compact: two control heights).

Status: ready.

### Legend

```cpp
bool Legend(const char* id, std::span<LegendItem> items, const LegendOptions& options = {});
```

Color swatches with labels, flowed into rows of the legend width (`Orientation::Horizontal`) or
stacked one per row (`Vertical`). An entry's color is `color`, or the chart palette color of its
index, so a legend next to a chart with automatic colors matches it.

- **Toggle:** with `toggle` (the default) every entry is its own item and Tab stop, with the hover
  fill, the hand cursor and the focus ring after keyboard input. A click, Enter or Space flips the
  entry's `hidden` member; with Alt held, the entry is shown alone (every other entry hidden), and
  when it already is the only one shown, every entry is shown again. The legend returns true on
  the frame a member was written. Without `toggle` the entries are drawn but are not items.
- **Hidden entries** have a hollow swatch and their label in labelSecondary, so the state does not
  depend on color alone.
- **Read-only** (`readOnly` or a pushed `ImGuiItemFlags_ReadOnly`): entries stay Tab stops and show
  their state but never flip. Disabled (`ImGui::BeginDisabled`): the disabled colors, no Tab stops.
- **Ids:** entries get ids under the legend id from `LegendItem::id` when it is not 0, else from the
  index, so focus stays on an entry when the caller reorders entries with ids.
- **Size:** `width` 0 fills the available width; one row is the small control height. A label longer
  than the legend is cut with an ellipsis and shows in full as a tooltip on hover and keyboard focus.

Status: ready.

### ChartRangeBrush

```cpp
bool ChartRangeBrush(const char* label, double* low, double* high, double min, double max, std::span<const ChartSeries> overview, const ChartRangeBrushOptions& options = {});
```

An overview of the visible `overview` series (lines without axes, over `min` to `max` on x) with a
window from `*low` to `*high` over it: a wash outside the window, the accent wash inside, and the
edges as lines in the focus ring color with a grip in the middle (the level 1 surface with the
`controlBorder` hairline, `controlBorderHover` and a stronger fill under the pointer). The window
body keeps its look under the pointer and while it is dragged; only the grips show hover. The
selected range then drives the x axis of a larger chart (see the usage below).

- **Edges** are the two handles of the multi-handle block, as on RangeSlider: each its own item,
  Tab stop and focus ring around its grip. Dragging moves an edge; the window body drags both; a
  press outside the window moves the nearer edge there. The edges never cross and keep `minGap`
  (clamped to the range). With keyboard focus on an edge, the arrows step it by a hundredth of the
  range (Shift a thousandth, Ctrl a tenth), Home and End move it to the ends. Escape during a drag
  or a held key restores both edges. The item queries describe the whole brush; while an edge is
  active they report that edge, so `IsItemDeactivatedAfterEdit` and `facet::IsItemEditCommitted`
  report one edit per drag and per key press.
- **Wheel:** with `wheelZoom`, the wheel over the brush zooms the window around the pointer (one
  notch toward the user to 80 % of its width) while an edge has keyboard focus or Alt is held;
  otherwise the wheel scrolls the page.
- **Read-only** (`readOnly`, a pushed `ImGuiItemFlags_ReadOnly`, a null pointer or `min == max`):
  the window is shown and the edges can be focused but nothing is written. Disabled: the disabled
  colors, no Tab stops.
- **Label:** not drawn beside the brush; it is the tooltip of the overview on hover and on keyboard
  focus of an edge.
- **Returns** true on the frame `*low` or `*high` was written.
- **Size:** `width` 0 fills, `height` 0 is twice the control height.

Status: ready.

## Usage

### A legend that hides series

```cpp
static facet::LegendItem legend[] = {{"Input"}, {"Output"}};
facet::ChartSeries series[] = {
    {.label = "Input", .values = input},
    {.label = "Output", .values = output},
};
facet::Legend("legend", legend);
for (int i = 0; i < 2; ++i)
    series[i].hidden = legend[i].hidden;
facet::LineChart("signal", series, {.yAxis = {.min = -1.0, .max = 1.0}});
```

### Large data through a getter

```cpp
// samples is the application's ring buffer of (time, value) pairs.
const facet::ChartSeries series{
    .label = "Samples",
    .count = samples.Count(),
    .getter = [](void* user, int index, facet::ChartPoint* out) {
        const Sample& s = static_cast<const SampleRing*>(user)->At(index);
        out->x = s.time;
        out->y = s.value;
        return true;
    },
    .user = &samples,
};
facet::LineChart("samples", {&series, 1}, {.xAxis = {.format = "%.1f s"}});
```

### Range brush driving a chart

```cpp
static double low = 0.0, high = 60.0;
facet::ChartRangeBrush("Window", &low, &high, 0.0, duration, series, {.minGap = 1.0});
facet::LineChart("detail", series, {.xAxis = {.min = low, .max = high}});
```

## Planned, not declared yet

- **HeatStrip**: a row or grid of cells colored by value on a ramp derived from the accent, with
  hatched cells for missing values and a hover readout.
