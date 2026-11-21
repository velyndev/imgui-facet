// CountBadge (facet/widgets/display.h): a counter in a solid pill on the text baseline, at least as
// wide as it is tall, capped at "maxCount+". A hidden count (0 without showZero) still advances
// like one item of no size. Not interactive.

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

void CountBadge(int count, const CountBadgeOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;
    if (count <= 0 && !options.showZero)
    {
        ImGui::Dummy(ImVec2(0.0f, 0.0f));
        return;
    }
    char text[24];
    if (options.maxCount > 0 && count > options.maxCount)
        ImFormatString(text, sizeof(text), "%d+", options.maxCount);
    else
        ImFormatString(text, sizeof(text), "%d", std::max(count, 0));

    const Theme& t = T();
    const internal::DisplayMarkMetrics m = internal::DisplayMarkLook(ControlSize::Regular);
    const float baselineInBox = internal::DisplayMarkBaseline(m);
    internal::ControlTextScope scope(m.textStyle);
    const float textWidth = std::ceil(ImGui::CalcTextSize(text).x);
    // A short count is a circle: the narrow padding keeps one digit inside the height.
    const float width = std::max(m.height, textWidth + Px(t.spacing.xs) * 2.0f);
    const ImGuiID id = internal::DisplayAnonymousId(window, "##CountBadge");
    const internal::DisplayInlineBox box =
        internal::DisplayPlaceInline(window, id, ImVec2(width, m.height), baselineInBox, m.lineStyle);
    if (!box.visible)
        return;

    const bool disabled = (GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled) != 0;
    const internal::DisplayToneColors tone = internal::DisplayToneLook(options.tone, disabled);
    ImDrawList* drawList = window->DrawList;
    // A counter is a pill: its round ends tell it apart from a badge, which names a status.
    FillRoundRect(drawList, box.rect.Min, box.rect.Max, ToU32(tone.solid), box.rect.GetHeight() * 0.5f);
    drawList->AddText(ImVec2(std::round(box.rect.GetCenter().x - textWidth * 0.5f), internal::DisplayTextTop(box.baseline)),
                      ToU32(tone.onSolid), text);
}

} // namespace facet
