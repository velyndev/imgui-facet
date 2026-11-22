// IconButton: a square button one control height wide that shows only its icon, with its label as
// the tooltip, the button variants, a latched (selected) look and a loading state.

#include "facet/widgets/buttons.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "widgets/buttons/button_look.h"

namespace facet
{

bool IconButton(const char* label, Icon icon, const IconButtonOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    IM_ASSERT_USER_ERROR(labelEnd != label, "IconButton needs a visible label: it is the tooltip and the name.");
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    internal::ControlTextScope text(m);
    const Colors& c = T().colors;

    // One control height square, unless rule 6 asks for another width.
    const float width = internal::ResolveItemWidth(0.0f, next, m.height);
    const ImGuiID id = window->GetID(label);
    const float top = window->DC.CursorPos.y;
    internal::ItemOptions itemOptions;
    itemOptions.baseline = CenteredTextY(top, m.height) - top;
    itemOptions.handCursor = !options.loading;
    internal::ApplyNextItemData(next);
    const internal::ItemState item = internal::ItemBehavior(id, ImVec2(width, m.height), itemOptions);

    if (item.visible)
    {
        internal::ItemState look = item;
        if (options.loading)
        {
            look.hoverAmount = 0.0f;
            look.pressAmount = 0.0f;
        }
        internal::ButtonColors colors = internal::ButtonLook(options.variant, look);
        ImDrawList* drawList = window->DrawList;
        if (options.selected)
        {
            // The latched look: the selection tint with a focusRing hairline, so the latch reads by
            // its edge as well as its tint; hover and press tint it further.
            colors.outline = item.disabled ? c.separator : c.focusRing;
            colors.content = internal::ControlLabelColor(item.disabled);
            colors.fill = item.disabled ? c.fill[1] : c.accentSubtle;
        }
        FillRect(drawList, item.rect.Min, item.rect.Max, ToU32(colors.fill), m.radius);
        if (options.selected && !item.disabled)
        {
            const float feedback = std::max(look.hoverAmount, look.pressAmount);
            if (feedback > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[2], c.fill[1], look.pressAmount);
                FillRect(drawList, item.rect.Min, item.rect.Max, ToU32(WithAlpha(tint, tint.w * feedback)), m.radius);
            }
        }
        if (colors.outline.w > 0.0f)
            internal::DrawOutline(drawList, item.rect, m.radius, colors.outline);
        const float iconSize = text.IconSize();
        const ImVec2 center = item.rect.GetCenter();
        if (options.loading)
            internal::DrawControlSpinner(drawList, center, iconSize, ToU32(colors.content));
        else
            internal::DrawControlIcon(drawList, icon, std::round(center.x - iconSize * 0.5f), center.y, iconSize,
                                      ToU32(colors.content));
        internal::DrawItemFocusRing(item, m.radius);
    }

    // The label is the tooltip, on hover and on keyboard focus.
    internal::CutTextTooltip(item, label, labelEnd, labelEnd != label);
    return item.pressed && !options.loading;
}

} // namespace facet
