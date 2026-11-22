// The tile grid of IconPicker and ThumbnailPicker (icon_picker_parts.h).

#include "widgets/pickers/icon_picker_parts.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/list_core.h"
#include "internal/list_view.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"

namespace facet::internal
{
namespace
{

struct PickerGridState
{
    int focus = -1;        // the tile that had the keyboard focus last
    bool focused = false;  // a tile had the keyboard focus in the last frame
    ImVector<ImRect> drawnRects;  // the tiles laid out in the last frame and their indices
    ImVector<int> drawnIndices;
    int drawnFrame = -1;   // the frame the rects belong to
};

// The line holding tile `index`: the last line whose first tile is at or before it (a header line
// shares its first index with the tile line after it and comes first). -1 when there is none.
int PickerGridRowOf(std::span<const PickerGridRow> rows, int index)
{
    int lo = 0;
    int hi = static_cast<int>(rows.size()) - 1;
    int found = -1;
    while (lo <= hi)
    {
        const int mid = (lo + hi) / 2;
        if (rows[static_cast<std::size_t>(mid)].first <= index)
        {
            found = mid;
            lo = mid + 1;
        }
        else
        {
            hi = mid - 1;
        }
    }
    while (found >= 0 && rows[static_cast<std::size_t>(found)].count == 0)
        --found;
    if (found >= 0)
    {
        const PickerGridRow& row = rows[static_cast<std::size_t>(found)];
        if (index >= row.first + row.count)
            return -1;
    }
    return found;
}

PickerGridTileInfo PickerGridInfo(const PickerGridDesc& desc, int index)
{
    PickerGridTileInfo info;
    if (desc.info != nullptr)
        desc.info(desc.user, index, &info);
    return info;
}

bool PickerGridFocusable(const PickerGridDesc& desc, int index)
{
    return index >= 0 && index < desc.tileCount && !PickerGridInfo(desc, index).disabled;
}

// The first focusable tile from `from` in `direction`, -1 when there is none.
int PickerGridScan(const PickerGridDesc& desc, int from, int direction)
{
    for (int i = from; i >= 0 && i < desc.tileCount; i += direction)
        if (PickerGridFocusable(desc, i))
            return i;
    return -1;
}

} // namespace

void PickerGridBuildRows(PickerGridRows* out, int count, int columns, PickerGridGroupFn group, void* user)
{
    if (out == nullptr)
        return;
    out->rows.resize(0);
    columns = std::max(1, columns);
    const char* current = nullptr;
    int column = 0;
    for (int i = 0; i < count; ++i)
    {
        const char* text = group != nullptr ? group(user, i) : nullptr;
        if (text != nullptr && text[0] == '\0')
            text = nullptr;
        // A new group starts a new line, under its header when it has a name.
        const bool same = (text == nullptr && current == nullptr) ||
                          (text != nullptr && current != nullptr && std::strcmp(text, current) == 0);
        if (i > 0 && !same)
            column = 0;
        if (text != nullptr && (i == 0 || !same))
            out->rows.push_back(PickerGridRow{i, 0, text});
        current = text;
        if (column == 0)
            out->rows.push_back(PickerGridRow{i, 0, nullptr});
        ++out->rows.back().count;
        column = (column + 1) % columns;
    }
}

ImGuiID PickerGridTileId(ImGuiID id, const PickerGridDesc& desc, int index)
{
    const PickerGridTileInfo info = PickerGridInfo(desc, index);
    const ImGuiID key = info.id != 0 ? info.id : static_cast<ImGuiID>(index);
    return ImHashData(&key, sizeof(key), ImHashStr("##tile", 0, id));
}

ImRect PickerGridTileRect(ImGuiID id, int index)
{
    const PickerGridState* state =
        static_cast<const PickerGridState*>(GlobalStateStore().Find(id, StateTypeOf<PickerGridState>()));
    if (state == nullptr || state->drawnFrame != ImGui::GetFrameCount())
        return ImRect();
    for (int i = 0; i < state->drawnIndices.Size; ++i)
        if (state->drawnIndices[i] == index)
            return state->drawnRects[i];
    return ImRect();
}

PickerGridResult PickerGrid(ImGuiID id, const PickerGridDesc& desc)
{
    PickerGridResult result;
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr || id == 0)
        return result;
    ImGuiContext& g = *context;
    ImGuiWindow* window = g.CurrentWindow;
    const Theme& t = T();
    PickerGridState& state = GetState<PickerGridState>(id);
    state.drawnRects.resize(0);
    state.drawnIndices.resize(0);
    state.drawnFrame = g.FrameCount;
    const int rowCount = static_cast<int>(desc.rows.size());
    if (rowCount == 0 || desc.tileCount <= 0)
        return result;
    const float rowHeight = desc.tile.y + desc.gap;
    const float step = desc.tile.x + desc.gap;
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    if (state.focus >= desc.tileCount || desc.resetFocus)
        state.focus = -1;

    // The tile Tab lands on: the focused one, else the chosen one, else the first.
    int tabStop = PickerGridFocusable(desc, state.focus) ? state.focus : -1;
    if (tabStop < 0 && PickerGridFocusable(desc, desc.chosen))
        tabStop = desc.chosen;
    if (tabStop < 0)
        tabStop = PickerGridScan(desc, 0, 1);
    const auto rectOf = [&](int index) {
        const int line = PickerGridRowOf(desc.rows, index);
        if (line < 0)
            return ImRect();
        const float x = origin.x + static_cast<float>(index - desc.rows[static_cast<std::size_t>(line)].first) * step;
        const float y = origin.y + static_cast<float>(line) * rowHeight;
        return ImRect(x, y, x + desc.tile.x, y + desc.tile.y);
    };

    const ImGuiID focusScope = ImGui::GetCurrentFocusScope();
    BeginListRows({.rowCount = rowCount, .rowHeight = rowHeight, .columns = 1,
                   .keepAlive = desc.multi ? -1 : PickerGridRowOf(desc.rows, tabStop)});
    ListRowGroup group;
    group.Begin(desc.forced);
    bool anyFocused = false;
    int part = 1;
    RowRange range;
    while (NextListRows(&range))
    {
        for (int line = range.first; line < range.last; ++line)
        {
            const PickerGridRow& row = desc.rows[static_cast<std::size_t>(line)];
            const float y = origin.y + static_cast<float>(line) * rowHeight;
            if (row.count == 0)
            {
                // A header line: the group text at the bottom, in Footnote and labelSecondary.
                if (row.header != nullptr)
                {
                    ControlTextScope footnote(TextStyle::Footnote);
                    const FittedText shown = FitText(row.header, nullptr, std::max(1.0f, desc.width - PxSnapped(t.spacing.xs)));
                    window->DrawList->AddText(ImVec2(origin.x + PxSnapped(t.spacing.xs),
                                                     y + desc.tile.y - ImGui::GetFontSize() - PxSnapped(t.spacing.xs)),
                                              ToU32(t.colors.labelSecondary), shown.text);
                }
                window->DC.CursorPos = ImVec2(origin.x, y + rowHeight);
                continue;
            }
            for (int k = 0; k < row.count; ++k)
            {
                const int index = row.first + k;
                const float x = origin.x + static_cast<float>(k) * step;
                const ImRect rect(x, y, x + desc.tile.x, y + desc.tile.y);
                const PickerGridTileInfo info = PickerGridInfo(desc, index);
                const bool chosen = desc.multi ? (index >= 0 && index < static_cast<int>(desc.checked.size()) &&
                                                  desc.checked[static_cast<std::size_t>(index)])
                                               : index == desc.chosen;
                ListRowDesc rowDesc;
                rowDesc.selected = chosen;
                rowDesc.disabled = info.disabled;
                rowDesc.tabStop = desc.multi ? !info.disabled : index == tabStop;
                rowDesc.part = index == tabStop ? 0 : ++part;
                const ImGuiID key = info.id != 0 ? info.id : static_cast<ImGuiID>(index);
                const ImGuiID tileId = ImHashData(&key, sizeof(key), ImHashStr("##tile", 0, id));
                const ListRowState tile = group.Row(tileId, rect, rowDesc);
                if (index == tabStop && desc.defaultFocus && !desc.multi)
                    ImGui::SetItemDefaultFocus();
                if (tile.focused)
                {
                    anyFocused = true;
                    state.focus = index;
                    result.focused = index;
                }
                state.drawnRects.push_back(rect);
                state.drawnIndices.push_back(index);
                if (desc.draw != nullptr)
                {
                    PickerGridTile drawn;
                    drawn.index = index;
                    drawn.rect = rect;
                    drawn.item = tile.item;
                    drawn.chosen = chosen;
                    drawn.focused = tile.focused;
                    drawn.disabled = info.disabled;
                    drawn.locked = info.locked;
                    desc.draw(desc.user, drawn);
                }
                if (desc.live && tile.item.pressed && !info.disabled && !info.locked && !desc.readOnly)
                {
                    result.activated = index;
                    result.byKey = tile.item.keyboardPressed;
                    state.focus = index;
                }
            }
            window->DC.CursorPos = ImVec2(origin.x, y + rowHeight);
        }
    }
    group.End();
    EndListRows();
    state.focused = anyFocused;

    // Keys on a focused tile of the single form: two-dimensional movement over the lines.
    if (desc.live && !desc.multi && anyFocused && state.focus >= 0)
    {
        const ImGuiKey keys[] = {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow,
                                 ImGuiKey_PageUp,    ImGuiKey_PageDown,   ImGuiKey_Home,    ImGuiKey_End};
        for (const ImGuiKey key : keys)
            ImGui::SetKeyOwner(key, id);
        const auto pressed = [&](ImGuiKey key) { return ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, id); };
        const int from = state.focus;
        int target = from;
        // The tile in the same column `lines` tile lines away (header lines passed over), at most
        // the last tile of a shorter line.
        const auto vertical = [&](int lines) {
            int line = PickerGridRowOf(desc.rows, from);
            if (line < 0)
                return from;
            const int column = from - desc.rows[static_cast<std::size_t>(line)].first;
            const int direction = lines > 0 ? 1 : -1;
            int moved = 0;
            int last = line;
            for (int l = line + direction; l >= 0 && l < rowCount && moved < std::abs(lines); l += direction)
            {
                if (desc.rows[static_cast<std::size_t>(l)].count == 0)
                    continue;
                last = l;
                ++moved;
            }
            if (last == line)
                return from;
            const PickerGridRow& row = desc.rows[static_cast<std::size_t>(last)];
            const int candidate = row.first + std::min(column, row.count - 1);
            return PickerGridFocusable(desc, candidate) ? candidate : PickerGridScan(desc, candidate, direction);
        };
        if (pressed(ImGuiKey_LeftArrow))
            target = PickerGridScan(desc, from - 1, -1);
        if (pressed(ImGuiKey_RightArrow))
            target = PickerGridScan(desc, from + 1, 1);
        if (pressed(ImGuiKey_UpArrow))
            target = vertical(-1);
        if (pressed(ImGuiKey_DownArrow))
            target = vertical(1);
        const int page = std::max(1, static_cast<int>(window->InnerRect.GetHeight() / rowHeight) - 1);
        if (pressed(ImGuiKey_PageUp))
            target = vertical(-page);
        if (pressed(ImGuiKey_PageDown))
            target = vertical(page);
        if (pressed(ImGuiKey_Home))
            target = PickerGridScan(desc, 0, 1);
        if (pressed(ImGuiKey_End))
            target = PickerGridScan(desc, desc.tileCount - 1, -1);
        if (target >= 0 && target != from)
        {
            state.focus = target;
            const ImRect rect = rectOf(target);
            ListFocusRow(window, PickerGridTileId(id, desc, target), focusScope, rect);
            ScrollAreaRevealRect(window, rect, ScrollAlign::Nearest, true);
        }
    }
    if (desc.takeFocus && tabStop >= 0)
    {
        const ImRect rect = rectOf(tabStop);
        ListFocusRow(window, PickerGridTileId(id, desc, tabStop), focusScope, rect);
        ScrollAreaRevealRect(window, rect, ScrollAlign::Nearest, false);
    }
    if (desc.reveal >= 0 && desc.reveal < desc.tileCount)
        ScrollAreaRevealRect(window, rectOf(desc.reveal), ScrollAlign::Nearest, false);
    return result;
}

} // namespace facet::internal
