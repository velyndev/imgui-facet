#pragma once

// Charts: sparklines, line, bar and donut charts, gauges, level meters, frame time graphs,
// legends and the range brush.
//
// One data form for every chart. A series is a ChartSeries: a span of float values (y), an
// optional span of double x values of the same length, or, for large or generated data, a
// count plus a ChartGetter that fills one point at a time. The data stays the caller's: a
// chart reads it during the call and keeps nothing but its hover and animation state. One
// option pattern for axes: ChartAxis (range, scale, format, grid) on every chart with axes.
// Series colors are data and may be set per series; 0 picks color number i of the chart
// palette for series i, so a Legend with color 0 matches the chart it sits next to.
// Legend and ChartRangeBrush write values and have `readOnly` (rule 7): they show the value
// and can be focused, but write nothing and return false.
//
// The declarations follow docs/api-conventions.md; widgets/common.h holds the types shared
// with other families. Every widget here is declared and planned: its body draws a
// placeholder until Phase 2 implements it (docs/widgets/charts.md lists the status of each).
// Lengths are design units. Every function is for the thread that runs the Dear ImGui frame.

#include <span>

#include <imgui.h>

#include "facet/widgets/common.h"

namespace facet
{

// One data point of a getter. x and y are in the caller's units.
struct ChartPoint
{
    double x = 0.0;
    double y = 0.0;
};

// Fills `out` for point `index` and returns true, or returns false for a gap (missing data:
// the line breaks there). Called during the chart call only, possibly for a subset of points.
using ChartGetter = bool (*)(void* user, int index, ChartPoint* out);

// One series. Set `values` (and optionally `x`), or `count` with `getter`; `values` wins when
// both are set.
struct ChartSeries
{
    const char* label = nullptr;           // legend and hover text; "##" hides it
    std::span<const float> values = {};    // y values
    std::span<const double> x = {};        // x values, same length as values; empty: x is the index.
                                           // double, so time axes in seconds keep their precision
    int count = 0;                         // points of the getter
    ChartGetter getter = nullptr;
    void* user = nullptr;
    ImU32 color = 0;                       // series color (data); 0: palette color of the series index
    bool hidden = false;                   // not drawn and left out of the auto range (see Legend)
};

// One axis of a chart. With min == max the range follows the data of the visible series.
struct ChartAxis
{
    double min = 0.0;                      // value units
    double max = 0.0;                      // value units; equal to min: automatic
    ValueScale scale = ValueScale::Linear; // Logarithmic needs min > 0 (automatic ranges start at the smallest positive value)
    const char* format = nullptr;          // printf for tick labels; null: "%g"
    FormatFn formatter = nullptr;          // used instead of format when set
    void* formatterUser = nullptr;
    const char* label = nullptr;           // axis title; null for none
    bool hideTicks = false;                // no tick labels (the room is given to the plot)
    bool hideGrid = false;                 // no grid lines along this axis
};

// --- Small charts ---------------------------------------------------------------------------

struct SparklineOptions
{
    bool bars = false;                     // one bar per value instead of a line
    bool area = false;                     // line only: fills under the line with the color at low opacity
    bool markMinMax = false;               // dots on the smallest and largest value
    double min = 0.0;                      // value units; min == max: the range of the values
    double max = 0.0;
    ImU32 color = 0;                       // 0: the accent
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 the control height
};

// A small trend line or bar row without axes, for a table cell or next to a number. The
// visible label part is not drawn: it is the hover tooltip title ("label: value") and the
// accessible name. Not a Tab stop. Hover state only.
void Sparkline(const char* label, std::span<const float> values, const SparklineOptions& options = {});
void Sparkline(const char* label, int count, ChartGetter getter, void* user, const SparklineOptions& options = {});

// --- Charts with axes -----------------------------------------------------------------------

struct LineChartOptions
{
    ChartAxis xAxis;
    ChartAxis yAxis;
    bool area = false;                     // fill under each line
    bool stacked = false;                  // series add up (needs equal x for all series)
    bool markers = false;                  // a dot at every point
    bool crosshair = true;                 // hover snaps to the nearest x and shows every series there
    bool legend = false;                   // a passive legend under the plot; use Legend to toggle series
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;       // shown when every series is empty; null uses Strings::empty
    const char* errorText = nullptr;       // null uses Strings::loadFailed
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 for the chart height default
};

// Lines (or areas) of one or more series over a shared x axis. The plot is one Tab stop: while
// focused, Left and Right move the readout from point to point. Hover and readout state are
// kept under the id.
void LineChart(const char* id, std::span<const ChartSeries> series, const LineChartOptions& options = {});

struct BarChartOptions
{
    ChartAxis valueAxis;
    Orientation orientation = Orientation::Vertical;  // Vertical: bars grow upward
    bool stacked = false;                  // series stack in one bar per category; side by side otherwise
    bool legend = false;
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;
    const char* errorText = nullptr;
    float width = 0.0f;
    float height = 0.0f;                   // units; 0 for the chart height default
};

// Grouped or stacked bars. Value i of each series belongs to category i; the x values of the
// series are ignored. `categories` are the category labels, read during the call. The plot is
// one Tab stop; Left and Right move the readout between categories.
void BarChart(const char* id, std::span<const ChartSeries> series, std::span<const char* const> categories,
              const BarChartOptions& options = {});

// One slice of a donut chart.
struct DonutSlice
{
    const char* label = nullptr;           // legend and hover text
    double value = 0.0;                    // negative values count as 0
    ImU32 color = 0;                       // 0: palette color of the slice index
};

struct DonutChartOptions
{
    bool pie = false;                      // a full pie instead of a ring
    const char* centerText = nullptr;      // text in the hole; null shows the total with format
    const char* format = nullptr;          // printf for values; null: "%g"
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    float minSliceFraction = 0.0f;         // slices below this share of the total merge into one "other" slice; 0 keeps all
    const char* otherLabel = nullptr;      // label of the merged slice; null uses Strings::more
    bool legend = false;
    float diameter = 0.0f;                 // units; 0 for the chart height default
    DataState state = DataState::Ready;
    const char* emptyText = nullptr;       // shown when the total is 0
    const char* errorText = nullptr;
};

// A ring (or pie) of slices proportional to their values. The chart is one Tab stop; Left and
// Right move the readout between slices. The hovered slice is kept under the id.
void DonutChart(const char* id, std::span<const DonutSlice> slices, const DonutChartOptions& options = {});

// --- Gauges and meters ----------------------------------------------------------------------

// A colored band of a gauge or meter, in value units.
struct GaugeZone
{
    double from = 0.0;
    double to = 0.0;
    Tone tone = Tone::Warning;
};

enum class GaugeShape
{
    Arc,         // three quarters of a circle
    HalfCircle,
};

struct GaugeOptions
{
    GaugeShape shape = GaugeShape::Arc;
    const char* format = nullptr;          // printf for the readout; null: "%.0f"
    FormatFn formatter = nullptr;
    void* formatterUser = nullptr;
    std::span<const GaugeZone> zones = {}; // bands along the arc; read during the call
    bool needle = false;                   // a needle instead of the filled arc
    float diameter = 0.0f;                 // units; 0 for the chart height default
};

// A radial gauge of `value` between `min` and `max` with a readout in the middle and the
// visible label part under it. A value outside the range is drawn at the end with an overflow
// mark. Not interactive. The needle or arc moves with the quick spring, keyed by the id.
// min > max is swapped; min == max draws the readout without an arc.
void Gauge(const char* label, double value, double min, double max, const GaugeOptions& options = {});

struct LevelMeterOptions
{
    Orientation orientation = Orientation::Horizontal;
    std::span<const GaugeZone> zones = {}; // warning and clipping bands; read during the call
    bool peakHold = true;                  // a peak mark that falls back after a moment
    bool segments = false;                 // a segmented look instead of a solid bar
    float width = 0.0f;                    // units; horizontal: 0 fills; vertical: 0 the control height per channel
    float height = 0.0f;                   // units; horizontal: 0 the control height; vertical: 0 for the chart height default
};

// A meter with one bar per channel (audio level, load). Each bar follows its value with
// exponential smoothing and the peak marks are kept under the id. `min` and `max` bound the
// values (for audio in dB, for example -60 and 0); they are double like every range bound of
// the chart family, while the channel values stay float. The visible label part is drawn in
// front. Not interactive.
void LevelMeter(const char* label, std::span<const float> channels, double min, double max,
                const LevelMeterOptions& options = {});

struct FrameTimeGraphOptions
{
    float target = 0.0f;                   // seconds per frame for the target line; 0: 1/60
    bool compact = false;                  // only the frames per second readout and a small graph
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 for the chart height default
};

// A rolling graph of the frame times Facet sampled in each facet::NewFrame (io.DeltaTime), with
// the frames per second, minimum, average and maximum, the target line and frames over the
// target marked in the warning token. Not interactive.
void FrameTimeGraph(const char* id, const FrameTimeGraphOptions& options = {});
// The same for frame times the caller recorded, in seconds, oldest first.
void FrameTimeGraph(const char* id, std::span<const float> frameTimes, const FrameTimeGraphOptions& options = {});

// --- Legend and range brush -----------------------------------------------------------------

// One legend entry. `hidden` is written by Legend when the entry is toggled.
struct LegendItem
{
    const char* label = nullptr;
    ImU32 color = 0;                       // 0: palette color of the entry index
    bool hidden = false;                   // written: copy it into ChartSeries::hidden
    ImGuiID id = 0;                        // stable id of the entry's state when entries change order; 0 uses the index
};

struct LegendOptions
{
    Orientation orientation = Orientation::Horizontal;  // horizontal entries wrap to the width
    bool toggle = true;                    // entries are Tab stops; activation flips hidden,
                                           // Alt+activation shows only that entry
    float width = 0.0f;                    // units; 0 fills the available width
    bool readOnly = false;                 // entries stay Tab stops and show hidden, but never flip it
};

// Color swatches with labels. With toggle, a click, Enter or Space flips the entry's hidden
// member; Alt with any of them hides all others (again: shows all). Returns true when any
// hidden member was written. Entries get ids under the id: item.id when non-zero, else the index.
bool Legend(const char* id, std::span<LegendItem> items, const LegendOptions& options = {});

struct ChartRangeBrushOptions
{
    double minGap = 0.0;                   // value units; smallest allowed high - low, clamped to the range
    bool wheelZoom = true;                 // the wheel over the brush zooms around the cursor
    float width = 0.0f;                    // units; 0 fills the available width
    float height = 0.0f;                   // units; 0 for twice the control height
    bool readOnly = false;
};

// A small overview of `overview` (drawn like a LineChart without axes) with a window from *low
// to *high over it, all in x units between `min` and `max`. Each window edge is a handle with
// its own Tab stop and focus ring (arrows move it; Shift fine, Ctrl coarse); dragging the
// window body pans it; a click outside the window moves the nearer edge. Returns true when
// *low or *high was written. The selected range then drives the x axis of a larger chart.
// min > max is swapped; edges never cross and keep minGap. The visible label part is the
// accessible name and is not drawn, so the overview lines up with the chart above it.
bool ChartRangeBrush(const char* label, double* low, double* high, double min, double max,
                     std::span<const ChartSeries> overview, const ChartRangeBrushOptions& options = {});

} // namespace facet
