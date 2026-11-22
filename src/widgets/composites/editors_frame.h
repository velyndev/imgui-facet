#pragma once

// Helpers shared by the editor and canvas composites (property grid, curve, easing and gradient
// editors, preview canvas, canvas elements, layer list). Every name carries the widget it belongs
// to, because every family links into one library.
//
// The frame hook in editors_frame.cpp calls the Clear functions at facet::EndFrame, so a Begin
// whose End was never called (the scope stack reports it) leaves nothing behind for the next frame.

#include <algorithm>
#include <climits>
#include <cstddef>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/composites.h"

namespace facet::internal
{

// ---- Storage the editors grow -------------------------------------------------------------------------

// The vector overloads of the editors (rule 5 of docs/api-conventions.md) run the span form over the
// vector resized to its capacity (no allocation) and pass this function to grow it when an add finds
// no room: the vector reserves twice its count (at least 8) and *data and *capacity point at the new
// room. The only allocation of an editor, made when an element is added. Returns false when the
// allocation failed; the add then does not happen.
template <class T>
bool EditorsGrowVector(void* user, T** data, int* capacity, int count)
{
    auto* items = static_cast<std::vector<T>*>(user);
    try
    {
        items->resize(static_cast<std::size_t>(count));
        items->reserve(std::max<std::size_t>(8, static_cast<std::size_t>(count) * 2));
        items->resize(items->capacity());
    }
    catch (...)
    {
        return false;
    }
    *data = items->data();
    *capacity = static_cast<int>(std::min<std::size_t>(items->size(), INT_MAX));
    return true;
}

// The capacity of a span or vector as an int.
inline int EditorsCapacity(std::size_t size)
{
    return static_cast<int>(std::min<std::size_t>(size, INT_MAX));
}

// ---- Property grid ---------------------------------------------------------------------------------

// The label column of the property grid `gridId` (the hash of its id string in the window), in
// design units with its gap: the column the user dragged, else the option or the token. 0 for a grid
// never shown. For tests.
float PropertyGridColumnUnits(ImGuiID gridId);
// The column edge (the divider item) of the property grid ended last: its item id and the band it was
// laid out on (screen pixels), an empty rect when the grid was stacked or had no rows. For tests and
// demo probes.
struct PropertyGridDividerRecord
{
    ImGuiID id = 0;
    ImRect band;
};
const PropertyGridDividerRecord& LastPropertyGridDivider();
// Rows the property grid ended last showed (after the filter).
int LastPropertyGridRowCount();
void ClearPropertyGridStacks();

// ---- Curve editor ------------------------------------------------------------------------------------

// Where the curve editor `id` (its label hashed in its window) draws a point of `value` (screen pixels,
// from the canvas and ranges of its last frame), and the item id of its point `index`. For tests and
// demo probes.
ImVec2 CurveEditorPointPos(ImGuiID id, ImVec2 value);
ImGuiID CurveEditorPointId(ImGuiID id, int index);

// ---- Bezier editor ---------------------------------------------------------------------------------

// Where the easing editor `id` draws a control point of `value` (screen pixels, from its last frame),
// the item id of control point `index` (0 or 1), and whether its preview dot is running. For tests and
// demo probes.
ImVec2 BezierEditorPointPos(ImGuiID id, ImVec2 value);
ImGuiID BezierEditorPointId(ImGuiID id, int index);
bool BezierEditorPreviewRunning(ImGuiID id);

// ---- Layer list ------------------------------------------------------------------------------------

// The rectangle of row `index` of the layer list `listId` (its id hashed in its window) as laid out in
// the last frame (screen pixels, before scrolling moves it): `part` 0 the whole row, 1 its visibility
// toggle, 2 its lock toggle. Empty for a row that does not exist. *itemId (may be null) receives the
// item id of that part for the row whose key is key (LayerItem::id, or the index when that is 0).
// For tests and demo probes.
ImRect LayerListRowRect(ImGuiID listId, int index, int part, ImGuiID key = 0, ImGuiID* itemId = nullptr);

// ---- Gradient editor -------------------------------------------------------------------------------

// Where the gradient editor `id` (its label hashed in its window) drew its bar and the marker of stop
// `index` (an index into the caller's sorted stops) in the last frame, screen pixels; an empty rect
// for an editor not drawn or a stop without a marker. The item id of that marker's handle in *itemId
// (may be null). For tests and demo probes.
ImRect GradientEditorBarRect(ImGuiID id);
ImRect GradientEditorStopRect(ImGuiID id, int index, ImGuiID* itemId = nullptr);
// The id of the popover level of the gradient editor `id`.
ImGuiID GradientEditorPopoverLevel(ImGuiID id);

} // namespace facet::internal
