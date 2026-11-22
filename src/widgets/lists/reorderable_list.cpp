// ReorderableList (lists.h): rows that are dragged to a new place while the other rows move aside.
//
// Built on the drag and drop core (drag_drop.h): every row is a drag source of the type below
// (picked up with the mouse, or with Space for a keyboard drag), the list is one ordered drop zone,
// and the rows are laid out in data order but drawn where they move to: their positions spring
// toward the layout plus the gap shift, keyed by the item id, so they stay still when the caller
// applies the move. Alt+Up and Alt+Down move the focused row at once.

#include "facet/widgets/lists.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/drag_drop.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/state_storage.h"
#include "widgets/lists/list_parts.h"

namespace facet
{
namespace
{

// The payload type of a reorderable row. The payload names the list, so a row dropped on
// another list is refused there.
constexpr const char* kReorderRowType = "facet.reorder.row";
// Rows the list lays out at once; longer lists are refused with a usage error and drawn up to it.
constexpr int kReorderMaxRows = 512;
// Captures: how far below its place a forced drag shows the dragged row, in rows.
constexpr float kForcedDragRows = 1.4f;

struct ReorderRowPayload
{
    ImGuiID list = 0;
    int index = -1;
};

struct ReorderState
{
    ImGuiID focusKey = 0;     // after a move: the row that takes the focus in the next frame
    bool focusPending = false;
};

// The key of row `index`: the item id, else the index (the label overload).
ImGuiID ReorderKey(const ChoiceItem& item, int index)
{
    return item.id != 0 ? item.id : static_cast<ImGuiID>(index);
}

struct ReorderRows
{
    ImRect slots[kReorderMaxRows];
    internal::ItemState items[kReorderMaxRows];
    ImRect grips[kReorderMaxRows];
};

ReorderRows& Scratch()
{
    static ReorderRows rows;  // one list at a time is laid out; no allocation per frame
    return rows;
}

bool ReorderableListImpl(const char* id, ReorderMove* move, const internal::ListsSource& source,
                         const ReorderableListOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const Theme& theme = T();
    const Colors& c = theme.colors;
    const ControlSize size = ResolveControlSize(options.size);
    const float width = ResolveItemWidth(options.width, next, kFill);
    const float rowHeight = ListRowHeight(size);
    const float radius = Px(theme.radii.control);
    const bool readOnly = options.readOnly || move == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const ImGuiID listId = window->GetID(id);
    ReorderState& state = GetState<ReorderState>(listId);
    ImDrawList* drawList = window->DrawList;

    ImGui::PushID(id);
    ImGui::BeginGroup();
    const ImVec2 start = window->DC.CursorPos;

    // The loading, error or empty state takes one row and nothing moves.
    int count = source.count;
    if (count > kReorderMaxRows)
    {
        ReportUsageError("ReorderableList with %d rows; only the first %d are shown", count, kReorderMaxRows);
        count = kReorderMaxRows;
    }
    if (options.state != DataState::Ready || count == 0)
    {
        const ImRect rect(start, ImVec2(start.x + width, start.y + rowHeight));
        ImGui::Dummy(rect.GetSize());
        DrawListState(rect, options.state, options.emptyText, options.errorText);
        ImGui::EndGroup();
        ImGui::PopID();
        FinishWidgetGroup(listId);
        return false;
    }

    DragDropInfo drag;
    const bool dragging = DragDropActive(kReorderRowType, &drag);
    const ImGuiID zoneId = ImGui::GetID("##zone");
    const ImVec2 origin(window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y);
    ControlTextScope text(ControlTextStyle(size));
    const float glyph = text.IconSize();
    const float pad = Px(theme.spacing.sm);
    const ImGuiIO& io = ImGui::GetIO();
    ReorderRows& rows = Scratch();

    // Pass 1: the rows as items and drag sources, laid out in data order without item spacing.
    int lifted = -1;
    bool changed = false;
    ReorderMove result;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    for (int i = 0; i < count; ++i)
    {
        ChoiceItem item;
        source.Get(i, &item);
        const ImGuiID key = ReorderKey(item, i);
        ImGui::PushID(static_cast<int>(key));
        ItemOptions itemOptions;
        itemOptions.hitTarget = false;
        itemOptions.readOnly = readOnly;
        itemOptions.disabled = item.disabled;
        itemOptions.forced = &forced;
        itemOptions.part = i;
        itemOptions.handCursor = !readOnly && !options.handleOnly;
        itemOptions.baseline = std::max(0.0f, (rowHeight - text.FontSize()) * 0.5f);
        const ItemState row = ItemBehavior(ImGui::GetID("##row"), ImVec2(width, rowHeight), itemOptions);
        rows.items[i] = row;
        rows.slots[i] = row.rect;
        rows.grips[i] = ImRect(row.rect.Min.x, row.rect.Min.y, row.rect.Min.x + pad * 2.0f + glyph, row.rect.Max.y);

        if (state.focusPending && key == state.focusKey && !row.focused && !row.disabled)
        {
            ImGui::SetFocusID(row.id, window);
            ImGui::SetNavCursorVisible(true);
            SetKeyboardModality(true);
        }
        if (row.focused || key == state.focusKey)
            state.focusPending = false;

        if (!readOnly && !row.disabled)
        {
            // Alt+Up and Alt+Down move the focused row at once; navigation does not move the focus
            // on the same press.
            if (row.focused && io.KeyAlt && !io.KeyCtrl && !io.KeyShift && !dragging)
            {
                const bool up = ImGui::IsKeyPressed(ImGuiKey_UpArrow);
                const bool down = ImGui::IsKeyPressed(ImGuiKey_DownArrow);
                if (up != down)
                {
                    ImGui::NavMoveRequestCancel();
                    const int to = up ? i - 1 : i + 1;
                    if (to >= 0 && to < count)
                    {
                        result = {i, to};
                        changed = true;
                        state.focusKey = item.id != 0 ? key : static_cast<ImGuiID>(to);
                        state.focusPending = true;
                    }
                }
            }
            // A row is a drag source; with handleOnly only a press on its grip starts a drag.
            const bool pressOnGrip = rows.grips[i].Contains(io.MouseClickedPos[ImGuiMouseButton_Left]);
            const bool mayStart = !options.handleOnly || !row.active || !io.MouseDown[ImGuiMouseButton_Left] ||
                                  pressOnGrip || dragging;
            if (mayStart)
            {
                if (Has(row.forced, ForcedItemState::Dragging))
                    DragDropForceNextSource(ImVec2(0.0f, rowHeight * kForcedDragRows));
                const ReorderRowPayload payload{listId, i};
                DragDropPreview preview;
                preview.previewText = item.label;
                preview.icon = item.icon.IsValid() ? item.icon : Icon("grip");
                preview.keyboard = true;
                if (DragDropBeginSource(kReorderRowType, &payload, sizeof(payload), preview))
                {
                    lifted = i;
                    DragDropEndSource();
                }
            }
        }
        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    // The zone over every row.
    const ImRect zoneRect(start, ImVec2(start.x + width, start.y + rowHeight * static_cast<float>(count)));
    DragDropZoneOptions zoneOptions;
    zoneOptions.slots = std::span<const ImRect>(rows.slots, static_cast<std::size_t>(count));
    zoneOptions.accept = !readOnly;
    const DragDropZoneResult drop = DragDropZone(zoneId, zoneRect, kReorderRowType, zoneOptions);
    int sourceIndex = -1;
    const ImGuiPayload* payload = drop.payload != nullptr ? drop.payload : (dragging ? drag.payload : nullptr);
    if (payload != nullptr && payload->DataSize == static_cast<int>(sizeof(ReorderRowPayload)))
    {
        const ReorderRowPayload* row = static_cast<const ReorderRowPayload*>(payload->Data);
        if (row->list == listId)
            sourceIndex = row->index;
    }
    const int insert = drop.hovered && sourceIndex >= 0 ? drop.insertIndex : -1;
    const float dragHeight = drag.sourceSize.y > 0.0f ? drag.sourceSize.y : rowHeight;

    // Pass 2: the landing place, then the rows where they move to.
    if (insert >= 0)
    {
        const ImRect gap = DragDropGapRect(zoneOptions.slots, sourceIndex, insert, Orientation::Vertical, dragHeight);
        FillRect(drawList, gap.Min, gap.Max, ToU32(c.fill[2]), radius);
    }
    for (int i = 0; i < count; ++i)
    {
        ChoiceItem item;
        source.Get(i, &item);
        const ImGuiID key = ReorderKey(item, i);
        const ItemState& row = rows.items[i];
        float target = rows.slots[i].Min.y - origin.y;
        if (i == lifted)
            target = drag.pointer.y - drag.grabOffset.y - origin.y;  // after the drop it settles from the ghost
        else
            target += DragDropGapShift(i, sourceIndex, insert, dragHeight);
        const float y = origin.y + DragDropAnimatedPosition(zoneId, key, target);
        if (i == lifted || !row.visible)
            continue;
        const ImRect rect(rows.slots[i].Min.x, y, rows.slots[i].Max.x, y + rowHeight);
        // The shared row look, so a reorderable row hovers and presses like every other list row.
        const ImRect fill = ListRowFillRect(rect);
        ListRowLook rowLook;
        rowLook.hoverAmount = dragging ? 0.0f : row.hoverAmount;
        rowLook.pressAmount = dragging ? 0.0f : row.pressAmount;
        rowLook.disabled = row.disabled;
        DrawListRowFill(drawList, fill, radius, rowLook);
        float x = rect.Min.x + pad;
        if (!readOnly)
        {
            // The grip shows that the row moves; on hover over it the hand cursor says so too.
            DrawControlIcon(drawList, Icon("grip"), x, rect.GetCenter().y, glyph, ToU32(ControlSecondaryColor(row.disabled)));
            x += glyph + pad;
            if (options.handleOnly && !row.disabled && rows.grips[i].Contains(io.MousePos) && row.hovered)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        const ImRect content(x, rect.Min.y, std::max(x, rect.Max.x - pad), rect.Max.y);
        ListsRowLook look;
        const bool truncated = DrawListsRowContent(content, row, item, look, size);
        DrawFocusRingInside(drawList, fill, radius, row.focusAmount);
        CutTextTooltip(row.id, rect, row.hovered, row.focusVisible, item.label, ImGui::FindRenderedTextEnd(item.label),
                       truncated);
    }

    if (drop.delivered && sourceIndex >= 0 && !readOnly)
    {
        const ReorderMove dropped = DragDropReorderMove(sourceIndex, drop.insertIndex);
        if (dropped.IsValid())
        {
            result = dropped;
            changed = true;
            ChoiceItem item;
            source.Get(sourceIndex, &item);
            state.focusKey = item.id != 0 ? item.id : static_cast<ImGuiID>(dropped.to);
            state.focusPending = drop.keyboard;
        }
    }

    window->DC.CursorPos = ImVec2(start.x, zoneRect.Max.y);
    ImGui::Dummy(ImVec2(width, 0.0f));
    ImGui::EndGroup();
    ImGui::PopID();
    FinishWidgetGroup(listId);
    if (changed && move != nullptr)
    {
        *move = result;
        ListsMarkEdited();
        return true;
    }
    return false;
}

} // namespace

bool ReorderableList(const char* id, ReorderMove* move, std::span<const ChoiceItem> items,
                     const ReorderableListOptions& options)
{
    return ReorderableListImpl(id, move, internal::ListsSourceOf(items), options);
}

bool ReorderableList(const char* id, ReorderMove* move, std::span<const char* const> items,
                     const ReorderableListOptions& options)
{
    return ReorderableListImpl(id, move, internal::ListsSourceOf(items), options);
}

} // namespace facet
