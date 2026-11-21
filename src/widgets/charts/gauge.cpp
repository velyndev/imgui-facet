// Gauge (facet/widgets/charts.h): a radial gauge of a value between min and max: a track along
// three quarters of a circle (or the upper half), colored zones along its outer edge, the filled
// arc (or a needle) moving with the quick spring, the value in the middle and the visible label
// part under it. A value outside the range is drawn at the end of the track with a mark in the
// label color, and the readout shows the value as it is. Not interactive: one item, no Tab stop.

#include "facet/widgets/charts.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "widgets/charts/chart_parts.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

constexpr float kPi = std::numbers::pi_v<float>;
// Animation sub key of the shown fraction under the gauge id.
constexpr int kGaugeAnimValue = 1;
// The zone band is this share of the gauge width, at least two hairlines.
constexpr float kGaugeZoneShare = 0.34f;
// The readout and label take at most this share of the inner diameter.
constexpr float kGaugeTextShare = 0.8f;

// The tone color of the zone `value` falls in, or the accent progress color.
ImVec4 GaugeValueColor(std::span<const GaugeZone> zones, double value)
{
    for (const GaugeZone& zone : zones)
        if (value >= std::min(zone.from, zone.to) && value <= std::max(zone.from, zone.to))
            return internal::DisplayProgressColor(zone.tone);
    return internal::DisplayProgressColor(Tone::Accent);
}

} // namespace

void Gauge(const char* label, double value, double min, double max, const GaugeOptions& o)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    if (min > max)
        std::swap(min, max);
    const bool half = o.shape == GaugeShape::HalfCircle;

    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float diameter = std::max(1.0f, std::min(std::round(o.diameter > 0.0f ? Px(o.diameter) : Px(t.metrics.chartHeight)), available));
    const float radius = diameter * 0.5f;
    float labelHeight = 0.0f;
    {
        internal::ControlTextScope text(internal::kChartTextStyle);
        labelHeight = ImGui::GetFontSize();
    }
    const internal::ChartLabelText name = internal::ChartVisibleLabel(label);
    const float height = half ? std::round(radius + (name.Empty() ? 0.0f : labelHeight + Px(t.spacing.xs))) : diameter;
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + diameter, origin.y + std::max(1.0f, height)));
    const ImGuiID id = window->GetID(label);
    ImGui::ItemSize(bounds.GetSize());
    internal::ApplyNextItemData(next);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav))
        return;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    ImDrawList* drawList = window->DrawList;
    drawList->PushClipRect(bounds.Min, bounds.Max, true);

    const ImVec2 center(std::round(bounds.Min.x + radius), std::round(bounds.Min.y + radius));
    const float width = std::min(Px(t.charts.gaugeWidth), radius * 0.5f);
    const bool zones = !o.zones.empty() && max > min;
    const float zoneWidth = std::max(2.0f * HairlineThickness(), std::round(width * kGaugeZoneShare));
    const float zoneGap = zones ? zoneWidth + HairlineThickness() : 0.0f;
    const float track = std::max(1.0f, radius - zoneGap - width * 0.5f);
    const float sweep = half ? kPi : std::clamp(t.metrics.dialSweep, 1.0f, 360.0f) * kPi / 180.0f;
    const float a0 = half ? -kPi : -0.5f * kPi - 0.5f * sweep;
    const auto angleOf = [&](double fraction) { return a0 + sweep * static_cast<float>(std::clamp(fraction, 0.0, 1.0)); };
    const auto fractionOf = [&](double v) { return (v - min) / (max - min); };

    if (max > min)
    {
        StrokeArc(drawList, center, track, a0, a0 + sweep, ToU32(disabled ? c.fill[3] : c.fill[1]), width, true);
        for (const GaugeZone& zone : o.zones)
        {
            const double from = fractionOf(std::min(zone.from, zone.to));
            const double to = fractionOf(std::max(zone.from, zone.to));
            if (!(to > 0.0 && from < 1.0) || !(to > from))
                continue;
            StrokeArc(drawList, center, radius - zoneWidth * 0.5f, angleOf(from), angleOf(to),
                      ToU32(disabled ? c.labelTertiary : internal::DisplayProgressColor(zone.tone)), zoneWidth, false);
        }
        const double target = std::isnan(value) ? 0.0 : std::clamp(fractionOf(value), 0.0, 1.0);
        const float shown = std::clamp(Spring(id, kGaugeAnimValue, static_cast<float>(target), t.motion.quick), 0.0f, 1.0f);
        const ImVec4 fill = disabled ? c.labelTertiary : GaugeValueColor(o.zones, value);
        const float end = angleOf(shown);
        if (o.needle)
        {
            const ImVec2 tip(center.x + std::cos(end) * (track + width * 0.5f), center.y + std::sin(end) * (track + width * 0.5f));
            const ImU32 needle = ToU32(disabled ? c.labelTertiary : c.label);
            drawList->AddLine(center, tip, needle, Px(t.charts.lineWidth));
            drawList->AddCircleFilled(center, Px(t.charts.markerSize) * 0.5f, needle);
        }
        else if (shown > 0.0f && !std::isnan(value))
        {
            StrokeArc(drawList, center, track, a0, end, ToU32(fill), width, true);
        }
        // Past either end: a mark in the label color on the end of the track.
        if (!std::isnan(value) && (value > max || value < min))
        {
            const float at = value > max ? a0 + sweep : a0;
            const ImVec2 mark(center.x + std::cos(at) * track, center.y + std::sin(at) * track);
            internal::DrawChartMarker(drawList, mark, width * 0.5f, internal::ChartMarker::Circle,
                                      ToU32(disabled ? c.labelTertiary : c.label), false);
        }
    }

    // The value in the middle, the label under it.
    const float room = std::max(1.0f, (track - width * 0.5f) * 2.0f * kGaugeTextShare);
    char text[64];
    const ChartAxis format{.format = o.format != nullptr ? o.format : "%.0f", .formatter = o.formatter,
                           .formatterUser = o.formatterUser};
    internal::ChartAxisText(format, value, internal::TickSet{}, text, sizeof(text));
    float valueHeight = 0.0f;
    float valueTop = 0.0f;
    {
        internal::ControlTextScope style(TextStyle::Title2);
        valueHeight = ImGui::GetFontSize();
        const internal::FittedText shown = internal::FitText(text, nullptr, room);
        // A needle crosses the middle, so its readout sits below the hub.
        const float lower = o.needle && !half ? Px(t.charts.markerSize) + valueHeight * 0.5f : 0.0f;
        const float top = half ? center.y - valueHeight
                               : std::round(center.y + lower - (valueHeight + (name.Empty() ? 0.0f : labelHeight)) * 0.5f);
        valueTop = std::round(top);
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(std::round(center.x - shown.width * 0.5f), valueTop),
                          ToU32(disabled ? c.labelTertiary : c.label), shown.text);
    }
    bool cut = false;
    if (!name.Empty())
    {
        internal::ControlTextScope style(internal::kChartTextStyle);
        const internal::FittedText shown = internal::FitText(name.text, name.end, half ? diameter : room);
        cut = shown.truncated;
        const float top = half ? center.y + Px(t.spacing.xs) : valueTop + valueHeight;
        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(std::round(center.x - shown.width * 0.5f), std::round(top)),
                          ToU32(disabled ? c.labelTertiary : c.labelSecondary), shown.text);
    }
    drawList->PopClipRect();
    internal::CutTextTooltip(id, bounds, hovered, false, name.text, name.end, cut);
}

} // namespace facet
