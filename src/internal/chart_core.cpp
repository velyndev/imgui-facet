// Chart core (chart_core.h).

#include "internal/chart_core.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/theme.h"

namespace facet::internal
{
namespace
{

bool Finite(double value)
{
    return std::isfinite(value);
}

double SafeLog10(double value)
{
    return std::log10(std::max(value, kLogFloor));
}

// Relative tolerance for "a tick lies on the end of the range".
constexpr double kTickTolerance = 1e-9;
// Tick labels switch to exponent form from this magnitude, past this many decimals, or (on a
// logarithmic axis) from this decade in either direction.
constexpr double kExponentFrom = 1e6;
constexpr int kMaxFixedDecimals = 6;
constexpr int kExponentDecade = 6;

// Series colors: the chroma of every series, the lightness each theme starts from, the step
// the lightness moves by until the contrast is reached, and that contrast (WCAG non-text).
constexpr float kSeriesChroma = 0.14f;
constexpr float kSeriesLightnessLight = 0.56f;
constexpr float kSeriesLightnessDark = 0.72f;
constexpr float kSeriesLightnessStep = 0.01f;
constexpr float kSeriesContrast = 3.0f;
// Each later cycle of the palette moves the lightness this far from the surfaces.
constexpr float kSeriesCycleShift = 0.12f;
// Hue offsets from the accent hue in degrees, ordered so that consecutive series differ by at
// least 90 degrees and the first four split the circle into quarters.
constexpr float kSeriesHueOffsets[kSeriesPaletteSize] = {0.0f, 180.0f, 90.0f, 270.0f, 45.0f, 225.0f, 135.0f, 315.0f};

// Polylines are drawn in pieces of this many points (each piece overlaps the next by one).
constexpr int kPolylineChunk = 4096;

} // namespace

// --- Scales --------------------------------------------------------------------------------

float Scale::ToPixel(double value) const
{
    double t = 0.0;
    if (kind == ScaleKind::Logarithmic)
    {
        const double lo = SafeLog10(domainMin);
        const double hi = SafeLog10(domainMax);
        t = hi != lo ? (SafeLog10(value) - lo) / (hi - lo) : 0.0;
        if (std::isnan(value))
            t = value;
    }
    else
    {
        t = domainMax != domainMin ? (value - domainMin) / (domainMax - domainMin) : 0.0;
    }
    return static_cast<float>(static_cast<double>(pixelMin) + t * static_cast<double>(pixelMax - pixelMin));
}

double Scale::FromPixel(float pixel) const
{
    const double span = static_cast<double>(pixelMax) - static_cast<double>(pixelMin);
    const double t = span != 0.0 ? (static_cast<double>(pixel) - pixelMin) / span : 0.0;
    if (kind == ScaleKind::Logarithmic)
    {
        const double lo = SafeLog10(domainMin);
        const double hi = SafeLog10(domainMax);
        return std::pow(10.0, lo + t * (hi - lo));
    }
    return domainMin + t * (domainMax - domainMin);
}

Scale MakeScale(ScaleKind kind, double min, double max, float pixelMin, float pixelMax)
{
    Scale scale;
    scale.kind = kind;
    scale.pixelMin = pixelMin;
    scale.pixelMax = pixelMax;
    if (!Finite(min) || !Finite(max))
    {
        scale.domainMin = kind == ScaleKind::Logarithmic ? 1.0 : 0.0;
        scale.domainMax = kind == ScaleKind::Logarithmic ? 10.0 : 1.0;
        return scale;
    }
    if (min > max)
        std::swap(min, max);
    if (kind == ScaleKind::Logarithmic)
    {
        min = std::max(min, kLogFloor);
        max = std::max(max, kLogFloor);
        if (min == max)
        {
            const double factor = std::sqrt(10.0);
            min /= factor;
            max *= factor;
        }
    }
    else if (min == max)
    {
        min -= 0.5;
        max += 0.5;
    }
    scale.domainMin = min;
    scale.domainMax = max;
    return scale;
}

// --- Ticks ---------------------------------------------------------------------------------

int TickTargetCount(float pixelLength, float labelExtent, float gap)
{
    const float each = std::max(labelExtent + gap, 1.0f);
    if (!(pixelLength > 0.0f))
        return 2;
    const float fits = std::floor((pixelLength + gap) / each);
    return static_cast<int>(std::clamp(fits, 2.0f, static_cast<float>(kMaxTicks)));
}

double NiceStep(double span, int targetCount)
{
    if (!Finite(span) || span <= 0.0)
        return 0.0;
    targetCount = std::clamp(targetCount, 2, kMaxTicks);
    const double rough = span / static_cast<double>(targetCount - 1);
    const double magnitude = std::pow(10.0, std::floor(std::log10(rough)));
    for (const double multiple : {1.0, 2.0, 5.0, 10.0})
    {
        if (multiple * magnitude >= rough * (1.0 - kTickTolerance))
            return multiple * magnitude;
    }
    return 10.0 * magnitude;
}

int StepDecimals(double step)
{
    if (!Finite(step) || step <= 0.0)
        return 0;
    return std::max(0, -static_cast<int>(std::floor(std::log10(step) + kTickTolerance)));
}

TickSet LinearTicks(double min, double max, int targetCount, std::span<Tick> out)
{
    TickSet set;
    if (!Finite(min) || !Finite(max))
        return set;
    if (min > max)
        std::swap(min, max);
    set.step = NiceStep(max - min, targetCount);
    if (set.step <= 0.0)
    {
        if (!out.empty())
            out[0] = {min, true};
        set.count = 1;
        set.exponent = std::fabs(min) >= kExponentFrom;
        return set;
    }
    set.decimals = StepDecimals(set.step);
    const double tolerance = set.step * kTickTolerance * 1e3;
    const double first = std::ceil((min - tolerance) / set.step);
    const double last = std::floor((max + tolerance) / set.step);
    const double largest = std::max(std::fabs(first), std::fabs(last)) * set.step;
    set.exponent = largest >= kExponentFrom || set.decimals > kMaxFixedDecimals;
    for (double k = first; k <= last && set.count <= kMaxTicks * 4; k += 1.0)
    {
        double value = k * set.step;
        if (std::fabs(value) < tolerance)
            value = 0.0;
        if (set.count < static_cast<int>(out.size()))
            out[static_cast<std::size_t>(set.count)] = {value, true};
        ++set.count;
    }
    return set;
}

TickSet LogTicks(double min, double max, int targetCount, std::span<Tick> out)
{
    if (!Finite(min) || !Finite(max))
        return {};
    if (min > max)
        std::swap(min, max);
    min = std::max(min, kLogFloor);
    max = std::max(max, kLogFloor);
    const double lo = std::log10(min);
    const double hi = std::log10(max);
    const int firstDecade = static_cast<int>(std::ceil(lo - kTickTolerance));
    const int lastDecade = static_cast<int>(std::floor(hi + kTickTolerance));
    const int decades = lastDecade - firstDecade + 1;
    if (decades < 2)
        return LinearTicks(min, max, targetCount, out);  // less than a decade: plain steps

    targetCount = std::clamp(targetCount, 2, kMaxTicks);
    const int stride = (decades + targetCount - 1) / targetCount;
    const bool minors = stride == 1 && decades <= targetCount;
    TickSet set;
    set.logarithmic = true;
    set.step = static_cast<double>(stride);
    set.exponent = std::max(std::abs(firstDecade), std::abs(lastDecade)) >= kExponentDecade;
    const auto emit = [&](double value, bool major) {
        if (set.count < static_cast<int>(out.size()))
            out[static_cast<std::size_t>(set.count)] = {value, major};
        ++set.count;
    };
    for (int decade = static_cast<int>(std::floor(lo)); decade <= lastDecade; ++decade)
    {
        const double power = std::pow(10.0, decade);
        if (decade >= firstDecade && (decade - firstDecade) % stride == 0)
            emit(power, true);
        if (!minors)
            continue;
        for (int m = 2; m <= 9; ++m)
        {
            const double value = m * power;
            if (value >= min * (1.0 - kTickTolerance) && value <= max * (1.0 + kTickTolerance))
                emit(value, false);
        }
    }
    return set;
}

int FormatTick(double value, const TickSet& set, char* buffer, int bufferSize)
{
    char text[64];
    if (!Finite(value))
    {
        std::snprintf(text, sizeof(text), "%s", "");
    }
    else if (set.logarithmic)
    {
        // Ticks sit at m * 10^k with m from 1 to 9: one significant digit is enough.
        const double magnitude = std::max(std::fabs(value), kLogFloor);
        int exponent = static_cast<int>(std::floor(std::log10(magnitude) + kTickTolerance));
        double mantissa = std::round(magnitude / std::pow(10.0, exponent));
        if (mantissa >= 10.0)
        {
            mantissa /= 10.0;
            ++exponent;
        }
        if (set.exponent)
            std::snprintf(text, sizeof(text), "%.0fe%d", mantissa, exponent);
        else
            std::snprintf(text, sizeof(text), "%.*f", std::max(0, -exponent), mantissa * std::pow(10.0, exponent));
    }
    else
    {
        const double step = set.step > 0.0 ? set.step : 1.0;
        if (std::fabs(value) < step * 1e-6)
            value = 0.0;
        if (set.exponent && value == 0.0)
        {
            std::snprintf(text, sizeof(text), "%s", "0");
        }
        else if (set.exponent)
        {
            const int exponent = static_cast<int>(std::floor(std::log10(std::fabs(value)) + kTickTolerance));
            const int stepExponent = static_cast<int>(std::floor(std::log10(step) + kTickTolerance));
            const int digits = std::max(0, exponent - stepExponent);
            const double mantissa = value / std::pow(10.0, exponent);
            std::snprintf(text, sizeof(text), "%.*fe%d", digits, mantissa, exponent);
        }
        else
        {
            // A value that rounds to zero prints without a sign.
            if (std::fabs(value) < 0.5 * std::pow(10.0, -set.decimals))
                value = 0.0;
            std::snprintf(text, sizeof(text), "%.*f", set.decimals, value);
        }
    }
    const int length = static_cast<int>(std::strlen(text));
    if (buffer != nullptr && bufferSize > 0)
    {
        const int n = std::min(length, bufferSize - 1);
        std::copy(text, text + n, buffer);
        buffer[n] = '\0';
    }
    return length;
}

// --- Data ------------------------------------------------------------------------------------

float SeriesView::X(int i) const
{
    if (x == nullptr)
        return static_cast<float>(i);
    return *reinterpret_cast<const float*>(reinterpret_cast<const char*>(x) + static_cast<std::ptrdiff_t>(i) * stride);
}

float SeriesView::Y(int i) const
{
    return *reinterpret_cast<const float*>(reinterpret_cast<const char*>(y) + static_cast<std::ptrdiff_t>(i) * stride);
}

namespace
{

template <typename Read>
Range RangeOf(const SeriesView& series, Read read)
{
    Range range;
    for (int i = 0; i < series.count; ++i)
    {
        const double value = read(i);
        if (!Finite(value))
            continue;
        if (!range.valid)
        {
            range = {value, value, true};
            continue;
        }
        range.min = std::min(range.min, value);
        range.max = std::max(range.max, value);
    }
    return range;
}

} // namespace

Range SeriesRangeX(const SeriesView& series)
{
    return RangeOf(series, [&](int i) { return static_cast<double>(series.X(i)); });
}

Range SeriesRangeY(const SeriesView& series)
{
    if (series.y == nullptr)
        return {};
    return RangeOf(series, [&](int i) { return static_cast<double>(series.Y(i)); });
}

Range SeriesRangeY(std::span<const SeriesView> series)
{
    Range all;
    for (const SeriesView& one : series)
    {
        const Range range = SeriesRangeY(one);
        if (!range.valid)
            continue;
        all = all.valid ? Range{std::min(all.min, range.min), std::max(all.max, range.max), true} : range;
    }
    return all;
}

Range PadRange(Range range, double fraction, ScaleKind kind)
{
    if (!range.valid)
        return range;
    const Scale widened = MakeScale(kind, range.min, range.max, 0.0f, 1.0f);
    fraction = Finite(fraction) ? std::max(fraction, 0.0) : 0.0;
    if (kind == ScaleKind::Logarithmic)
    {
        const double lo = std::log10(widened.domainMin);
        const double hi = std::log10(widened.domainMax);
        const double pad = (hi - lo) * fraction;
        return {std::pow(10.0, lo - pad), std::pow(10.0, hi + pad), true};
    }
    const double pad = (widened.domainMax - widened.domainMin) * fraction;
    return {widened.domainMin - pad, widened.domainMax + pad, true};
}

Range NiceRange(Range range, int targetCount, ScaleKind kind)
{
    if (!range.valid)
        return range;
    const Scale widened = MakeScale(kind, range.min, range.max, 0.0f, 1.0f);
    if (kind == ScaleKind::Logarithmic)
    {
        const double lo = std::floor(std::log10(widened.domainMin) + kTickTolerance);
        const double hi = std::ceil(std::log10(widened.domainMax) - kTickTolerance);
        return {std::pow(10.0, lo), std::pow(10.0, std::max(hi, lo + 1.0)), true};
    }
    const double step = NiceStep(widened.domainMax - widened.domainMin, targetCount);
    if (step <= 0.0)
        return {widened.domainMin, widened.domainMax, true};
    const double tolerance = step * kTickTolerance * 1e3;
    return {std::floor((widened.domainMin + tolerance) / step) * step,
            std::ceil((widened.domainMax - tolerance) / step) * step, true};
}

// --- Hover -----------------------------------------------------------------------------------

bool IsSortedByX(const SeriesView& series)
{
    for (int i = 1; i < series.count; ++i)
    {
        const float a = series.X(i - 1);
        const float b = series.X(i);
        if (std::isnan(a) || std::isnan(b) || b < a)
            return false;
    }
    return series.count < 1 || !std::isnan(series.X(0));
}

namespace
{

bool Usable(const SeriesView& series, int i)
{
    return !std::isnan(series.X(i)) && (series.y == nullptr || !std::isnan(series.Y(i)));
}

// First index whose x is not below `x` (a sorted series), in [0, count].
int LowerBoundX(const SeriesView& series, double x)
{
    int lo = 0;
    int hi = series.count;
    while (lo < hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (static_cast<double>(series.X(mid)) < x)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

// First index whose x is above `x` (a sorted series), in [0, count].
int UpperBoundX(const SeriesView& series, double x)
{
    int lo = 0;
    int hi = series.count;
    while (lo < hi)
    {
        const int mid = lo + (hi - lo) / 2;
        if (static_cast<double>(series.X(mid)) <= x)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

} // namespace

int NearestSampleByX(const SeriesView& series, double x, bool sortedByX)
{
    if (series.count <= 0 || std::isnan(x))
        return -1;
    if (sortedByX)
    {
        // The nearest usable sample is the first one left of x or the first one right of it.
        int right = LowerBoundX(series, x);
        int left = right - 1;
        while (left >= 0 && !Usable(series, left))
            --left;
        while (right < series.count && !Usable(series, right))
            ++right;
        // Among samples with the same x the lowest index wins, as in the scan.
        while (left > 0 && series.X(left - 1) == series.X(left) && Usable(series, left - 1))
            --left;
        if (left < 0)
            return right < series.count ? right : -1;
        if (right >= series.count)
            return left;
        const double toLeft = x - static_cast<double>(series.X(left));
        const double toRight = static_cast<double>(series.X(right)) - x;
        return toLeft <= toRight ? left : right;
    }
    int best = -1;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (int i = 0; i < series.count; ++i)
    {
        if (!Usable(series, i))
            continue;
        const double distance = std::fabs(static_cast<double>(series.X(i)) - x);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

Crosshair HoverCrosshair(const SeriesView& series, const Scale& xScale, const Scale& yScale, ImVec2 mouse,
                         const ImRect& plot, bool sortedByX)
{
    Crosshair crosshair;
    if (series.count <= 0 || series.y == nullptr || !plot.Contains(mouse))
        return crosshair;
    const int index = NearestSampleByX(series, xScale.FromPixel(mouse.x), sortedByX);
    if (index < 0)
        return crosshair;
    crosshair.visible = true;
    crosshair.index = index;
    crosshair.valueX = series.X(index);
    crosshair.valueY = series.Y(index);
    crosshair.point = ImVec2(xScale.ToPixel(crosshair.valueX), yScale.ToPixel(crosshair.valueY));
    crosshair.lineX = std::floor(crosshair.point.x);
    return crosshair;
}

// --- Decimation ------------------------------------------------------------------------------

int DecimatedCapacity(float pixelWidth)
{
    const float columns = std::ceil(std::max(pixelWidth, 0.0f)) + 2.0f;
    return 4 * static_cast<int>(columns) + 2;
}

namespace
{

// Merges every run of consecutive samples in samples [begin, end) that falls into one column
// (columns `columnWidth` pixels wide, counted from `origin`) into its first, lowest, highest
// and last sample, in sample order. Returns the points written; `overflow` tells whether
// `out` was too small (then the points written cover the series only up to there).
int MergeColumnRuns(const SeriesView& series, const Scale& xScale, const Scale& yScale, int begin, int end,
                    float origin, float columnWidth, std::span<ImVec2> out, bool& overflow)
{
    const int capacity = static_cast<int>(out.size());
    int written = 0;
    overflow = false;
    const auto point = [&](int i) { return ImVec2(xScale.ToPixel(series.X(i)), yScale.ToPixel(series.Y(i))); };

    // The current column: its first, last, lowest and highest sample.
    bool open = false;
    float column = 0.0f;
    int first = 0, last = 0, low = 0, high = 0;
    const auto flush = [&]() {
        int order[4] = {first, low, high, last};
        std::sort(order + 1, order + 3);  // the lowest and highest in sample order, between the ends
        int previous = -1;
        for (const int index : order)
        {
            if (index == previous)
                continue;
            if (written >= capacity)
            {
                overflow = true;
                return;
            }
            out[static_cast<std::size_t>(written++)] = point(index);
            previous = index;
        }
    };
    for (int i = begin; i < end && !overflow; ++i)
    {
        if (!Usable(series, i))
            continue;
        const float c = std::floor((xScale.ToPixel(series.X(i)) - origin) / columnWidth);
        if (!open || c != column)
        {
            if (open)
                flush();
            open = true;
            column = c;
            first = last = low = high = i;
            continue;
        }
        last = i;
        if (series.Y(i) < series.Y(low))
            low = i;
        if (series.Y(i) > series.Y(high))
            high = i;
    }
    if (open && !overflow)
        flush();
    return written;
}

} // namespace

int DecimatePolyline(const SeriesView& series, const Scale& xScale, const Scale& yScale, std::span<ImVec2> out,
                     bool sortedByX)
{
    if (series.count <= 0 || series.y == nullptr || out.empty())
        return 0;

    if (sortedByX)
    {
        // Only the samples inside the x domain, plus one on each side so lines enter the plot.
        // They span at most the plot's columns plus two, which DecimatedCapacity holds.
        const double domainLo = std::min(xScale.domainMin, xScale.domainMax);
        const double domainHi = std::max(xScale.domainMin, xScale.domainMax);
        const int begin = std::max(0, LowerBoundX(series, domainLo) - 1);
        const int end = std::min(series.count, UpperBoundX(series, domainHi) + 1);
        bool overflow = false;
        return MergeColumnRuns(series, xScale, yScale, begin, end, 0.0f, 1.0f, out, overflow);
    }

    // Unsorted (a parametric curve, a curve that turns back): every sample is visited. Runs in
    // one-pixel columns come first; when they do not fit, the columns double in width until
    // they do. A column wider than all samples holds one run of at most four points, so the
    // polyline always spans the whole series once `out` holds four points.
    float lo = 0.0f, hi = 0.0f;
    bool any = false;
    for (int i = 0; i < series.count; ++i)
    {
        const float x = Usable(series, i) ? xScale.ToPixel(series.X(i)) : NAN;
        if (!std::isfinite(x))
            continue;
        lo = any ? std::min(lo, x) : x;
        hi = any ? std::max(hi, x) : x;
        any = true;
    }
    if (!any)
        return 0;
    const float span = std::max(hi - lo, 1.0f);
    int written = 0;
    for (float width = 1.0f;; width *= 2.0f)
    {
        const bool last = width > span;
        bool overflow = false;
        written = MergeColumnRuns(series, xScale, yScale, 0, series.count, lo, last ? 2.0f * span + 1.0f : width,
                                  out, overflow);
        if (!overflow || last)
            return written;
    }
}

// --- Drawing ---------------------------------------------------------------------------------

ImRect SnapPlotRect(const ImRect& rect)
{
    ImRect snapped(std::ceil(rect.Min.x), std::ceil(rect.Min.y), std::floor(rect.Max.x), std::floor(rect.Max.y));
    snapped.Max.x = std::max(snapped.Max.x, snapped.Min.x);
    snapped.Max.y = std::max(snapped.Max.y, snapped.Min.y);
    return snapped;
}

void DrawPolyline(ImDrawList* drawList, std::span<const ImVec2> points, ImU32 color, float thickness,
                  const ImRect& clip)
{
    const int count = static_cast<int>(points.size());
    if (drawList == nullptr || count < 2)
        return;
    drawList->PushClipRect(clip.Min, clip.Max, true);
    for (int start = 0; start < count - 1; start += kPolylineChunk - 1)
    {
        const int n = std::min(kPolylineChunk, count - start);
        drawList->AddPolyline(points.data() + start, n, color, thickness);
    }
    drawList->PopClipRect();
}

void DrawArea(ImDrawList* drawList, std::span<const ImVec2> points, float baseline, ImU32 color,
              const ImRect& clip)
{
    const int count = static_cast<int>(points.size());
    if (drawList == nullptr || count < 2)
        return;
    drawList->PushClipRect(clip.Min, clip.Max, true);
    const ImVec2 uv = drawList->_Data->TexUvWhitePixel;
    const auto triangle = [&](ImVec2 a, ImVec2 b, ImVec2 c) {
        drawList->PrimReserve(3, 3);
        const ImDrawIdx base = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
        drawList->PrimWriteVtx(a, uv, color);
        drawList->PrimWriteVtx(b, uv, color);
        drawList->PrimWriteVtx(c, uv, color);
        drawList->PrimWriteIdx(base);
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 1));
        drawList->PrimWriteIdx(static_cast<ImDrawIdx>(base + 2));
    };
    for (int i = 0; i + 1 < count; ++i)
    {
        const ImVec2 a = points[static_cast<std::size_t>(i)];
        const ImVec2 b = points[static_cast<std::size_t>(i + 1)];
        const ImVec2 aBase(a.x, baseline);
        const ImVec2 bBase(b.x, baseline);
        const float da = a.y - baseline;
        const float db = b.y - baseline;
        if ((da < 0.0f && db > 0.0f) || (da > 0.0f && db < 0.0f))
        {
            const float t = da / (da - db);
            const ImVec2 cross(a.x + t * (b.x - a.x), baseline);
            triangle(a, cross, aBase);
            triangle(cross, b, bBase);
            continue;
        }
        triangle(a, b, bBase);
        triangle(a, bBase, aBase);
    }
    drawList->PopClipRect();
}

void DrawGridLines(ImDrawList* drawList, const ImRect& plot, std::span<const float> xPixels,
                   std::span<const float> yPixels, ImU32 color)
{
    if (drawList == nullptr)
        return;
    for (const float x : xPixels)
    {
        if (x >= plot.Min.x && x <= plot.Max.x)
            HairlineV(drawList, x, plot.Min.y, plot.Max.y, color);
    }
    for (const float y : yPixels)
    {
        if (y >= plot.Min.y && y <= plot.Max.y)
            HairlineH(drawList, plot.Min.x, plot.Max.x, y, color);
    }
}

// --- Series colors ---------------------------------------------------------------------------

float MinSurfaceContrast(const Colors& colors, const ImVec4& color)
{
    const ImVec4 surfaces[] = {colors.bg, colors.bgSecondary, colors.bgTertiary, colors.ElevatedSurface(1),
                               colors.ElevatedSurface(2), colors.ElevatedSurface(3)};
    float lowest = 21.0f;
    for (const ImVec4& surface : surfaces)
        lowest = std::min(lowest, ContrastRatio(color, surface));
    return lowest;
}

ImVec4 SeriesColor(const Colors& colors, int index)
{
    index = std::max(index, 0);
    // Light surfaces need darker series, dark surfaces lighter ones: the luminance of bg says
    // which way to move.
    const bool darkSurfaces = RelativeLuminance(colors.bg) < 0.18f;
    const int cycle = index / kSeriesPaletteSize;
    const float hue = ToOklch(colors.accent).h + kSeriesHueOffsets[index % kSeriesPaletteSize];
    float lightness = darkSurfaces ? kSeriesLightnessDark + kSeriesCycleShift * static_cast<float>(cycle)
                                   : kSeriesLightnessLight - kSeriesCycleShift * static_cast<float>(cycle);
    lightness = std::clamp(lightness, 0.05f, 0.98f);
    const float direction = darkSurfaces ? kSeriesLightnessStep : -kSeriesLightnessStep;
    const Oklch lch{lightness, kSeriesChroma, std::fmod(hue, 360.0f)};
    // Every candidate is rounded to 8 bits per channel first, so the contrast holds for the
    // color exactly as it is drawn.
    const auto drawn = [](const Oklch& c) { return FromU32(ToU32(FromOklch(c))); };
    ImVec4 color = drawn(lch);
    for (int step = 1; step <= 100 && MinSurfaceContrast(colors, color) < kSeriesContrast; ++step)
    {
        Oklch next = lch;
        next.L = std::clamp(lch.L + direction * static_cast<float>(step), 0.0f, 1.0f);
        color = drawn(next);
    }
    return color;
}

void SeriesPalette(const Colors& colors, std::span<ImVec4> out)
{
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = SeriesColor(colors, static_cast<int>(i));
}

} // namespace facet::internal
