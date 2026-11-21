// FrameTimeGraph (facet/widgets/charts.h): a rolling graph of frame times, newest at the right,
// one bar per frame, with the frames per second, the minimum, average and maximum frame time of
// the frames shown, the target line, and frames over the target in the warning token (they also
// reach past the target line, so color is not the only cue). The first overload draws the frame
// times the charts subsystem records in every facet::NewFrame (charts_frame.cpp); the second the
// caller's. Not interactive: one item, no Tab stop. It reads the samples in place and allocates
// nothing.

#include "facet/widgets/charts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/chart_core.h"
#include "internal/control.h"
#include "widgets/charts/chart_parts.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

using internal::SeriesView;

// The target when the options leave it at 0: 60 frames per second.
constexpr float kFrameDefaultTarget = 1.0f / 60.0f;
// The graph's top is at least this many targets, and the slowest frame shown with this headroom.
constexpr float kFrameTopTargets = 2.0f;
constexpr float kFrameHeadroom = 1.1f;
// Unit and statistic names of the readout. They are not in the Strings table yet.
constexpr const char* kFrameRateUnit = "fps";
constexpr const char* kFrameTimeUnit = "ms";
constexpr const char* kFrameMin = "min";
constexpr const char* kFrameAverage = "avg";
constexpr const char* kFrameMax = "max";

struct FrameStats
{
    int first = 0;     // the oldest sample shown
    int shown = 0;     // samples shown
    float min = 0.0f;  // seconds
    float max = 0.0f;
    float average = 0.0f;
};

FrameStats MeasureFrames(const SeriesView& view, int capacity)
{
    FrameStats stats;
    stats.shown = std::clamp(capacity, 0, view.count);
    stats.first = view.count - stats.shown;
    double sum = 0.0;
    int used = 0;
    for (int i = stats.first; i < view.count; ++i)
    {
        const double value = view.YAt(i);
        if (!std::isfinite(value) || value < 0.0)
            continue;
        const float v = static_cast<float>(value);
        stats.min = used == 0 ? v : std::min(stats.min, v);
        stats.max = used == 0 ? v : std::max(stats.max, v);
        sum += value;
        ++used;
    }
    stats.average = used > 0 ? static_cast<float>(sum / used) : 0.0f;
    return stats;
}

void FrameTimeGraphImpl(const char* id, const SeriesView& view, const FrameTimeGraphOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const float width = internal::ResolveItemWidth(o.width, next, kFill);
    const float natural = o.compact ? ControlHeight(ControlSize::Inherit) * 2.0f : Px(t.metrics.chartHeight);
    const float height = std::max(1.0f, std::round(o.height > 0.0f ? Px(o.height) : natural));
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + height));
    const ImGuiID itemId = window->GetID(id);
    ImGui::ItemSize(bounds.GetSize());
    internal::ApplyNextItemData(next);
    if (!ImGui::ItemAdd(bounds, itemId, nullptr, ImGuiItemFlags_NoNav))
        return;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    ImDrawList* drawList = window->DrawList;
    drawList->PushClipRect(bounds.Min, bounds.Max, true);

    const float gap = std::round(Px(t.spacing.sm));
    const float pitch = std::max(1.0f, std::round(Px(t.charts.barGap)));
    const float target = o.target > 0.0f && std::isfinite(o.target) ? o.target : kFrameDefaultTarget;
    const ImU32 primary = ToU32(disabled ? c.labelTertiary : c.label);
    const ImU32 secondary = ToU32(disabled ? c.labelTertiary : c.labelSecondary);

    // The frames per second: Headline, at the start (compact: beside the graph, else above it).
    char rate[32] = {};
    char times[96] = {};
    float rateWidth = 0.0f;
    float headline = 0.0f;
    {
        internal::ControlTextScope text(TextStyle::Headline);
        headline = ImGui::GetFontSize();
    }
    ImRect graph;
    if (o.compact)
    {
        internal::ControlTextScope text(TextStyle::Headline);
        // Room for a three-digit rate, so the graph does not move as the rate changes.
        char widest[32];
        std::snprintf(widest, sizeof(widest), "000 %s", kFrameRateUnit);
        rateWidth = std::min(ImGui::CalcTextSize(widest).x, width * 0.5f);
        graph = ImRect(bounds.Min.x + rateWidth + gap, bounds.Min.y, bounds.Max.x, bounds.Max.y);
    }
    else
    {
        graph = ImRect(bounds.Min.x, bounds.Min.y + headline + gap, bounds.Max.x, bounds.Max.y);
    }
    graph = internal::SnapPlotRect(graph);
    const int capacity = static_cast<int>(std::floor(graph.GetWidth() / pitch));
    const FrameStats stats = MeasureFrames(view, capacity);

    if (stats.average > 0.0f)
    {
        std::snprintf(rate, sizeof(rate), "%.0f %s", 1.0 / stats.average, kFrameRateUnit);
        std::snprintf(times, sizeof(times), "%s %.1f   %s %.1f   %s %.1f %s", kFrameMin, stats.min * 1000.0f, kFrameAverage,
                      stats.average * 1000.0f, kFrameMax, stats.max * 1000.0f, kFrameTimeUnit);
    }
    {
        internal::ControlTextScope text(TextStyle::Headline);
        const float room = o.compact ? rateWidth : width;
        const internal::FittedText shown = internal::FitText(rate, nullptr, room);
        const float y = o.compact ? CenteredTextY(bounds.Min.y, bounds.GetHeight()) : bounds.Min.y;
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(bounds.Min.x, y), primary, shown.text);
        rateWidth = std::max(rateWidth, shown.width);
    }
    if (!o.compact && times[0] != '\0')
    {
        internal::ControlTextScope text(internal::kChartTextStyle);
        const float room = std::max(0.0f, width - rateWidth - gap);
        const internal::FittedText shown = internal::FitText(times, nullptr, room);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                          ImVec2(std::round(bounds.Max.x - shown.width), std::round(bounds.Min.y + (headline - ImGui::GetFontSize()) * 0.5f)),
                          secondary, shown.text);
    }

    // The bars, newest at the right edge, over a baseline; the target line over them.
    if (graph.GetWidth() >= 1.0f && graph.GetHeight() >= 1.0f)
    {
        const float top = std::max(target * kFrameTopTargets, stats.max * kFrameHeadroom);
        const float scale = graph.GetHeight() / std::max(top, 1e-6f);
        const ImU32 normal = ToU32(disabled ? c.labelTertiary : internal::DisplayProgressColor(Tone::Accent));
        const ImU32 slow = ToU32(disabled ? c.labelTertiary : c.warning);
        const float barWidth = pitch > 2.0f ? pitch - 1.0f : pitch;
        for (int i = stats.first; i < view.count; ++i)
        {
            const double value = view.YAt(i);
            if (!std::isfinite(value) || value <= 0.0)
                continue;
            const float x = graph.Max.x - static_cast<float>(view.count - i) * pitch;
            const float h = std::min(graph.GetHeight(), static_cast<float>(value) * scale);
            if (h < 0.5f)
                continue;
            FillRect(drawList, ImVec2(x, std::round(graph.Max.y - h)), ImVec2(x + barWidth, graph.Max.y),
                     static_cast<float>(value) > target ? slow : normal);
        }
        HairlineH(drawList, graph.Min.x, graph.Max.x, graph.Max.y - HairlineThickness(), ToU32(c.separator));
        const float targetY = std::floor(graph.Max.y - target * scale);
        HairlineH(drawList, graph.Min.x, graph.Max.x, targetY, secondary);
    }
    drawList->PopClipRect();
}

} // namespace

void FrameTimeGraph(const char* id, const FrameTimeGraphOptions& options)
{
    FrameTimeGraphImpl(id, internal::ChartFrameTimes(), options);
}

void FrameTimeGraph(const char* id, std::span<const float> frameTimes, const FrameTimeGraphOptions& options)
{
    SeriesView view;
    view.y = frameTimes.data();
    view.count = static_cast<int>(std::min<std::size_t>(frameTimes.size(), 1u << 30));
    FrameTimeGraphImpl(id, view, options);
}

} // namespace facet
