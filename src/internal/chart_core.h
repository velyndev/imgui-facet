#pragma once

// Chart core: what every chart, sparkline, meter and curve editor shares. Scales map values to
// pixels and back, ticks come in "nice" steps with labels that print only the digits the step
// needs, hover snaps to the nearest sample, long series are decimated to a few points per
// pixel column, and series colors are derived from the accent so that each one reaches 3:1
// on the chart surfaces in both themes.
//
// Everything here works in screen pixels, like ImDrawList: widgets convert their design units
// with Px() first. Nothing allocates; results go into caller storage.
//
// Sketch of a line chart built on it:
//
//     const SeriesView series{.y = values, .count = n};               // x is the sample index
//     Range ys = PadRange(SeriesRangeY(series), 0.05, ScaleKind::Linear);
//     const int target = TickTargetCount(plot.GetHeight(), labelHeight, Px(T().spacing.lg));
//     ys = NiceRange(ys, target);
//     const Scale x = MakeScale(ScaleKind::Linear, 0.0, n - 1.0, plot.Min.x, plot.Max.x);
//     const Scale y = MakeScale(ScaleKind::Linear, ys.min, ys.max, plot.Max.y, plot.Min.y);
//     Tick ticks[kMaxTicks];
//     const TickSet set = LinearTicks(ys.min, ys.max, target, ticks);  // gridlines and labels
//     const int count = DecimatePolyline(series, x, y, points, true);   // points: DecimatedCapacity()
//     DrawPolyline(drawList, std::span(points, count), SeriesColor(T().colors, 0), Px(1.5f), plot);

#include <cstddef>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

namespace facet
{
struct Colors;
}

namespace facet::internal
{

// --- Scales --------------------------------------------------------------------------------

enum class ScaleKind
{
    Linear,
    Logarithmic,  // base 10; the domain must be positive
};

// The smallest value a logarithmic scale shows. Values at or below 0 map to it, far below any
// range a chart shows, so a line to such a sample leaves the plot through its bottom edge,
// where the clip rect cuts it. SeriesRangeY with ScaleKind::Logarithmic leaves them out of the
// range, so they never stretch the axis down to kLogFloor.
inline constexpr double kLogFloor = 1e-12;

// Maps the domain [domainMin, domainMax] onto the pixels [pixelMin, pixelMax]. The pixel range
// may run backwards: a y axis maps its minimum to the bottom (pixelMin = plot.Max.y).
struct Scale
{
    ScaleKind kind = ScaleKind::Linear;
    double domainMin = 0.0;
    double domainMax = 1.0;
    float pixelMin = 0.0f;
    float pixelMax = 1.0f;

    // Values outside the domain map outside the pixel range (not clamped); NaN maps to NaN.
    float ToPixel(double value) const;
    // The inverse of ToPixel.
    double FromPixel(float pixel) const;
};

// A scale with a usable domain: min > max is swapped; an empty domain (min == max) is widened
// to [v - h, v + h] with h = max(0.5, |v| * 1e-9) (linear, so a large value still gets a
// span) or [v / sqrt(10), v * sqrt(10)] (logarithmic); a logarithmic
// domain is raised to kLogFloor where it is not positive. NaN or infinite bounds give [0, 1]
// (linear) or [1, 10] (logarithmic).
Scale MakeScale(ScaleKind kind, double min, double max, float pixelMin, float pixelMax);

// --- Ticks ---------------------------------------------------------------------------------

inline constexpr int kMaxTicks = 64;

struct Tick
{
    double value = 0.0;
    bool major = true;  // gets a label and a grid line; minor ticks (logarithmic) only a mark
};

struct TickSet
{
    int count = 0;         // ticks that exist in the range (may exceed the span given)
    double step = 0.0;     // linear: the distance between ticks; logarithmic: decades per major tick
    int decimals = 0;      // digits after the point that the labels of this set need
    bool logarithmic = false;
    // Every label of the set uses exponent form ("1.5e6"): set when the largest tick reaches
    // 1e6 in magnitude, or the step needs more than 6 decimals (logarithmic: a decade at or
    // beyond 1e6 or 1e-6). One form per axis, so labels never mix "500000" and "1e6".
    bool exponent = false;
};

// How many ticks fit along `pixelLength` when each label takes `labelExtent` pixels along the
// axis (its width on an x axis, its height on a y axis) plus `gap` pixels between labels. At
// least 2, at most kMaxTicks.
int TickTargetCount(float pixelLength, float labelExtent, float gap);

// The step 1, 2 or 5 times a power of ten that puts at most `targetCount` ticks into `span`.
// 0 for an empty or invalid span.
double NiceStep(double span, int targetCount);

// Ticks at the multiples of NiceStep inside [min, max] (ends included up to a small
// tolerance), all major, written into `out` in increasing order. A single tick at min with
// step 0 when the span is empty or not finite, when no multiple of the step lies inside the
// range (a target of 2 over a span between two multiples), or when the step is so fine next
// to the values that labels would need more than 15 significant digits (1e12 to
// 1e12 + 0.001, values near 2^53 with steps below 1). So there is always at least one tick.
TickSet LinearTicks(double min, double max, int targetCount, std::span<Tick> out);

// Major ticks at powers of ten inside [min, max] (every n-th decade when more than
// targetCount decades fit), and minor ticks at 2 to 9 times each power of ten while the major
// ticks are one decade apart and there are at most targetCount decades. The range must be
// positive; values at or below 0 are raised to kLogFloor.
TickSet LogTicks(double min, double max, int targetCount, std::span<Tick> out);

// Digits after the point that labels at multiples of `step` need: 0 for steps of 1 and more,
// 1 for 0.5 and 0.2, 2 for 0.05, and so on.
int StepDecimals(double step);

// Writes the label of a tick of `set` (zero terminated, cut at bufferSize) and returns its
// full length. Fixed point with set.decimals digits, or exponent form ("2e6", "1.5e-7") when
// set.exponent is true, with only the digits the step needs and at most 15 significant
// digits. A set with step 0 (a single tick) prints the shortest text that keeps the value
// ("3.25", "-1e308"). 0 prints as "0" in exponent form, and -0 never shows a sign. Labels are
// at most 24 characters long.
int FormatTick(double value, const TickSet& set, char* buffer, int bufferSize);

// --- Data ------------------------------------------------------------------------------------

// Fills x and y of the sample in storage slot `slot` of a series that is not kept in arrays (a
// caller's getter) and returns true, or returns false for a gap (missing data).
using SampleReader = bool (*)(const void* source, int slot, double* x, double* y);

// A series of samples, read in place. With x null, the x of sample i is i. `stride` is the
// distance in bytes between two samples of x and of y (interleaved data), sizeof(float) for
// plain arrays. `offset` reads a ring buffer of `count` slots in place: sample i is stored in
// slot (offset + i) mod count, so a history whose oldest sample sits at the write index is
// one view, oldest first (offset outside [0, count) is reduced mod count).
//
// Two other sources, for the chart widgets: `xd` holds x as doubles (one per slot in a plain
// array; stride does not apply to it), so a time axis in seconds keeps its precision, and is
// read instead of `x` when set. `reader` with `source` reads every sample through a function
// instead of the arrays (x, xd and y are then ignored); a gap reads as NaN y. Every function of
// this header reads samples through XAt and YAt, in double precision.
struct SeriesView
{
    const float* x = nullptr;
    const float* y = nullptr;
    int count = 0;
    int stride = static_cast<int>(sizeof(float));
    int offset = 0;
    const double* xd = nullptr;
    SampleReader reader = nullptr;
    const void* source = nullptr;

    float X(int i) const;
    float Y(int i) const;
    // The x and y of sample i in double precision; y is NaN for a gap of a reader.
    double XAt(int i) const;
    double YAt(int i) const;
    // Both at once (one reader call); returns false when either is NaN.
    bool Sample(int i, double* outX, double* outY) const;
    // There are y values to read: y or a reader is set.
    bool HasY() const { return y != nullptr || reader != nullptr; }
    // The storage slot of sample i (0 <= i < count).
    int Slot(int i) const;
};

struct Range
{
    double min = 0.0;
    double max = 0.0;
    bool valid = false;  // false: no finite sample
};

// The smallest and largest finite x or y of the series; NaN and infinite samples are skipped.
// For a logarithmic y axis (`kind`), samples at or below 0 are skipped too (see kLogFloor).
Range SeriesRangeX(const SeriesView& series);
Range SeriesRangeY(const SeriesView& series, ScaleKind kind = ScaleKind::Linear);
// The range of y over every series in the span.
Range SeriesRangeY(std::span<const SeriesView> series, ScaleKind kind = ScaleKind::Linear);

// Widens a range by `fraction` of its span on each side (logarithmic: of its span in
// decades). An empty range is first widened as in MakeScale. An invalid range stays invalid.
Range PadRange(Range range, double fraction, ScaleKind kind);
// Extends a linear range outward to multiples of NiceStep(span, targetCount), so the first
// and last tick sit on the ends; a logarithmic range to whole decades.
Range NiceRange(Range range, int targetCount, ScaleKind kind = ScaleKind::Linear);

// --- Hover -----------------------------------------------------------------------------------

// True when the x of the samples never decreases (NaN breaks the order).
bool IsSortedByX(const SeriesView& series);

// The sample whose x is nearest to `x`: by binary search when the series is sorted by x
// (IsSortedByX), by a linear scan otherwise; on a tie the lower index wins. Samples with a
// NaN x or y are skipped by the scan. -1 for an empty series.
int NearestSampleByX(const SeriesView& series, double x, bool sortedByX);

// Where the crosshair of a hovered chart goes.
struct Crosshair
{
    bool visible = false;  // the mouse is over the plot and the series has a sample
    int index = -1;        // the snapped sample
    double valueX = 0.0;
    double valueY = 0.0;
    ImVec2 point;          // the sample in pixels
    float lineX = 0.0f;    // x of the vertical hairline (HairlineV): the pixel column of the point
};

Crosshair HoverCrosshair(const SeriesView& series, const Scale& xScale, const Scale& yScale, ImVec2 mouse,
                         const ImRect& plot, bool sortedByX);

// --- Decimation ------------------------------------------------------------------------------

// Points DecimatePolyline may write for a plot `pixelWidth` pixels wide.
int DecimatedCapacity(float pixelWidth);

// Writes a polyline that covers the same pixels as the whole series but has at most four
// points per pixel column: for every column the first and last sample, and the lowest and the
// highest one, in sample order, each at its own exact position. Samples with a NaN x or y are
// skipped. For a series sorted by x (`sortedByX`, from IsSortedByX, which the caller keeps
// while the data does not change) only the samples within the x domain (and one on each side)
// are visited, so the cost follows the visible samples. An unsorted series (a parametric
// curve) is merged the same way over all its samples, run by run of consecutive samples in one
// column; when those runs do not fit `out`, the columns double in width (2, 4, 8 ... pixels)
// until they do, so the polyline always reaches the last sample. Returns the number of points
// written.
int DecimatePolyline(const SeriesView& series, const Scale& xScale, const Scale& yScale, std::span<ImVec2> out,
                     bool sortedByX);

// The same for the samples [begin, end) only (a run between two gaps): the sorted path visits
// the samples of the run within the x domain (and one on each side), the unsorted path every
// sample of the run. When `indices` is not empty (at least out.size() long), it receives the
// sample index of every point written, so a second series of the same x (a stack) can be read
// at the same samples.
int DecimatePolyline(const SeriesView& series, const Scale& xScale, const Scale& yScale, std::span<ImVec2> out,
                     bool sortedByX, int begin, int end, std::span<int> indices);

// The samples a chart visits for an x domain: all of them, or for a series sorted by x those
// within [domainMin, domainMax] plus one on each side. Written to *begin and *end.
void VisibleSampleRange(const SeriesView& series, const Scale& xScale, bool sortedByX, int* begin, int* end);

// The first sample at or after `from` (below `end`) whose x and y are both numbers, or `end`.
int NextUsableSample(const SeriesView& series, int from, int end);
// The first sample at or after `from` (below `end`) that is a gap (NaN x or y), or `end`. The
// samples [NextUsableSample, NextGap) are one run of a line.
int NextGap(const SeriesView& series, int from, int end);

// --- Drawing ---------------------------------------------------------------------------------

// The plot rectangle moved inward to whole pixels (Min rounded up, Max down). Layout sizes
// are fractional at most scales; on whole pixels the clip edges stay crisp, and a mouse at the
// plot's left or top edge, which Dear ImGui floors to a whole pixel, still lies inside it.
ImRect SnapPlotRect(const ImRect& rect);

// A polyline clipped to `clip`, drawn in chunks so any length stays within one draw command's
// index range.
void DrawPolyline(ImDrawList* drawList, std::span<const ImVec2> points, ImU32 color, float thickness,
                  const ImRect& clip);

// The area between a polyline and the horizontal line y = baseline, clipped to `clip`.
// Segments that cross the baseline are split there, so the fill never folds over itself.
void DrawArea(ImDrawList* drawList, std::span<const ImVec2> points, float baseline, ImU32 color,
              const ImRect& clip);

// The band between two polylines of the same length whose points pair up (point i of `top`
// and of `bottom` belong to the same sample, as in a stacked area), clipped to `clip`. Where the
// two cross, the quads fold; a stack of values of one sign never crosses.
void DrawBand(ImDrawList* drawList, std::span<const ImVec2> top, std::span<const ImVec2> bottom, ImU32 color,
              const ImRect& clip);

// Hairline grid lines (draw.h HairlineV / HairlineH, snapped to the pixel grid) at the given
// pixel positions inside `plot`.
void DrawGridLines(ImDrawList* drawList, const ImRect& plot, std::span<const float> xPixels,
                   std::span<const float> yPixels, ImU32 color);

// --- Series colors ---------------------------------------------------------------------------

// Distinct series colors per palette cycle (kChartPaletteSize in theme.h); later series repeat
// the hues at another lightness.
inline constexpr int kSeriesPaletteSize = 8;

// The color of series `index` (0 is the accent hue). The first cycle is the theme's chart
// palette, colors.chart (Theme::ApplyAccent derives it: hues stepping around the OKLCH circle
// from the accent hue, neighbours far apart, one chroma). Each later cycle moves the lightness
// of the same hue further from the surfaces. Every color is checked again and moved until it
// reaches 3:1 on bg, bgSecondary, bgTertiary and every elevated surface, as drawn (8 bits per
// channel), so a theme whose surfaces changed after ApplyAccent stays readable too.
ImVec4 SeriesColor(const Colors& colors, int index);
// SeriesColor for indices 0 to out.size() - 1.
void SeriesPalette(const Colors& colors, std::span<ImVec4> out);
// The lowest contrast of `color` on the chart surfaces listed above.
float MinSurfaceContrast(const Colors& colors, const ImVec4& color);

} // namespace facet::internal
