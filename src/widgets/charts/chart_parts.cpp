// Parts shared by the chart widgets (chart_parts.h).

#include "widgets/charts/chart_parts.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/widgets/display.h"
#include "internal/control.h"
#include "internal/loading.h"
#include "internal/reserve.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"

namespace facet::internal
{
namespace
{

// A hollow marker or swatch strokes this share of the series line width, at least a hairline.
constexpr float kChartHollowShare = 0.75f;
// The corner radius of a legend swatch as a share of its side, at most radii.element.
constexpr float kChartSwatchRoundness = 0.25f;
// The share of the plot an automatic value range adds on each side (rule pad).
constexpr double kChartRangePad = 0.05;
// Rounds of measuring tick labels and asking for fewer ticks before labels are thinned out.
constexpr int kChartTickRounds = 4;
// Significant digits of a readout value on an axis without a step (logarithmic, or none).
constexpr double kChartReadoutDigits = 4.0;
// Text that stands for the widest tick label before any label is measured.
constexpr const char* kChartTickGuess = "0000";

bool ReadChartSeries(const void* source, int slot, double* x, double* y)
{
    const ChartSeries& series = *static_cast<const ChartSeries*>(source);
    ChartPoint point;
    if (series.getter == nullptr || !series.getter(series.user, slot, &point))
        return false;
    *x = point.x;
    *y = point.y;
    return true;
}

bool ReadChartGetter(const void* source, int slot, double* x, double* y)
{
    const ChartGetterSource& getter = *static_cast<const ChartGetterSource*>(source);
    ChartPoint point;
    if (getter.getter == nullptr || !getter.getter(getter.user, slot, &point))
        return false;
    *x = point.x;
    *y = point.y;
    return true;
}

struct ChartScratch
{
    std::vector<ImVec2> points;
    std::vector<ImVec2> base;
    std::vector<int> indices;
};

ChartScratch& Scratch()
{
    static ChartScratch scratch;
    return scratch;
}

template <class T>
std::span<T> Grown(std::vector<T>& buffer, int count)
{
    const std::size_t need = static_cast<std::size_t>(std::max(count, 0));
    if (buffer.size() < need)
    {
        ReserveAtLeast(buffer, need);
        buffer.resize(need);
    }
    return std::span<T>(buffer.data(), need);
}

ImVec4 ChartTextColor(bool disabled)
{
    return disabled ? T().colors.labelTertiary : T().colors.labelSecondary;
}

float TextWidth(const char* text)
{
    return ImGui::CalcTextSize(text).x;
}

bool Finite(double value)
{
    return std::isfinite(value);
}

} // namespace

// ---- Series --------------------------------------------------------------------------------

SeriesView ChartSeriesView(const ChartSeries& series)
{
    SeriesView view;
    if (!series.values.empty())
    {
        view.y = series.values.data();
        view.count = static_cast<int>(std::min<std::size_t>(series.values.size(), std::numeric_limits<int>::max()));
        if (series.x.size() >= series.values.size())
            view.xd = series.x.data();
        return view;
    }
    if (series.getter != nullptr && series.count > 0)
    {
        view.reader = &ReadChartSeries;
        view.source = &series;
        view.count = series.count;
    }
    return view;
}

SeriesView ChartGetterView(const ChartGetterSource& source, int count)
{
    SeriesView view;
    if (source.getter != nullptr && count > 0)
    {
        view.reader = &ReadChartGetter;
        view.source = &source;
        view.count = count;
    }
    return view;
}

ImVec4 ChartDataColor(ImU32 color, int index, bool disabled)
{
    if (disabled)
        return T().colors.labelTertiary;
    return color != 0 ? FromU32(color) : SeriesColor(T().colors, index);
}

ChartMarker ChartMarkerOf(int index)
{
    constexpr int kShapes = 4;
    return static_cast<ChartMarker>(((index % kShapes) + kShapes) % kShapes);
}

void DrawChartMarker(ImDrawList* drawList, ImVec2 center, float size, ChartMarker shape, ImU32 color, bool hollow)
{
    const float r = size * 0.5f;
    const float stroke = std::max(HairlineThickness(), std::round(Px(T().charts.lineWidth) * kChartHollowShare));
    switch (shape)
    {
    case ChartMarker::Circle:
        if (hollow)
            drawList->AddCircle(center, r, color, 0, stroke);
        else
            drawList->AddCircleFilled(center, r, color);
        break;
    case ChartMarker::Square:
    {
        // A square of the same area as the circle reads as the same size.
        const float h = r * 0.886f;
        if (hollow)
            drawList->AddRect(ImVec2(center.x - h, center.y - h), ImVec2(center.x + h, center.y + h), color, 0.0f, stroke);
        else
            drawList->AddRectFilled(ImVec2(center.x - h, center.y - h), ImVec2(center.x + h, center.y + h), color);
        break;
    }
    case ChartMarker::Diamond:
    {
        const ImVec2 a(center.x, center.y - r), b(center.x + r, center.y), c(center.x, center.y + r), d(center.x - r, center.y);
        if (hollow)
            drawList->AddQuad(a, b, c, d, color, stroke);
        else
            drawList->AddQuadFilled(a, b, c, d, color);
        break;
    }
    case ChartMarker::Triangle:
    {
        const ImVec2 a(center.x, center.y - r), b(center.x + r, center.y + r * 0.8f), c(center.x - r, center.y + r * 0.8f);
        if (hollow)
            drawList->AddTriangle(a, b, c, color, stroke);
        else
            drawList->AddTriangleFilled(a, b, c, color);
        break;
    }
    }
}

ChartLabelText ChartVisibleLabel(const char* label)
{
    if (label == nullptr)
        return {};
    return {label, ImGui::FindRenderedTextEnd(label)};
}

// ---- Scratch buffers --------------------------------------------------------------------------

std::span<ImVec2> ChartScratchPoints(int count)
{
    return Grown(Scratch().points, count);
}

std::span<ImVec2> ChartScratchBase(int count)
{
    return Grown(Scratch().base, count);
}

std::span<int> ChartScratchIndices(int count)
{
    return Grown(Scratch().indices, count);
}

void ReleaseChartScratch()
{
    ChartScratch& scratch = Scratch();
    std::vector<ImVec2>().swap(scratch.points);
    std::vector<ImVec2>().swap(scratch.base);
    std::vector<int>().swap(scratch.indices);
}

// ---- Text -----------------------------------------------------------------------------------

int ChartAxisText(const ChartAxis& axis, double value, const TickSet& set, char* buffer, int bufferSize)
{
    if (buffer == nullptr || bufferSize <= 0)
        return 0;
    buffer[0] = '\0';
    if (axis.formatter != nullptr)
    {
        axis.formatter(buffer, bufferSize, value, axis.formatterUser);
        buffer[bufferSize - 1] = '\0';
        return static_cast<int>(std::strlen(buffer));
    }
    if (axis.format != nullptr)
        return static_cast<int>(ImFormatString(buffer, static_cast<std::size_t>(bufferSize), axis.format, value));
    return std::min(FormatTick(value, set, buffer, bufferSize), bufferSize - 1);
}

int ChartReadoutText(const ChartAxis& axis, double value, const TickSet& set, char* buffer, int bufferSize)
{
    if (axis.formatter != nullptr || axis.format != nullptr)
        return ChartAxisText(axis, value, set, buffer, bufferSize);
    if (set.logarithmic || set.step <= 0.0)
    {
        // Four significant digits of the value itself, in the shortest text that keeps them.
        const double magnitude = std::fabs(value);
        double rounded = value;
        if (magnitude > 0.0 && Finite(magnitude))
        {
            const double unit = std::pow(10.0, std::floor(std::log10(magnitude)) - kChartReadoutDigits + 1.0);
            rounded = std::round(value / unit) * unit;
        }
        TickSet single;
        single.exponent = magnitude >= 1e6 || (magnitude > 0.0 && magnitude < 1e-4);
        return std::min(FormatTick(rounded, single, buffer, bufferSize), bufferSize - 1);
    }
    TickSet finer;
    finer.exponent = set.exponent;
    finer.step = set.step / 10.0;
    finer.decimals = StepDecimals(finer.step);
    if (finer.decimals > 6)
        finer.exponent = true;
    return std::min(FormatTick(value, finer, buffer, bufferSize), bufferSize - 1);
}

// ---- Numeric axes -----------------------------------------------------------------------------

Range ChartResolveRange(const ChartAxis& axis, Range data, const ChartRangeRule& rule, int targetTicks)
{
    const bool log = axis.scale == ValueScale::Logarithmic;
    const ScaleKind kind = log ? ScaleKind::Logarithmic : ScaleKind::Linear;
    if (Finite(axis.min) && Finite(axis.max) && axis.min != axis.max)
    {
        Range fixed{std::min(axis.min, axis.max), std::max(axis.min, axis.max), true};
        if (log && fixed.min <= 0.0)
        {
            if (fixed.max <= 0.0)
                return {1.0, 10.0, true};
            fixed.min = data.valid && data.min > 0.0 && data.min < fixed.max ? data.min : fixed.max / 1000.0;
        }
        return fixed;
    }
    Range range = data;
    if (!range.valid)
        return log ? Range{1.0, 10.0, true} : Range{0.0, 1.0, true};
    const bool fromZero = rule.includeZero && !log;
    if (fromZero)
    {
        range.min = std::min(range.min, 0.0);
        range.max = std::max(range.max, 0.0);
    }
    if (rule.pad)
    {
        const Range before = range;
        range = PadRange(range, kChartRangePad, kind);
        // Bars and areas start at the baseline itself, not below it.
        if (fromZero && before.min == 0.0)
            range.min = 0.0;
        if (fromZero && before.max == 0.0)
            range.max = 0.0;
    }
    // Extending the range can make the step of its ticks coarser; extend again until the ends sit
    // on ticks of the range itself.
    for (int round = 0; rule.nice && round < kChartTickRounds; ++round)
    {
        const Range wider = NiceRange(range, targetTicks, kind);
        const bool same = wider.min == range.min && wider.max == range.max;
        range = wider;
        if (same)
            break;
    }
    return range;
}

int ChartTickTarget(float length, bool vertical, float gap)
{
    const float extent = vertical ? ImGui::GetFontSize() : TextWidth(kChartTickGuess);
    return TickTargetCount(length, extent, gap);
}

ChartNumericAxis ChartValueAxis(const ChartAxis& options, Range range, float length, bool vertical, float gap)
{
    ChartNumericAxis axis;
    axis.kind = options.scale == ValueScale::Logarithmic ? ScaleKind::Logarithmic : ScaleKind::Linear;
    axis.labels = !options.hideTicks;
    axis.grid = !options.hideGrid;
    axis.labelHeight = ImGui::GetFontSize();
    const Scale widened = MakeScale(axis.kind, range.min, range.max, 0.0f, 1.0f);
    axis.min = widened.domainMin;
    axis.max = widened.domainMax;
    if (!(length > 0.0f))
        length = 1.0f;

    int target = ChartTickTarget(length, vertical, gap);
    char text[64];
    int majors = 0;
    for (int round = 0; round < kChartTickRounds; ++round)
    {
        axis.set = axis.kind == ScaleKind::Logarithmic ? LogTicks(axis.min, axis.max, target, axis.ticks)
                                                       : LinearTicks(axis.min, axis.max, target, axis.ticks);
        axis.count = std::min(axis.set.count, kMaxTicks);
        axis.labelWidth = 0.0f;
        majors = 0;
        for (int i = 0; i < axis.count; ++i)
        {
            if (!axis.ticks[i].major)
                continue;
            ++majors;
            if (axis.labels)
            {
                ChartAxisText(options, axis.ticks[i].value, axis.set, text, sizeof(text));
                axis.labelWidth = std::max(axis.labelWidth, TextWidth(text));
            }
        }
        if (vertical || !axis.labels || majors <= 1)
            break;
        const float need = static_cast<float>(majors) * (axis.labelWidth + gap) - gap;
        if (need <= length || target <= 2)
            break;
        const int fits = std::max(2, static_cast<int>(std::floor((length + gap) / (axis.labelWidth + gap))));
        target = std::min(fits, target - 1);
    }

    // Labels that still would not fit between their neighbours: every n-th one, measured from the
    // pixel distance of two neighbouring major ticks.
    axis.labelEvery = 1;
    if (axis.labels && majors > 1)
    {
        const double lo = axis.kind == ScaleKind::Logarithmic ? std::log10(std::max(axis.min, kLogFloor)) : axis.min;
        const double hi = axis.kind == ScaleKind::Logarithmic ? std::log10(std::max(axis.max, kLogFloor)) : axis.max;
        double firstMajor = 0.0, secondMajor = 0.0;
        int found = 0;
        for (int i = 0; i < axis.count && found < 2; ++i)
        {
            if (!axis.ticks[i].major)
                continue;
            const double v = axis.kind == ScaleKind::Logarithmic ? std::log10(std::max(axis.ticks[i].value, kLogFloor))
                                                                 : axis.ticks[i].value;
            (found == 0 ? firstMajor : secondMajor) = v;
            ++found;
        }
        const double span = hi - lo;
        if (found == 2 && span > 0.0)
        {
            const float pixels = static_cast<float>((secondMajor - firstMajor) / span * length);
            const float extent = (vertical ? axis.labelHeight : axis.labelWidth) + gap;
            if (pixels > 0.0f && extent > pixels)
                axis.labelEvery = std::max(1, static_cast<int>(std::ceil(extent / pixels)));
        }
    }
    if (!axis.labels)
        axis.labelWidth = 0.0f;
    return axis;
}

Scale ChartAxisScale(const ChartNumericAxis& axis, float pixelMin, float pixelMax)
{
    return MakeScale(axis.kind, axis.min, axis.max, pixelMin, pixelMax);
}

namespace
{

// Calls `visit(value)` for every major tick that carries a label (every labelEvery-th one).
template <class Visit>
void ForLabelledTicks(const ChartNumericAxis& axis, Visit visit)
{
    int major = 0;
    for (int i = 0; i < axis.count; ++i)
    {
        if (!axis.ticks[i].major)
            continue;
        if (major++ % axis.labelEvery == 0)
            visit(axis.ticks[i].value);
    }
}

bool CrossesZero(const ChartNumericAxis& axis)
{
    return axis.kind == ScaleKind::Linear && axis.min < 0.0 && axis.max > 0.0;
}

} // namespace

void DrawChartGridX(ImDrawList* drawList, const ImRect& plot, const ChartNumericAxis& axis, const Scale& scale,
                    bool zeroLine)
{
    const Colors& c = T().colors;
    if (axis.grid)
        ForLabelledTicks(axis, [&](double value) {
            const float x = std::floor(scale.ToPixel(value));
            if (x >= plot.Min.x && x <= plot.Max.x)
                HairlineV(drawList, x, plot.Min.y, plot.Max.y, ToU32(c.separator));
        });
    if (zeroLine && CrossesZero(axis))
        HairlineV(drawList, std::floor(scale.ToPixel(0.0)), plot.Min.y, plot.Max.y, ToU32(c.separatorOpaque));
}

void DrawChartGridY(ImDrawList* drawList, const ImRect& plot, const ChartNumericAxis& axis, const Scale& scale,
                    bool zeroLine)
{
    const Colors& c = T().colors;
    if (axis.grid)
        ForLabelledTicks(axis, [&](double value) {
            const float y = std::floor(scale.ToPixel(value));
            if (y >= plot.Min.y && y <= plot.Max.y)
                HairlineH(drawList, plot.Min.x, plot.Max.x, y, ToU32(c.separator));
        });
    if (zeroLine && CrossesZero(axis))
        HairlineH(drawList, plot.Min.x, plot.Max.x, std::floor(scale.ToPixel(0.0)), ToU32(c.separatorOpaque));
}

void DrawChartLabelsY(ImDrawList* drawList, const ChartAxis& options, const ChartNumericAxis& axis,
                      const Scale& scale, float right, bool disabled)
{
    if (!axis.labels)
        return;
    const ImU32 color = ToU32(ChartTextColor(disabled));
    char text[64];
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    ForLabelledTicks(axis, [&](double value) {
        ChartAxisText(options, value, axis.set, text, sizeof(text));
        const float w = TextWidth(text);
        const float y = std::round(scale.ToPixel(value) - axis.labelHeight * 0.5f);
        drawList->AddText(font, size, ImVec2(std::round(right - w), y), color, text);
    });
}

void DrawChartLabelsX(ImDrawList* drawList, const ChartAxis& options, const ChartNumericAxis& axis,
                      const Scale& scale, float top, float clipMin, float clipMax, bool disabled)
{
    if (!axis.labels)
        return;
    const ImU32 color = ToU32(ChartTextColor(disabled));
    char text[64];
    ImFont* font = ImGui::GetFont();
    const float size = ImGui::GetFontSize();
    ForLabelledTicks(axis, [&](double value) {
        ChartAxisText(options, value, axis.set, text, sizeof(text));
        const float w = TextWidth(text);
        const float left = std::max(clipMin, std::min(scale.ToPixel(value) - w * 0.5f, clipMax - w));
        drawList->AddText(font, size, ImVec2(std::round(left), std::round(top)), color, text);
    });
}

ChartBands LayoutChartBands(const ImRect& bounds, bool yTitle, bool xLabels, bool xTitle, float legendHeight)
{
    const Theme& t = T();
    const float gap = std::round(Px(t.spacing.xs));
    float tickHeight = 0.0f;
    {
        ControlTextScope text(kChartTickStyle);
        tickHeight = ImGui::GetFontSize();
    }
    float titleHeight = 0.0f;
    {
        ControlTextScope text(kChartTextStyle);
        titleHeight = ImGui::GetFontSize();
    }
    ChartBands bands;
    float top = bounds.Min.y;
    bands.yTitleTop = top;
    if (yTitle)
        top += titleHeight + gap;
    bands.plotTop = std::round(top + tickHeight * 0.5f);
    float bottom = bounds.Max.y;
    if (legendHeight > 0.0f)
    {
        bottom -= legendHeight;
        bands.legendTop = bottom;
        bottom -= std::round(Px(t.spacing.sm));
    }
    if (xTitle)
    {
        bottom -= titleHeight;
        bands.xTitleTop = bottom;
        bottom -= gap;
    }
    if (xLabels)
    {
        bottom -= tickHeight;
        bands.xLabelsTop = bottom;
        bottom -= gap;
    }
    else
    {
        // The plot keeps half a label line at the bottom too, so the lowest y label is not cut.
        bottom -= std::round(tickHeight * 0.5f);
    }
    bands.plotBottom = std::max(bands.plotTop, std::round(bottom));
    return bands;
}

void DrawChartAxisTitle(ImDrawList* drawList, const char* title, float left, float right, float top, bool center,
                        bool disabled)
{
    if (title == nullptr || title[0] == '\0' || right <= left)
        return;
    ControlTextScope text(kChartTextStyle);
    const FittedText shown = FitText(title, nullptr, right - left);
    const float x = center ? std::round((left + right - shown.width) * 0.5f) : left;
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(x, top), ToU32(ChartTextColor(disabled)), shown.text);
}

// ---- Readout ----------------------------------------------------------------------------------

void DrawChartReadout(ImDrawList* drawList, const ImRect& area, ImVec2 anchor, const char* title,
                      std::span<const ChartReadoutLine> lines, int more)
{
    const Theme& t = T();
    const Colors& c = t.colors;
    if (area.GetWidth() < 1.0f || area.GetHeight() < 1.0f)
        return;
    ControlTextScope text(kChartTextStyle);
    const float lineHeight = ImGui::GetFontSize();
    const float padX = std::round(Px(t.spacing.sm));
    const float padY = std::round(Px(t.spacing.xs));
    const float marker = Px(t.charts.markerSize);
    const float gap = std::round(Px(t.spacing.xs));
    const float valueGap = std::round(Px(t.spacing.md));
    const float margin = std::round(Px(t.spacing.sm));
    const float maxWidth = std::max(1.0f, area.GetWidth() - 2.0f * margin);

    char moreText[32] = {};
    if (more > 0)
    {
        const TextToken tokens[] = {{"count", more}};
        FormatTokens(moreText, sizeof(moreText), GetStrings().moreCount, tokens);
    }

    // Measure: the widest row decides the box, cut to the room the area has.
    float labelWidth = 0.0f;
    float valueWidth = 0.0f;
    for (const ChartReadoutLine& line : lines)
    {
        labelWidth = std::max(labelWidth, ImGui::CalcTextSize(line.label.text, line.label.end).x);
        valueWidth = std::max(valueWidth, TextWidth(line.value));
    }
    const float titleWidth = title != nullptr ? TextWidth(title) : 0.0f;
    const float rowLead = lines.empty() ? 0.0f : marker + gap;
    float inner = std::max({titleWidth, rowLead + labelWidth + (valueWidth > 0.0f ? valueGap + valueWidth : 0.0f),
                            TextWidth(moreText)});
    inner = std::min(inner, maxWidth - 2.0f * padX);
    const int rows = (title != nullptr ? 1 : 0) + static_cast<int>(lines.size()) + (more > 0 ? 1 : 0);
    const ImVec2 size(std::round(inner + 2.0f * padX), std::round(static_cast<float>(rows) * lineHeight + 2.0f * padY));

    // Beside the anchor: to the right, else to the left; at the top of the area.
    float x = anchor.x + margin;
    if (x + size.x > area.Max.x - margin)
        x = anchor.x - margin - size.x;
    x = std::clamp(x, area.Min.x + margin, std::max(area.Min.x + margin, area.Max.x - margin - size.x));
    const float y = std::round(area.Min.y + margin);
    const ImRect box(ImVec2(std::round(x), y), ImVec2(std::round(x) + size.x, y + size.y));

    drawList->PushClipRect(area.Min, area.Max, true);
    const float radius = Px(t.radii.element);
    FillRect(drawList, box.Min, box.Max, ToU32(c.ElevatedSurface(1)), radius);
    // A floating surface: its elevated grey and the separator hairline.
    DrawSurfaceOutline(drawList, box, radius);
    ImFont* font = ImGui::GetFont();
    const float fontSize = ImGui::GetFontSize();
    float rowTop = box.Min.y + padY;
    const float left = box.Min.x + padX;
    const float right = box.Max.x - padX;
    if (title != nullptr)
    {
        const FittedText shown = FitText(title, nullptr, right - left);
        drawList->AddText(font, fontSize, ImVec2(left, rowTop), ToU32(c.labelSecondary), shown.text);
        rowTop += lineHeight;
    }
    for (const ChartReadoutLine& line : lines)
    {
        const float center = rowTop + lineHeight * 0.5f;
        DrawChartMarker(drawList, ImVec2(left + marker * 0.5f, center), marker, line.marker, ToU32(line.color), false);
        const float valueW = TextWidth(line.value);
        drawList->AddText(font, fontSize, ImVec2(right - valueW, rowTop), ToU32(c.label), line.value);
        const float labelRoom = right - valueW - (valueW > 0.0f ? valueGap : 0.0f) - (left + rowLead);
        const FittedText shown = FitText(line.label.text, line.label.end, std::max(0.0f, labelRoom));
        drawList->AddText(font, fontSize, ImVec2(left + rowLead, rowTop), ToU32(c.label), shown.text);
        rowTop += lineHeight;
    }
    if (more > 0)
        drawList->AddText(font, fontSize, ImVec2(left, rowTop), ToU32(c.labelSecondary), moreText);
    drawList->PopClipRect();
}

int ChartReadoutIndex(ImGuiID chartId)
{
    const void* state = GlobalStateStore().Find(chartId, StateTypeOf<ChartReadoutState>());
    return state != nullptr ? static_cast<const ChartReadoutState*>(state)->index : -1;
}

bool ChartPointerReadout(bool hovered, const ImRect& area, ChartReadoutState& state)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (!hovered || !area.Contains(io.MousePos))
        return false;
    if (state.keyboard && io.MouseDelta.x == 0.0f && io.MouseDelta.y == 0.0f)
        return false;
    state.keyboard = false;
    return true;
}

bool ChartReadoutKeys(ImGuiID id, bool focused, int count, bool vertical, ChartReadoutState& state)
{
    if (!focused || count <= 0 || id == 0)
        return false;
    const ImGuiKey keys[] = {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_Home, ImGuiKey_End,
                             ImGuiKey_UpArrow, ImGuiKey_DownArrow};
    for (int k = 0; k < (vertical ? 6 : 4); ++k)
        ImGui::SetKeyOwner(keys[k], id);
    const int current = std::clamp(state.index, -1, count - 1);
    int index = current;
    const ImGuiInputFlags repeat = ImGuiInputFlags_Repeat;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, repeat, id) || (vertical && ImGui::IsKeyPressed(ImGuiKey_UpArrow, repeat, id)))
        index = current < 0 ? 0 : std::max(0, current - 1);
    else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, repeat, id) ||
             (vertical && ImGui::IsKeyPressed(ImGuiKey_DownArrow, repeat, id)))
        index = current < 0 ? 0 : std::min(count - 1, current + 1);
    else if (ImGui::IsKeyPressed(ImGuiKey_Home, ImGuiInputFlags_None, id))
        index = 0;
    else if (ImGui::IsKeyPressed(ImGuiKey_End, ImGuiInputFlags_None, id))
        index = count - 1;
    else
        return false;
    state.index = index;
    state.keyboard = true;
    return true;
}

// ---- Legend entries -----------------------------------------------------------------------------

ChartLegendMetrics GetChartLegendMetrics()
{
    const Theme& t = T();
    ChartLegendMetrics m;
    m.height = ControlHeight(ControlSize::Small);
    m.padding = std::round(Px(t.spacing.xs));
    m.swatch = std::round(Px(t.charts.legendSwatch));
    m.gap = std::round(Px(t.spacing.sm));
    m.spacing = std::round(Px(t.spacing.sm));
    m.radius = Px(t.radii.element);
    return m;
}

float ChartLegendEntryWidth(const ChartLabelText& label, const ChartLegendMetrics& m, float maxWidth)
{
    const float textWidth = label.Empty() ? 0.0f : ImGui::CalcTextSize(label.text, label.end).x;
    const float width = m.padding * 2.0f + m.swatch + (textWidth > 0.0f ? m.gap + textWidth : 0.0f);
    return std::max(1.0f, std::min(std::ceil(width), maxWidth));
}

FittedText DrawChartLegendEntry(ImDrawList* drawList, const ImRect& rect, const ChartLabelText& label,
                                const ChartLegendLook& look, const ChartLegendMetrics& m)
{
    const Colors& c = T().colors;
    const ImU32 color = ToU32(look.disabled ? c.labelTertiary : look.color);
    const float centerY = rect.GetCenter().y;
    const float swatchLeft = rect.Min.x + m.padding;
    const ImVec2 swatchCenter(swatchLeft + m.swatch * 0.5f, centerY);
    if (look.useMarker)
    {
        DrawChartMarker(drawList, swatchCenter, m.swatch, look.marker, color, look.hidden);
    }
    else
    {
        const ImVec2 a(std::round(swatchLeft), std::round(centerY - m.swatch * 0.5f));
        const ImVec2 b(a.x + m.swatch, a.y + m.swatch);
        const float radius = std::min(m.radius, m.swatch * kChartSwatchRoundness);
        if (look.hidden)
        {
            const float stroke = std::max(HairlineThickness(), std::round(Px(T().charts.lineWidth) * kChartHollowShare));
            StrokeRect(drawList, a, b, color, radius, stroke);
        }
        else
        {
            FillRect(drawList, a, b, color, radius);
        }
    }
    FittedText shown;
    if (label.Empty())
        return shown;
    const float textLeft = swatchLeft + m.swatch + m.gap;
    shown = FitText(label.text, label.end, std::max(0.0f, rect.Max.x - m.padding - textLeft));
    const ImVec4 textColor = look.disabled ? c.labelTertiary : (look.hidden ? c.labelSecondary : c.label);
    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(textLeft, CenteredTextY(rect.Min.y, rect.GetHeight())),
                      ToU32(textColor), shown.text);
    return shown;
}

namespace
{

// Flows the entries of a passive legend into rows; `place(i, rect, label, look)` for each one.
template <class Place>
float FlowChartLegend(ImVec2 origin, float width, bool column, ChartLegendSource source, const void* data, int count,
                      Place place)
{
    const ChartLegendMetrics m = GetChartLegendMetrics();
    float x = 0.0f;
    float y = 0.0f;
    bool any = false;
    for (int i = 0; i < count; ++i)
    {
        ChartLabelText label;
        ChartLegendLook look;
        if (!source(data, i, &label, &look))
            continue;
        const float entry = ChartLegendEntryWidth(label, m, width);
        if (any && (column || x + entry > width))
        {
            x = 0.0f;
            y += m.height;
        }
        place(ImRect(ImVec2(origin.x + x, origin.y + y), ImVec2(origin.x + x + entry, origin.y + y + m.height)), label,
              look, m);
        x += entry + m.spacing;
        any = true;
    }
    return any ? y + m.height : 0.0f;
}

} // namespace

float MeasureChartLegend(ChartLegendSource source, const void* data, int count, float width, bool column)
{
    if (source == nullptr || count <= 0)
        return 0.0f;
    ControlTextScope text(kChartTextStyle);
    return FlowChartLegend(ImVec2(0.0f, 0.0f), width, column, source, data, count,
                           [](const ImRect&, const ChartLabelText&, const ChartLegendLook&, const ChartLegendMetrics&) {});
}

void DrawChartLegend(ImDrawList* drawList, ImVec2 origin, float width, ChartLegendSource source, const void* data,
                     int count, bool column)
{
    if (source == nullptr || count <= 0)
        return;
    ControlTextScope text(kChartTextStyle);
    FlowChartLegend(origin, width, column, source, data, count,
                    [&](const ImRect& rect, const ChartLabelText& label, const ChartLegendLook& look,
                        const ChartLegendMetrics& m) { DrawChartLegendEntry(drawList, rect, label, look, m); });
}

float ChartLegendColumnWidth(ChartLegendSource source, const void* data, int count, float maxWidth)
{
    if (source == nullptr || count <= 0)
        return 0.0f;
    ControlTextScope text(kChartTextStyle);
    const ChartLegendMetrics m = GetChartLegendMetrics();
    float widest = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        ChartLabelText label;
        ChartLegendLook look;
        if (source(data, i, &label, &look))
            widest = std::max(widest, ChartLegendEntryWidth(label, m, maxWidth));
    }
    return widest;
}

// ---- Data states ------------------------------------------------------------------------------

bool DrawChartDataState(ImGuiID id, const ImRect& bounds, DataState state, bool empty, const char* emptyText,
                        const char* errorText)
{
    if (state == DataState::Ready && !empty)
        return false;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const Theme& t = T();
    if (!ImGui::IsRectVisible(bounds.Min, bounds.Max))
        return true;
    if (state == DataState::Loading)
    {
        DrawSkeleton(window->DrawList, bounds.Min, bounds.Max, Px(t.radii.element));
        return true;
    }
    const Strings& strings = GetStrings();
    const bool error = state == DataState::Error;
    const char* text = error ? (errorText != nullptr ? errorText : strings.loadFailed)
                             : (emptyText != nullptr ? emptyText : strings.empty);
    // The empty state widget sizes itself in units; its items go under the chart id.
    const ImVec2 savedCursor = window->DC.CursorPos;
    const ImVec2 savedMax = window->DC.CursorMaxPos;
    const float savedLine = window->DC.CurrLineSize.y;
    const float savedPrevLine = window->DC.PrevLineSize.y;
    ImGui::PushOverrideID(id);
    ImGui::SetCursorScreenPos(bounds.Min);
    EmptyState(text, {.icon = error ? Icon("error") : Icon(),
                      .tone = error ? Tone::Destructive : Tone::Neutral,
                      .compact = true,
                      .width = std::max(1.0f, bounds.GetWidth()) / facet::Scale(),
                      .height = std::max(1.0f, bounds.GetHeight()) / facet::Scale()});
    ImGui::PopID();
    window->DC.CursorPos = savedCursor;
    window->DC.CursorMaxPos = savedMax;
    window->DC.CurrLineSize.y = savedLine;
    window->DC.PrevLineSize.y = savedPrevLine;
    return true;
}

} // namespace facet::internal
