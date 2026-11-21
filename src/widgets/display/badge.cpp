// Badge (facet/widgets/display.h): a small label in a box with the element radius, on the text
// baseline of the line it sits in. A subtle tone wash with label text by default, or a solid tone
// fill. Not interactive: one item that is not a Tab stop.

#include "facet/widgets/display.h"

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

void Badge(const char* text, const BadgeOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (text == nullptr)
        text = "";
    const Theme& t = T();
    const internal::DisplayMarkMetrics m = internal::DisplayMarkLook(ControlSize::Regular);
    const float baselineInBox = internal::DisplayMarkBaseline(m);

    internal::ControlTextScope scope(m.textStyle);
    const bool hasText = text[0] != '\0';
    const bool hasIcon = options.icon.IsValid();
    const float iconSize = scope.IconSize();
    const float gap = Px(t.spacing.xs);
    const float padding = Px(t.spacing.sm);
    const float textWidth = hasText ? std::ceil(ImGui::CalcTextSize(text).x) : 0.0f;
    float width = padding * 2.0f + textWidth;
    if (hasIcon)
        width += iconSize + (hasText ? gap : 0.0f);

    const ImGuiID id = internal::DisplayAnonymousId(window, hasText ? text : "##Badge");
    const internal::DisplayInlineBox box =
        internal::DisplayPlaceInline(window, id, ImVec2(width, m.height), baselineInBox, m.lineStyle);
    if (!box.visible)
        return;

    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::DisplayToneColors tone = internal::DisplayToneLook(options.tone, disabled);
    ImDrawList* drawList = window->DrawList;
    const ImU32 fill = ToU32(options.solid ? tone.solid : tone.wash);
    const ImU32 content = ToU32(options.solid ? tone.onSolid : internal::ControlLabelColor(disabled));
    FillRect(drawList, box.rect.Min, box.rect.Max, fill, Px(t.radii.element));
    const float textTop = internal::DisplayTextTop(box.baseline);
    float x = box.rect.Min.x + padding;
    if (hasIcon)
    {
        const ImU32 glyph = ToU32(options.solid ? tone.onSolid : tone.glyph);
        internal::DrawControlIcon(drawList, options.icon, x, internal::DisplayCapsCenter(textTop), iconSize, glyph);
        x += iconSize + gap;
    }
    if (hasText)
        drawList->AddText(ImVec2(x, textTop), content, text);
}

} // namespace facet
