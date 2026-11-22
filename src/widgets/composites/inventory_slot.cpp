// InventorySlot (facet/widgets/composites.h): a single slot (an equipment slot) that is a drop
// target of inventory drags and a drag source of its item. The look, the drag source and the drop
// rules are the module's (cards_look.h), so a slot takes and gives items exactly as a grid slot does.

#include "facet/widgets/composites.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "widgets/composites/cards_look.h"

namespace facet
{

InventoryResult InventorySlot(const char* label, const InventoryItem& item, const InventorySlotOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return {};
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;
    const char* type = options.payloadType != nullptr ? options.payloadType : kInventoryPayloadType;

    const float slot = Px(options.slotSize > 0.0f ? options.slotSize : t.metrics.slotSize);
    const float side = internal::ResolveItemWidth(0.0f, next, slot);
    const ImGuiID slotId = window->GetID(label);
    internal::ApplyNextItemData(next);
    const internal::ItemState state = internal::ItemBehavior(slotId, ImVec2(side, side));
    const bool blocked = state.disabled || state.readOnly;

    // The slot gives its item away by a drag; Space picks it up while it has the focus.
    internal::InventorySourceDesc source;
    source.owner = slotId;
    source.item = &item;
    source.type = type;
    source.pickUp = ImGuiKey_Space;
    source.forced = Has(state.forced, internal::ForcedItemState::Dragging);
    const bool lifted = internal::InventoryDragSource(source);

    ImDrawList* drawList = window->DrawList;
    const float radius = internal::InventoryTileRadius();
    if (state.visible)
    {
        internal::InventoryTileLook look;
        look.disabled = state.disabled;
        look.hoverAmount = state.hoverAmount;
        look.lifted = lifted;
        look.placeholderIcon = options.placeholderIcon;
        internal::InventoryDrawTile(drawList, state.rect, item, look);
        const ImVec4 wash = internal::StateFill(WithAlpha(c.fill[2], 0.0f), c.fill[2], c.fill[1], state);
        if (!state.disabled && wash.w > 0.0f)
            FillRect(drawList, state.rect.Min, state.rect.Max, ToU32(wash), radius);
    }

    // The drop target over the slot: highlighted while a drag it takes hovers it.
    internal::InventoryTargetDesc target;
    target.zoneId = ImHashStr("##inventory.slot.drop", 0, slotId);
    target.rect = state.rect;
    target.type = type;
    target.item = &item;
    target.owner = slotId;
    target.accept = !blocked;
    const internal::InventoryDropOutcome drop = internal::InventoryDropTarget(target);
    // Inside the slot, as on the tiles of a grid: keyboard focus only.
    if (state.visible)
        internal::DrawItemFocusRingInside(state, radius);

    // The visible part of the label names the slot while it is empty.
    internal::InventoryTileTooltip(slotId, state.rect, state.hovered, state.focusVisible, item, label);

    InventoryResult result = drop.result;
    if (drop.delivered && drop.keyboard)
    {
        // A keyboard drop leaves the focus on the slot that took the item.
        ImGui::SetFocusID(slotId, window);
        ImGui::SetNavCursorVisible(true);
        internal::SetKeyboardModality(true);
    }
    if (result.action == InventoryAction::None && !blocked && !lifted)
    {
        // Enter or a double click activates the slot.
        const bool doubleClicked = state.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        if ((state.pressed && state.keyboardPressed) || doubleClicked)
        {
            result.action = InventoryAction::Activated;
            result.source = slotId;
        }
    }
    if (result.action != InventoryAction::None && result.action != InventoryAction::Activated)
        internal::MarkWidgetEdited(slotId);
    return result;
}

} // namespace facet
