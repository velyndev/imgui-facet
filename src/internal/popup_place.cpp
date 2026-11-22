// Popup placement (popup_place.h).

#include "internal/popup_place.h"

#include <algorithm>
#include <cmath>

namespace facet::internal
{
namespace
{

bool IsVertical(PopupSide side)
{
    return side == PopupSide::Below || side == PopupSide::Above;
}

// Room between the anchor (plus the gap) and the bounds edge on `side`.
float RoomOn(const PopupPlaceInput& in, PopupSide side)
{
    switch (side)
    {
    case PopupSide::Below:
        return in.bounds.Max.y - (in.anchor.Max.y + in.gap);
    case PopupSide::Above:
        return (in.anchor.Min.y - in.gap) - in.bounds.Min.y;
    case PopupSide::After:
        return in.bounds.Max.x - (in.anchor.Max.x + in.gap);
    case PopupSide::Before:
        return (in.anchor.Min.x - in.gap) - in.bounds.Min.x;
    }
    return 0.0f;
}

// Keeps a span of `length` starting at `pos` inside [lo, hi]; the start wins when it is longer.
float ClampSpan(float pos, float length, float lo, float hi)
{
    return std::max(lo, std::min(pos, hi - length));
}

float AlignOnEdge(PopupAlign align, float anchorMin, float anchorMax, float length)
{
    switch (align)
    {
    case PopupAlign::Start:
        return anchorMin;
    case PopupAlign::Center:
        return (anchorMin + anchorMax - length) * 0.5f;
    case PopupAlign::End:
        return anchorMax - length;
    }
    return anchorMin;
}

// How much of the span from `pos`, `length` long, lies outside [lo, hi].
float OutsideSpan(float pos, float length, float lo, float hi)
{
    return std::max(0.0f, lo - pos) + std::max(0.0f, pos + length - hi);
}

} // namespace

PopupSide OppositePopupSide(PopupSide side)
{
    switch (side)
    {
    case PopupSide::Below:
        return PopupSide::Above;
    case PopupSide::Above:
        return PopupSide::Below;
    case PopupSide::After:
        return PopupSide::Before;
    case PopupSide::Before:
        return PopupSide::After;
    }
    return PopupSide::Below;
}

PopupPlacement PlacePopup(const PopupPlaceInput& in)
{
    PopupPlacement out;
    const ImVec2 avail(std::max(0.0f, in.bounds.GetWidth()), std::max(0.0f, in.bounds.GetHeight()));

    // 1. Size: grown to the minimum widths, then shrunk to the bounds.
    ImVec2 size(std::max(0.0f, in.size.x), std::max(0.0f, in.size.y));
    size.x = std::max(size.x, in.minWidth);
    if (in.matchWidth && IsVertical(in.side))
        size.x = std::max(size.x, in.anchor.GetWidth());
    size.x = std::min(size.x, avail.x);
    size.y = std::min(size.y, avail.y);

    ImVec2 pos;
    if (in.center)
    {
        pos = ImVec2(in.bounds.Min.x + (avail.x - size.x) * 0.5f, in.bounds.Min.y + (avail.y - size.y) * 0.5f);
        out.side = in.side;
    }
    else
    {
        // 2. The side. A level with a fallback leaves the preferred side's axis when neither of
        // its two sides holds it.
        const auto mainOf = [&](PopupSide side) { return IsVertical(side) ? size.y : size.x; };
        const auto fits = [&](PopupSide side) { return RoomOn(in, side) >= mainOf(side); };
        PopupSide preferred = in.side;
        PopupAlign wantedAlign = in.align;
        if (in.fallback && !fits(preferred) && !fits(OppositePopupSide(preferred)))
        {
            preferred = in.fallbackSide;
            wantedAlign = in.fallbackAlign;
            out.fellBack = true;
            if (in.matchWidth && IsVertical(preferred))
                size.x = std::min(std::max(size.x, in.anchor.GetWidth()), avail.x);
        }
        const PopupSide opposite = OppositePopupSide(preferred);
        PopupSide side = preferred;
        bool shrink = false;
        bool cover = false;
        if (in.keepSide && (in.currentSide == preferred || in.currentSide == opposite) && fits(in.currentSide))
            side = in.currentSide;
        else if (fits(preferred))
            side = preferred;
        else if (fits(opposite))
            side = opposite;
        else
        {
            side = RoomOn(in, opposite) > RoomOn(in, preferred) ? opposite : preferred;
            if (RoomOn(in, side) >= std::max(in.minExtent, 1.0f))
                shrink = true;
            else
                cover = true;
        }
        const float room = RoomOn(in, side);
        if (shrink)
        {
            if (IsVertical(side))
                size.y = std::min(size.y, room);
            else
                size.x = std::min(size.x, room);
        }

        // Position on the side's axis.
        switch (side)
        {
        case PopupSide::Below:
            pos.y = in.anchor.Max.y + in.gap;
            break;
        case PopupSide::Above:
            pos.y = in.anchor.Min.y - in.gap - size.y;
            break;
        case PopupSide::After:
            pos.x = in.anchor.Max.x + in.gap;
            break;
        case PopupSide::Before:
            pos.x = in.anchor.Min.x - in.gap - size.x;
            break;
        }
        // 3. Along the anchor edge; a Start or End alignment turns to the other end when that
        // keeps more of the level over the anchor's window (both measured after the shift into
        // the bounds).
        const int axis = IsVertical(side) ? 0 : 1;
        const float boundsMin = in.bounds.Min[axis];
        const float boundsMax = in.bounds.Min[axis] + avail[axis];
        PopupAlign align = wantedAlign;
        float along = AlignOnEdge(align, in.anchor.Min[axis], in.anchor.Max[axis], size[axis]);
        const bool hasRegion = in.region.Max[axis] > in.region.Min[axis];
        if (hasRegion && align != PopupAlign::Center)
        {
            const PopupAlign other = align == PopupAlign::Start ? PopupAlign::End : PopupAlign::Start;
            const float otherAlong = AlignOnEdge(other, in.anchor.Min[axis], in.anchor.Max[axis], size[axis]);
            const float lo = in.region.Min[axis];
            const float hi = in.region.Max[axis];
            const float outside = OutsideSpan(ClampSpan(along, size[axis], boundsMin, boundsMax), size[axis], lo, hi);
            const float otherOutside =
                OutsideSpan(ClampSpan(otherAlong, size[axis], boundsMin, boundsMax), size[axis], lo, hi);
            if (otherOutside + 0.5f < outside)
            {
                align = other;
                along = otherAlong;
            }
        }
        pos[axis] = along;

        out.side = side;
        out.align = align;
        out.flipped = side != preferred;
        out.shrunk = shrink;
        out.coversAnchor = cover;
    }

    // Inside the bounds on both axes (the side's axis only moves when the level covers the
    // anchor or the anchor itself lies outside the bounds).
    pos.x = ClampSpan(pos.x, size.x, in.bounds.Min.x, in.bounds.Min.x + avail.x);
    pos.y = ClampSpan(pos.y, size.y, in.bounds.Min.y, in.bounds.Min.y + avail.y);

    // 4. Whole pixels.
    pos.x = std::floor(pos.x);
    pos.y = std::floor(pos.y);
    out.rect = ImRect(pos, ImVec2(pos.x + size.x, pos.y + size.y));
    return out;
}

} // namespace facet::internal
