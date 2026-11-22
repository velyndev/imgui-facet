// SwatchPalette: a grid of color swatches with an optional row of recent colors above it. The whole
// palette is one item and one Tab stop; the arrow keys move a focused swatch in two dimensions
// (Up and Down to the swatch above or below, across the recent row and the grid), Home and End go
// to the first and last swatch, Enter or Space picks the focused swatch, a click picks the swatch
// under the pointer. The swatch equal to the color shows the selection ring and a check mark, so
// the choice does not rest on color alone. The editable forms add a trailing + swatch that adds
// the color, Delete or Backspace removes the focused swatch, Alt with Left or Right moves it, and
// a drag moves a swatch to another place.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cmath>
#include <new>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/pickers/color_model.h"

namespace facet
{
namespace
{

// The accent outline of the chosen swatch sits this many hairlines outside the swatch, so it reads
// against the surface whatever the swatch color is; it is one hairline wide like every outline.
constexpr float kSwatchRingGap = 1.0f;
// The line that marks where a dragged swatch lands: two hairlines, a mark (not an outline) that
// must read between swatches of any color.
constexpr float kSwatchDropLineWidth = 2.0f;
// The check mark of the chosen swatch takes this share of the swatch side.
constexpr float kSwatchCheckShare = 0.6f;
// A held swatch shrinks by this many hairlines on each side.
constexpr float kSwatchPressInset = 1.0f;

struct SwatchPaletteState
{
    int focus = -1;        // the focused cell; -1 until the palette first has the focus
    int dragFrom = -1;     // the grid swatch held for a drag
    bool dragging = false; // the held swatch moved past the drag threshold
};

// The swatches the palette shows and, in the editable forms, changes.
struct SwatchList
{
    const ImVec4* view = nullptr;   // the swatches
    ImVec4* data = nullptr;         // writable storage (editable forms)
    int count = 0;
    int capacity = 0;
    std::vector<ImVec4>* vector = nullptr;
    int* countOut = nullptr;
    bool editable = false;

    bool CanAdd() const { return editable && (vector != nullptr || count < capacity); }

    bool Add(const ImVec4& color)
    {
        if (!CanAdd())
            return false;
        if (vector != nullptr)
        {
            try
            {
                vector->push_back(color);
            }
            catch (const std::bad_alloc&)
            {
                return false;
            }
            view = data = vector->data();
            count = static_cast<int>(vector->size());
            return true;
        }
        data[count++] = color;
        *countOut = count;
        return true;
    }

    void Remove(int index)
    {
        if (!editable || index < 0 || index >= count)
            return;
        if (vector != nullptr)
        {
            vector->erase(vector->begin() + index);
            view = data = vector->data();
            count = static_cast<int>(vector->size());
            return;
        }
        for (int i = index; i + 1 < count; ++i)
            data[i] = data[i + 1];
        --count;
        *countOut = count;
    }

    void Move(int from, int to)
    {
        if (!editable || from < 0 || from >= count || to < 0 || to >= count || from == to)
            return;
        const ImVec4 moved = data[from];
        if (from < to)
            for (int i = from; i < to; ++i)
                data[i] = data[i + 1];
        else
            for (int i = from; i > to; --i)
                data[i] = data[i - 1];
        data[to] = moved;
    }
};

// Where the cells are: the recent row first (wrapping), then the grid; index h + j is grid cell j.
struct SwatchLayout
{
    ImVec2 origin;
    float side = 0.0f;
    float gap = 0.0f;
    float sectionGap = 0.0f;
    int columns = 1;
    int history = 0;    // recent cells
    int grid = 0;       // grid cells, the + cell included
    int historyRows = 0;
    int gridRows = 0;

    int Total() const { return history + grid; }
    int Rows() const { return historyRows + gridRows; }
    // The visual row and column of cell `i`.
    void Place(int i, int* row, int* column) const
    {
        if (i < history)
        {
            *row = i / columns;
            *column = i % columns;
            return;
        }
        const int j = i - history;
        *row = historyRows + j / columns;
        *column = j % columns;
    }
    int RowLength(int row) const
    {
        if (row < historyRows)
            return std::min(columns, history - row * columns);
        const int r = row - historyRows;
        return std::min(columns, grid - r * columns);
    }
    int CellAt(int row, int column) const
    {
        row = std::clamp(row, 0, std::max(0, Rows() - 1));
        column = std::clamp(column, 0, std::max(0, RowLength(row) - 1));
        if (row < historyRows)
            return row * columns + column;
        return history + (row - historyRows) * columns + column;
    }
    ImRect Rect(int i) const
    {
        int row = 0, column = 0;
        Place(i, &row, &column);
        float y = origin.y + static_cast<float>(row) * (side + gap);
        if (row >= historyRows && historyRows > 0)
            y += sectionGap - gap;
        const float x = origin.x + static_cast<float>(column) * (side + gap);
        return ImRect(x, y, x + side, y + side);
    }
    float Height() const
    {
        const int rows = Rows();
        if (rows == 0)
            return side;
        float height = static_cast<float>(rows) * (side + gap) - gap;
        if (historyRows > 0 && gridRows > 0)
            height += sectionGap - gap;
        return height;
    }
};

void PushHistory(ColorHistory* history, const ImVec4& color, bool alpha)
{
    if (history == nullptr)
        return;
    int count = std::clamp(history->count, 0, ColorHistory::kCapacity);
    int found = -1;
    for (int i = 0; i < count; ++i)
    {
        if (internal::ColorSame(history->colors[i], color, alpha))
            found = i;
    }
    const int last = found >= 0 ? found : std::min(count, ColorHistory::kCapacity - 1);
    for (int i = last; i > 0; --i)
        history->colors[i] = history->colors[i - 1];
    history->colors[0] = color;
    if (found < 0)
        count = std::min(count + 1, ColorHistory::kCapacity);
    history->count = count;
}

bool RunSwatchPalette(const char* label, ImVec4* color, SwatchList list, ColorHistory* history,
                      const SwatchPaletteOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    const bool readOnly = options.readOnly || color == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    SwatchPaletteState& state = internal::GetState<SwatchPaletteState>(id);

    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    SwatchLayout layout;
    layout.side = std::round(ControlHeight(options.size));
    layout.gap = std::round(Px(t.spacing.xs));
    layout.sectionGap = std::round(Px(t.spacing.sm));
    layout.columns = options.columns > 0 ? options.columns
                                         : static_cast<int>((width + layout.gap) / (layout.side + layout.gap));
    layout.columns = std::max(1, layout.columns);
    // A fixed column count that does not fit the width makes the swatches smaller instead of wider
    // than the palette.
    const float fitted = std::floor((width - layout.gap * static_cast<float>(layout.columns - 1)) / static_cast<float>(layout.columns));
    layout.side = std::max(1.0f, std::min(layout.side, fitted));
    layout.history = history != nullptr ? std::clamp(history->count, 0, ColorHistory::kCapacity) : 0;
    const bool addCell = list.CanAdd() && !readOnly;
    layout.grid = list.count + (addCell ? 1 : 0);
    layout.historyRows = (layout.history + layout.columns - 1) / layout.columns;
    layout.gridRows = (layout.grid + layout.columns - 1) / layout.columns;
    layout.origin = ImVec2(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    // A palette narrower than its columns (a fixed column count) is cut by its width, never wider.
    const ImVec2 size(width, layout.Height());

    internal::ApplyNextItemData(next);
    internal::ItemOptions itemOptions;
    itemOptions.readOnly = readOnly;
    itemOptions.hitTarget = false;
    const internal::ItemState item = internal::ItemBehavior(id, size, itemOptions);
    const int total = layout.Total();
    if (state.focus >= total)
        state.focus = total - 1;

    const ImVec4 current = color != nullptr ? *color : t.colors.accent;
    // The cell showing the chosen color: the first grid swatch equal to it, else a recent one.
    int chosen = -1;
    for (int j = 0; j < list.count && chosen < 0; ++j)
    {
        if (internal::ColorSame(list.view[j], current, options.alpha))
            chosen = layout.history + j;
    }
    for (int i = 0; i < layout.history && chosen < 0; ++i)
    {
        if (internal::ColorSame(history->colors[i], current, options.alpha))
            chosen = i;
    }
    const auto cellColor = [&](int i) -> ImVec4 {
        if (i < layout.history)
            return history->colors[i];
        return list.view[i - layout.history];
    };
    const auto isAddCell = [&](int i) { return addCell && i == total - 1; };
    const auto cellUnder = [&](ImVec2 point) {
        for (int i = 0; i < total; ++i)
        {
            if (layout.Rect(i).Contains(point))
                return i;
        }
        return -1;
    };

    bool written = false;
    bool listChanged = false;
    const auto pick = [&](int i) {
        if (readOnly || i < 0 || i >= total)
            return;
        if (isAddCell(i))
        {
            if (list.Add(current))
            {
                listChanged = true;
                state.focus = i;
            }
            return;
        }
        const ImVec4 picked = cellColor(i);
        const ImVec4 out = options.alpha ? picked : ImVec4(picked.x, picked.y, picked.z, current.w);
        if (!internal::ColorSame(out, current, true))
        {
            *color = out;
            written = true;
        }
        PushHistory(history, out, options.alpha);
    };

    // ---- Keyboard: the focused cell ----
    const bool focused = item.focused && g.NavId == id && !item.disabled && item.forced == internal::ForcedItemState::None;
    if (focused && total > 0)
    {
        if (state.focus < 0)
            state.focus = chosen >= 0 ? chosen : 0;
        static constexpr ImGuiKey kKeys[] = {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow,
                                             ImGuiKey_Home,      ImGuiKey_End,        ImGuiKey_Delete,  ImGuiKey_Backspace};
        for (const ImGuiKey key : kKeys)
            ImGui::SetKeyOwner(key, id);
        const auto pressed = [&](ImGuiKey key) { return ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, id); };
        int row = 0, column = 0;
        layout.Place(state.focus, &row, &column);
        int target = state.focus;
        const bool alt = g.IO.KeyAlt;
        const int focusGrid = state.focus - layout.history;
        const bool movable = list.editable && !readOnly && focusGrid >= 0 && focusGrid < list.count;
        if (pressed(ImGuiKey_LeftArrow))
        {
            if (alt && movable && focusGrid > 0)
            {
                list.Move(focusGrid, focusGrid - 1);
                listChanged = true;
                target = state.focus - 1;
            }
            else if (!alt)
            {
                target = std::max(0, state.focus - 1);
            }
        }
        if (pressed(ImGuiKey_RightArrow))
        {
            if (alt && movable && focusGrid + 1 < list.count)
            {
                list.Move(focusGrid, focusGrid + 1);
                listChanged = true;
                target = state.focus + 1;
            }
            else if (!alt)
            {
                target = std::min(total - 1, state.focus + 1);
            }
        }
        if (pressed(ImGuiKey_UpArrow) && row > 0)
            target = layout.CellAt(row - 1, column);
        if (pressed(ImGuiKey_DownArrow) && row + 1 < layout.Rows())
            target = layout.CellAt(row + 1, column);
        if (pressed(ImGuiKey_Home))
            target = 0;
        if (pressed(ImGuiKey_End))
            target = total - 1;
        if ((pressed(ImGuiKey_Delete) || pressed(ImGuiKey_Backspace)) && movable)
        {
            list.Remove(focusGrid);
            listChanged = true;
        }
        if (target != state.focus)
        {
            state.focus = target;
            internal::SetKeyboardModality(true);
            ImGui::SetNavCursorVisible(true);
        }
        if (item.pressed && item.keyboardPressed)
            pick(state.focus);
    }
    const int totalNow = layout.history + list.count + (list.CanAdd() && !readOnly ? 1 : 0);
    if (listChanged)
    {
        layout.grid = totalNow - layout.history;
        layout.gridRows = (layout.grid + layout.columns - 1) / layout.columns;
        state.focus = std::clamp(state.focus, 0, std::max(0, totalNow - 1));
    }

    // ---- Mouse: a click picks, a drag of a grid swatch moves it ----
    const float threshold = Px(t.drag.startThreshold);
    if (item.activated && !item.keyboardPressed && g.ActiveIdSource == ImGuiInputSource_Mouse)
    {
        const int under = cellUnder(g.IO.MouseClickedPos[ImGuiMouseButton_Left]);
        state.dragFrom = list.editable && !readOnly && under >= layout.history && under < layout.history + list.count
                             ? under - layout.history
                             : -1;
        state.dragging = false;
        if (under >= 0)
            state.focus = under;
    }
    if (item.held && state.dragFrom >= 0 && !state.dragging && ImGui::IsMouseDragPastThreshold(ImGuiMouseButton_Left, threshold))
        state.dragging = true;
    int dropAt = -1;
    if (state.dragging && state.dragFrom >= 0 && state.dragFrom < list.count)
    {
        // The grid swatch nearest the pointer is where the held one goes.
        float best = FLT_MAX;
        for (int j = 0; j < list.count; ++j)
        {
            const ImVec2 c = layout.Rect(layout.history + j).GetCenter();
            const float dx = c.x - g.IO.MousePos.x;
            const float dy = c.y - g.IO.MousePos.y;
            const float d = dx * dx + dy * dy;
            if (d < best)
            {
                best = d;
                dropAt = j;
            }
        }
    }
    if (item.released || (!item.held && state.dragFrom >= 0))
    {
        if (state.dragging && dropAt >= 0 && dropAt != state.dragFrom)
        {
            list.Move(state.dragFrom, dropAt);
            listChanged = true;
            state.focus = layout.history + dropAt;
        }
        else if (item.pressed && !state.dragging)
        {
            pick(cellUnder(g.IO.MousePos));
        }
        state.dragFrom = -1;
        state.dragging = false;
    }
    else if (item.pressed && !item.keyboardPressed && state.dragFrom < 0)
    {
        pick(cellUnder(g.IO.MousePos));
    }
    if (written || listChanged)
        internal::MarkWidgetEdited(id);

    // ---- Draw ----
    const int drawTotal = layout.history + list.count + (list.CanAdd() && !readOnly ? 1 : 0);
    layout.grid = drawTotal - layout.history;
    layout.gridRows = (layout.grid + layout.columns - 1) / layout.columns;
    const ImVec4 shownColor = color != nullptr ? *color : current;
    const int hovered = item.hovered && !item.disabled ? cellUnder(g.IO.MousePos) : -1;
    if (item.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const float radius = std::min(Px(t.radii.element), layout.side * 0.5f);
        const float hairline = HairlineThickness();
        if (drawTotal == 0)
        {
            internal::ControlTextScope text(TextStyle::Footnote);
            drawList->AddText(ImVec2(item.rect.Min.x, CenteredTextY(item.rect.Min.y, layout.side)),
                              ToU32(internal::ControlSecondaryColor(item.disabled)), GetStrings().empty);
        }
        int chosenNow = -1;
        for (int j = 0; j < list.count && chosenNow < 0; ++j)
        {
            if (internal::ColorSame(list.view[j], shownColor, options.alpha))
                chosenNow = layout.history + j;
        }
        for (int i = 0; i < layout.history && chosenNow < 0; ++i)
        {
            if (internal::ColorSame(history->colors[i], shownColor, options.alpha))
                chosenNow = i;
        }
        for (int i = 0; i < drawTotal; ++i)
        {
            ImRect rect = layout.Rect(i);
            const bool held = item.held && state.focus == i && !state.dragging && g.ActiveIdSource == ImGuiInputSource_Mouse;
            if (held)
                rect.Expand(-hairline * kSwatchPressInset);
            const bool dragged = state.dragging && i == layout.history + state.dragFrom;
            if (i == drawTotal - 1 && list.CanAdd() && !readOnly)
            {
                // The + cell: a neutral fill with the plus glyph.
                const internal::ItemState look = [&] {
                    internal::ItemState s = item;
                    s.hoverAmount = hovered == i ? 1.0f : 0.0f;
                    s.pressAmount = held ? 1.0f : 0.0f;
                    return s;
                }();
                FillRect(drawList, rect.Min, rect.Max, ToU32(internal::NeutralFill(look)), radius);
                const float icon = std::round(layout.side * 0.5f);
                internal::DrawControlIcon(drawList, Icon("plus"), std::round(rect.GetCenter().x - icon * 0.5f),
                                          rect.GetCenter().y, icon, ToU32(internal::ControlSecondaryColor(item.disabled)));
            }
            else
            {
                const ImVec4 c = cellColor(i);
                if (item.disabled)
                {
                    const ImVec4 dim = WithAlpha(internal::ColorOnChecker(c), internal::kColorDisabledOpacity);
                    FillRect(drawList, rect.Min, rect.Max, ToU32(dim), radius);
                }
                else if (dragged)
                {
                    // The place the held swatch left, until it is dropped.
                    FillRect(drawList, rect.Min, rect.Max, ToU32(t.colors.fill[2]), radius);
                }
                else
                {
                    internal::DrawColorSwatch(drawList, rect, c, radius, true);
                }
                // The one controlBorder hairline of a tile, so a swatch of the surface color still
                // shows its shape; controlBorderHover under the pointer.
                internal::OutlineLook edge;
                edge.hoverAmount = hovered == i && !readOnly ? 1.0f : 0.0f;
                edge.disabled = item.disabled;
                internal::DrawOutline(drawList, rect, radius, internal::ControlOutlineColor(edge));
                if (i == chosenNow && !dragged)
                {
                    // Chosen: the accent hairline one gap outside the swatch and the check mark on it.
                    const float out = hairline * (kSwatchRingGap + 1.0f);
                    ImRect ring = rect;
                    ring.Expand(out);
                    internal::OutlineLook chosenEdge;
                    chosenEdge.selected = true;
                    chosenEdge.disabled = item.disabled;
                    internal::DrawOutline(drawList, ring, radius + out, internal::ControlOutlineColor(chosenEdge));
                    const float icon = std::round(layout.side * kSwatchCheckShare);
                    internal::DrawControlIcon(drawList, Icon("check"), std::round(rect.GetCenter().x - icon * 0.5f),
                                              rect.GetCenter().y, icon, ToU32(ReadableOn(internal::ColorOnChecker(c))));
                }
            }
            if (i == state.focus && item.focusVisible)
                internal::DrawFocusRing(drawList, layout.Rect(i), radius, item.focusAmount);
        }
        // The held swatch follows the pointer while it is dragged, and a line marks where it goes.
        if (state.dragging && state.dragFrom >= 0 && state.dragFrom < list.count)
        {
            if (dropAt >= 0)
            {
                const ImRect target = layout.Rect(layout.history + dropAt);
                const float x = dropAt > state.dragFrom ? target.Max.x + layout.gap * 0.5f : target.Min.x - layout.gap * 0.5f;
                drawList->AddLine(ImVec2(std::round(x), target.Min.y), ImVec2(std::round(x), target.Max.y),
                                  ToU32(t.colors.focusRing), hairline * kSwatchDropLineWidth);
            }
            const ImVec2 half(layout.side * 0.5f, layout.side * 0.5f);
            const ImRect ghost(g.IO.MousePos.x - half.x, g.IO.MousePos.y - half.y, g.IO.MousePos.x + half.x,
                               g.IO.MousePos.y + half.y);
            ImDrawList* foreground = ImGui::GetForegroundDrawList();
            const ImVec4 c = list.view[state.dragFrom];
            FillRect(foreground, ghost.Min, ghost.Max,
                     ToU32(WithAlpha(internal::ColorOnChecker(c), t.drag.ghostOpacity)), radius);
        }
    }

    // The palette's label is its tooltip; a hovered swatch names the + cell. A grid swatch with a
    // name in options.names shows it on hover and on keyboard focus, so keyboard users read it too.
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const auto swatchName = [&](int cell) -> const char* {
        const int j = cell - layout.history;
        if (cell < 0 || j < 0 || j >= list.count || j >= static_cast<int>(options.names.size()))
            return nullptr;
        const char* name = options.names[static_cast<std::size_t>(j)];
        return name != nullptr && name[0] != '\0' ? name : nullptr;
    };
    const int namedCell = hovered >= 0 ? hovered : (item.focusVisible ? state.focus : -1);
    if (hovered >= 0 && hovered == drawTotal - 1 && list.CanAdd() && !readOnly)
    {
        if (internal::BeginPopupTooltip(id, layout.Rect(hovered), true, false))
        {
            Text(TextStyle::Footnote, "%s", GetStrings().add);
            internal::EndPopupTooltip();
        }
    }
    else if (const char* name = swatchName(namedCell); name != nullptr && !state.dragging)
    {
        if (internal::BeginPopupTooltip(id, layout.Rect(namedCell), hovered >= 0, hovered < 0 && item.focusVisible))
        {
            Text(TextStyle::Footnote, "%s", name);
            internal::EndPopupTooltip();
        }
    }
    else
    {
        internal::CutTextTooltip(item, label, labelEnd, labelEnd != label);
    }
    return (written || listChanged) && !readOnly;
}

} // namespace

bool SwatchPalette(const char* label, ImVec4* color, std::span<const ImVec4> swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options)
{
    SwatchList list;
    list.view = swatches.data();
    list.count = static_cast<int>(swatches.size());
    return RunSwatchPalette(label, color, list, history, options);
}

bool SwatchPalette(const char* label, ImVec4* color, std::span<ImVec4> storage, int* count, ColorHistory* history,
                   const SwatchPaletteOptions& options)
{
    SwatchList list;
    list.view = list.data = storage.data();
    list.capacity = static_cast<int>(storage.size());
    list.count = count != nullptr ? std::clamp(*count, 0, list.capacity) : 0;
    list.countOut = count;
    list.editable = count != nullptr;
    return RunSwatchPalette(label, color, list, history, options);
}

bool SwatchPalette(const char* label, ImVec4* color, std::vector<ImVec4>* swatches, ColorHistory* history,
                   const SwatchPaletteOptions& options)
{
    SwatchList list;
    if (swatches != nullptr)
    {
        list.view = list.data = swatches->data();
        list.count = static_cast<int>(swatches->size());
        list.vector = swatches;
        list.editable = true;
    }
    return RunSwatchPalette(label, color, list, history, options);
}

} // namespace facet
