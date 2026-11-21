// LevelMeter (facet/widgets/charts.h): one bar per channel (audio level, load). Each bar follows
// its value with exponential smoothing; the part of the fill inside a zone takes the zone's tone,
// so a warning or clipping band shows where it starts; a peak mark stays at the highest recent
// value for charts.peakHold, then falls back (at once while motion is reduced). The horizontal
// meter draws the visible label part in front, the vertical one above its columns. Not
// interactive: one item, no Tab stop. The peaks are kept under the id in fixed storage, so a meter
// allocates nothing per frame.

#include "facet/widgets/charts.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/context.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/state_storage.h"
#include "widgets/charts/chart_parts.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

// Channels whose smoothing and peak are kept; later channels show their value directly.
constexpr int kMeterChannels = 32;
// Zones a meter reads; later zones are ignored.
constexpr int kMeterZones = 16;
// After its hold, a peak falls the whole range in this many slow durations.
constexpr float kMeterFallDurations = 4.0f;
// A segment of the segmented look is at least this many pixels long.
constexpr float kMeterMinSegment = 2.0f;

struct LevelMeterState
{
    float peak[kMeterChannels] = {};
    float hold[kMeterChannels] = {};  // seconds the peak still stays
    bool started[kMeterChannels] = {};
};

// The zones as fractions of the range, in the order given.
struct MeterZones
{
    float from[kMeterZones] = {};
    float to[kMeterZones] = {};
    ImVec4 color[kMeterZones];
    int count = 0;

    // The tone of the zone `f` falls in, else `normal`.
    ImVec4 At(float f, const ImVec4& normal) const
    {
        for (int i = 0; i < count; ++i)
            if (f >= from[i] && f <= to[i])
                return color[i];
        return normal;
    }
};

// Draws the fill of one channel along `track` up to fraction `shown` (0 to 1), split at the zone
// edges, or as segments. `vertical`: from the bottom up; else from the left.
void DrawMeterFill(ImDrawList* drawList, const ImRect& track, bool vertical, float shown, const MeterZones& zones,
                   const ImVec4& normal, const ImVec4& rest, bool segments, float gap)
{
    const float length = vertical ? track.GetHeight() : track.GetWidth();
    const float cross = vertical ? track.GetWidth() : track.GetHeight();
    const auto rectOf = [&](float f0, float f1) {
        if (vertical)
            return ImRect(track.Min.x, std::round(track.Max.y - f1 * length), track.Max.x, std::round(track.Max.y - f0 * length));
        return ImRect(std::round(track.Min.x + f0 * length), track.Min.y, std::round(track.Min.x + f1 * length), track.Max.y);
    };
    if (segments)
    {
        const float cell = std::max(kMeterMinSegment, std::round(cross));
        const int cells = std::max(1, static_cast<int>(std::floor((length + gap) / (cell + gap))));
        const float pitch = (length + gap) / static_cast<float>(cells);
        for (int i = 0; i < cells; ++i)
        {
            const float f0 = static_cast<float>(i) * pitch / length;
            const float f1 = (static_cast<float>(i) * pitch + pitch - gap) / length;
            const ImRect r = rectOf(f0, std::min(f1, 1.0f));
            const bool lit = f0 < shown;
            FillRect(drawList, r.Min, r.Max, ToU32(lit ? zones.At((f0 + f1) * 0.5f, normal) : rest));
        }
        return;
    }
    FillRect(drawList, track.Min, track.Max, ToU32(rest));
    if (shown <= 0.0f)
        return;
    // Breakpoints: 0, every zone edge inside the fill, and the fill end.
    float stops[2 * kMeterZones + 2];
    int n = 0;
    stops[n++] = 0.0f;
    for (int i = 0; i < zones.count; ++i)
        for (const float edge : {zones.from[i], zones.to[i]})
            if (edge > 0.0f && edge < shown)
                stops[n++] = edge;
    stops[n++] = shown;
    std::sort(stops, stops + n);
    for (int i = 0; i + 1 < n; ++i)
    {
        if (stops[i + 1] <= stops[i])
            continue;
        const ImRect r = rectOf(stops[i], stops[i + 1]);
        FillRect(drawList, r.Min, r.Max, ToU32(zones.At((stops[i] + stops[i + 1]) * 0.5f, normal)));
    }
}

} // namespace

void LevelMeter(const char* label, std::span<const float> channels, double min, double max, const LevelMeterOptions& o)
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
    const bool vertical = o.orientation == Orientation::Vertical;
    const int count = static_cast<int>(std::min<std::size_t>(channels.size(), 1u << 16));
    const int shownChannels = std::max(count, 1);
    const ControlSize size = ResolveControlSize(ControlSize::Inherit);
    const float controlHeight = ControlHeight(size);

    float width = 0.0f;
    float height = 0.0f;
    internal::FrontLabel front;
    internal::AboveLabel above;
    if (vertical)
    {
        width = internal::ResolveItemWidth(o.width, next, controlHeight * static_cast<float>(shownChannels));
        above = internal::MeasureAboveLabel(label, size);
        height = std::round(above.height + (o.height > 0.0f ? Px(o.height) : Px(t.metrics.chartHeight)));
    }
    else
    {
        width = internal::ResolveItemWidth(o.width, next, kFill);
        front = internal::MeasureFrontLabel(label, size, width);
        height = std::round(o.height > 0.0f ? Px(o.height) : controlHeight);
    }
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + std::max(1.0f, height)));
    const ImGuiID id = window->GetID(label);
    ImGui::ItemSize(bounds.GetSize());
    internal::ApplyNextItemData(next);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav))
        return;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    ImDrawList* drawList = window->DrawList;
    drawList->PushClipRect(bounds.Min, bounds.Max, true);

    // The area the bars share, and the zones as fractions of the range.
    const ImRect area = vertical ? ImRect(bounds.Min.x, bounds.Min.y + above.height, bounds.Max.x, bounds.Max.y)
                                 : ImRect(bounds.Min.x + front.width, bounds.Min.y, bounds.Max.x, bounds.Max.y);
    MeterZones zones;
    const double span = max - min;
    if (span > 0.0)
        for (const GaugeZone& zone : o.zones)
        {
            if (zones.count >= kMeterZones)
                break;
            const float f0 = static_cast<float>(std::clamp((std::min(zone.from, zone.to) - min) / span, 0.0, 1.0));
            const float f1 = static_cast<float>(std::clamp((std::max(zone.from, zone.to) - min) / span, 0.0, 1.0));
            if (!(f1 > f0))
                continue;
            zones.from[zones.count] = f0;
            zones.to[zones.count] = f1;
            zones.color[zones.count] = disabled ? c.labelTertiary : internal::DisplayProgressColor(zone.tone);
            ++zones.count;
        }
    const ImVec4 normal = disabled ? c.labelTertiary : internal::DisplayProgressColor(Tone::Accent);
    const ImVec4 rest = disabled ? c.fill[3] : c.fill[1];
    const float gap = std::round(Px(t.charts.meterSegmentGap));
    const float across = vertical ? area.GetWidth() : area.GetHeight();
    const float thickness = std::max(1.0f, std::floor((across - gap * static_cast<float>(shownChannels - 1)) / static_cast<float>(shownChannels)));
    const float peakWidth = std::max(HairlineThickness(), std::round(Px(t.charts.lineWidth)));

    LevelMeterState& state = internal::GetState<LevelMeterState>(id);
    const float dt = std::max(0.0f, DeltaTime());
    const float motionScale = std::max(t.motion.scale, 0.01f);
    const float speed = 1.0f / (std::max(t.motion.durationFast, 0.001f) * motionScale);
    const float fall = dt / (std::max(t.motion.durationSlow, 0.001f) * kMeterFallDurations * motionScale);
    const bool reduced = ReduceMotion();
    for (int ch = 0; ch < shownChannels; ++ch)
    {
        const float offset = static_cast<float>(ch) * (thickness + gap);
        const ImRect track = vertical ? ImRect(area.Min.x + offset, area.Min.y, area.Min.x + offset + thickness, area.Max.y)
                                      : ImRect(area.Min.x, area.Min.y + offset, area.Max.x, area.Min.y + offset + thickness);
        const float raw = ch < count ? channels[static_cast<std::size_t>(ch)] : static_cast<float>(min);
        // NaN reads as the bottom of the range; out of range is drawn clamped.
        const float target = span > 0.0 && !std::isnan(raw) ? static_cast<float>(std::clamp((raw - min) / span, 0.0, 1.0)) : 0.0f;
        float shown = target;
        float peak = target;
        if (ch < kMeterChannels)
        {
            shown = std::clamp(Smooth(id, 1 + ch, target, speed), 0.0f, 1.0f);
            if (!state.started[ch] || shown >= state.peak[ch])
            {
                state.peak[ch] = shown;
                state.hold[ch] = t.charts.peakHold * motionScale;
                state.started[ch] = true;
            }
            else if (state.hold[ch] > 0.0f)
            {
                state.hold[ch] -= dt;
            }
            else
            {
                state.peak[ch] = reduced ? shown : std::max(shown, state.peak[ch] - fall);
            }
            peak = state.peak[ch];
        }
        DrawMeterFill(drawList, track, vertical, shown, zones, normal, rest, o.segments, gap);
        if (o.peakHold && peak > 0.0f)
        {
            const float length = vertical ? track.GetHeight() : track.GetWidth();
            const ImU32 mark = ToU32(disabled ? c.labelTertiary : c.label);
            if (vertical)
            {
                const float y = std::round(track.Max.y - peak * length);
                FillRect(drawList, ImVec2(track.Min.x, std::max(track.Min.y, y - peakWidth)), ImVec2(track.Max.x, std::max(track.Min.y + peakWidth, y)), mark);
            }
            else
            {
                const float x = std::round(track.Min.x + peak * length);
                FillRect(drawList, ImVec2(std::min(track.Max.x - peakWidth, x), track.Min.y), ImVec2(std::min(track.Max.x, x + peakWidth), track.Max.y), mark);
            }
        }
    }

    bool aboveCut = false;
    if (vertical && above.text != nullptr)
        aboveCut = internal::DrawAboveLabel(drawList, above, bounds.Min, width, size, disabled).truncated;
    if (!vertical && front.text != nullptr)
    {
        internal::ControlTextScope text(ControlTextStyle(size));
        internal::DrawFrontLabel(drawList, front, ImVec2(bounds.Min.x, CenteredTextY(bounds.Min.y, bounds.GetHeight())), size,
                                 disabled);
    }
    drawList->PopClipRect();
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    internal::CutTextTooltip(id, bounds, hovered, false, label, labelEnd,
                             vertical ? aboveCut : front.shown.truncated);
}

} // namespace facet
