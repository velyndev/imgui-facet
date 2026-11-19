# Charts

Sparklines, line, bar and donut charts, gauges, level meters, frame time graphs, legends and the
range brush.

Header: `facet/widgets/charts.h`. Every declaration follows [the API conventions](../api-conventions.md),
and the header comments are the full reference. The demo gallery shows every widget on the page
Widgets > Charts.

**Status: planned.** Each widget below is declared, compiles and can be called; until Phase 2
implements it, a call draws a placeholder of the widget's default size and returns the neutral
result.

## One data form, one axis pattern

- **Series** are `ChartSeries`: `values` (a span of float y values) with an optional `x` span of
  doubles of the same length (double, so time axes in seconds keep their precision), or `count` plus a `ChartGetter` that fills one `ChartPoint` (double x and y)
  at a time and may return false for a gap. The data stays the caller's and is read during the
  call only.
- **Axes** are `ChartAxis`: `min` and `max` (equal values follow the data), `ValueScale`,
  `format` or `formatter`, an axis title and switches for ticks and grid.
- **Colors** are data: `color` on a series, slice or legend entry; 0 picks palette color number
  i for entry i, so a `Legend` next to a chart matches it without repeating colors.
- **Visibility** is the caller's: `Legend` writes `LegendItem::hidden`, and the caller copies it
  into `ChartSeries::hidden`.
- **States**: charts that load data take `state`, `emptyText` and `errorText` like every data
  view.

## Widgets

| Widget | Declaration | What it does | Status |
|---|---|---|---|
| Sparkline | `void Sparkline(const char* label, std::span<const float> values, const SparklineOptions& options = {})`, `void Sparkline(const char* label, int count, ChartGetter getter, void* user, const SparklineOptions& options = {})` | Small trend line or bar row without axes | planned |
| LineChart | `void LineChart(const char* id, std::span<const ChartSeries> series, const LineChartOptions& options = {})` | Lines or areas over a shared x axis with a snapping crosshair | planned |
| BarChart | `void BarChart(const char* id, std::span<const ChartSeries> series, std::span<const char* const> categories, const BarChartOptions& options = {})` | Grouped or stacked bars per category | planned |
| DonutChart | `void DonutChart(const char* id, std::span<const DonutSlice> slices, const DonutChartOptions& options = {})` | Ring or pie of proportional slices with a center total | planned |
| Gauge | `void Gauge(const char* label, double value, double min, double max, const GaugeOptions& options = {})` | Radial gauge with zones and a readout | planned |
| LevelMeter | `void LevelMeter(const char* label, std::span<const float> channels, double min, double max, const LevelMeterOptions& options = {})` | Smoothed bar per channel with zones and peak hold | planned |
| FrameTimeGraph | `void FrameTimeGraph(const char* id, const FrameTimeGraphOptions& options = {})`, `void FrameTimeGraph(const char* id, std::span<const float> frameTimes, const FrameTimeGraphOptions& options = {})` (caller frame times) | Rolling frame time graph with the frame rate and a target line | planned |
| Legend | `bool Legend(const char* id, std::span<LegendItem> items, const LegendOptions& options = {})` | Swatches with labels that toggle series; Alt isolates one | planned |
| ChartRangeBrush | `bool ChartRangeBrush(const char* label, double* low, double* high, double min, double max, std::span<const ChartSeries> overview, const ChartRangeBrushOptions& options = {})` | Overview with a two-handle window that selects the x range of a larger chart | planned |

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
