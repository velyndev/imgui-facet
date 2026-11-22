// LayerList (facet/widgets/composites.h): ordered layer rows, topmost first, each with a visibility and
// a lock toggle at its end.
//
// Built like the ReorderableList of the lists family, on the drag and drop core (drag_drop.h): every
// row is a drag source (a pointer drag, or Space for a keyboard drag: the arrows move it, Space or
// Enter drop it, Escape puts it back), the list is one ordered drop zone, and an accent line shows
// where the row lands. The caller applies the move (LayerAction::Moved, ReorderMove). Alt+Up and
// Alt+Down move the focused row at once.
//
// Keys: the rows are one Tab stop (the selected row, else the first): Up and Down move the focus and
// the selection, Home and End go to the first and the last row. Tab from the focused row goes on to
// its visibility toggle and its lock toggle (the toggles of the other rows are no Tab stops), then
// out of the list. Space or Enter on a toggle flips it.
//
// The toggles: a click flips one; dragging on from it across the same toggle of other rows paints
// them with the value the first one took. Alt+click on a visibility toggle (with options.solo) shows
// only that layer. A toggle is drawn with its glyph only, in labelSecondary, and its name
// (Strings::hide or show, Strings::lock or unlock) is its tooltip.
//
// Nothing is allocated per frame: the row rectangles of a frame are kept in a fixed scratch of
// kLayerMaxRows rows (longer lists are reported and drawn up to it).

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/drag_drop.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/scroll.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "widgets/composites/editors_frame.h"

namespace facet
{
namespace
{

// The payload type of a layer row; the payload names the list, so another list refuses it.
constexpr const char* kLayerRowType = "facet.layer.row";
constexpr int kLayerMaxRows = 256;
// Rows shown by the loading, error and empty states of a list without a height.
constexpr float kLayerStateRows = 3.0f;
// Captures: how far below its place a forced drag shows the dragged row, in rows.
constexpr float kLayerForcedDragRows = 1.4f;

enum LayerToggle
{
    kLayerVisible = 0,
    kLayerLock = 1,
};

struct LayerRowPayload
{
    ImGuiID list = 0;
    int index = -1;
};

struct LayerListState
{
    ImGuiID focusKey = 0;       // after a move or a key: the row that takes the focus in the next frame
    bool focusPending = false;
    ImGuiID tabKey = 0;         // the row the list's Tab stop is on, while tabKnown
    bool tabKnown = false;
    int paintColumn = -1;       // the toggle a paint drag sets (LayerToggle), -1 none
    bool paintValue = false;
    // The rows of the last frame (LayerListRowRect): the first row's top-left, width and height.
    ImVec2 rowsOrigin;
    float rowWidth = 0.0f;
    float rowHeight = 0.0f;
    int rowCount = 0;
};

struct LayerScratch
{
    ImRect slots[kLayerMaxRows];
    internal::ItemState rows[kLayerMaxRows];
    internal::ItemState toggles[kLayerMaxRows][2];
};

LayerScratch& Scratch()
{
    static LayerScratch scratch;  // one list is laid out at a time; no allocation per frame
    return scratch;
}

ImGuiID LayerKey(const LayerItem& layer, int index)
{
    return layer.id != 0 ? layer.id : static_cast<ImGuiID>(index);
}

bool& LayerToggleValue(LayerItem& layer, int column)
{
    return column == kLayerVisible ? layer.visible : layer.locked;
}

} // namespace

namespace internal
{

ImRect LayerListRowRect(ImGuiID listId, int index, int part, ImGuiID key, ImGuiID* itemId)
{
    if (itemId != nullptr)
    {
        // The ids LayerList gives its parts: the list id, the row's key (PushID of an int), the part.
        const int keyInt = static_cast<int>(key);
        const ImGuiID row = ImHashData(&keyInt, sizeof(keyInt), listId);
        *itemId = ImHashStr(part == 1 ? "##visible" : (part == 2 ? "##lock" : "##row"), 0, row);
    }
    if (ImGui::GetCurrentContext() == nullptr)
        return ImRect();
    const LayerListState& state = GetState<LayerListState>(listId);
    if (index < 0 || index >= state.rowCount)
        return ImRect();
    const float top = state.rowsOrigin.y + state.rowHeight * static_cast<float>(index);
    const ImRect row(state.rowsOrigin.x, top, state.rowsOrigin.x + state.rowWidth, top + state.rowHeight);
    if (part == 1)
        return ImRect(row.Max.x - state.rowHeight * 2.0f, row.Min.y, row.Max.x - state.rowHeight, row.Max.y);
    if (part == 2)
        return ImRect(row.Max.x - state.rowHeight, row.Min.y, row.Max.x, row.Max.y);
    return row;
}

} // namespace internal

LayerListResult LayerList(const char* id, int* selected, std::span<LayerItem> layers, const LayerListOptions& options)
{
    using namespace internal;
    LayerListResult result;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return result;
    if (id == nullptr)
        id = "";
    ImGuiContext& g = *GImGui;
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const Theme& theme = T();
    const Colors& c = theme.colors;
    const Strings& strings = GetStrings();
    const ControlSize size = ResolveControlSize(ControlSize::Inherit);
    const float width = ResolveItemWidth(options.width, next, kFill);
    const float rowHeight = ListRowHeight(size);
    const float radius = Px(theme.radii.control);
    const bool readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const ImGuiID listId = window->GetID(id);
    LayerListState& state = GetState<LayerListState>(listId);
    const ImGuiIO& io = g.IO;

    int count = static_cast<int>(std::min<std::size_t>(layers.size(), kLayerMaxRows));
    if (layers.size() > static_cast<std::size_t>(kLayerMaxRows))
        ReportUsageError("LayerList with %d layers; only the first %d are shown", static_cast<int>(layers.size()), kLayerMaxRows);
    const float fitHeight = rowHeight * static_cast<float>(count);
    const float height = options.height > 0.0f ? Px(options.height)
                                                : (options.state != DataState::Ready || count == 0 ? rowHeight * kLayerStateRows
                                                                                                    : fitHeight);

    ImGui::PushID(id);
    ImGui::BeginGroup();
    const ImVec2 start = window->DC.CursorPos;

    // The loading, error and empty states keep the size and take no input.
    if (options.state != DataState::Ready || count == 0)
    {
        const ImRect rect(start, ImVec2(start.x + width, start.y + height));
        ImGui::Dummy(rect.GetSize());
        DrawListState(rect, options.state, options.emptyText, options.errorText);
        ImGui::EndGroup();
        ImGui::PopID();
        FinishWidgetGroup(listId);
        return result;
    }

    // The row text style, pushed in the caller's window before the scroll area begins, so it is popped
    // there too when the function returns (after the scroll area ended); Dear ImGui checks that a
    // window pops what it pushed.
    ControlTextScope text(ControlTextStyle(size));

    // A set height scrolls the rows in a scroll area of the scroll core.
    const bool scrolled = options.height > 0.0f && fitHeight > height;
    if (scrolled)
    {
        ScrollAreaDesc area;
        area.size = ImVec2(width, height);
        area.padding = 0.0f;
        area.gutter = false;  // the rows below end before the bar themselves, from the first frame on
        if (!BeginScrollAreaCore("##scroll", area))
        {
            ImGui::EndGroup();
            ImGui::PopID();
            FinishWidgetGroup(listId);
            return result;
        }
        window = ImGui::GetCurrentWindow();
    }
    ImDrawList* drawList = window->DrawList;
    const ImVec2 rowsStart = window->DC.CursorPos;
    // While the rows scroll, the overlay bar lies over their end: the rows, and the visibility and
    // lock toggles at their end, stop before the gutter of the bar (scroll.h).
    const float barReserve = scrolled ? ScrollBarGutter() : 0.0f;
    const float rowWidth = scrolled ? std::max(1.0f, ImGui::GetContentRegionAvail().x - barReserve) : width;
    state.rowsOrigin = rowsStart;
    state.rowWidth = rowWidth;
    state.rowHeight = rowHeight;
    state.rowCount = count;

    DragDropInfo drag;
    const bool dragging = DragDropActive(kLayerRowType, &drag);
    const ImGuiID zoneId = ImGui::GetID("##zone");
    const float glyph = text.IconSize();
    const float pad = Px(theme.spacing.sm);
    const float indentStep = Px(theme.spacing.lg);
    const float toggleSize = rowHeight;
    LayerScratch& scratch = Scratch();

    // The row the Tab stop is on: the row that holds the focus (or one of its toggles), else the
    // selected row, else the row that had the focus last, else the first.
    int focusedRow = -1;
    for (int i = 0; i < count; ++i)
    {
        const ImGuiID key = LayerKey(layers[i], i);
        ImGui::PushID(static_cast<int>(key));
        if (g.NavWindow == window && (g.NavId == ImGui::GetID("##row") || g.NavId == ImGui::GetID("##visible") ||
                                      g.NavId == ImGui::GetID("##lock")))
            focusedRow = i;
        ImGui::PopID();
    }
    int tabRow = focusedRow;
    if (tabRow < 0 && selected != nullptr && *selected >= 0 && *selected < count)
        tabRow = *selected;
    for (int i = 0; i < count && tabRow < 0; ++i)
        if (state.tabKnown && LayerKey(layers[i], i) == state.tabKey)
            tabRow = i;
    if (tabRow < 0)
        tabRow = 0;

    // Pass 1: the rows and their toggles as items, the rows as drag sources.
    int lifted = -1;
    int moveFrom = -1;
    int moveTo = -1;
    int selectRow = -1;
    int toggledRow = -1;
    int toggledColumn = -1;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    for (int i = 0; i < count; ++i)
    {
        LayerItem& layer = layers[i];
        const ImGuiID key = LayerKey(layer, i);
        ImGui::PushID(static_cast<int>(key));
        ItemOptions rowOptions;
        rowOptions.hitTarget = false;
        rowOptions.allowOverlap = true;  // the toggles over the row take their own hover and clicks
        rowOptions.tabStop = i == tabRow;
        rowOptions.forced = &forced;
        rowOptions.part = i;
        rowOptions.baseline = std::max(0.0f, (rowHeight - text.FontSize()) * 0.5f);
        const ItemState row = ItemBehavior(ImGui::GetID("##row"), ImVec2(rowWidth, rowHeight), rowOptions);
        scratch.rows[i] = row;
        scratch.slots[i] = row.rect;
        const bool rowActive = !row.disabled && row.forced == ForcedItemState::None;

        if (state.focusPending && key == state.focusKey && !row.focused && rowActive)
        {
            ImGui::SetFocusID(row.id, window);
            ImGui::SetNavCursorVisible(true);
            SetKeyboardModality(true);
        }
        if (row.focused || key == state.focusKey)
            state.focusPending = false;
        if (row.focused)
        {
            state.tabKey = key;
            state.tabKnown = true;
        }
        if (row.pressed && rowActive && !row.keyboardPressed)
            selectRow = i;

        // Keys on the focused row: Up and Down move the focus and the selection, Home and End go to the
        // ends; with Alt, Up and Down move the row itself.
        if (row.focused && g.NavId == row.id && rowActive && !dragging)
        {
            const ImGuiKey keys[] = {ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_Home, ImGuiKey_End};
            for (const ImGuiKey k : keys)
                ImGui::SetKeyOwner(k, row.id);
            const bool up = ImGui::IsKeyPressed(ImGuiKey_UpArrow, ImGuiInputFlags_Repeat, row.id);
            const bool down = ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_Repeat, row.id);
            int to = -1;
            if (up != down)
                to = up ? i - 1 : i + 1;
            if (ImGui::IsKeyPressed(ImGuiKey_Home, ImGuiInputFlags_None, row.id))
                to = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_End, ImGuiInputFlags_None, row.id))
                to = count - 1;
            if (to >= 0 && to < count && to != i)
            {
                if (io.KeyAlt && !readOnly && (up || down))
                {
                    moveFrom = i;
                    moveTo = to;
                    state.focusKey = layer.id != 0 ? key : static_cast<ImGuiID>(to);
                }
                else
                {
                    selectRow = to;
                    state.focusKey = LayerKey(layers[to], to);
                }
                state.focusPending = true;
            }
        }

        // The row is a drag source (the toggles after it are not); not while a paint drag runs.
        if (!readOnly && rowActive && state.paintColumn < 0)
        {
            if (Has(row.forced, ForcedItemState::Dragging))
                DragDropForceNextSource(ImVec2(0.0f, rowHeight * kLayerForcedDragRows));
            const LayerRowPayload payload{listId, i};
            DragDropPreview preview;
            preview.previewText = layer.label;
            preview.icon = layer.icon.IsValid() ? layer.icon : Icon("layers");
            preview.keyboard = true;
            if (DragDropBeginSource(kLayerRowType, &payload, sizeof(payload), preview))
            {
                lifted = i;
                DragDropEndSource();
            }
        }
        // The toggles at the end of the row.
        const float toggleY = row.rect.Min.y;
        const ImRect toggleRects[2] = {
            ImRect(row.rect.Max.x - toggleSize * 2.0f, toggleY, row.rect.Max.x - toggleSize, toggleY + rowHeight),
            ImRect(row.rect.Max.x - toggleSize, toggleY, row.rect.Max.x, toggleY + rowHeight)};
        for (int column = 0; column < 2; ++column)
        {
            ItemOptions toggleOptions;
            toggleOptions.press = PressMode::Click;
            toggleOptions.hitTarget = false;
            toggleOptions.readOnly = readOnly;
            toggleOptions.navigable = i == tabRow;  // only the focused row's toggles take the keyboard
            toggleOptions.forced = &forced;
            toggleOptions.part = count + i * 2 + column;
            const ItemState toggle =
                PartBehavior(ImGui::GetID(column == kLayerVisible ? "##visible" : "##lock"), toggleRects[column], toggleOptions);
            scratch.toggles[i][column] = toggle;
            if (toggle.focused)
            {
                state.tabKey = key;
                state.tabKnown = true;
            }
            const bool real = toggle.forced == ForcedItemState::None && !toggle.disabled && !readOnly;
            if (real && toggle.pressed)
            {
                bool& value = LayerToggleValue(layer, column);
                if (column == kLayerVisible && options.solo && io.KeyAlt && !toggle.keyboardPressed)
                {
                    // Solo: only this layer stays visible.
                    for (int k = 0; k < count; ++k)
                        layers[k].visible = k == i;
                }
                else
                {
                    value = !value;
                    if (!toggle.keyboardPressed)
                    {
                        state.paintColumn = column;
                        state.paintValue = value;
                    }
                }
                toggledRow = i;
                toggledColumn = column;
            }
        }


        ImGui::PopID();
    }
    ImGui::PopStyleVar();

    // A paint drag: the toggle under the pointer in the painted column takes the painted value.
    if (state.paintColumn >= 0)
    {
        if (!io.MouseDown[ImGuiMouseButton_Left] || readOnly)
        {
            state.paintColumn = -1;
        }
        else
        {
            for (int i = 0; i < count; ++i)
            {
                const ImRect& slot = scratch.slots[i];
                if (io.MousePos.y >= slot.Min.y && io.MousePos.y < slot.Max.y)
                {
                    bool& value = LayerToggleValue(layers[i], state.paintColumn);
                    if (value != state.paintValue && !scratch.rows[i].disabled)
                    {
                        value = state.paintValue;
                        toggledRow = i;
                        toggledColumn = state.paintColumn;
                    }
                }
            }
        }
    }

    // The zone over every row.
    const ImRect zoneRect(rowsStart, ImVec2(rowsStart.x + rowWidth, rowsStart.y + fitHeight));
    DragDropZoneOptions zoneOptions;
    zoneOptions.slots = std::span<const ImRect>(scratch.slots, static_cast<std::size_t>(count));
    zoneOptions.accept = !readOnly;
    const DragDropZoneResult drop = DragDropZone(zoneId, zoneRect, kLayerRowType, zoneOptions);
    int sourceIndex = -1;
    const ImGuiPayload* payload = drop.payload != nullptr ? drop.payload : (dragging ? drag.payload : nullptr);
    if (payload != nullptr && payload->DataSize == static_cast<int>(sizeof(LayerRowPayload)))
    {
        const LayerRowPayload* row = static_cast<const LayerRowPayload*>(payload->Data);
        if (row->list == listId)
            sourceIndex = row->index;
    }
    const int insert = drop.hovered && sourceIndex >= 0 ? drop.insertIndex : -1;

    // Pass 2: draw.
    for (int i = 0; i < count; ++i)
    {
        const LayerItem& layer = layers[i];
        const ItemState& row = scratch.rows[i];
        if (!row.visible)
            continue;
        const ImRect rect = scratch.slots[i];
        const bool isSelected = selected != nullptr && *selected == i;
        if (i == lifted)
        {
            // The row's place while its ghost is dragged.
            FillRect(drawList, rect.Min, rect.Max, ToU32(c.fill[3]), radius);
            continue;
        }
        // The shared row look: fill[2] under the pointer, accentSubtle while selected (with or without
        // the list's focus), no outline. No hover feedback while a row is carried.
        ListRowLook look;
        look.hoverAmount = dragging ? 0.0f : row.hoverAmount;
        look.pressAmount = dragging ? 0.0f : row.pressAmount;
        look.selected = isSelected;
        look.disabled = row.disabled;
        DrawListRowFill(drawList, rect, radius, look);
        const bool hidden = !layer.visible;
        const ImU32 secondary = ToU32(ControlSecondaryColor(row.disabled));
        float x = rect.Min.x + pad + indentStep * static_cast<float>(std::max(0, layer.depth));
        const float centerY = rect.GetCenter().y;
        if (layer.icon.IsValid())
        {
            DrawControlIcon(drawList, layer.icon, x, centerY, glyph, secondary);
            x += glyph + pad;
        }
        // The label, in semibold while selected (the cue that does not depend on color), in
        // labelSecondary while the layer is hidden.
        const float labelRight = rect.Max.x - toggleSize * 2.0f - pad;
        PushPillLabelStyle(ControlTextStyle(size), isSelected);
        const char* labelEnd = layer.label != nullptr ? ImGui::FindRenderedTextEnd(layer.label) : nullptr;
        const FittedText shown = FitText(layer.label != nullptr ? layer.label : "", labelEnd, std::max(0.0f, labelRight - x));
        drawList->AddText(ImVec2(x, CenteredTextY(rect.Min.y, rowHeight)),
                          ToU32(hidden ? ControlSecondaryColor(row.disabled) : ControlLabelColor(row.disabled)), shown.text);
        PopTextStyle();

        // The toggles: the glyph of the state, its name as the tooltip.
        for (int column = 0; column < 2; ++column)
        {
            const ItemState& toggle = scratch.toggles[i][column];
            const bool on = column == kLayerVisible ? layer.visible : layer.locked;
            const Icon icon = column == kLayerVisible ? Icon(on ? "eye" : "eye-off") : Icon(on ? "lock" : "unlock");
            const float hover = toggle.disabled ? 0.0f : std::max(toggle.hoverAmount, toggle.pressAmount);
            if (hover > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[2], c.fill[1], toggle.pressAmount);
                FillRect(drawList, toggle.rect.Min, toggle.rect.Max, ToU32(WithAlpha(tint, tint.w * hover)), radius);
            }
            // A set lock is drawn in label, everything else in labelSecondary: both read as text.
            const ImU32 color = ToU32(column == kLayerLock && on ? ControlLabelColor(toggle.disabled)
                                                                 : ControlSecondaryColor(toggle.disabled));
            DrawControlIcon(drawList, icon, std::round(toggle.rect.GetCenter().x - glyph * 0.5f), centerY, glyph, color);
            // Rings inside the row's bounds (the toggle's and the row's), so neighbouring rows never
            // cover them and the list edge never cuts them.
            DrawItemFocusRingInside(toggle, radius);
            const char* name = column == kLayerVisible ? (on ? strings.hide : strings.show) : (on ? strings.unlock : strings.lock);
            CutTextTooltip(toggle, name, nullptr, true);
        }
        DrawFocusRingInside(drawList, rect, radius, row.focusAmount);
        CutTextTooltip(row, layer.label, labelEnd, shown.truncated);
    }
    if (insert >= 0)
        DragDropDrawInsertLine(drawList, DragDropInsertLineRect(zoneOptions.slots, insert, Orientation::Vertical));

    if (drop.delivered && sourceIndex >= 0 && !readOnly)
    {
        const ReorderMove dropped = DragDropReorderMove(sourceIndex, drop.insertIndex);
        if (dropped.IsValid())
        {
            moveFrom = dropped.from;
            moveTo = dropped.to;
            state.focusKey = layers[sourceIndex].id != 0 ? layers[sourceIndex].id : static_cast<ImGuiID>(dropped.to);
            state.focusPending = drop.keyboard;
        }
    }

    window->DC.CursorPos = ImVec2(rowsStart.x, zoneRect.Max.y);
    ImGui::Dummy(ImVec2(rowWidth, 0.0f));
    if (scrolled)
        EndScrollAreaCore();
    ImGui::EndGroup();
    ImGui::PopID();
    FinishWidgetGroup(listId);

    // One result per frame: a move first, then a toggle, then the selection.
    if (moveFrom >= 0 && moveTo >= 0)
    {
        result.action = LayerAction::Moved;
        result.move = ReorderMove{moveFrom, moveTo};
        // The selection follows the layers the move shifts, so it stays on the same layer once the
        // caller applies the move.
        if (selected != nullptr && *selected >= 0)
        {
            int& s = *selected;
            if (s == moveFrom)
                s = moveTo;
            else if (moveFrom < moveTo && s > moveFrom && s <= moveTo)
                --s;
            else if (moveTo < moveFrom && s >= moveTo && s < moveFrom)
                ++s;
        }
    }
    else if (toggledRow >= 0)
    {
        result.action = toggledColumn == kLayerVisible ? LayerAction::Visibility : LayerAction::Lock;
        result.index = toggledRow;
    }
    else if (selectRow >= 0 && selected != nullptr && *selected != selectRow)
    {
        *selected = selectRow;
        result.action = LayerAction::Selected;
    }
    if (result.action != LayerAction::None)
        MarkWidgetEdited(listId);
    return result;
}

} // namespace facet
