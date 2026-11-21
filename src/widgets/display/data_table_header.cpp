// The header row of DataTable (display.h): Facet header cells in place of Dear ImGui's
// TableHeadersRow, on the same table state, so sorting, order, widths and visibility stay Dear
// ImGui's (and are saved in its ini file).
//
//   - A header cell is one item (PartBehavior over the cell) in the table's main navigation
//     layer, a Tab stop in display order. A click, Enter or Space sorts by its column
//     (TableSetColumnSortDirection with the column's next direction; Shift adds the column with
//     multiSort). Dragging it sideways past a neighbour moves the column there, as Dear ImGui's
//     own header does (TableQueueSetColumnDisplayOrder; never across the frozen columns).
//   - Look: the visible label part in Footnote, labelSecondary, or label for a sorted column,
//     cut with an ellipsis (the whole label on hover and focus) and placed by the column's align;
//     a sorted column shows an arrow for its direction (up ascending, down descending) and, while
//     more than one column sorts, its place in the order; hover and press fill the cell inset by
//     spacing.xs; the focus ring goes around that fill.
//   - Between columns a hairline divider; on the edge of a resizable column a band of the
//     table.resizeBand token that resizes it by drag (a double click fits it to its content). The
//     divider turns into a focusRing line while its band is hovered or dragged. The narrowest width
//     is the table.minColumnWidth token (BeginDataTable sets it for the layout).
//   - The column menu (a right click on a header or after the last one, or the Menu key or
//     Shift+F10 on a focused header): Strings::sortAscending and Strings::sortDescending for a
//     sortable column, and the Strings::columns submenu that shows and hides columns (hideable);
//     a column with NoHide, and the last shown one, cannot be hidden.
// Header items and drawing use the background channel with the clip of their column, so a column
// that scrolls never draws over the frozen ones.

#include <algorithm>
#include <cmath>
#include <cstdio>

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
#include "internal/popup_menu.h"
#include "widgets/display/data_table_internal.h"

namespace facet::internal
{
namespace
{

// The line a hovered or dragged resize band shows, in hairlines.
constexpr float kDataTableResizeLineHairlines = 2.0f;

// What a header cell or the empty header area asked of the column menu this frame.
struct DataTableMenuRequest
{
    bool open = false;
    bool forced = false;  // a capture's Open state: shown for this frame only
    int column = -1;      // -1: the menu of the header row (no column)
    ImRect anchor;
};

ImGuiID DataTableMenuId(const DataTableFrame& frame)
{
    return ImHashStr("##facet.columns", 0, frame.id);
}

ImGuiID DataTableColumnsMenuId(const DataTableFrame& frame)
{
    return ImHashStr("##facet.columns.list", 0, frame.id);
}

bool DataTableHasMenu(const DataTableFrame& frame)
{
    return frame.sortable || frame.hideable;
}

// The width of the sort mark of a sorted column: the arrow, and its place in the order while
// more than one column sorts. `order` receives the text of that place.
float DataTableSortMarkWidth(const DataTableFrame& frame, const ImGuiTableColumn& column, float iconSize, char* order,
                             int orderSize)
{
    order[0] = '\0';
    if (column.SortOrder < 0)
        return 0.0f;
    float width = iconSize;
    if (frame.table->SortSpecsCount > 1)
    {
        std::snprintf(order, static_cast<std::size_t>(orderSize), "%d", column.SortOrder + 1);
        width += Px(T().spacing.xs) + ImGui::CalcTextSize(order).x;
    }
    return width;
}

void DataTableHeaderCell(DataTableFrame& frame, int n, const DataTableColumn& declared, float top, float bottom,
                         DataTableMenuRequest* menu)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = frame.table;
    ImGuiWindow* window = frame.window;
    const ImGuiTableColumn& column = table->Columns[n];
    const Theme& theme = T();
    const Colors& c = theme.colors;

    const ImRect cell(column.MinX, top, column.MaxX, bottom);
    ImRect hit = cell;
    hit.Min.x = std::max(hit.Min.x, column.ClipRect.Min.x);
    hit.Max.x = std::min(hit.Max.x, column.ClipRect.Max.x);
    if (hit.Max.x <= hit.Min.x)
        return;
    const float inset = Px(theme.spacing.xs);
    const ImRect fill(cell.Min.x + inset, cell.Min.y + inset, std::max(cell.Min.x + inset, cell.Max.x - inset),
                      cell.Max.y - inset);
    const bool sortable = frame.sortable && (column.Flags & ImGuiTableColumnFlags_NoSort) == 0;
    const bool interactive = sortable || DataTableHasMenu(frame) || frame.reorderable;

    // Header cells sit in the table's main navigation layer: Dear ImGui puts the frozen header row
    // in the menu layer, which Tab never reaches.
    window->DC.NavLayerCurrent = static_cast<ImGuiNavLayer>(table->NavLayer);
    ItemOptions options;
    options.navigable = interactive;
    options.tabStop = interactive;
    options.hitTarget = false;
    options.allowOverlap = true;  // the resize bands submitted after it take the pointer on the edges
    options.handCursor = interactive;
    options.part = kDataTableHeaderPart + n;
    options.forced = &frame.forced;
    const ItemState item = PartBehavior(window->GetID("##header"), hit, options);
    const bool real = item.forced == ForcedItemState::None;

    if (real && item.held)
        table->HeldHeaderColumn = static_cast<ImGuiTableColumnIdx>(n);
    // Reordering by drag, as Dear ImGui's header does: past a neighbour's edge, the column moves there.
    if (real && item.dragging && frame.reorderable && ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
        !g.DragDropActive)
    {
        table->InstanceInteracted = table->InstanceCurrent;
        const ImGuiIO& io = g.IO;
        if (io.MouseDelta.x < 0.0f && io.MousePos.x < cell.Min.x && column.PrevEnabledColumn != -1)
            ImGui::TableQueueSetColumnDisplayOrder(table, n, table->Columns[column.PrevEnabledColumn].DisplayOrder);
        if (io.MouseDelta.x > 0.0f && io.MousePos.x > cell.Max.x && column.NextEnabledColumn != -1)
            ImGui::TableQueueSetColumnDisplayOrder(table, n, table->Columns[column.NextEnabledColumn].DisplayOrder);
    }
    if (real && item.pressed && sortable && table->ReorderColumn != n)
        ImGui::TableSetColumnSortDirection(n, ImGui::TableGetColumnNextSortDirection(&table->Columns[n]),
                                           frame.multiSort && g.IO.KeyShift);

    // Draw, clipped to the column's visible part.
    ImDrawList* drawList = window->DrawList;
    const ImRect clip = DataTableColumnClip(frame, n);
    drawList->PushClipRect(clip.Min, clip.Max, true);
    const float radius = Px(theme.radii.control);
    if (interactive && !item.disabled)
    {
        const float feedback = std::max(item.hoverAmount, item.pressAmount);
        if (feedback > 0.0f)
        {
            const ImVec4 tint = Mix(c.fill[2], c.fill[1], item.pressAmount);
            FillRect(drawList, fill.Min, fill.Max, ToU32(WithAlpha(tint, tint.w * feedback)), radius);
        }
    }

    ControlTextScope text(TextStyle::Footnote);
    const bool sorted = sortable && column.SortOrder >= 0;
    const ImU32 color = ToU32(sorted ? ControlLabelColor(item.disabled) : ControlSecondaryColor(item.disabled));
    char order[8];
    const float iconSize = text.IconSize();
    const float markWidth = sortable ? DataTableSortMarkWidth(frame, column, iconSize, order, sizeof(order)) : 0.0f;
    const float gap = markWidth > 0.0f ? Px(theme.spacing.xs) : 0.0f;
    const float left = column.WorkMinX;
    const float right = std::max(left, column.WorkMaxX);
    const char* label = declared.label != nullptr ? declared.label : "";
    const char* labelEnd = ImGui::FindRenderedTextEnd(label);
    const FittedText shown = FitText(label, labelEnd, std::max(0.0f, right - left - markWidth - gap));
    const float content = shown.width + gap + markWidth;
    float x = left;
    if (declared.align == Align::Center)
        x = std::round(left + std::max(0.0f, (right - left - content) * 0.5f));
    else if (declared.align == Align::End)
        x = std::round(std::max(left, right - content));
    const float textY = CenteredTextY(cell.Min.y, cell.GetHeight());
    const auto drawMark = [&](float at) {
        if (markWidth <= 0.0f)
            return;
        if (order[0] != '\0')
        {
            drawList->AddText(ImVec2(at, textY), color, order);
            at += ImGui::CalcTextSize(order).x + Px(theme.spacing.xs);
        }
        const Icon arrow(column.SortDirection == ImGuiSortDirection_Descending ? "arrow-down" : "arrow-up");
        DrawControlIcon(drawList, arrow, at, cell.GetCenter().y, iconSize, color);
    };
    // The mark follows the label, except in an End column, where it leads so the label keeps its edge.
    if (declared.align == Align::End)
    {
        drawMark(x);
        drawList->AddText(ImVec2(x + markWidth + gap, textY), color, shown.text);
    }
    else
    {
        drawList->AddText(ImVec2(x, textY), color, shown.text);
        drawMark(x + shown.width + gap);
    }
    DrawFocusRing(drawList, fill, radius, item.focusAmount);
    drawList->PopClipRect();
    CutTextTooltip(item, label, labelEnd, shown.truncated);

    // The column menu.
    if (!DataTableHasMenu(frame))
        return;
    ImVec2 at;
    if (real && ContextMenuRequested(ContextRequestTarget::LastItem, &at))
    {
        menu->open = true;
        menu->column = n;
        menu->anchor = ImRect(at, at);
    }
    else if (Has(item.forced, ForcedItemState::Open))
    {
        menu->open = true;
        menu->forced = true;
        menu->column = n;
        menu->anchor = ImRect(ImVec2(cell.Min.x, cell.Max.y), ImVec2(cell.Min.x, cell.Max.y));
    }
}

// The band on the right edge of column `n` that resizes it.
void DataTableResizeBand(DataTableFrame& frame, int n, float top, float bottom)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = frame.table;
    ImGuiWindow* window = frame.window;
    DataTableState& state = *frame.state;
    const ImGuiTableColumn& column = table->Columns[n];
    const Theme& theme = T();
    const float half = Px(theme.table.resizeBand) * 0.5f;
    ImRect band(column.MaxX - half, top, column.MaxX + half, bottom);
    band.Min.x = std::max(band.Min.x, std::max(column.ClipRect.Min.x, table->InnerClipRect.Min.x));
    band.Max.x = std::min(band.Max.x, table->InnerClipRect.Max.x);

    float highlight = 0.0f;
    if (band.Max.x > band.Min.x)
    {
        ImGui::PushID(n);
        ItemOptions options;
        options.press = PressMode::Click;
        options.navigable = false;
        options.tabStop = false;
        options.hitTarget = false;
        options.handCursor = false;
        const ItemState item = PartBehavior(window->GetID("##resize"), band, options);
        ImGui::PopID();
        const ImGuiIO& io = g.IO;
        if ((item.hovered || item.held) && !item.disabled)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (item.activated)
        {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                ImGui::TableSetColumnWidthAutoSingle(table, n);
                ImGui::ClearActiveID();
                state.resizeColumn = -1;
            }
            else
            {
                state.resizeColumn = n;
                state.resizeStartWidth = column.WidthGiven;
                state.resizeStartMouse = io.MousePos.x;
                state.resizeWidth = column.WidthGiven;
                if (table->LastResizedColumn == -1)
                    table->ResizeLockMinContentsX2 = table->RightMostEnabledColumn != -1
                                                         ? table->Columns[table->RightMostEnabledColumn].MaxX
                                                         : -FLT_MAX;
            }
        }
        if (item.held && state.resizeColumn == n)
        {
            // Dear ImGui applies the width in the next layout (TableApplyQueuedRequests), clamped
            // to the minimum column width; DataTableApplyResize hands it over after EndTable.
            state.resizeWidth = std::trunc(state.resizeStartWidth + (io.MousePos.x - state.resizeStartMouse));
            table->ResizedColumn = static_cast<ImGuiTableColumnIdx>(n);
            table->InstanceInteracted = table->InstanceCurrent;
        }
        else if (state.resizeColumn == n && !item.held)
        {
            state.resizeColumn = -1;
        }
        highlight = std::max(item.hoverAmount, item.pressAmount);
    }
    // Dear ImGui's own border hit area (inside the band) counts as well.
    if (table->HoveredColumnBorder == n || table->ResizedColumn == n)
        highlight = 1.0f;

    ImDrawList* drawList = window->DrawList;
    ImRect clip = table->InnerClipRect;
    clip.Min.x = std::max(clip.Min.x, column.ClipRect.Min.x);
    drawList->PushClipRect(clip.Min, clip.Max, true);
    const float hairline = HairlineThickness();
    const float x = std::round(column.MaxX);
    if (highlight > 0.0f)
    {
        const float width = hairline * kDataTableResizeLineHairlines;
        const ImVec4& ring = theme.colors.focusRing;
        FillRect(drawList, ImVec2(x - width * 0.5f, top), ImVec2(x + width * 0.5f, bottom),
                 ToU32(WithAlpha(ring, ring.w * highlight)), 0.0f);
    }
    drawList->PopClipRect();
}

// The hairline between two columns of the header.
void DataTableDivider(const DataTableFrame& frame, int n, float top, float bottom)
{
    const ImGuiTable* table = frame.table;
    const ImGuiTableColumn& column = table->Columns[n];
    if (column.NextEnabledColumn == -1)
        return;
    const Theme& theme = T();
    ImDrawList* drawList = frame.window->DrawList;
    ImRect clip = table->InnerClipRect;
    clip.Min.x = std::max(clip.Min.x, column.ClipRect.Min.x);
    drawList->PushClipRect(clip.Min, clip.Max, true);
    const float inset = Px(theme.spacing.sm);
    const float x = std::round(column.MaxX);
    FillRect(drawList, ImVec2(x, top + inset), ImVec2(x + HairlineThickness(), bottom - inset),
             ToU32(theme.colors.separator), 0.0f);
    drawList->PopClipRect();
}

// The column menu (see the top of this file). Actions run after its levels closed for this frame:
// inside a popup Dear ImGui has no current table.
void DataTableColumnMenu(DataTableFrame& frame, std::span<const DataTableColumn> columns,
                         const DataTableMenuRequest& request)
{
    ImGuiTable* table = frame.table;
    DataTableState& state = *frame.state;
    const ImGuiID menuId = DataTableMenuId(frame);
    const ImGuiID listId = DataTableColumnsMenuId(frame);
    if (request.open)
    {
        state.menuColumn = request.column;
        PopupAnchor anchor;
        anchor.rect = request.anchor;
        anchor.side = PopupSide::Below;
        PopupOptions popup;
        popup.minWidth = Px(T().overlay.menuMinWidth);
        if (request.forced)
            ForcePopupLevelOpen(menuId, anchor, popup);
        else
            OpenPopupLevel(menuId, anchor, popup);
    }
    enum class Action
    {
        None,
        SortAscending,
        SortDescending,
        Toggle,
    };
    Action action = Action::None;
    int toggle = -1;
    const int menuColumn = state.menuColumn;
    if (BeginPopupLevel(menuId))
    {
        MenuLevelBegin(menuId);
        const Strings& strings = GetStrings();
        const bool sortableColumn = frame.sortable && menuColumn >= 0 && menuColumn < frame.columnCount &&
                                    (table->Columns[menuColumn].Flags & ImGuiTableColumnFlags_NoSort) == 0;
        if (sortableColumn)
        {
            const ImGuiTableColumn& column = table->Columns[menuColumn];
            const bool ascending = column.SortOrder >= 0 && column.SortDirection == ImGuiSortDirection_Ascending;
            const bool descending = column.SortOrder >= 0 && column.SortDirection == ImGuiSortDirection_Descending;
            const bool canAscend = (column.SortDirectionsAvailMask & (1 << ImGuiSortDirection_Ascending)) != 0;
            const bool canDescend = (column.SortDirectionsAvailMask & (1 << ImGuiSortDirection_Descending)) != 0;
            if (MenuRow(menuId, strings.sortAscending, {.checked = ascending, .disabled = !canAscend}).pressed)
                action = Action::SortAscending;
            if (MenuRow(menuId, strings.sortDescending, {.checked = descending, .disabled = !canDescend}).pressed)
                action = Action::SortDescending;
        }
        if (frame.hideable)
        {
            (void)MenuRow(menuId, strings.columns, {.submenu = listId});
            if (BeginPopupLevel(listId))
            {
                MenuLevelBegin(listId);
                for (int o = 0; o < frame.columnCount; ++o)
                {
                    const int n = table->DisplayOrderToIndex[o];
                    const ImGuiTableColumn& column = table->Columns[n];
                    const bool shown = column.IsUserEnabled;
                    const bool locked = (column.Flags & ImGuiTableColumnFlags_NoHide) != 0 ||
                                        (shown && table->ColumnsEnabledCount <= 1);
                    const char* label = columns[static_cast<std::size_t>(n)].label;
                    ImGui::PushID(n);
                    if (MenuRow(listId, label != nullptr ? label : "", {.checked = shown, .disabled = locked}).pressed)
                    {
                        action = Action::Toggle;
                        toggle = n;
                    }
                    ImGui::PopID();
                }
                EndPopupLevel();
            }
        }
        EndPopupLevel();
    }
    switch (action)
    {
    case Action::SortAscending:
        ImGui::TableSetColumnSortDirection(menuColumn, ImGuiSortDirection_Ascending, false);
        break;
    case Action::SortDescending:
        ImGui::TableSetColumnSortDirection(menuColumn, ImGuiSortDirection_Descending, false);
        break;
    case Action::Toggle:
        ImGui::TableSetColumnEnabled(toggle, !table->Columns[toggle].IsUserEnabled);
        break;
    case Action::None:
        break;
    }
}

} // namespace

void DataTableHeaderRow(DataTableFrame& frame, std::span<const DataTableColumn> columns)
{
    ImGuiContext& g = *GImGui;
    ImGuiTable* table = frame.table;
    ImGuiWindow* window = frame.window;
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers, frame.headerHeight);
    // No stock header fill: the table's surface shows, and rows scroll under the header only below it.
    table->RowBgColor[0] = IM_COL32_DISABLE;
    const float top = table->RowPosY1;
    const float bottom = top + frame.headerHeight;
    DataTableMenuRequest menu;
    for (int o = 0; o < frame.columnCount; ++o)
    {
        const int n = table->DisplayOrderToIndex[o];
        if (!table->Columns[n].IsEnabled)
            continue;
        if (!ImGui::TableSetColumnIndex(n) && table->LastHeldHeaderColumn != n)
            continue;
        DataTableBeginBackground(frame);
        ImGui::PushID(n);
        DataTableHeaderCell(frame, n, columns[static_cast<std::size_t>(n)], top, bottom, &menu);
        ImGui::PopID();
        DataTableEndBackground(frame);
    }
    if (table->CurrentColumn < 0)
        (void)ImGui::TableSetColumnIndex(table->LeftMostEnabledColumn);

    DataTableBeginBackground(frame);
    // The dividers, the resize bands over the cells' edges, and the hairline under the header.
    for (int o = 0; o < frame.columnCount; ++o)
    {
        const int n = table->DisplayOrderToIndex[o];
        const ImGuiTableColumn& column = table->Columns[n];
        if (!column.IsEnabled || !column.IsVisibleX)
            continue;
        DataTableDivider(frame, n, top, bottom);
        if (frame.resizable && (column.Flags & ImGuiTableColumnFlags_NoResize) == 0)
            DataTableResizeBand(frame, n, top, bottom);
    }
    {
        ImDrawList* drawList = window->DrawList;
        const ImRect clip = table->InnerClipRect;
        drawList->PushClipRect(clip.Min, clip.Max, true);
        const float hairline = HairlineThickness();
        FillRect(drawList, ImVec2(clip.Min.x, bottom - hairline), ImVec2(clip.Max.x, bottom),
                 ToU32(T().colors.separator), 0.0f);
        drawList->PopClipRect();
    }
    // A right click on the header row after the last column opens the menu without a column.
    if (DataTableHasMenu(frame) && !menu.open && ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
        ImGui::IsWindowHovered(ImGuiHoveredFlags_None) && !ImGui::IsAnyItemHovered())
    {
        const ImVec2 mouse = g.IO.MousePos;
        const float after = table->Columns[table->RightMostEnabledColumn].MaxX;
        if (mouse.y >= top && mouse.y < bottom && mouse.x >= after && mouse.x < table->InnerClipRect.Max.x)
        {
            menu.open = true;
            menu.column = -1;
            menu.anchor = ImRect(mouse, mouse);
        }
    }
    DataTableEndBackground(frame);
    if (DataTableHasMenu(frame))
        DataTableColumnMenu(frame, columns, menu);
}

void DataTableApplyResize(DataTableFrame& frame)
{
    // EndTable computed a width from the pointer for the column being resized; the band's own drag
    // replaces it, so the width follows the pointer exactly at every scale.
    const DataTableState& state = *frame.state;
    ImGuiTable* table = frame.table;
    if (state.resizeColumn >= 0 && table->ResizedColumn == state.resizeColumn)
        table->ResizedColumnNextWidth = std::max(1.0f, state.resizeWidth);
}

} // namespace facet::internal
