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

Lines or areas over a shared x axis with a snapping crosshair. Status: planned.

### BarChart

```cpp
void BarChart(const char* id, std::span<const ChartSeries> series, std::span<const char* const> categories, const BarChartOptions& options = {});
```

Grouped or stacked bars per category. Status: planned.

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

Swatches with labels that toggle series; Alt isolates one. Status: planned.

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
