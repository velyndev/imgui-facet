// StatTile (facet/widgets/display.h): a tile with the visible label part as caption, a large value
// that counts to a new value on the standard spring, an optional delta chip (tone, arrow and
// signed text) and a trend line along the bottom. Loading shows a skeleton in place of the value,
// Error the error icon with Strings::loadFailed. Not interactive: one item, not a Tab stop.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/loading.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

// Animation sub key of the counting value under the widget id.
constexpr int kTileAnimValue = 1;
// A value within this share of the target (or exactly zero apart) shows the target itself, so
// the spring's float precision never shows in the final text.
constexpr double kTileSettled = 1e-4;
// The skeleton bar in place of the value: a share of the value line's height and of the width.
constexpr float kTileSkeletonHeight = 0.6f;
constexpr float kTileSkeletonWidth = 0.4f;
// The trend line in hairlines: thicker than a divider, so it reads as data.
constexpr float kTileTrendStroke = 1.5f;

int FormatTileNumber(char* buffer, int size, double value, const char* format, FormatFn formatter, void* user,
                     const char* fallback)
{
    if (formatter != nullptr)
        return formatter(buffer, size, value, user);
    return ImFormatString(buffer, static_cast<std::size_t>(size), format != nullptr ? format : fallback, value);
}

} // namespace

void StatTile(const char* label, double value, const StatTileOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const float pad = Px(t.spacing.md);
    const float gapXs = Px(t.spacing.xs);
    const float captionHeight = MeasureText(TextStyle::Footnote, "").y;
    const float valueHeight = MeasureText(TextStyle::Title1, "").y;
    const bool hasTrend = options.trend.size() >= 2;
    const float trendHeight = hasTrend ? Px(t.spacing.xxl) : 0.0f;
    const float height = std::ceil(pad * 2.0f + captionHeight + gapXs + valueHeight + (hasTrend ? gapXs + trendHeight : 0.0f));

    const ImVec2 origin = window->DC.CursorPos;
    const ImRect bounds(origin, ImVec2(origin.x + width, origin.y + height));
    const ImGuiID id = window->GetID(label);
    ImGui::ItemSize(bounds.GetSize());
    internal::ApplyNextItemData(next);
    // The value counts every frame, also while clipped; it jumps while motion is reduced.
    const float spring = Spring(id, kTileAnimValue, std::isfinite(value) ? static_cast<float>(value) : 0.0f, t.motion.standard);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav))
        return;
    double shown = static_cast<double>(spring);
    if (std::fabs(shown - value) <= std::fabs(value) * kTileSettled || !std::isfinite(value))
        shown = value;

    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    ImDrawList* drawList = window->DrawList;
    // The tile: the grouped surface with a hairline, so it shows on bg and on bgSecondary alike.
    FillRect(drawList, bounds.Min, bounds.Max, ToU32(c.bgSecondary), Px(t.radii.card));
    StrokeRect(drawList, bounds.Min, bounds.Max, ToU32(c.separator), Px(t.radii.card), HairlineThickness());
    const float innerLeft = bounds.Min.x + pad;
    const float innerWidth = std::max(0.0f, width - pad * 2.0f);

    PushTextStyle(TextStyle::Footnote);
    const char* captionEnd = ImGui::FindRenderedTextEnd(label);
    const internal::FittedText caption = internal::FitText(label, captionEnd, innerWidth);
    const ImVec2 captionPos(innerLeft, bounds.Min.y + pad);
    drawList->AddText(captionPos, ToU32(internal::ControlSecondaryColor(disabled)), caption.text);
    PopTextStyle();
    const ImRect captionRect(captionPos, ImVec2(captionPos.x + caption.width, captionPos.y + captionHeight));

    const float valueTop = captionPos.y + captionHeight + gapXs;
    if (options.state == DataState::Loading)
    {
        const float bar = std::round(valueHeight * kTileSkeletonHeight);
        const float top = std::round(valueTop + (valueHeight - bar) * 0.5f);
        internal::DrawSkeleton(drawList, ImVec2(innerLeft, top), ImVec2(innerLeft + std::round(innerWidth * kTileSkeletonWidth), top + bar),
                               Px(t.radii.element));
    }
    else if (options.state == DataState::Error)
    {
        PushTextStyle(TextStyle::Body);
        const float textTop = CenteredTextY(valueTop, valueHeight);
        const float iconSize = std::round(ImGui::GetFontSize());
        const internal::DisplayToneColors tone = internal::DisplayToneLook(Tone::Destructive, disabled);
        internal::DrawControlIcon(drawList, Icon("error"), innerLeft, internal::DisplayCapsCenter(textTop), iconSize,
                                  ToU32(tone.glyph));
        const float x = innerLeft + iconSize + Px(t.spacing.sm);
        const internal::FittedText text = internal::FitText(GetStrings().loadFailed, nullptr, std::max(0.0f, innerLeft + innerWidth - x));
        drawList->AddText(ImVec2(x, textTop), ToU32(internal::ControlLabelColor(disabled)), text.text);
        PopTextStyle();
    }
    else
    {
        char number[64];
        FormatTileNumber(number, sizeof(number), shown, options.format, options.formatter, options.formatterUser, "%.0f");
        // The delta chip sits after the value on its baseline: the tone of the direction (good or
        // bad), an arrow for the direction and the signed text, so it never rests on color alone.
        char delta[48] = {};
        const bool hasDelta = !std::isnan(options.delta);
        float chipWidth = 0.0f;
        const internal::DisplayMarkMetrics chip = internal::DisplayMarkLook(ControlSize::Regular);
        PushTextStyle(chip.textStyle);
        const float arrowSize = std::round(ImGui::GetFontSize());
        if (hasDelta)
        {
            ImFormatString(delta, sizeof(delta), options.deltaFormat != nullptr ? options.deltaFormat : "%+.1f", options.delta);
            chipWidth = Px(t.spacing.sm) * 2.0f + arrowSize + Px(t.spacing.xs) + std::ceil(ImGui::CalcTextSize(delta).x);
        }
        PopTextStyle();
        PushTextStyle(TextStyle::Title1);
        const float valueRoom = hasDelta ? std::max(0.0f, innerWidth - chipWidth - Px(t.spacing.sm)) : innerWidth;
        const internal::FittedText text = internal::FitText(number, nullptr, valueRoom);
        drawList->AddText(ImVec2(innerLeft, valueTop), ToU32(internal::ControlLabelColor(disabled)), text.text);
        const float baseline = valueTop + std::round(internal::DisplayFontAscent());
        PopTextStyle();
        if (hasDelta && chipWidth <= innerWidth)
        {
            const bool up = options.delta > 0.0;
            const bool good = options.invertDelta ? options.delta < 0.0 : options.delta > 0.0;
            const Tone toneKind = options.delta == 0.0 ? Tone::Neutral : (good ? Tone::Success : Tone::Destructive);
            const internal::DisplayToneColors tone = internal::DisplayToneLook(toneKind, disabled);
            const float chipLeft = innerLeft + text.width + Px(t.spacing.sm);
            const float chipBaseline = internal::DisplayMarkBaseline(chip);
            const ImRect chipRect(ImVec2(chipLeft, baseline - chipBaseline), ImVec2(chipLeft + chipWidth, baseline - chipBaseline + chip.height));
            FillRect(drawList, chipRect.Min, chipRect.Max, ToU32(tone.wash), Px(t.radii.element));
            PushTextStyle(chip.textStyle);
            const float textTop = internal::DisplayTextTop(baseline);
            float x = chipRect.Min.x + Px(t.spacing.sm);
            const Icon arrow = options.delta == 0.0 ? Icon("minus") : Icon(up ? "arrow-up" : "arrow-down");
            internal::DrawControlIcon(drawList, arrow, x, internal::DisplayCapsCenter(textTop), arrowSize, ToU32(tone.glyph));
            x += arrowSize + Px(t.spacing.xs);
            drawList->AddText(ImVec2(x, textTop), ToU32(internal::ControlLabelColor(disabled)), delta);
            PopTextStyle();
        }
    }

    if (hasTrend)
    {
        // The trend: one line over the samples, scaled to their range, in the 3:1 accent.
        float low = options.trend[0];
        float high = options.trend[0];
        for (const float sample : options.trend)
        {
            if (std::isnan(sample))
                continue;
            low = std::isnan(low) ? sample : std::min(low, sample);
            high = std::isnan(high) ? sample : std::max(high, sample);
        }
        const float range = high - low;
        const float top = valueTop + valueHeight + gapXs;
        const float stroke = std::max(HairlineThickness(), std::round(Px(t.metrics.hairline) * kTileTrendStroke));
        const ImU32 color = ToU32(disabled ? c.labelTertiary : c.focusRing);
        const int count = static_cast<int>(options.trend.size());
        drawList->PathClear();
        for (int i = 0; i < count; ++i)
        {
            const float sample = std::isnan(options.trend[i]) ? low : options.trend[i];
            const float fy = range > 0.0f ? (sample - low) / range : 0.5f;
            const float x = innerLeft + innerWidth * static_cast<float>(i) / static_cast<float>(count - 1);
            const float y = top + trendHeight - stroke - (trendHeight - stroke * 2.0f) * fy;
            drawList->PathLineTo(ImVec2(x, y));
        }
        drawList->PathStroke(color, stroke);
    }

    const bool captionHovered = ImGui::IsItemHovered() && ImGui::IsMouseHoveringRect(captionRect.Min, captionRect.Max);
    internal::CutTextTooltip(id, captionRect, captionHovered, false, label, captionEnd, caption.truncated);
}

} // namespace facet
