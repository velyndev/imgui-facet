# Charts

Sparklines, line, bar and donut charts, gauges, level meters, frame time graphs, legends and the
range brush.

Header: `facet/widgets/charts.h`. Every declaration follows [the API conventions](../api-conventions.md),
and the header comments are the full reference. The demo gallery shows every widget on the page
Widgets > Charts.

Each entry ends with its status. A widget whose status is planned is declared, compiles and can be
called; until it is implemented, a call draws a placeholder of the widget's default size and
returns the neutral result.

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
  overlap. `hideTicks` gives the room to the plot, `hideGrid` leaves out the grid lines; a linear
  y axis through 0 draws its zero line stronger. A logarithmic axis leaves values at or below 0
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
  a dot at its sample nearest to that x, and a box beside it names the x and every series value
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

Ring or pie of proportional slices with a center total. Status: planned.

### Gauge

```cpp
void Gauge(const char* label, double value, double min, double max, const GaugeOptions& options = {});
```

Radial gauge with zones and a readout. Status: planned.

### LevelMeter

```cpp
void LevelMeter(const char* label, std::span<const float> channels, double min, double max, const LevelMeterOptions& options = {});
```

Smoothed bar per channel with zones and peak hold. Status: planned.

### FrameTimeGraph

```cpp
void FrameTimeGraph(const char* id, const FrameTimeGraphOptions& options = {});
void FrameTimeGraph(const char* id, std::span<const float> frameTimes, const FrameTimeGraphOptions& options = {});
```

Rolling frame time graph with the frame rate and a target line; the second overload draws the
caller's frame times. Status: planned.

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

Overview with a two-handle window that selects the x range of a larger chart. Status: planned.

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
