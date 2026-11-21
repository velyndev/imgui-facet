// BeginGrid, GridNext and EndGrid (layout.h): equal columns that reflow with the width.
//
// The column count is the most columns of at least minCellWidth that fit the grid's width with the
// gap between them (at least one, at most maxColumns), so a narrower region shows fewer, wider
// columns. Every cell is a group placed at its column; its item width and the window's work rect
// are narrowed to the cell, so fields and items that fill the available width fill the cell. A cell
// that spans more columns than are left on its row starts the next row. A row starts below the
// tallest cell of the row above, measured as its cells end in the same frame, so nothing overlaps
// even when a cell grows. The grid is one group: the last item after EndGrid.

#include "facet/widgets/layout.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/scope_stack.h"
#include "widgets/layout/layout_internal.h"

namespace facet
{
namespace
{

struct GridEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;
    ImVec2 origin = {0.0f, 0.0f};
    float width = 0.0f;
    float gap = 0.0f;
    int columns = 1;
    float cellWidth = 0.0f;
    int column = 0;          // the next free column of the current row
    int cells = 0;           // cells started
    float rowTop = 0.0f;
    float rowBottom = 0.0f;  // the bottom of the tallest cell of the current row
    bool cellOpen = false;
    internal::LayoutWorkRect work;
};

struct GridStack
{
    GridEntry entries[internal::kScopeStackDepth];
    int depth = 0;
    int overflow = 0;
};

GridStack& Grids()
{
    static GridStack stack;
    return stack;
}

void CloseGridCell(GridEntry& entry)
{
    if (!entry.cellOpen)
        return;
    ImGui::PopItemWidth();
    internal::RestoreLayoutWorkRect(entry.work);
    entry.work = {};
    ImGui::EndGroup();
    entry.cellOpen = false;
    entry.rowBottom = std::max(entry.rowBottom, GImGui->LastItemData.Rect.Max.y);
}

} // namespace

void BeginGrid(const char* id, const GridOptions& options)
{
    (void)id;  // the grid scopes no ids and keeps no state across frames
    internal::PushScope("BeginGrid");
    GridStack& stack = Grids();
    if (stack.depth >= internal::kScopeStackDepth)
    {
        ++stack.overflow;
        return;
    }
    GridEntry& entry = stack.entries[stack.depth++];
    entry = GridEntry{};
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return;

    const Theme& theme = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    entry.active = true;
    entry.window = window;
    entry.width = internal::ResolveItemWidth(options.width, next, kFill);
    entry.gap = internal::LayoutGapPixels(options.gap);
    const float minCell = std::max(1.0f, Px(options.minCellWidth > 0.0f ? options.minCellWidth : theme.layout.minRowControl));
    int columns = static_cast<int>(std::floor((entry.width + entry.gap) / (minCell + entry.gap)));
    if (options.maxColumns > 0)
        columns = std::min(columns, options.maxColumns);
    entry.columns = std::max(1, columns);
    const float gaps = entry.gap * static_cast<float>(entry.columns - 1);
    entry.cellWidth = std::max(1.0f, std::floor((entry.width - gaps) / static_cast<float>(entry.columns)));
    ImGui::BeginGroup();
    entry.origin = window->DC.CursorPos;
    entry.rowTop = entry.origin.y;
    entry.rowBottom = entry.origin.y;
}

void GridNext(const GridCellOptions& options)
{
    GridStack& stack = Grids();
    const char* innermost = internal::InnermostScope();
    if (stack.overflow > 0 || stack.depth == 0 || innermost == nullptr || std::strcmp(innermost, "BeginGrid") != 0)
    {
        internal::ReportUsageError("GridNext outside a grid (or inside a container nested in one)");
        return;
    }
    GridEntry& entry = stack.entries[stack.depth - 1];
    if (!entry.active || GImGui->CurrentWindow != entry.window)
        return;
    CloseGridCell(entry);
    const int span = std::clamp(options.columnSpan, 1, entry.columns);
    if (entry.column + span > entry.columns)
    {
        entry.rowTop = entry.rowBottom + entry.gap;
        entry.rowBottom = entry.rowTop;
        entry.column = 0;
    }
    const float x = entry.origin.x + static_cast<float>(entry.column) * (entry.cellWidth + entry.gap);
    const float width = entry.cellWidth * static_cast<float>(span) + entry.gap * static_cast<float>(span - 1);
    ImGui::SetCursorScreenPos(ImVec2(x, entry.rowTop));
    ImGui::BeginGroup();
    entry.work = internal::NarrowLayoutWorkRect(entry.window, x + width);
    ImGui::PushItemWidth(width);
    entry.cellOpen = true;
    entry.column += span;
    ++entry.cells;
}

void EndGrid()
{
    if (!internal::PopScope("BeginGrid"))
        return;
    GridStack& stack = Grids();
    if (stack.overflow > 0)
    {
        --stack.overflow;
        return;
    }
    if (stack.depth == 0)
        return;
    GridEntry& entry = stack.entries[--stack.depth];
    if (!entry.active)
        return;
    if (GImGui->CurrentWindow != entry.window)
    {
        internal::RestoreLayoutWorkRect(entry.work);
        return;  // the caller ended the window inside the grid; Dear ImGui reports that itself
    }
    CloseGridCell(entry);
    ImGui::SetCursorScreenPos(entry.origin);
    ImGui::Dummy(ImVec2(entry.width, entry.rowBottom - entry.origin.y));
    ImGui::EndGroup();
}

namespace internal
{

void ClearGridStack()
{
    GridStack& stack = Grids();
    for (int i = stack.depth - 1; i >= 0; --i)
        if (stack.entries[i].active)
            RestoreLayoutWorkRect(stack.entries[i].work);
    stack.depth = 0;
    stack.overflow = 0;
}

} // namespace internal

} // namespace facet
