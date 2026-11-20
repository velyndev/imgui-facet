// ProgressBar (facet/widgets/display.h): a linear progress track with the label line above it.
// Not interactive: one item that is not a Tab stop. It follows the order of the model widget
// (src/widgets/buttons/button.cpp): entry, measure, one item, draw from tokens.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/loading.h"

namespace facet
{
namespace
{

// Animation sub key of the smoothed fill under the widget id.
constexpr int kProgressAnimFill = 1;

// The share of the track the sweeping segment of an indeterminate bar covers.
constexpr float kIndeterminateShare = 0.3f;

// The track thickness of a control size.
float ProgressThickness(ControlSize size)
{
    const Metrics& m = T().metrics;
    switch (ResolveControlSize(size))
    {
    case ControlSize::Small:
        return PxSnapped(m.progressHeightSmall);
    case ControlSize::Large:
        return PxSnapped(m.progressHeightLarge);
    default:
        return PxSnapped(m.progressHeight);
    }
}

// The fill color of a tone. Every one reaches 3:1 on the content surfaces, so the value reads as
// a graphic: the accent through focusRing (the accent moved to 3:1), the status colors as they are.
ImVec4 ProgressToneColor(Tone tone)
{
    const Colors& c = T().colors;
    switch (tone)
    {
    case Tone::Neutral:
        return c.labelSecondary;
    case Tone::Success:
        return c.success;
    case Tone::Warning:
        return c.warning;
    case Tone::Destructive:
        return c.destructive;
    case Tone::Accent:
    default:
        return c.focusRing;
    }
}

} // namespace

void ProgressBar(const char* label, float fraction, const ProgressBarOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const bool indeterminate = fraction == kIndeterminate;
    // NaN and values outside 0 to 1 are drawn clamped (rule 14).
    const float value = std::isnan(fraction) ? 0.0f : std::clamp(fraction, 0.0f, 1.0f);

    // The label line holds the visible label part and, with showPercent, the percentage at its end.
    char percent[8] = {};
    if (options.showPercent && !indeterminate)
        ImFormatString(percent, sizeof(percent), "%d%%", static_cast<int>(std::round(value * 100.0f)));
    const internal::AboveLabel above = internal::MeasureAboveLabel(label, m.size);
    float lineHeight = above.height;
    if (lineHeight == 0.0f && percent[0] != '\0')
        lineHeight = internal::MeasureAboveLabel(percent, m.size).height;  // the line of the percentage alone

    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const float thickness = ProgressThickness(m.size);
    // Without a label line the track is centered in one control height, so it lines up with the
    // labels of a row; under a label line it follows the line directly.
    const float trackRow = lineHeight > 0.0f ? thickness : m.height;
    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + lineHeight + trackRow));

    // One item, not interactive and not a Tab stop; the id keys the smoothed fill.
    const ImGuiID id = window->GetID(label);
    ImGui::ItemSize(bounds.GetSize());
    internal::ApplyNextItemData(next);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav))
        return;

    const Theme& t = T();
    const Colors& c = t.colors;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    ImDrawList* drawList = window->DrawList;
    if (lineHeight > 0.0f)
        internal::DrawAboveLabel(drawList, above, origin, width, m.size, disabled, percent);

    const float trackTop = std::round(bounds.Min.y + lineHeight + (trackRow - thickness) * 0.5f);
    const ImRect track(bounds.Min.x, trackTop, bounds.Max.x, trackTop + thickness);
    const float radius = std::min(Px(t.radii.element), thickness * 0.5f);
    FillRect(drawList, track.Min, track.Max, ToU32(disabled ? c.fill[3] : c.fill[0]), radius);
    const ImVec4 tone = disabled ? c.labelTertiary : ProgressToneColor(options.tone);

    if (indeterminate)
    {
        const float phase = internal::ShimmerPhase();
        if (phase < 0.0f)
        {
            // Reduced motion: the whole track fades in and out in place.
            FillRect(drawList, track.Min, track.Max, ToU32(WithAlpha(tone, tone.w * internal::SpinnerOpacity())), radius);
            return;
        }
        // A segment enters at the start and leaves at the end, on the shared loading clock.
        const float segment = track.GetWidth() * kIndeterminateShare;
        const float x = track.Min.x - segment + (track.GetWidth() + segment) * phase;
        const float x0 = std::max(track.Min.x, x);
        const float x1 = std::min(track.Max.x, x + segment);
        if (x1 > x0)
            FillRect(drawList, ImVec2(x0, track.Min.y), ImVec2(x1, track.Max.y), ToU32(tone), radius);
        return;
    }

    if (options.buffer > 0.0f)
    {
        const float buffer = std::clamp(options.buffer, 0.0f, 1.0f);
        const float x = track.Min.x + track.GetWidth() * buffer;
        if (x > track.Min.x)
            FillRect(drawList, track.Min, ImVec2(x, track.Max.y), ToU32(disabled ? c.fill[2] : c.labelTertiary), radius);
    }
    // The fill eases toward a new value over the standard duration (at once with reduced motion).
    const float shown = Tween(id, kProgressAnimFill, value, t.motion.durationStandard);
    const float x = track.Min.x + track.GetWidth() * std::clamp(shown, 0.0f, 1.0f);
    if (x > track.Min.x)
        FillRect(drawList, track.Min, ImVec2(std::max(x, track.Min.x + std::min(thickness, track.GetWidth())), track.Max.y),
                 ToU32(tone), radius);
}

} // namespace facet
