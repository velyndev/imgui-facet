#pragma once

// Parts shared by the chart widgets (facet/widgets/charts.h), on top of the chart core block
// (src/internal/chart_core.h): the series of the public data form as chart core views, series
// colors and marker shapes, the scratch buffers of decimated lines, the numeric axes with labels
// that never overlap, the readout box of a hovered or keyboard-inspected point, the legend entry
// look shared by Legend and the passive legends under the charts, the data states and the keys
// that move a readout. Every name carries the family, because every family links into one library.
//
// A chart with axes, in the order LineChart and BarChart use them:
//
//     ChartNumericAxis y = ChartValueAxis(options.yAxis, dataRange, plotHeight, labelHeight, ...);
//     const float left = bounds.Min.x + y.labelWidth + gap;           // the y labels decide the plot
//     ChartNumericAxis x = ChartValueAxis(options.xAxis, xRange, plotWidth, ...);
//     DrawChartGrid(drawList, plot, x, y, disabled);
//     ... series through ChartSeriesView, DecimatePolyline into ChartScratchPoints ...
//     DrawChartReadout(drawList, plot, anchor, title, lines, more, disabled);
//
// Lengths are pixels here, as in the chart core; the helpers apply Px() to the tokens.

#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/theme.h"
#include "facet/widgets/charts.h"
#include "internal/chart_core.h"
#include "internal/control.h"

namespace facet::internal
{

// ---- Series --------------------------------------------------------------------------------

// The series as a chart core view: `values` (with `x` when it is as long as the values; a
// shorter x span is ignored and the index is the x), or `count` points of the getter. A series
// without data gives a view of count 0.
SeriesView ChartSeriesView(const ChartSeries& series);

// The points of a series of a getter overload (Sparkline): the getter and its user.
struct ChartGetterSource
{
    ChartGetter getter = nullptr;
    void* user = nullptr;
};
SeriesView ChartGetterView(const ChartGetterSource& source, int count);

// The color of data entry `index`: `color` when it is not 0 (data, rule 10), else the series
// palette color of the index (SeriesColor). Disabled: labelTertiary.
ImVec4 ChartDataColor(ImU32 color, int index, bool disabled);

// Marker shapes, one per series index in turn, so series stay apart without relying on color.
enum class ChartMarker
{
    Circle,
    Square,
    Diamond,
    Triangle,
};
ChartMarker ChartMarkerOf(int index);
// A marker of `size` pixels across around `center`. `hollow` draws its outline only.
void DrawChartMarker(ImDrawList* drawList, ImVec2 center, float size, ChartMarker shape, ImU32 color, bool hollow);

// The visible part of a series or entry label ([text, end), up to "##"); empty for null.
struct ChartLabelText
{
    const char* text = "";
    const char* end = "";
    bool Empty() const { return text == end; }
};
ChartLabelText ChartVisibleLabel(const char* label);

// The id of entry `index` under a widget: from `entryId` when it is not 0 (rule 2), else from the
// index, so the two never meet.
inline ImGuiID ChartEntryId(ImGuiID widgetId, ImGuiID entryId, int index)
{
    if (entryId != 0)
        return ImHashData(&entryId, sizeof(entryId), ~widgetId);
    return ImHashData(&index, sizeof(index), widgetId);
}

// ---- Scratch buffers --------------------------------------------------------------------------

// Buffers shared by every chart of the frame thread, grown to the largest size asked for and
// never shrunk, so a chart that is shown allocates nothing per frame once they are large
// enough. The spans stay valid until the next call of the same function.
std::span<ImVec2> ChartScratchPoints(int count);
std::span<ImVec2> ChartScratchBase(int count);   // the lower edge of a stacked band
std::span<int> ChartScratchIndices(int count);
// Frees them (the charts subsystem at Shutdown).
void ReleaseChartScratch();

// ---- Text -----------------------------------------------------------------------------------

// Tick labels, the readout and the passive legend use Caption and Footnote.
inline constexpr TextStyle kChartTickStyle = TextStyle::Caption;
inline constexpr TextStyle kChartTextStyle = TextStyle::Footnote;

// A value as an axis shows it: the axis formatter, else its printf format, else the tick label
// of `set` (FormatTick). Returns the length written.
int ChartAxisText(const ChartAxis& axis, double value, const TickSet& set, char* buffer, int bufferSize);
// A value as a readout shows it: the axis formatter or format when set, else a tick label with
// one more digit than the ticks of `set` need (about four significant digits on a logarithmic
// axis).
int ChartReadoutText(const ChartAxis& axis, double value, const TickSet& set, char* buffer, int bufferSize);

// ---- Numeric axes -----------------------------------------------------------------------------

// One numeric axis: its range, its ticks and which of them carry a label.
struct ChartNumericAxis
{
    ScaleKind kind = ScaleKind::Linear;
    double min = 0.0;           // the domain the scale maps
    double max = 1.0;
    TickSet set;
    Tick ticks[kMaxTicks];
    int count = 0;              // ticks stored (at most kMaxTicks)
    int labelEvery = 1;         // every n-th major tick has a label, so labels never overlap
    float labelWidth = 0.0f;    // the widest label shown, pixels
    float labelHeight = 0.0f;   // one label line, pixels
    bool labels = true;         // false: hideTicks
    bool grid = true;           // false: hideGrid
};

// How an automatic range is made from the data.
struct ChartRangeRule
{
    bool includeZero = false;   // bars and areas grow from 0 (linear axes)
    bool pad = false;           // 5 % on each side (values, not a time or index axis)
    bool nice = false;          // out to whole tick steps
};

// The range of `axis`: its min and max when they differ (swapped when reversed), else the data
// range by `rule`; [0, 1] without data. A logarithmic range stays above 0.
Range ChartResolveRange(const ChartAxis& axis, Range data, const ChartRangeRule& rule, int targetTicks);

// Builds the axis for `range` along `length` pixels. `vertical`: labels stack along the axis, one
// line apart; otherwise they sit side by side and every n-th one is left out until the widest fits
// between its neighbours with `gap` pixels to spare. Call with the tick text style pushed.
ChartNumericAxis ChartValueAxis(const ChartAxis& axis, Range range, float length, bool vertical, float gap);
// The number of ticks to ask for along `length` pixels (TickTargetCount from the label size).
int ChartTickTarget(float length, bool vertical, float gap);

// The scale of an axis onto [pixelMin, pixelMax].
Scale ChartAxisScale(const ChartNumericAxis& axis, float pixelMin, float pixelMax);

// Grid lines of the labelled major ticks inside `plot` (separator), and the zero line of a linear
// value axis that crosses 0 (separatorOpaque), so the baseline of bars and areas reads stronger.
void DrawChartGridX(ImDrawList* drawList, const ImRect& plot, const ChartNumericAxis& axis, const Scale& scale,
                    bool zeroLine);
void DrawChartGridY(ImDrawList* drawList, const ImRect& plot, const ChartNumericAxis& axis, const Scale& scale,
                    bool zeroLine);
// The labels: y labels right-aligned before `right` (centered on their tick), x labels centered
// under their tick at `top`, kept inside [clipMin, clipMax]. Call with the tick style pushed.
void DrawChartLabelsY(ImDrawList* drawList, const ChartAxis& options, const ChartNumericAxis& axis,
                      const Scale& scale, float right, bool disabled);
void DrawChartLabelsX(ImDrawList* drawList, const ChartAxis& options, const ChartNumericAxis& axis,
                      const Scale& scale, float top, float clipMin, float clipMax, bool disabled);

// ---- Readout ----------------------------------------------------------------------------------

inline constexpr int kChartReadoutLines = 8;  // series rows a readout lists; the rest is "+{count}"

struct ChartReadoutLine
{
    ChartLabelText label;
    ImVec4 color;
    ChartMarker marker = ChartMarker::Square;
    char value[48] = {};
};

// The box that names the inspected point: `title` (the x value or category) in labelSecondary,
// then a row per line with its marker, label and value, and "+{count}" for `more` rows left out.
// Placed beside `anchor` inside `area` (right of it, or left where it does not fit), on the
// elevated surface with a hairline, clipped to `area`. Call with no text style pushed.
void DrawChartReadout(ImDrawList* drawList, const ImRect& area, ImVec2 anchor, const char* title,
                      std::span<const ChartReadoutLine> lines, int more);

// What a chart keeps under its id for its readout.
struct ChartReadoutState
{
    int index = -1;          // the inspected entry (sample, category, slice), -1 none
    bool keyboard = false;   // set by the keys; the pointer takes over once it moves
};

// Left and Right (Up and Down too when `vertical`), Home and End on the focused chart `id`: moves
// state.index within [0, count) and returns true when it moved. The keys are taken from keyboard
// navigation while the chart has focus.
bool ChartReadoutKeys(ImGuiID id, bool focused, int count, bool vertical, ChartReadoutState& state);

// ---- Legend entries -----------------------------------------------------------------------------

struct ChartLegendMetrics
{
    float height = 0.0f;     // one entry (the small control height)
    float padding = 0.0f;    // inside an entry, left and right
    float swatch = 0.0f;     // the color square (charts.legendSwatch)
    float gap = 0.0f;        // between the swatch and the label
    float spacing = 0.0f;    // between entries and rows
    float radius = 0.0f;     // of an entry's hover fill
};
ChartLegendMetrics GetChartLegendMetrics();

// The width of an entry for `label` (Footnote pushed), at most `maxWidth`.
float ChartLegendEntryWidth(const ChartLabelText& label, const ChartLegendMetrics& m, float maxWidth);

struct ChartLegendLook
{
    ImVec4 color;
    bool hidden = false;            // a hollow swatch and labelSecondary text
    bool disabled = false;
    bool useMarker = false;         // the series' marker shape instead of the square
    ChartMarker marker = ChartMarker::Square;
};
// Draws an entry in `rect` (Footnote pushed). Returns what was drawn of the label.
FittedText DrawChartLegendEntry(ImDrawList* drawList, const ImRect& rect, const ChartLabelText& label,
                                const ChartLegendLook& look, const ChartLegendMetrics& m);

// For selftests and tests: the rectangle (screen pixels) entry `index` of the Legend `legendId`
// (its id hashed in its window) had in its last frame; empty when unknown. The first
// kChartLegendTracked entries are kept.
inline constexpr int kChartLegendTracked = 32;
ImRect ChartLegendEntryRect(ImGuiID legendId, int index);

// A passive legend: `count` entries from `entry(data, i, out)` (false skips one) flowed into rows
// of `width` pixels. Measure returns the height; Draw draws them with the top-left at `origin`.
using ChartLegendSource = bool (*)(const void* data, int index, ChartLabelText* label, ChartLegendLook* look);
float MeasureChartLegend(ChartLegendSource source, const void* data, int count, float width);
void DrawChartLegend(ImDrawList* drawList, ImVec2 origin, float width, ChartLegendSource source, const void* data,
                     int count);

// ---- Frame times ------------------------------------------------------------------------------

// Frame times (io.DeltaTime, seconds) the charts subsystem records in every facet::NewFrame, in a
// ring of kChartFrameHistory slots; the view reads them oldest first. Nothing is allocated.
inline constexpr int kChartFrameHistory = 300;
SeriesView ChartFrameTimes();
// Forgets them (facet::Init and Shutdown, and tests).
void ResetChartFrameTimes();

// ---- Data states ------------------------------------------------------------------------------

// For a chart that is not Ready or has nothing to show: a skeleton over `bounds` while Loading,
// else the error or empty state (EmptyState, compact when low) centered in it. Submits the empty
// state's items under `id`, then puts the cursor back to bounds.Min and leaves the item data for
// the caller's own item. Returns false when the chart is Ready with data (nothing drawn).
bool DrawChartDataState(ImGuiID id, const ImRect& bounds, DataState state, bool empty, const char* emptyText,
                        const char* errorText);

} // namespace facet::internal
