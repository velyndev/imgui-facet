// Popup placement (src/internal/popup_place.h).

#include <cmath>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "internal/popup_place.h"

namespace fi = facet::internal;
using fi::PopupAlign;
using fi::PopupPlaceInput;
using fi::PopupPlacement;
using fi::PopupSide;

namespace
{

// ---- Placement helpers ----------------------------------------------------------------------

const ImRect kBounds(ImVec2(8.0f, 8.0f), ImVec2(792.0f, 592.0f));

PopupPlaceInput Input(ImRect anchor, PopupSide side, ImVec2 size)
{
    PopupPlaceInput in;
    in.anchor = anchor;
    in.side = side;
    in.size = size;
    in.bounds = kBounds;
    in.gap = 4.0f;
    in.minExtent = 56.0f;
    return in;
}

bool Inside(const ImRect& r, const ImRect& bounds)
{
    return r.Min.x >= bounds.Min.x && r.Min.y >= bounds.Min.y && r.Max.x <= bounds.Max.x && r.Max.y <= bounds.Max.y;
}

bool Overlaps(const ImRect& a, const ImRect& b)
{
    return a.Min.x < b.Max.x && b.Min.x < a.Max.x && a.Min.y < b.Max.y && b.Min.y < a.Max.y;
}

} // namespace

// ==== Placement ===================================================================================

TEST_CASE("popup placement: each side when there is room")
{
    const ImRect anchor(ImVec2(300.0f, 250.0f), ImVec2(400.0f, 280.0f));
    const ImVec2 size(120.0f, 80.0f);
    PopupPlacement p = fi::PlacePopup(Input(anchor, PopupSide::Below, size));
    CHECK(p.side == PopupSide::Below);
    CHECK_FALSE(p.flipped);
    CHECK(p.rect.Min.x == 300.0f);
    CHECK(p.rect.Min.y == 284.0f);
    CHECK(p.rect.GetWidth() == 120.0f);
    CHECK(p.rect.GetHeight() == 80.0f);

    p = fi::PlacePopup(Input(anchor, PopupSide::Above, size));
    CHECK(p.side == PopupSide::Above);
    CHECK(p.rect.Max.y == 246.0f);
    CHECK(p.rect.Min.x == 300.0f);

    p = fi::PlacePopup(Input(anchor, PopupSide::After, size));
    CHECK(p.side == PopupSide::After);
    CHECK(p.rect.Min.x == 404.0f);
    CHECK(p.rect.Min.y == 250.0f);

    p = fi::PlacePopup(Input(anchor, PopupSide::Before, size));
    CHECK(p.side == PopupSide::Before);
    CHECK(p.rect.Max.x == 296.0f);
    CHECK(p.rect.Min.y == 250.0f);
}

TEST_CASE("popup placement: flips to the opposite side when the preferred one has no room")
{
    const ImVec2 size(120.0f, 80.0f);
    PopupPlacement p = fi::PlacePopup(Input(ImRect(ImVec2(300, 540), ImVec2(400, 570)), PopupSide::Below, size));
    CHECK(p.side == PopupSide::Above);
    CHECK(p.flipped);
    CHECK(p.rect.Max.y == 536.0f);

    p = fi::PlacePopup(Input(ImRect(ImVec2(300, 20), ImVec2(400, 50)), PopupSide::Above, size));
    CHECK(p.side == PopupSide::Below);
    CHECK(p.rect.Min.y == 54.0f);

    p = fi::PlacePopup(Input(ImRect(ImVec2(700, 250), ImVec2(760, 280)), PopupSide::After, size));
    CHECK(p.side == PopupSide::Before);
    CHECK(p.rect.Max.x == 696.0f);

    p = fi::PlacePopup(Input(ImRect(ImVec2(20, 250), ImVec2(80, 280)), PopupSide::Before, size));
    CHECK(p.side == PopupSide::After);
    CHECK(p.rect.Min.x == 84.0f);
}

TEST_CASE("popup placement: every corner and edge of the screen, every side, stays inside and off the anchor")
{
    const ImVec2 size(160.0f, 120.0f);
    const float w = 40.0f;
    const float h = 24.0f;
    // Anchors in the four corners and at the middle of each edge, touching the bounds.
    const ImVec2 corners[] = {
        {kBounds.Min.x, kBounds.Min.y},     {kBounds.Max.x - w, kBounds.Min.y},     {kBounds.Min.x, kBounds.Max.y - h},
        {kBounds.Max.x - w, kBounds.Max.y - h}, {400.0f, kBounds.Min.y},            {400.0f, kBounds.Max.y - h},
        {kBounds.Min.x, 300.0f},            {kBounds.Max.x - w, 300.0f},
    };
    const PopupSide sides[] = {PopupSide::Below, PopupSide::Above, PopupSide::After, PopupSide::Before};
    const PopupAlign aligns[] = {PopupAlign::Start, PopupAlign::Center, PopupAlign::End};
    for (const ImVec2& corner : corners)
    {
        const ImRect anchor(corner, ImVec2(corner.x + w, corner.y + h));
        for (const PopupSide side : sides)
        {
            for (const PopupAlign align : aligns)
            {
                PopupPlaceInput in = Input(anchor, side, size);
                in.align = align;
                const PopupPlacement p = fi::PlacePopup(in);
                CAPTURE(corner.x);
                CAPTURE(corner.y);
                CAPTURE(static_cast<int>(side));
                CHECK(Inside(p.rect, kBounds));
                CHECK_FALSE(p.coversAnchor);
                CHECK_FALSE(Overlaps(p.rect, anchor));
                CHECK(p.rect.GetWidth() == size.x);
                CHECK(p.rect.GetHeight() == size.y);
                // The side is the preferred one or its opposite.
                CHECK((p.side == side || p.side == fi::OppositePopupSide(side)));
            }
        }
    }
}

TEST_CASE("popup placement: shifts along the edge and aligns")
{
    const ImVec2 size(120.0f, 80.0f);
    // Near the right edge: shifted left to stay inside.
    PopupPlacement p = fi::PlacePopup(Input(ImRect(ImVec2(760, 100), ImVec2(790, 120)), PopupSide::Below, size));
    CHECK(p.rect.Max.x == kBounds.Max.x);
    CHECK(p.side == PopupSide::Below);

    const ImRect anchor(ImVec2(300, 250), ImVec2(400, 280));
    PopupPlaceInput in = Input(anchor, PopupSide::Below, size);
    in.align = PopupAlign::Center;
    p = fi::PlacePopup(in);
    CHECK(p.rect.GetCenter().x == doctest::Approx(350.0f).epsilon(0.01));
    in.align = PopupAlign::End;
    p = fi::PlacePopup(in);
    CHECK(p.rect.Max.x == 400.0f);

    in = Input(anchor, PopupSide::After, size);
    in.align = PopupAlign::End;
    p = fi::PlacePopup(in);
    CHECK(p.rect.Max.y == 280.0f);
}

TEST_CASE("popup placement: minimum width and the anchor width for dropdowns")
{
    const ImRect anchor(ImVec2(300, 250), ImVec2(520, 280));
    PopupPlaceInput in = Input(anchor, PopupSide::Below, ImVec2(100.0f, 80.0f));
    in.matchWidth = true;
    CHECK(fi::PlacePopup(in).rect.GetWidth() == 220.0f);
    in.matchWidth = false;
    in.minWidth = 180.0f;
    CHECK(fi::PlacePopup(in).rect.GetWidth() == 180.0f);
    // matchWidth only applies above and below.
    in = Input(anchor, PopupSide::After, ImVec2(100.0f, 80.0f));
    in.matchWidth = true;
    CHECK(fi::PlacePopup(in).rect.GetWidth() == 100.0f);
}

TEST_CASE("popup placement: content larger than the screen")
{
    // Taller than the display: below and above both too small; the larger room wins and the
    // level shrinks to it.
    PopupPlacement p = fi::PlacePopup(Input(ImRect(ImVec2(300, 400), ImVec2(400, 430)), PopupSide::Below, ImVec2(200, 2000)));
    CHECK(p.side == PopupSide::Above);
    CHECK(p.shrunk);
    CHECK(Inside(p.rect, kBounds));
    CHECK(p.rect.Max.y == 396.0f);
    CHECK(p.rect.Min.y == kBounds.Min.y);

    // Wider and taller than the display: clamped to the bounds on both axes.
    p = fi::PlacePopup(Input(ImRect(ImVec2(300, 250), ImVec2(400, 280)), PopupSide::Below, ImVec2(5000, 5000)));
    CHECK(Inside(p.rect, kBounds));
    CHECK(p.rect.GetWidth() == kBounds.GetWidth());

    // An anchor as tall as the display: no room on either side, so the level covers it.
    p = fi::PlacePopup(Input(ImRect(ImVec2(300, 10), ImVec2(400, 590)), PopupSide::Below, ImVec2(200, 300)));
    CHECK(p.coversAnchor);
    CHECK(Inside(p.rect, kBounds));
    CHECK(p.rect.GetHeight() == 300.0f);
}

TEST_CASE("popup placement: keeps its side while it fits, point anchors, anchors outside, whole pixels, centered")
{
    const ImRect anchor(ImVec2(300, 250), ImVec2(400, 280));
    PopupPlaceInput in = Input(anchor, PopupSide::Below, ImVec2(120, 80));
    in.keepSide = true;
    in.currentSide = PopupSide::Above;
    CHECK(fi::PlacePopup(in).side == PopupSide::Above);
    // A side that is neither the preferred one nor its opposite is not kept.
    in.currentSide = PopupSide::After;
    CHECK(fi::PlacePopup(in).side == PopupSide::Below);

    // The pointer as a zero-size anchor.
    PopupPlacement p = fi::PlacePopup(Input(ImRect(ImVec2(200, 200), ImVec2(200, 200)), PopupSide::Below, ImVec2(100, 50)));
    CHECK(p.rect.Min.x == 200.0f);
    CHECK(p.rect.Min.y == 204.0f);

    // The anchor scrolled out above the display: the level stays inside.
    p = fi::PlacePopup(Input(ImRect(ImVec2(200, -300), ImVec2(260, -270)), PopupSide::Below, ImVec2(100, 50)));
    CHECK(Inside(p.rect, kBounds));

    // Fractional anchors give whole pixel positions.
    p = fi::PlacePopup(Input(ImRect(ImVec2(200.6f, 200.3f), ImVec2(260.2f, 230.7f)), PopupSide::Below, ImVec2(100, 50)));
    CHECK(p.rect.Min.x == std::floor(p.rect.Min.x));
    CHECK(p.rect.Min.y == std::floor(p.rect.Min.y));

    in = Input(anchor, PopupSide::Below, ImVec2(200, 100));
    in.center = true;
    p = fi::PlacePopup(in);
    CHECK(p.rect.GetCenter().x == doctest::Approx(kBounds.GetCenter().x).epsilon(0.01));
    CHECK(p.rect.GetCenter().y == doctest::Approx(kBounds.GetCenter().y).epsilon(0.01));
}
