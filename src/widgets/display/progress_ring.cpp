// ProgressRing (facet/widgets/display.h): a circular progress arc with round caps from the top,
// clockwise, over a track ring, with an optional text in the middle and the visible label part to
// its right. The arc eases to a new value; kIndeterminate turns a quarter arc on the shared loading
// clock (a slow fade in place with reduced motion). Not interactive: one item, not a Tab stop.

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

constexpr float kRingPi = 3.14159265358979f;
// Animation sub key of the smoothed arc under the widget id.
constexpr int kRingAnimArc = 1;
// The stroke of the ring as a share of its diameter, as for the control spinner.
constexpr float kRingStrokeShare = 1.0f / 8.0f;
// The share of a turn the indeterminate arc covers.
constexpr float kRingIndeterminateSweep = 0.25f;

} // namespace

void ProgressRing(const char* label, float fraction, const ProgressRingOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const bool indeterminate = fraction == kIndeterminate;
    const float value = std::isnan(fraction) ? 0.0f : std::clamp(fraction, 0.0f, 1.0f);
    const float diameter = std::round(options.diameter > 0.0f ? Px(options.diameter) : m.height);
    const float stroke = std::max(HairlineThickness(), std::round(diameter * kRingStrokeShare));

    // The text in the middle: centerText, or the percentage with showPercent. It goes after the
    // label instead when the ring is too small for it.
    char percent[8] = {};
    const char* inner = options.centerText;
    if (inner == nullptr && options.showPercent && !indeterminate)
    {
        ImFormatString(percent, sizeof(percent), "%d%%", static_cast<int>(std::round(value * 100.0f)));
        inner = percent;
    }
    const float innerRoom = diameter - stroke * 2.0f - Px(t.spacing.xs);
    TextStyle innerStyle = TextStyle::Footnote;
    bool innerFits = false;
    if (inner != nullptr && inner[0] != '\0')
    {
        for (const TextStyle style : {TextStyle::Footnote, TextStyle::Caption})
        {
            const ImVec2 size = MeasureText(style, inner);
            if (size.x <= innerRoom && size.y <= innerRoom + stroke)
            {
                innerStyle = style;
                innerFits = true;
                break;
            }
        }
    }

    internal::ControlTextScope scope(m);
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const bool hasLabel = labelEnd != label;
    const char* trailing = inner != nullptr && inner[0] != '\0' && !innerFits ? inner : nullptr;
    const float gap = Px(t.spacing.sm);
    float textWidth = hasLabel ? std::ceil(ImGui::CalcTextSize(label, labelEnd).x) : 0.0f;
    if (trailing != nullptr)
        textWidth += (hasLabel ? gap : 0.0f) + std::ceil(ImGui::CalcTextSize(trailing).x);
    const float height = std::max(diameter, std::round(ImGui::GetTextLineHeight()));
    const float width = diameter + (textWidth > 0.0f ? gap + textWidth : 0.0f);

    // The label is centered on the ring, and the ring sits so that the label meets the text of the
    // line around it (ImGui::SameLine), like the marks of display_look.h.
    const float textTopInBox = CenteredTextY(0.0f, height);
    const internal::DisplayInlineBox box = internal::DisplayLayoutInline(
        window, ImVec2(width, height), textTopInBox + std::round(internal::DisplayFontAscent()), m.textStyle);
    const ImVec2 origin = box.rect.Min;
    const ImRect bounds = box.rect;
    const float textTop = origin.y + textTopInBox;
    const ImGuiID id = window->GetID(label[0] != '\0' ? label : "##ProgressRing");
    internal::ApplyNextItemData(next);
    // The arc is smoothed every frame, also while clipped.
    const float shown = Tween(id, kRingAnimArc, value, t.motion.durationStandard);
    if (!ImGui::ItemAdd(bounds, id, nullptr, ImGuiItemFlags_NoNav))
        return;

    const Colors& c = t.colors;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const ImVec4 tone = disabled ? c.labelTertiary : internal::DisplayProgressColor(options.tone);
    ImDrawList* drawList = window->DrawList;
    const ImVec2 center(std::round(origin.x + diameter * 0.5f), std::round(origin.y + height * 0.5f));
    const float radius = (diameter - stroke) * 0.5f;
    StrokeArc(drawList, center, radius, 0.0f, 2.0f * kRingPi, ToU32(disabled ? c.fill[3] : c.fill[0]), stroke, false);
    const float top = -0.5f * kRingPi;
    if (indeterminate)
    {
        // The shared loading clock turns the arc; with reduced motion it stands at the top and fades.
        const float start = top + internal::SpinnerAngle();
        StrokeArc(drawList, center, radius, start, start + kRingIndeterminateSweep * 2.0f * kRingPi,
                  ToU32(WithAlpha(tone, tone.w * internal::SpinnerOpacity())), stroke, true);
    }
    else if (shown > 0.0f)
    {
        StrokeArc(drawList, center, radius, top, top + std::clamp(shown, 0.0f, 1.0f) * 2.0f * kRingPi, ToU32(tone),
                  stroke, true);
    }

    if (innerFits)
    {
        PushTextStyle(innerStyle);
        const ImVec2 size = ImGui::CalcTextSize(inner);
        drawList->AddText(ImVec2(std::round(center.x - size.x * 0.5f), CenteredTextY(center.y - diameter * 0.5f, diameter)),
                          ToU32(internal::ControlLabelColor(disabled)), inner);
        PopTextStyle();
    }
    float x = origin.x + diameter + gap;
    if (hasLabel)
    {
        drawList->AddText(ImVec2(x, textTop), ToU32(internal::ControlLabelColor(disabled)), label, labelEnd);
        x += ImGui::CalcTextSize(label, labelEnd).x + gap;
    }
    if (trailing != nullptr)
        drawList->AddText(ImVec2(std::round(x), textTop), ToU32(internal::ControlSecondaryColor(disabled)), trailing);
}

} // namespace facet
