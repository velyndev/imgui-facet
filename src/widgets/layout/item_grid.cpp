// BeginItemGrid, ItemGridTile and EndItemGrid (layout.h): a scrolling, virtualized grid of equal
// tiles.
//
// The grid is a scroll area of the scroll core, `width` x `height` (0 fills the available room;
// with no height left, metrics.panelHeight). Its column count is the most tiles of at least
// minTileWidth that fit the area's width with the gap between them (at least one); the tiles share
// the width, and each is tileAspect times as tall as it is wide. The tiles are handed out by the
// row ranges of the list core (BeginListRows with one line of tiles per range row), so only the
// visible lines, the focused tile and the range start of a Shift selection are submitted, and the
// cost of a frame does not grow with the tile count.
//
// Every tile runs through the row block of the list view (ListRowGroup): the hover fill, the
// selection fill (accentSubtle while the grid has the keyboard focus, fill[1] otherwise), the focus
// ring over every tile, and activation by a double click or Enter. With a selection the tiles live in
// ImGui::BeginMultiSelect with two-dimensional box selection: click, Ctrl, Shift, Ctrl+A and Escape
// as in every Facet list, and the arrows move in two dimensions and wrap at the line ends. The grid
// is one Tab stop: without a selection the focused tile (else the first) is the only tile Tab stops
// on; with one, the list view's tab stop (ListTabStop) carries Tab into and out of the selection
// scope. Home and End jump to the first and last tile (with Shift: a range). The focused tile is
// kept under the id, so the focus survives scrolling.
//
// While the state is not Ready, or the grid has no tile, the area keeps its size and shows the
// loading, error or empty state of the list view (DrawListState), and BeginItemGrid returns false.

#include "facet/widgets/layout.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "core/core_internal.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/list_core.h"
#include "internal/list_view.h"
#include "internal/scope_stack.h"
#include "internal/scroll.h"
#include "internal/state_storage.h"
#include "widgets/layout/layout_internal.h"

namespace facet
{
namespace
{

// The focused tile, under the grid's id.
struct ItemGridMemory
{
    int focusIndex = -1;
};

struct ItemGridEntry
{
    bool active = false;
    ImGuiWindow* window = nullptr;  // the scroll area
    ImGuiID id = 0;
    ImGuiID scope = 0;              // the id stack top inside the area: the selection's focus scope
    int count = 0;
    int columns = 1;
    float tileWidth = 0.0f;
    float tileHeight = 0.0f;
    float gap = 0.0f;
    float pitch = 0.0f;             // from one line of tiles to the next
    float left = 0.0f;
    double top = 0.0;               // y of line 0, in double precision
    int last = -2;                  // the tile started last
    int tabStop = 0;
    ImGuiSelectionBasicStorage* selection = nullptr;
    internal::ForcedItemRequest forced;
    internal::ListRowGroup group;
};

struct ItemGridStack
{
    ItemGridEntry entries[internal::kListViewDepth];
    int depth = 0;
};

ItemGridStack& Grids()
{
    static ItemGridStack stack;
    return stack;
}

ImGuiID ItemGridTileId(const ItemGridEntry& e, int index)
{
    return ImHashData(&index, sizeof(index), e.scope);
}

ImRect ItemGridTileRect(const ItemGridEntry& e, int index)
{
    const int line = index / e.columns;
    const int column = index % e.columns;
    const float x = e.left + static_cast<float>(column) * (e.tileWidth + e.gap);
    const float y = static_cast<float>(e.top + static_cast<double>(line) * e.pitch);
    return ImRect(ImVec2(x, y), ImVec2(x + e.tileWidth, y + e.tileHeight));
}

void FocusItemGridTile(ItemGridEntry& e, int index)
{
    const ImRect rect = ItemGridTileRect(e, index);
    internal::ListFocusRow(e.window, ItemGridTileId(e, index), e.scope, rect);
    internal::ScrollAreaRevealRect(e.window, rect, internal::ScrollAlign::Nearest, false);
    internal::GetState<ItemGridMemory>(e.id).focusIndex = index;
}

} // namespace

bool BeginItemGrid(const char* id, int itemCount, ImGuiSelectionBasicStorage* selection, const ItemGridOptions& options)
{
    ImGuiWindow* parent = internal::ControlWindow();
    if (parent == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    ItemGridStack& stack = Grids();
    if (stack.depth >= internal::kListViewDepth)
    {
        internal::ReportUsageError("BeginItemGrid nested deeper than %d; the grid is not shown", internal::kListViewDepth);
        return false;
    }
    const Theme& theme = T();
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 size(internal::ResolveItemWidth(options.width, next, kFill),
                      options.height > 0.0f ? Px(options.height)
                                            : (available.y >= 1.0f ? available.y : Px(theme.metrics.panelHeight)));
    const ImGuiID gridId = parent->GetID(id);
    const int count = std::max(0, itemCount);

    // Loading, failed or empty: the state in the grid's place.
    if (options.state != DataState::Ready || count == 0)
    {
        const ImVec2 at = parent->DC.CursorPos;
        const ImRect rect(at, ImVec2(at.x + size.x, at.y + size.y));
        ImGui::ItemSize(size);
        if (ImGui::ItemAdd(rect, gridId))
            internal::DrawListState(rect, options.state, options.emptyText, options.errorText);
        return false;
    }

    internal::ScrollAreaDesc area;
    area.size = size;
    if (!internal::BeginScrollAreaCore(gridId, area))
        return false;

    ImGuiContext& g = *GImGui;
    ItemGridEntry& e = stack.entries[stack.depth++];
    e = ItemGridEntry{};
    e.active = true;
    e.window = g.CurrentWindow;
    e.id = gridId;
    e.count = count;
    e.selection = selection;
    e.forced = forced;
    e.gap = internal::LayoutGapPixels(options.gap);
    const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float minTile = std::max(1.0f, Px(options.minTileWidth > 0.0f ? options.minTileWidth : theme.layout.minRowControl));
    e.columns = std::max(1, static_cast<int>(std::floor((width + e.gap) / (minTile + e.gap))));
    e.tileWidth = std::max(1.0f, std::floor((width - e.gap * static_cast<float>(e.columns - 1)) / static_cast<float>(e.columns)));
    e.tileHeight = std::max(1.0f, std::round(e.tileWidth * (options.tileAspect > 0.0f ? options.tileAspect : 1.0f)));
    e.pitch = e.tileHeight + e.gap;
    e.left = e.window->DC.CursorPos.x;

    // The tiles get ids under a scope of their own: the focus scope of the selection.
    ImGui::PushID("##tiles");
    e.scope = e.window->IDStack.back();
    ItemGridMemory& memory = internal::GetState<ItemGridMemory>(gridId);
    if (memory.focusIndex >= count)
        memory.focusIndex = count - 1;
    e.tabStop = memory.focusIndex >= 0 ? memory.focusIndex : 0;
    const bool focused = g.NavId != 0 && g.NavWindow != nullptr && g.NavFocusScopeId == e.scope;

    // The tab stop (with a selection) and Home and End.
    bool enter = false;
    if (selection != nullptr)
        enter = internal::ListTabStop(e.window, gridId, e.window->InnerRect, focused);
    e.top = static_cast<double>(e.window->DC.CursorPos.y);
    if (enter)
        FocusItemGridTile(e, e.tabStop);
    if (focused)
    {
        ImGui::SetKeyOwner(ImGuiKey_Home, gridId);
        ImGui::SetKeyOwner(ImGuiKey_End, gridId);
        const bool home = ImGui::IsKeyPressed(ImGuiKey_Home, ImGuiInputFlags_Repeat, gridId);
        const bool end = ImGui::IsKeyPressed(ImGuiKey_End, ImGuiInputFlags_Repeat, gridId);
        if (home != end)
        {
            internal::ListJumpDesc jump;
            jump.rowCount = count;
            jump.selection = selection;
            jump.focusScope = e.scope;
            jump.focusIndex = memory.focusIndex;
            const int target = internal::ListJumpToEnd(jump, end, nullptr);
            if (target >= 0)
                FocusItemGridTile(e, target);
        }
    }
    e.tabStop = memory.focusIndex >= 0 ? memory.focusIndex : 0;

    if (selection != nullptr)
        internal::BeginListSelection(selection, count, {.boxSelect = options.boxSelect, .grid = true});
    internal::BeginListRows({.rowCount = count, .rowHeight = e.pitch, .columns = e.columns, .keepAlive = e.tabStop});
    e.group.Begin(focused, &e.forced);
    internal::PushScope("BeginItemGrid");
    return true;
}

bool ItemGridTile(int index)
{
    ItemGridStack& stack = Grids();
    if (stack.depth == 0 || internal::ControlWindow() == nullptr)
        return false;
    ItemGridEntry& e = stack.entries[stack.depth - 1];
    if (GImGui->CurrentWindow != e.window || index < 0 || index >= e.count)
        return false;
    ImGuiWindow* window = e.window;
    // The first tile of a range sits where the row ranges put the cursor: the start of its line.
    if (index != e.last + 1)
        e.top = static_cast<double>(window->DC.CursorPos.y) - static_cast<double>(index / e.columns) * e.pitch;
    e.last = index;
    const ImRect rect = ItemGridTileRect(e, index);
    internal::ListRowDesc desc;
    if (e.selection != nullptr)
    {
        desc.selection = index;
        desc.selected = internal::IsListEntrySelected(e.selection, index);
    }
    desc.tabStop = index == e.tabStop;
    desc.part = index;
    const internal::ListRowState row = e.group.Row(ItemGridTileId(e, index), rect, desc);
    if (row.focused)
        internal::GetState<ItemGridMemory>(e.id).focusIndex = index;
    return row.activated;
}

void EndItemGrid()
{
    if (!internal::PopScope("BeginItemGrid"))
        return;
    ItemGridStack& stack = Grids();
    if (stack.depth == 0)
        return;
    ItemGridEntry& e = stack.entries[--stack.depth];
    if (GImGui->CurrentWindow != e.window)
        return;  // the caller ended a window inside the grid; Dear ImGui reports that itself
    e.group.End();
    internal::EndListRows();
    if (e.selection != nullptr)
        (void)internal::EndListSelection();
    ImGui::PopID();
    internal::EndScrollAreaCore();
}

namespace internal
{

void ClearItemGridStack()
{
    Grids().depth = 0;
}

} // namespace internal

} // namespace facet
