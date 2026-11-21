#pragma once

// The tile grid that IconPicker and ThumbnailPicker (pickers.h) share: tiles of one size in rows,
// optional header rows between runs of tiles (groups), virtualized through the row ranges of the
// list core, so a grid of a hundred thousand tiles costs what its visible rows cost. Tiles are rows
// of the list core's row block (ListRowGroup): they hover, focus and draw the focus ring like list
// rows, and the caller draws their content.
//
//     PickerGridRows rows;                                  // kept in the widget's state
//     PickerGridBuildRows(&rows, count, columns, &GroupOf, user);   // when the tiles or columns change
//     PickerGridDesc desc{.rows = rows.rows, .tileCount = count, .columns = columns, .tile = size,
//                         .draw = &DrawTile, .user = user};
//     const PickerGridResult result = PickerGrid(ImGui::GetID("##grid"), desc);
//     if (result.activated >= 0) ... pick tile result.activated ...
//
// Keys while a tile has the keyboard focus (single form): Left and Right move to the previous and
// next tile, Up and Down to the tile in the same column of the row above or below (header rows are
// passed over), Home and End to the first and last tile, Page Up and Page Down by the rows in view;
// disabled tiles are skipped. Enter, Space or a click activates. The grid is one Tab stop: the
// focused tile, else the chosen one, else the first. In the multi form every tile is a Tab stop and
// Dear ImGui's navigation moves between them.

#include <span>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/common.h"
#include "internal/item.h"

namespace facet::internal
{

// One line of the grid: `count` tiles from `first` in the flat tile order, or a header line
// (count 0) showing `header`.
struct PickerGridRow
{
    int first = 0;
    int count = 0;
    const char* header = nullptr;
};

// The group text of tile `index` (null or "" for none), for the header lines.
using PickerGridGroupFn = const char* (*)(void* user, int index);

struct PickerGridRows
{
    ImVector<PickerGridRow> rows;
};

// Lays out `count` tiles in lines of `columns`, with a header line before each run of equal group
// text (no headers when `group` is null). Allocates only when the grid grows.
void PickerGridBuildRows(PickerGridRows* out, int count, int columns, PickerGridGroupFn group, void* user);

// What the caller needs to know about tile `index`.
struct PickerGridTileInfo
{
    ImGuiID id = 0;         // stable id; 0 uses the index
    bool disabled = false;  // shown, not focusable, never activated
    bool locked = false;    // focusable, never activated
};
using PickerGridInfoFn = void (*)(void* user, int index, PickerGridTileInfo* out);

struct PickerGridTile
{
    int index = 0;
    ImRect rect;            // screen pixels
    ItemState item;
    bool chosen = false;
    bool focused = false;
    bool disabled = false;
    bool locked = false;
};
// Draws tile content into the current window's draw list (the background, hover and focus ring are
// the grid's).
using PickerGridDrawFn = void (*)(void* user, const PickerGridTile& tile);

struct PickerGridDesc
{
    std::span<const PickerGridRow> rows;
    int tileCount = 0;
    int columns = 1;
    ImVec2 tile = ImVec2(0.0f, 0.0f);   // pixels
    float gap = 0.0f;                   // pixels between tiles and between lines
    float width = 0.0f;                 // pixels of the whole grid (headers span it)
    int chosen = -1;                    // single form: drawn chosen
    bool multi = false;                 // every tile a Tab stop; arrows left to navigation
    std::span<const bool> checked = {}; // multi form: drawn chosen
    PickerGridInfoFn info = nullptr;
    PickerGridDrawFn draw = nullptr;
    void* user = nullptr;
    bool readOnly = false;              // tiles focus and move, nothing is activated
    bool live = true;                   // false: a capture shows the grid; no input is read
    const ForcedItemRequest* forced = nullptr;  // part 0: the focused (or chosen) tile
    int reveal = -1;                    // scrolled into view in the window around the grid
    bool takeFocus = false;             // moves the keyboard focus to the focused, chosen or first tile
    bool defaultFocus = false;          // the Tab stop tile is the window's default focus (a popover opening on the grid)
    bool resetFocus = false;            // forget the tile focused before (the tiles changed their order)
};

struct PickerGridResult
{
    int activated = -1;   // the tile activated this frame (click, Enter, Space), -1 for none
    bool byKey = false;
    int focused = -1;     // the tile that has the keyboard focus, -1 for none
};

// The grid at the cursor of the current window; its lines are handed out by the list core's row
// ranges, so only the visible ones (and the focused one) are submitted. Advances the cursor by the
// grid's height. State (the focused tile) under `id`.
PickerGridResult PickerGrid(ImGuiID id, const PickerGridDesc& desc);
// The tile id of `index` as PickerGrid uses it (tests and selftests).
ImGuiID PickerGridTileId(ImGuiID id, const PickerGridDesc& desc, int index);
// The rect of tile `index` in the last frame, screen pixels; empty when it was not laid out.
ImRect PickerGridTileRect(ImGuiID id, int index);

} // namespace facet::internal
