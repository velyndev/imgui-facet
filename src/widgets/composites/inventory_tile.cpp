// InventoryTile (facet/widgets/composites.h): one item tile outside a grid. It follows the order of
// the model widget (src/widgets/buttons/button.cpp): entry, measure, the item, drawing from tokens,
// then the tooltip and the result. The tile look, the drag source and its ghost are the module's
// (cards_look.h), so a tile here looks and drags like a tile of an InventoryGrid.

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

bool InventoryTile(const char* id, const InventoryItem& item, const InventoryTileOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const Theme& t = T();
    const Colors& c = t.colors;

    // A square of the slot size; a width the caller sets keeps it square.
    const float slot = Px(options.slotSize > 0.0f ? options.slotSize : t.metrics.slotSize);
    const float side = internal::ResolveItemWidth(0.0f, next, slot);
    const ImGuiID itemId = window->GetID(id);
    internal::ApplyNextItemData(next);
    const internal::ItemState state = internal::ItemBehavior(itemId, ImVec2(side, side));

    // The tile is its own drag source; Space activates it, so Ctrl+Space picks it up.
    bool lifted = false;
    if (options.dragSource)
    {
        const char* type = options.payloadType != nullptr ? options.payloadType : kInventoryPayloadType;
        internal::InventorySourceDesc source;
        source.owner = itemId;
        source.item = &item;
        source.type = type;
        source.pickUp = ImGuiMod_Ctrl | ImGuiKey_Space;
        source.forced = Has(state.forced, internal::ForcedItemState::Dragging);
        lifted = internal::InventoryDragSource(source);
    }

    if (state.visible)
    {
        ImDrawList* drawList = window->DrawList;
        internal::InventoryTileLook look;
        look.disabled = state.disabled;
        look.selected = options.selected;
        look.lifted = lifted;
        internal::InventoryDrawTile(drawList, state.rect, item, look);
        // Hover and press as a wash over the tile, as over an activatable image.
        const ImVec4 wash = internal::StateFill(WithAlpha(c.fill[2], 0.0f), c.fill[2], c.fill[1], state);
        const float radius = internal::InventoryTileRadius();
        if (!state.disabled && wash.w > 0.0f)
            FillRect(drawList, state.rect.Min, state.rect.Max, ToU32(wash), radius);
        internal::DrawItemFocusRing(state, radius);
    }
    internal::InventoryTileTooltip(itemId, state.rect, state.hovered, state.focusVisible, item, nullptr);
    return state.pressed && !state.disabled && !lifted;
}

} // namespace facet
