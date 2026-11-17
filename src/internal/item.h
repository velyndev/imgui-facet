#pragma once

// Item behaviour: the hover, press, focus and disabled logic shared by every Facet widget.
// Owner from Phase 1c part A on: the interaction blocks package, which adds the behaviour
// itself to this header. What exists now is the forced state request below, the seam the demo
// capture and the tests use to show interaction states without real input.
//
// Forced states: capture runs render pages without any input, yet the gallery has to show how
// a widget looks hovered, pressed, focused, while dragging and with its popup open. A page
// asks for such a look right before submitting the widget:
//
//     facet::internal::ForceNextItemState(facet::internal::ForcedItemState::Hovered);
//     facet::Button("Save");
//
// The item behaviour takes the request when the next widget runs it and reports the forced
// state as if the input had happened, for drawing only: no value is written, no result is
// returned and no focus moves. Pages use the PageCtx helper (demo/pages/pages.h), which does
// nothing outside capture runs, so interactive and selftest runs keep real input. The disabled
// look needs no request: wrap the widget in ImGui::BeginDisabled as an app would.

#include <cstdint>

#include "facet/widgets/common.h"

namespace facet::internal
{

enum class ForcedItemState : std::uint32_t
{
    None = 0,
    Hovered = 1u << 0,   // the pointer rests on the item
    Pressed = 1u << 1,   // the item is held down (active)
    Focused = 1u << 2,   // keyboard focus with the focus ring, as after keyboard input
    Dragging = 1u << 3,  // a drag of the item's value or handle is under way; implies Pressed
    Open = 1u << 4,      // a widget with a popup or a list shows it open
};
FACET_FLAGS(ForcedItemState)

struct ForcedItemRequest
{
    ForcedItemState state = ForcedItemState::None;
    int part = -1;  // sub-item of a widget made of several items (handle, segment); -1 the widget
};

// Asks the next widget that runs the item behaviour to show `state`. `part` selects one of
// its sub-items (a range slider's handle index, a segment); -1 is the widget itself. A second
// call before a widget took the first replaces it. A request no widget took is dropped at
// facet::EndFrame, so it never reaches a widget of a later frame. Not public API: only the
// demo capture and tests call it.
void ForceNextItemState(ForcedItemState state, int part = -1);

// For the item behaviour: returns the pending request and clears it. None when there is none.
ForcedItemRequest TakeForcedItemState();

// Drops a pending request (facet::EndFrame).
void ClearForcedItemState();

} // namespace facet::internal
