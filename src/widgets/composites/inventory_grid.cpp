// InventoryGrid (facet/widgets/composites.h): a grid of inventory slots.
//
// The slots are the tiles of an ItemGrid (layout.h): a scroll area that submits only the visible
// tiles, reflows with the width, and selects through the list core (click, Ctrl, Shift and the arrow
// keys in two dimensions; no box select, a drag moves items) into a selection kept under its id; *selected is
// the focused selected slot, else the first selected one. Each tile is drawn with the module's tile
// look (cards_look.h) and is both a drag source (pointer, or Space for a keyboard drag) and a drop
// target of its own, so items move between slots of this grid, other grids and InventorySlot and
// InventoryTile widgets through the drag and drop core. Alt and an arrow key move the focused
// item to the neighbouring slot at once. Enter or a double click activates a slot.
//
// Size: the columns are options.columns, or as many slots as the width allows; the tiles keep the
// slot size and shrink only when the width is too small for one column. The height is the rows the
// slots need, at most options.height or, without it, the visible height of the window the grid is
// in; further rows scroll inside the grid. While a keyboard drag is in progress the line above and below the visible tiles are
// drop zones too, so the drag can move on into rows scrolled out of view.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <climits>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "facet/widgets/layout.h"
#include "internal/control.h"
#include "internal/drag_drop.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_core.h"
#include "internal/loading.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/composites/cards_look.h"

namespace facet
{
namespace
{

// The selection and the pending focus of a grid, under its id.
struct InventoryGridState
{
    ImGuiSelectionBasicStorage selection;
    int written = INT_MIN;     // *selected as the grid last saw or wrote it
    int focusPending = -1;     // after a move from the keyboard: the slot that takes the focus
    int pendingFrame = -1;
};

// The geometry of one frame.
struct InventoryGridLayout
{
    int columns = 1;
    float tile = 1.0f;         // pixels, square
    float gap = 0.0f;
    float outset = 0.0f;       // the scroll area's padding on every side
    ImVec2 size;               // the whole grid
};

InventoryGridLayout InventoryGridMeasure(const InventoryGridOptions& options, float available, int count)
{
    const Theme& t = T();
    InventoryGridLayout l;
    const float slot = std::max(1.0f, std::floor(Px(options.slotSize > 0.0f ? options.slotSize : t.metrics.slotSize)));
    l.gap = Px(t.spacing.xs);
    l.outset = internal::FocusRingOutset();
    const float room = std::max(1.0f, available - l.outset * 2.0f);
    const int fitting = std::max(1, static_cast<int>(std::floor((room + l.gap) / (slot + l.gap))));
    // Without a set column count, a few slots take only the columns they fill.
    l.columns = options.columns > 0 ? options.columns : std::clamp(count, 1, fitting);
    const float shared = std::floor((room - l.gap * static_cast<float>(l.columns - 1)) / static_cast<float>(l.columns));
    l.tile = std::clamp(shared, 1.0f, slot);
    const int rows = std::max(1, (count + l.columns - 1) / l.columns);
    const float pitch = l.tile + l.gap;
    l.size.x = static_cast<float>(l.columns) * pitch - l.gap + l.outset * 2.0f;
    l.size.y = static_cast<float>(rows) * pitch - l.gap + l.outset * 2.0f;
    return l;
}

// The rect of slot `index` from the rect of slot `ref`, in a grid of `l`.
ImRect InventoryGridSlotRect(const InventoryGridLayout& l, int ref, const ImRect& refRect, int index)
{
    const float pitch = l.tile + l.gap;
    const float dx = static_cast<float>(index % l.columns - ref % l.columns) * pitch;
    const float dy = static_cast<float>(index / l.columns - ref / l.columns) * pitch;
    return ImRect(refRect.Min.x + dx, refRect.Min.y + dy, refRect.Max.x + dx, refRect.Max.y + dy);
}

ImGuiID InventoryGridZoneId(ImGuiID gridId, int index)
{
    return ImHashData(&index, sizeof(index), ImHashStr("##inventory.grid.zone", 0, gridId));
}

void InventoryGridSelectOnly(ImGuiSelectionBasicStorage& selection, int index)
{
    selection.Clear();
    if (index >= 0)
        selection.SetItemSelected(static_cast<ImGuiID>(index), true);
}

// The neighbour of `index` for an Alt+arrow move (the key ReorderableList and LayerList move a row
// with), -1 at the edge, or -2 without such a press. Ctrl+arrow stays with the list core, which
// moves the focus without changing the selection.
int InventoryGridNeighbour(int index, int columns, int count)
{
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.KeyAlt || io.KeyShift || io.KeyCtrl)
        return -2;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
        return index % columns > 0 ? index - 1 : -1;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
        return index % columns < columns - 1 && index + 1 < count ? index + 1 : -1;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
        return index - columns >= 0 ? index - columns : -1;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        return index + columns < count ? index + columns : -1;
    return -2;
}

void InventoryGridDrawLoading(ImGuiWindow* window, ImGuiID gridId, const InventoryGridLayout& l, int count)
{
    const ImVec2 at = window->DC.CursorPos;
    const ImRect rect(at, ImVec2(at.x + l.size.x, at.y + l.size.y));
    ImGui::ItemSize(l.size);
    if (!ImGui::ItemAdd(rect, gridId, nullptr, ImGuiItemFlags_NoNav))
        return;
    const float pitch = l.tile + l.gap;
    const int shown = std::max(count, l.columns);
    const float radius = internal::InventoryTileRadius();
    for (int i = 0; i < shown; ++i)
    {
        const float x = rect.Min.x + l.outset + static_cast<float>(i % l.columns) * pitch;
        const float y = rect.Min.y + l.outset + static_cast<float>(i / l.columns) * pitch;
        if (y > rect.Max.y)
            break;
        if (!ImGui::IsRectVisible(ImVec2(x, y), ImVec2(x + l.tile, y + l.tile)))
            continue;
        internal::DrawSkeleton(window->DrawList, ImVec2(x, y), ImVec2(x + l.tile, std::min(rect.Max.y, y + l.tile)), radius);
    }
}

} // namespace

InventoryResult InventoryGrid(const char* id, int* selected, std::span<const InventoryItem> slots,
                              const InventoryGridOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return {};
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const Theme& t = T();
    const ImGuiID gridId = window->GetID(id);
    const int count = static_cast<int>(std::min<std::size_t>(slots.size(), static_cast<std::size_t>(INT_MAX)));
    const bool readOnly = options.readOnly || selected == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const char* type = options.payloadType != nullptr ? options.payloadType : kInventoryPayloadType;

    const float available = ResolveItemWidth(options.width, next, kFill);
    InventoryGridLayout l = InventoryGridMeasure(options, available, count);
    // Rows past the visible height of the window scroll inside the grid.
    const float oneRow = l.tile + l.outset * 2.0f;
    float cap = window->InnerRect.GetHeight();
    if (cap < oneRow)
        cap = std::max(Px(t.metrics.panelHeight), oneRow);
    // A set height bounds the grid instead, at least one row.
    if (options.height > 0.0f)
        cap = std::max(oneRow, std::round(Px(options.height)));
    l.size.y = std::max(oneRow, std::min(l.size.y, std::floor(cap)));
    // The empty and the error state take the whole width for their line of text.
    if (count == 0 || options.state == DataState::Error)
        l.size.x = std::max(l.size.x, available);

    if (options.state == DataState::Loading)
    {
        InventoryGridDrawLoading(window, gridId, l, count);
        return {};
    }

    InventoryGridState& state = GetState<InventoryGridState>(gridId);
    if (selected != nullptr && (*selected != state.written || readOnly))
    {
        // A selection the caller set (or, read-only, the one it shows) replaces the grid's own.
        InventoryGridSelectOnly(state.selection, *selected >= 0 && *selected < count ? *selected : -1);
        state.written = *selected;
    }
    const int sizeBefore = state.selection.Size;
    const int orderBefore = state.selection._SelectionOrder;
    if (state.focusPending >= 0 && ImGui::GetFrameCount() > state.pendingFrame + 2)
        state.focusPending = -1;

    InventoryResult result;
    int focusedTile = -1;
    ImGui::BeginGroup();
    const ImVec2 origin = window->DC.CursorPos;
    ItemGridOptions grid;
    grid.minTileWidth = l.tile / Scale();
    grid.tileAspect = 1.0f;
    grid.gap = Gap::Tight;
    // A drag from a slot moves its item, so a drag never starts a box selection.
    grid.boxSelect = false;
    grid.state = options.state;
    grid.emptyText = options.emptyText;
    grid.errorText = options.errorText;
    grid.width = l.size.x / Scale();
    grid.height = l.size.y / Scale();
    // The capture's forced look goes on to the tiles; a forced drag is the grid's own.
    if (forced.state != ForcedItemState::None)
        ForceNextItemState(forced.state & ~ForcedItemState::Dragging, forced.part);
    if (BeginItemGrid(id, count, selected != nullptr ? &state.selection : nullptr, grid))
    {
        ImGuiWindow* area = ImGui::GetCurrentWindow();
        ImDrawList* drawList = area->DrawList;
        DragDropInfo drag;
        const bool dragging = DragDropActive(type, &drag);
        int visibleFirst = -1;
        int visibleLast = -1;
        ImRect refRect;

        // One slot: the tile item, its drag source, its look, its drop zone and its tooltip.
        const auto slot = [&](int i, bool submitted, const ImRect& rectIn) {
            const InventoryItem& item = slots[static_cast<std::size_t>(i)];
            ImRect rect = rectIn;
            bool hovered = false;
            bool focused = false;
            bool tileDisabled = disabled;
            bool activated = false;
            ImGuiID tileId = 0;
            if (submitted)
            {
                activated = ItemGridTile(i);
                const ImGuiLastItemData& last = GImGui->LastItemData;
                tileId = last.ID;
                rect = last.Rect;
                hovered = ImGui::IsItemHovered();
                focused = ImGui::IsItemFocused();
                tileDisabled = tileDisabled || (last.ItemFlags & ImGuiItemFlags_Disabled) != 0;
                if (focused)
                    focusedTile = i;
                if (state.focusPending == i && !focused && !tileDisabled)
                {
                    ImGui::SetFocusID(tileId, area);
                    ImGui::SetNavCursorVisible(true);
                    SetKeyboardModality(true);
                }
                if (state.focusPending == i)
                    state.focusPending = -1;
                if (ImGui::IsRectVisible(rect.Min, rect.Max))
                {
                    if (visibleFirst < 0)
                    {
                        visibleFirst = i;
                        refRect = rect;
                    }
                    visibleLast = std::max(visibleLast, i);
                }
            }

            bool lifted = false;
            if (submitted && !readOnly && !tileDisabled)
            {
                InventorySourceDesc source;
                source.owner = gridId;
                source.slot = i;
                source.item = &item;
                source.type = type;
                source.pickUp = ImGuiKey_Space;
                source.forced = Has(forced.state, ForcedItemState::Dragging) && (forced.part == i || (forced.part < 0 && i == 0));
                lifted = InventoryDragSource(source);
            }
            if (submitted && ImGui::IsRectVisible(rect.Min, rect.Max))
            {
                InventoryTileLook look;
                look.disabled = tileDisabled;
                look.selected = selected != nullptr && state.selection.Contains(static_cast<ImGuiID>(i));
                look.lifted = lifted;
                InventoryDrawTile(drawList, rect, item, look);
            }

            InventoryTargetDesc target;
            target.zoneId = InventoryGridZoneId(gridId, i);
            target.rect = rect;
            target.type = type;
            target.item = &item;
            target.owner = gridId;
            target.slot = i;
            target.accept = !readOnly && !tileDisabled;
            const InventoryDropOutcome drop = InventoryDropTarget(target);
            if (drop.result.action != InventoryAction::None)
            {
                result = drop.result;
                if (selected != nullptr)
                    InventoryGridSelectOnly(state.selection, i);
                if (drop.keyboard)
                {
                    state.focusPending = i;
                    state.pendingFrame = ImGui::GetFrameCount();
                }
            }
            if (!submitted)
                return;
            InventoryTileTooltip(tileId, rect, hovered, focused && KeyboardModality(), item, nullptr);

            if (readOnly || tileDisabled)
                return;
            if (activated && result.action == InventoryAction::None)
            {
                result.action = InventoryAction::Activated;
                result.source = gridId;
                result.to = i;
            }
            // Alt and an arrow move the focused item to its neighbour at once.
            if (focused && !dragging && item.label != nullptr && !item.locked)
            {
                const int to = InventoryGridNeighbour(i, l.columns, count);
                if (to != -2)
                    ImGui::NavMoveRequestCancel();
                if (to >= 0 && !slots[static_cast<std::size_t>(to)].locked)
                {
                    const InventoryPayload moving{gridId, i, std::max(1, item.count)};
                    InventoryDragged stack;
                    stack.source = gridId;
                    stack.slot = i;
                    stack.count = moving.count;
                    stack.stackKey = item.stackKey;
                    stack.maxStack = item.maxStack;
                    result = InventoryDropRule(moving, &stack, false, slots[static_cast<std::size_t>(to)], to);
                    if (selected != nullptr)
                        InventoryGridSelectOnly(state.selection, to);
                    state.focusPending = to;
                    state.pendingFrame = ImGui::GetFrameCount();
                    ScrollAreaRevealRect(area, InventoryGridSlotRect(l, i, rect, to), ScrollAlign::Nearest);
                }
            }
        };

        RowRange rows;
        while (NextVisibleRows(&rows))
            for (int i = rows.first; i < rows.last; ++i)
                slot(i, true, ImRect());

        // A keyboard drag reaches the lines just out of view, so it can move on and scroll there.
        if (dragging && drag.keyboard && visibleFirst >= 0)
        {
            const int above = (visibleFirst / l.columns - 1) * l.columns;
            const int below = (visibleLast / l.columns + 1) * l.columns;
            for (int line = 0; line < 2; ++line)
            {
                const int first = line == 0 ? above : below;
                for (int i = std::max(0, first); i < std::min(count, first + l.columns); ++i)
                    slot(i, false, InventoryGridSlotRect(l, visibleFirst, refRect, i));
            }
        }
        EndItemGrid();
    }
    // The grid keeps its size also on a frame its scroll area is not shown (the first one).
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(l.size);
    ImGui::EndGroup();
    FinishWidgetGroup(gridId);

    if (selected != nullptr && !readOnly)
    {
        int primary = *selected;
        ImGuiSelectionBasicStorage& selection = state.selection;
        if (focusedTile >= 0 && selection.Contains(static_cast<ImGuiID>(focusedTile)))
        {
            primary = focusedTile;
        }
        else if (selection.Size != sizeBefore || selection._SelectionOrder != orderBefore)
        {
            if (selection.Size == 0)
                primary = -1;
            else if (primary < 0 || !selection.Contains(static_cast<ImGuiID>(primary)))
            {
                void* it = nullptr;
                ImGuiID first = 0;
                primary = selection.GetNextSelectedItem(&it, &first) ? static_cast<int>(first) : -1;
            }
        }
        if (result.action == InventoryAction::Moved || result.action == InventoryAction::Swapped ||
            result.action == InventoryAction::Merged || result.action == InventoryAction::Split)
        {
            if (result.source == gridId || result.to >= 0)
                primary = result.to;
        }
        if (primary != *selected)
        {
            *selected = primary;
            MarkWidgetEdited(gridId);
        }
        state.written = *selected;
    }
    if (result.action != InventoryAction::None && result.action != InventoryAction::Activated)
        MarkWidgetEdited(gridId);
    return result;
}

} // namespace facet
