#pragma once

// Helpers shared by the editor and canvas composites (property grid, curve, easing and gradient
// editors, preview canvas, canvas elements, layer list). Every name carries the widget it belongs
// to, because every family links into one library.
//
// The frame hook in editors_frame.cpp calls the Clear functions at facet::EndFrame, so a Begin
// whose End was never called (the scope stack reports it) leaves nothing behind for the next frame.

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/composites.h"

namespace facet::internal
{

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
