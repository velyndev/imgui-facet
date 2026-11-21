#pragma once

// Popup placement: where a floating layer goes next to its anchor. Pure math on rectangles in
// screen pixels, with no Dear ImGui state, so every case is unit tested (tests/test_popup_host.cpp).
// The popup host calls it for every level in every frame; widgets that place something of their
// own next to an anchor (an arrow, a preview) may call it too:
//
//     PopupPlaceInput in;
//     in.anchor = itemRect;
//     in.side = PopupSide::Below;
//     in.size = wantedSize;                               // content plus padding
//     in.bounds = workArea;  in.bounds.Expand(-margin);   // stay this far from the display edges
//     in.gap = Px(T().overlay.anchorGap);
//     in.minExtent = Px(T().metrics.controlHeight) * 2.0f;
//     const PopupPlacement placed = PlacePopup(in);       // placed.rect, placed.side
//
// The rules, in order:
//   1. The size grows to minWidth, and for Below and Above with matchWidth to the anchor width;
//      then it shrinks per axis to the bounds.
//   2. The side: the current side while it still has room (keepSide, so a level does not jump
//      between sides when its content changes), else the preferred side when the level fits
//      there, else the opposite side when it fits there. When it fits on neither, the side with
//      more room takes it and the level shrinks along that axis to the room (it then scrolls);
//      when even that room is below minExtent, the level is clamped into the bounds and may
//      cover its anchor (an anchor as tall as the display has no room beside it).
//   3. Along the anchor edge the level is aligned by `align`, then shifted to stay inside the
//      bounds. With a `region` (the window that holds the anchor), a Start or End alignment
//      turns to the other end when only that keeps more of the level over the region: a "more"
//      button at the leading edge of a narrow toolbar opens its menu toward the toolbar instead
//      of over the window beside it.
//   4. Positions are rounded down to whole pixels, so the surface edges stay crisp.
// A zero-size anchor (the pointer, the caret) works like any other. An anchor partly or wholly
// outside the bounds (scrolled away) still gives a level inside the bounds.

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/popup_host.h"

namespace facet::internal
{

struct PopupPlaceInput
{
    ImRect anchor;                      // the rectangle the level belongs to
    PopupSide side = PopupSide::Below;  // preferred side
    PopupAlign align = PopupAlign::Start;
    bool matchWidth = false;            // Below and Above: at least as wide as the anchor
    ImVec2 size = {0.0f, 0.0f};         // wanted size of the level
    float minWidth = 0.0f;              // narrowest level
    ImRect bounds;                      // the level stays inside (the work area minus the margin)
    float gap = 0.0f;                   // between the anchor and the level
    float minExtent = 0.0f;             // smallest extent beside the anchor before the level may cover it
    bool keepSide = false;              // stay on currentSide while it has room
    PopupSide currentSide = PopupSide::Below;
    bool center = false;                // centered in the bounds, the anchor ignored (dialogs)
    ImRect region;                      // the anchor's window; an empty rect for none (rule 3)
};

struct PopupPlacement
{
    ImRect rect;                        // where the level goes, inside the bounds
    PopupSide side = PopupSide::Below;  // the side it took
    bool flipped = false;               // the side is the opposite of the preferred one
    bool shrunk = false;                // smaller than wanted along the side's axis (it scrolls)
    bool coversAnchor = false;          // no room on either side: clamped over the anchor
    PopupAlign align = PopupAlign::Start;  // the alignment it took along the anchor edge (rule 3)
};

PopupPlacement PlacePopup(const PopupPlaceInput& input);

// The side across from `side`.
PopupSide OppositePopupSide(PopupSide side);

} // namespace facet::internal
