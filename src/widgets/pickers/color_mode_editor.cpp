// ColorModeEditor and SampleAnimatedColor. The editor is made of public widgets: a SegmentedControl
// for the animation (Strings::animationStatic and the others), a ColorWell for the base color, a
// second ColorWell for Pulse and Gradient, a DragNumber for the period in seconds (Strings::period)
// for every animation but Static, and a live preview swatch drawn with SampleAnimatedColor. The
// first row carries the label in front; the second row starts under the first row's control and
// wraps its items onto further lines when the width is too small.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/inputs.h"
#include "facet/widgets/selection.h"
#include "internal/control.h"
#include "internal/item.h"
#include "widgets/pickers/color_model.h"

namespace facet
{
namespace
{

constexpr double kTwoPi = 6.283185307179586;
constexpr double kDegrees = 360.0;
// The shortest period the editor writes, in seconds: faster cycles flicker.
constexpr double kColorModeMinPeriod = 0.1;
// The period field: the slowest period a drag reaches, and the width of the field in control
// heights, enough for "10.00" and the label.
constexpr double kColorModeMaxPeriod = 60.0;
constexpr float kColorModePeriodInControls = 4.0f;
constexpr int kColorAnimationCount = 4;
// Parts of the editor, for forced capture states.
constexpr int kPartAnimation = 0;
constexpr int kPartColor = 1;
constexpr int kPartSecond = 2;
constexpr int kPartPeriod = 3;

double PositiveFraction(double value)
{
    return value - std::floor(value);
}

} // namespace

ImVec4 SampleAnimatedColor(const AnimatedColor& animated, double seconds)
{
    if (animated.animation == ColorAnimation::Static || ReduceMotion() || !(animated.period > 0.0f) ||
        !std::isfinite(seconds) || !std::isfinite(animated.period))
        return animated.color;
    const double phase = PositiveFraction(seconds / static_cast<double>(animated.period));
    switch (animated.animation)
    {
    case ColorAnimation::Pulse:
    {
        // A breath: eased at both ends, so the color rests a moment at each extreme.
        const double t = 0.5 - 0.5 * std::cos(kTwoPi * phase);
        return Mix(animated.color, animated.second, static_cast<float>(t));
    }
    case ColorAnimation::Gradient:
    {
        // An even sweep there and back.
        const double t = 1.0 - std::fabs(2.0 * phase - 1.0);
        return Mix(animated.color, animated.second, static_cast<float>(t));
    }
    case ColorAnimation::Cycle:
    {
        // Through the hue circle in OKLCH, so the lightness and chroma of the base color stay.
        Oklch lch = ToOklch(animated.color);
        lch.h = static_cast<float>(PositiveFraction((lch.h + kDegrees * phase) / kDegrees) * kDegrees);
        return FromOklch(lch, animated.color.w);
    }
    case ColorAnimation::Static:
        break;
    }
    return animated.color;
}

bool ColorModeEditor(const char* label, AnimatedColor* animated, const ColorModeEditorOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
    const ImGuiID id = window->GetID(label);
    const bool readOnly = options.readOnly || animated == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    AnimatedColor edited = animated != nullptr ? *animated : AnimatedColor{ColorAnimation::Static, t.colors.accent,
                                                                           t.colors.accent, 2.0f};
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    internal::FrontLabel front;
    {
        internal::ControlTextScope text(m);
        front = internal::MeasureFrontLabel(label, m.size, width);
    }
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const float left = origin.x + std::round(front.width);
    const float right = origin.x + width;
    const float gap = std::round(Px(t.spacing.sm));
    bool written = false;
    const auto forceFor = [&](int part) {
        const int target = forced.part < 0 ? kPartAnimation : forced.part;
        if (forced.state != internal::ForcedItemState::None && target == part)
            internal::ForceNextItemState(forced.state);
    };

    ImGui::BeginGroup();
    ImGui::PushOverrideID(id);

    // Row 1: the animation.
    const Strings& s = GetStrings();
    const char* names[kColorAnimationCount] = {s.animationStatic, s.animationPulse, s.animationCycle, s.animationGradient};
    int animation = std::clamp(static_cast<int>(edited.animation), 0, kColorAnimationCount - 1);
    ImGui::SetCursorScreenPos(ImVec2(left, origin.y));
    SegmentedControlOptions segments;
    segments.readOnly = readOnly;
    segments.mixed = options.mixed && !written;
    segments.width = std::max(1.0f, right - left) / Px(1.0f);
    internal::ApplyNextItemData(next);
    forceFor(kPartAnimation);
    if (SegmentedControl("##animation", &animation, std::span<const char* const>(names, kColorAnimationCount), segments) &&
        !readOnly)
    {
        edited.animation = static_cast<ColorAnimation>(animation);
        written = true;
    }
    const float rowOneBottom = ImGui::GetItemRectMax().y;

    // Row 2: the colors, the period and the preview, wrapping when they do not fit.
    float x = left;
    float y = rowOneBottom + gap;
    float lineBottom = y;
    const auto place = [&](float itemWidth) {
        if (x > left && x + itemWidth > right)
        {
            x = left;
            y = lineBottom + gap;
        }
        ImGui::SetCursorScreenPos(ImVec2(x, y));
    };
    const auto advance = [&]() {
        x = ImGui::GetItemRectMax().x + gap;
        lineBottom = std::max(lineBottom, ImGui::GetItemRectMax().y);
    };
    ColorWellOptions well;
    well.alpha = options.alpha;
    well.readOnly = readOnly;
    well.mixed = options.mixed && !written;
    const float wellWidth = std::round(m.height * 1.5f);
    place(wellWidth);
    forceFor(kPartColor);
    if (ColorWell("##color", &edited.color, well) && !readOnly)
        written = true;
    advance();
    const bool twoColors = edited.animation == ColorAnimation::Pulse || edited.animation == ColorAnimation::Gradient;
    if (twoColors)
    {
        place(wellWidth);
        forceFor(kPartSecond);
        if (ColorWell("##second", &edited.second, well) && !readOnly)
            written = true;
        advance();
    }
    if (edited.animation != ColorAnimation::Static)
    {
        const float fieldWidth = std::min(std::round(m.height * kColorModePeriodInControls + ImGui::CalcTextSize(s.period).x),
                                          std::max(1.0f, right - left));
        place(fieldWidth);
        ImGui::SetNextItemWidth(fieldWidth);
        DragNumberOptions period;
        period.format = "%.2f";
        period.min = kColorModeMinPeriod;
        period.max = kColorModeMaxPeriod;
        period.readOnly = readOnly;
        period.mixed = options.mixed && !written;
        float value = edited.period;
        char periodLabel[64];
        std::snprintf(periodLabel, sizeof(periodLabel), "%s##period", s.period);
        forceFor(kPartPeriod);
        if (DragNumber(periodLabel, &value, period) && !readOnly)
        {
            edited.period = std::max(static_cast<float>(kColorModeMinPeriod), value);
            written = true;
        }
        advance();
    }
    // The preview: the color as SampleAnimatedColor gives it now, on the checkerboard when translucent.
    const float previewSide = m.height;
    place(previewSide);
    const ImVec2 previewAt = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(previewSide, previewSide));
    {
        const ImRect preview(previewAt, ImVec2(previewAt.x + previewSide, previewAt.y + previewSide));
        ImDrawList* drawList = window->DrawList;
        const float radius = Px(t.radii.element);
        ImVec4 now = SampleAnimatedColor(edited, ImGui::GetTime());
        if (!options.alpha)
            now.w = 1.0f;
        if (disabled)
            FillRect(drawList, preview.Min, preview.Max,
                     ToU32(WithAlpha(internal::ColorOnChecker(now), internal::kColorDisabledOpacity)), radius);
        else
            internal::DrawColorSwatch(drawList, preview, now, radius, false);
        StrokeRect(drawList, preview.Min, preview.Max, ToU32(t.colors.separator), radius, HairlineThickness());
    }
    advance();
    ImGui::PopID();

    if (front.text != nullptr)
    {
        internal::ControlTextScope text(m);
        internal::DrawFrontLabel(window->DrawList, front, ImVec2(origin.x, CenteredTextY(origin.y, m.height)), m.size,
                                 disabled);
    }
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const ImRect labelBox(origin, ImVec2(left, origin.y + m.height));
    internal::CutTextTooltip(id, labelBox, ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(labelBox.Min, labelBox.Max),
                             false, label, labelEnd, front.shown.truncated);
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(width, lineBottom - origin.y));
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    if (message.IsSet())
        internal::DrawControlMessage(message, ImVec2(left, lineBottom), right - left, disabled, id,
                                     ImRect(origin, ImVec2(right, lineBottom)));
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    if (written && animated != nullptr)
    {
        if (!options.alpha)
        {
            edited.color.w = animated->color.w;
            edited.second.w = animated->second.w;
        }
        *animated = edited;
    }
    return written && !readOnly;
}

} // namespace facet
