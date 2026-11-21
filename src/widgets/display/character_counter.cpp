// CharacterCounter (facet/widgets/display.h): the length of a text against its limit, as
// "count / max" in Footnote or as a small ring. Near the limit it turns to the warning tone with the
// warning icon, over it to the destructive tone with the error icon, so the state never rests on
// color alone; the ring then shows the characters left (negative when over). Not interactive.

#include "facet/widgets/display.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "internal/control.h"
#include "widgets/display/display_look.h"

namespace facet
{
namespace
{

constexpr float kCounterPi = 3.14159265358979f;
// The stroke of the ring as a share of its diameter, as for the progress ring.
constexpr float kCounterStrokeShare = 1.0f / 8.0f;

} // namespace

void CharacterCounter(int count, int max, const CharacterCounterOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    const Theme& t = T();
    const bool limited = max > 0;
    const int remaining = limited ? max - count : 0;
    const Tone tone = !limited ? Tone::Neutral
                               : (remaining < 0 ? Tone::Destructive : (remaining <= options.warnRemaining ? Tone::Warning : Tone::Neutral));
    const bool ring = options.ring && limited;

    // The text: "count / max", the count alone without a limit, or the characters left beside a
    // ring that warns.
    char text[48] = {};
    if (!limited)
        ImFormatString(text, sizeof(text), "%d", count);
    else if (!ring)
        ImFormatString(text, sizeof(text), "%d / %d", count, max);
    else if (tone != Tone::Neutral)
        ImFormatString(text, sizeof(text), "%d", remaining);

    internal::ControlTextScope scope(TextStyle::Footnote);
    const float lineHeight = std::round(ImGui::GetTextLineHeight());
    const float iconSize = scope.IconSize();
    const float gap = Px(t.spacing.xs);
    const bool hasIcon = tone != Tone::Neutral && !ring;
    const float textWidth = text[0] != '\0' ? std::ceil(ImGui::CalcTextSize(text).x) : 0.0f;
    const float ringSize = ring ? iconSize : 0.0f;
    float width = textWidth;
    if (hasIcon)
        width += iconSize + gap;
    if (ring)
        width += ringSize + (textWidth > 0.0f ? gap : 0.0f);
    const internal::DisplayInlineBox box = internal::DisplayPlaceInline(
        window, internal::DisplayAnonymousId(window, "##CharacterCounter"), ImVec2(std::max(width, 1.0f), lineHeight),
        std::round(internal::DisplayFontAscent()), TextStyle::Footnote);
    if (!box.visible)
        return;

    const Colors& c = t.colors;
    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::DisplayToneColors look = internal::DisplayToneLook(tone, disabled);
    const ImU32 textColor = ToU32(disabled ? c.labelTertiary : (tone == Tone::Neutral ? c.labelSecondary : look.text));
    ImDrawList* drawList = window->DrawList;
    const float textTop = internal::DisplayTextTop(box.baseline);
    const float capsCenter = internal::DisplayCapsCenter(textTop);
    float x = box.rect.Min.x;
    if (hasIcon)
    {
        internal::DrawControlIcon(drawList, tone == Tone::Destructive ? Icon("error") : Icon("warning"), x, capsCenter, iconSize,
                                  ToU32(look.glyph));
        x += iconSize + gap;
    }
    if (ring)
    {
        const float stroke = std::max(HairlineThickness(), std::round(ringSize * kCounterStrokeShare));
        const ImVec2 center(x + ringSize * 0.5f, std::round(capsCenter));
        const float radius = (ringSize - stroke) * 0.5f;
        const float fraction = std::clamp(static_cast<float>(count) / static_cast<float>(max), 0.0f, 1.0f);
        StrokeArc(drawList, center, radius, 0.0f, 2.0f * kCounterPi, ToU32(disabled ? c.fill[3] : c.fill[0]), stroke, false);
        if (fraction > 0.0f)
        {
            const ImVec4 arc = disabled ? c.labelTertiary : internal::DisplayProgressColor(tone == Tone::Neutral ? Tone::Accent : tone);
            const float top = -0.5f * kCounterPi;
            StrokeArc(drawList, center, radius, top, top + fraction * 2.0f * kCounterPi, ToU32(arc), stroke, true);
        }
        x += ringSize + gap;
    }
    if (text[0] != '\0')
        drawList->AddText(ImVec2(x, textTop), textColor, text);
}

} // namespace facet
