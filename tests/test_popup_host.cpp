// Popup host (src/internal/popup_host.h) and popup placement (src/internal/popup_place.h).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/popup_place.h"
#include "support/session.h"

namespace fi = facet::internal;
using fi::PopupAlign;
using fi::PopupCloseReason;
using fi::PopupKind;
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

// ---- Session helpers ------------------------------------------------------------------------

struct MotionGuard
{
    MotionGuard()
    {
        facet::SetFollowSystemMotion(false);
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
    ~MotionGuard()
    {
        facet::SetFollowSystemMotion(true);
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
};

ImGuiContext& G()
{
    return *ImGui::GetCurrentContext();
}

ImGuiID EscapeOwnerNow()
{
    return ImGui::GetKeyOwnerData(ImGui::GetCurrentContext(), ImGuiKey_Escape)->OwnerCurr;
}

ImRect LastItemRect()
{
    return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
}

fi::PopupAnchor BelowItem()
{
    fi::PopupAnchor anchor;
    anchor.rect = LastItemRect();
    anchor.side = PopupSide::Below;
    return anchor;
}

const ImGuiID kLevel1 = ImHashStr("test.level1");
const ImGuiID kLevel2 = ImHashStr("test.level2");
const ImGuiID kLevel2b = ImHashStr("test.level2b");
const ImGuiID kLevel3 = ImHashStr("test.level3");
const ImGuiID kLevel4 = ImHashStr("test.level4");

// Records the reason a level closed with, across frames.
struct ReasonLog
{
    PopupCloseReason last = PopupCloseReason::None;
    void Note(ImGuiID id)
    {
        const PopupCloseReason reason = fi::PopupLevelClosedReason(id);
        if (reason != PopupCloseReason::None)
            last = reason;
    }
};

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

TEST_CASE("popup placement: an end-aligned level turns toward the window that holds its anchor")
{
    // A narrow toolbar at the leading edge of a page window that starts at x 240, with a "more"
    // button at its start: aligned at the button's end, a 180 wide menu would cover the window
    // to the left of the page.
    const ImRect region(ImVec2(240.0f, 40.0f), ImVec2(792.0f, 592.0f));
    const ImRect button(ImVec2(300.0f, 100.0f), ImVec2(328.0f, 128.0f));
    PopupPlaceInput in = Input(button, PopupSide::Below, ImVec2(180.0f, 80.0f));
    in.align = PopupAlign::End;
    PopupPlacement p = fi::PlacePopup(in);
    CHECK(p.rect.Max.x == 328.0f);  // without a region the alignment is kept
    CHECK(p.align == PopupAlign::End);
    in.region = region;
    p = fi::PlacePopup(in);
    CHECK(p.align == PopupAlign::Start);
    CHECK(p.rect.Min.x == 300.0f);
    CHECK(p.rect.Min.x >= region.Min.x);

    // A button at the trailing end of the window keeps End: Start would leave the region.
    const ImRect trailing(ImVec2(740.0f, 100.0f), ImVec2(768.0f, 128.0f));
    in = Input(trailing, PopupSide::Below, ImVec2(180.0f, 80.0f));
    in.align = PopupAlign::End;
    in.region = region;
    p = fi::PlacePopup(in);
    CHECK(p.align == PopupAlign::End);
    CHECK(p.rect.Max.x == 768.0f);

    // The same for Start: a list wider than its field at the trailing edge turns to End.
    const ImRect field(ImVec2(640.0f, 100.0f), ImVec2(760.0f, 128.0f));
    in = Input(field, PopupSide::Below, ImVec2(240.0f, 80.0f));
    in.region = ImRect(ImVec2(240.0f, 40.0f), ImVec2(770.0f, 592.0f));
    p = fi::PlacePopup(in);
    CHECK(p.align == PopupAlign::End);
    CHECK(p.rect.Max.x == 760.0f);

    // Neither end fits inside a region narrower than the level: the alignment that leaves less of
    // it outside wins, and a tie keeps the requested one.
    in = Input(button, PopupSide::Below, ImVec2(180.0f, 80.0f));
    in.align = PopupAlign::End;
    in.region = ImRect(ImVec2(290.0f, 40.0f), ImVec2(340.0f, 592.0f));
    p = fi::PlacePopup(in);
    CHECK(p.align == PopupAlign::Start);
    in.region = ImRect(ImVec2(300.0f - 76.0f, 40.0f), ImVec2(328.0f + 76.0f, 592.0f));
    p = fi::PlacePopup(in);
    CHECK(p.align == PopupAlign::End);

    // Beside the anchor the rule acts on the vertical axis, and Center is never turned.
    in = Input(ImRect(ImVec2(300.0f, 500.0f), ImVec2(400.0f, 530.0f)), PopupSide::After, ImVec2(120.0f, 120.0f));
    in.region = ImRect(ImVec2(8.0f, 8.0f), ImVec2(792.0f, 560.0f));
    p = fi::PlacePopup(in);
    CHECK(p.align == PopupAlign::End);
    CHECK(p.rect.Max.y == 530.0f);
    in.align = PopupAlign::Center;
    CHECK(fi::PlacePopup(in).align == PopupAlign::Center);
}

TEST_CASE("popup host: an end-aligned level opened at the leading edge of a page window stays over it")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImRect page;
    session.SetUi([&] {
        // A page window that starts well right of the display edge, like the content beside the
        // gallery navigation, with a "more" button at its leading edge.
        ImGui::SetCursorPos(ImVec2(300.0f, 40.0f));
        ImGui::BeginChild("page", ImVec2(400.0f, 400.0f));
        page = ImGui::GetCurrentWindow()->Rect();
        ImGui::Button("more", ImVec2(28.0f, 28.0f));
        session.Track("more");
        if (ImGui::IsItemClicked())
            fi::OpenPopupLevel(kLevel1, {LastItemRect(), PopupSide::Below, PopupAlign::End},
                               {.size = ImVec2(180.0f, 60.0f)});
        if (fi::BeginPopupLevel(kLevel1))
        {
            ImGui::TextUnformatted("menu");
            fi::EndPopupLevel();
        }
        ImGui::EndChild();
    });
    session.Frames(2);
    session.Click(session.ItemCenter("more"));
    session.Frames(2);
    const ImRect menu = fi::PopupLevelRect(kLevel1);
    REQUIRE(menu.GetWidth() > 0.0f);
    // Aligned at the button's start instead of its end, so it stays over the page window.
    CHECK(menu.Min.x == doctest::Approx(std::floor(session.ItemRect("more").Min.x)));
    CHECK(menu.Min.x >= page.Min.x);
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

// ==== The open stack ================================================================================

TEST_CASE("popup host: a level opens, shows its content and closes with a reason")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool open = false;
    int insidePresses = 0;
    ReasonLog reasons;
    session.SetUi([&] {
        if (ImGui::Button("open"))
            fi::OpenPopupLevel(kLevel1, BelowItem());
        session.Track("open");
        if (fi::BeginPopupLevel(kLevel1))
        {
            if (ImGui::Button("inside"))
            {
                ++insidePresses;
                fi::ClosePopupLevel(kLevel1);
            }
            session.Track("inside");
            fi::EndPopupLevel();
        }
        open = fi::IsPopupLevelOpen(kLevel1);
        reasons.Note(kLevel1);
    });
    session.Frames(2);
    CHECK_FALSE(open);
    session.Click(session.ItemCenter("open"));
    session.Frames(2);
    CHECK(open);
    CHECK(G().OpenPopupStack.Size == 1);
    // Placed below the anchor, gap included.
    const ImRect level = fi::PopupLevelRect(kLevel1);
    CHECK(level.Min.y >= session.ItemRect("open").Max.y);
    CHECK(session.ItemRect("inside").Min.y >= level.Min.y);
    session.Click(session.ItemCenter("inside"));
    CHECK(insidePresses == 1);
    CHECK_FALSE(open);
    CHECK(reasons.last == PopupCloseReason::Closed);
    CHECK(G().OpenPopupStack.Size == 0);
}

TEST_CASE("popup host: a level without focus closes on a click on another item, which the click does not press")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool open = false;
    int otherPresses = 0;
    int anchorPresses = 0;
    bool allowOpen = true;
    ReasonLog reasons;
    session.SetUi([&] {
        if (ImGui::Button("other"))
            ++otherPresses;
        session.Track("other");
        ImGui::Dummy(ImVec2(0.0f, 40.0f));
        if (ImGui::Button("anchor"))
        {
            ++anchorPresses;
            if (allowOpen)
                fi::OpenPopupLevel(kLevel1, BelowItem(), {.takeFocus = false});
        }
        session.Track("anchor");
        if (fi::BeginPopupLevel(kLevel1))
        {
            ImGui::Button("row");
            session.Track("row");
            fi::EndPopupLevel();
        }
        open = fi::IsPopupLevelOpen(kLevel1);
        reasons.Note(kLevel1);
    });
    session.Frames(2);
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    REQUIRE(open);
    // A click inside the level keeps it open.
    session.Click(session.ItemCenter("row"));
    CHECK(open);
    session.Click(session.ItemCenter("other"));
    CHECK_FALSE(open);
    CHECK(otherPresses == 0);
    CHECK(reasons.last == PopupCloseReason::ClickOutside);
    session.Click(session.ItemCenter("other"));
    CHECK(otherPresses == 1);
    // A click on the anchor closes the level and still reaches the anchor.
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    REQUIRE(open);
    const int before = anchorPresses;
    allowOpen = false;
    session.Click(session.ItemCenter("anchor"));
    CHECK_FALSE(open);
    CHECK(anchorPresses == before + 1);
}

TEST_CASE("popup host: a click outside closes without pressing what is under it; a click on the anchor toggles")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool open = false;
    int anchorPresses = 0;
    int otherPresses = 0;
    int releasePresses = 0;
    ReasonLog reasons;
    session.SetUi([&] {
        if (ImGui::Button("other"))
            ++otherPresses;
        session.Track("other");
        // An item that presses on release, even after a press elsewhere: the hardest case.
        const fi::ItemState item = fi::ItemBehavior(ImGui::GetID("release"), ImVec2(80.0f, 24.0f),
                                                    {.press = fi::PressMode::Release});
        if (item.pressed)
            ++releasePresses;
        session.Track("release");
        ImGui::Dummy(ImVec2(0.0f, 40.0f));
        if (ImGui::Button("anchor"))
        {
            ++anchorPresses;
            fi::OpenPopupLevel(kLevel1, BelowItem());
        }
        session.Track("anchor");
        if (fi::BeginPopupLevel(kLevel1))
        {
            ImGui::Button("row");
            fi::EndPopupLevel();
        }
        open = fi::IsPopupLevelOpen(kLevel1);
        reasons.Note(kLevel1);
    });
    session.Frames(2);
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    REQUIRE(open);
    session.Click(session.ItemCenter("other"));
    CHECK_FALSE(open);
    CHECK(otherPresses == 0);
    CHECK(reasons.last == PopupCloseReason::ClickOutside);
    session.Click(session.ItemCenter("other"));
    CHECK(otherPresses == 1);

    // A release-press item under the dismissing click is not pressed either.
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    REQUIRE(open);
    session.Click(session.ItemCenter("release"));
    CHECK_FALSE(open);
    CHECK(releasePresses == 0);
    // Nothing keeps the mouse button afterwards.
    session.Frames(2);
    CHECK(ImGui::GetKeyOwnerData(&G(), ImGuiKey_MouseLeft)->OwnerCurr == ImGuiKeyOwner_NoOwner);
    session.Click(session.ItemCenter("release"));
    CHECK(releasePresses == 1);

    // The anchor toggles: open, click it again (closes), click again (opens).
    anchorPresses = 0;
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    CHECK(open);
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    CHECK_FALSE(open);
    CHECK(anchorPresses == 1);
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    CHECK(open);
    CHECK(anchorPresses == 2);
}

TEST_CASE("popup host: siblings close each other, a parent closes its children, four levels deep")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool request[5] = {};
    bool requestSibling = false;
    bool shown[5] = {};
    bool siblingShown = false;
    ReasonLog reason3;
    int parentRowPresses = 0;
    const ImGuiID ids[5] = {0, kLevel1, kLevel2, kLevel3, kLevel4};
    // Each level opens the next from inside its own submission, which makes it the child.
    std::function<void(int)> submit = [&](int depth) {
        if (depth > 4)
            return;
        if (request[depth])
        {
            fi::PopupAnchor anchor;
            anchor.rect = LastItemRect();
            anchor.side = PopupSide::After;
            fi::OpenPopupLevel(ids[depth], anchor);
            request[depth] = false;
        }
        if (depth == 2 && requestSibling)
        {
            fi::OpenPopupLevel(kLevel2b, {.rect = LastItemRect(), .side = PopupSide::After});
            requestSibling = false;
        }
        shown[depth] = false;
        if (fi::BeginPopupLevel(ids[depth]))
        {
            shown[depth] = true;
            if (ImGui::Button("row"))
                ++parentRowPresses;
            session.Track(depth == 1 ? "row1" : (depth == 2 ? "row2" : "rowN"));
            submit(depth + 1);
            fi::EndPopupLevel();
        }
        if (depth == 2)
        {
            siblingShown = false;
            if (fi::BeginPopupLevel(kLevel2b))
            {
                siblingShown = true;
                ImGui::TextUnformatted("sibling");
                fi::EndPopupLevel();
            }
        }
    };
    const auto beginFrame = [&] {
        for (bool& s : shown)
            s = false;
        siblingShown = false;
    };
    session.SetUi([&] {
        beginFrame();
        ImGui::Button("anchor");
        submit(1);
        reason3.Note(kLevel3);
    });
    session.Frame();
    for (int depth = 1; depth <= 4; ++depth)
    {
        request[depth] = true;
        session.Frames(2);
    }
    CHECK(shown[1]);
    CHECK(shown[2]);
    CHECK(shown[3]);
    CHECK(shown[4]);
    CHECK(G().OpenPopupStack.Size == 4);
    session.Frame();
    CHECK(fi::TopPopupLevel() == 0);  // outside a frame: the neutral result
    bool topIsFour = false;
    session.SetUi([&] {
        beginFrame();
        ImGui::Button("anchor");
        submit(1);
        reason3.Note(kLevel3);
        topIsFour = fi::TopPopupLevel() == kLevel4;
    });
    session.Frame();
    CHECK(topIsFour);

    // A sibling of level 2 closes levels 2, 3 and 4.
    requestSibling = true;
    session.Frames(3);
    CHECK(shown[1]);
    CHECK(siblingShown);
    CHECK_FALSE(shown[2]);
    CHECK_FALSE(shown[3]);
    CHECK_FALSE(shown[4]);
    CHECK(reason3.last == PopupCloseReason::Closed);
    CHECK(G().OpenPopupStack.Size == 2);

    // A click on a row of the parent level while its child is open: the child closes and the row
    // takes the click (menus).
    session.Click(session.ItemCenter("row1"));
    CHECK(parentRowPresses == 1);
    CHECK_FALSE(siblingShown);
    CHECK(shown[1]);

    // Closing the root closes everything above it.
    request[2] = true;
    session.Frames(2);
    CHECK(shown[2]);
    session.SetUi([&] {
        beginFrame();
        ImGui::Button("anchor");
        submit(1);
        if (ImGui::IsKeyPressed(ImGuiKey_C, false))
            fi::ClosePopupLevel(kLevel1);
    });
    session.KeyPress(ImGuiKey_C);
    CHECK_FALSE(shown[1]);
    CHECK_FALSE(shown[2]);
    CHECK(G().OpenPopupStack.Size == 0);
}

TEST_CASE("popup host: Escape closes one level per press in the order of the conventions, with and without keyboard navigation")
{
    for (const bool navigation : {true, false})
    {
        CAPTURE(navigation);
        MotionGuard guard;
        facet::test::Session session;
        REQUIRE(session.Ready());
        if (!navigation)
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        const ImGuiConfigFlags flagsBefore = ImGui::GetIO().ConfigFlags;
        bool requestModal = false;
        bool requestPopup1 = false;
        bool requestPopup2 = false;
        bool modal = false;
        bool popup1 = false;
        bool popup2 = false;
        bool shellClaim = false;
        int shellEscapes = 0;
        ReasonLog reasons;
        session.SetUi([&] {
            ImGui::Button("anchor");
            if (requestModal)
            {
                fi::OpenPopupLevel(kLevel1, {}, {.kind = PopupKind::Modal});
                requestModal = false;
            }
            modal = popup1 = popup2 = false;
            if (fi::BeginPopupLevel(kLevel1))
            {
                modal = true;
                ImGui::Button("modal row");
                if (requestPopup1)
                {
                    fi::OpenPopupLevel(kLevel2, BelowItem());
                    requestPopup1 = false;
                }
                if (fi::BeginPopupLevel(kLevel2))
                {
                    popup1 = true;
                    ImGui::Button("popup row");
                    if (requestPopup2)
                    {
                        fi::OpenPopupLevel(kLevel3, {.rect = LastItemRect(), .side = PopupSide::After});
                        requestPopup2 = false;
                    }
                    if (fi::BeginPopupLevel(kLevel3))
                    {
                        popup2 = true;
                        ImGui::Button("inner row");
                        fi::EndPopupLevel();
                    }
                    fi::EndPopupLevel();
                }
                fi::EndPopupLevel();
            }
            if (shellClaim && fi::ClaimEscape(ImHashStr("test.shell"), fi::EscapeLayer::Shell))
                ++shellEscapes;
            reasons.Note(kLevel3);
            reasons.Note(kLevel2);
            reasons.Note(kLevel1);
        });
        session.Frame();
        requestModal = true;
        session.Frames(2);
        requestPopup1 = true;
        session.Frames(2);
        requestPopup2 = true;
        session.Frames(3);
        REQUIRE(modal);
        REQUIRE(popup1);
        REQUIRE(popup2);
        CHECK(EscapeOwnerNow() != ImGuiKeyOwner_NoOwner);
        shellClaim = true;

        session.KeyPress(ImGuiKey_Escape);
        CHECK(modal);
        CHECK(popup1);
        CHECK_FALSE(popup2);
        CHECK(reasons.last == PopupCloseReason::Escape);
        session.KeyPress(ImGuiKey_Escape);
        CHECK(modal);
        CHECK_FALSE(popup1);
        session.KeyPress(ImGuiKey_Escape);
        CHECK_FALSE(modal);
        CHECK(shellEscapes == 0);
        // Now the shell is the only layer that claims Escape.
        session.KeyPress(ImGuiKey_Escape);
        CHECK(shellEscapes == 1);
        session.KeyPress(ImGuiKey_Escape);
        CHECK(shellEscapes == 2);
        // Nothing owns Escape once no layer asks for it.
        shellClaim = false;
        session.Frames(3);
        CHECK(EscapeOwnerNow() == ImGuiKeyOwner_NoOwner);
        CHECK_FALSE(fi::GetPopupHostStats().ownsEscape);
        CHECK(ImGui::GetIO().ConfigFlags == flagsBefore);
    }
}

TEST_CASE("popup host: Escape during an active edit cancels the edit first; a level that must be answered keeps the press")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool open = false;
    bool canceled = false;
    bool closeOnEscape = true;
    session.SetUi([&] {
        if (ImGui::Button("anchor"))
            fi::OpenPopupLevel(kLevel1, BelowItem(), {.closeOnEscape = closeOnEscape});
        session.Track("anchor");
        open = false;
        if (fi::BeginPopupLevel(kLevel1))
        {
            open = true;
            ImGui::Button("edit");
            session.Track("edit");
            // An edit session: while the item is active it owns Escape.
            if (ImGui::IsItemActive())
            {
                const ImGuiID id = ImGui::GetItemID();
                ImGui::SetKeyOwner(ImGuiKey_Escape, id);
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
                    canceled = true;
            }
            fi::EndPopupLevel();
        }
    });
    session.Frame();
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    REQUIRE(open);
    session.MouseMove(session.ItemCenter("edit"));
    session.MouseDown();
    session.KeyPress(ImGuiKey_Escape);
    CHECK(canceled);
    CHECK(open);
    session.MouseUp();
    session.Frame();
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(open);

    closeOnEscape = false;
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    REQUIRE(open);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(open);
}

TEST_CASE("popup host: focus moves into a level opened by keyboard and returns to the anchor")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool open = false;
    session.SetUi([&] {
        if (ImGui::Button("anchor"))
            fi::OpenPopupLevel(kLevel1, BelowItem());
        session.Track("anchor");
        open = false;
        if (fi::BeginPopupLevel(kLevel1))
        {
            open = true;
            ImGui::Button("first");
            session.Track("first");
            ImGui::Button("second");
            fi::EndPopupLevel();
        }
    });
    session.Frame();
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    REQUIRE(open);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(open);
    CHECK(G().NavId == session.ItemId("anchor"));
    // Enter on the focused anchor opens the level; the focus goes to its first item.
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    REQUIRE(open);
    CHECK(G().NavId == session.ItemId("first"));
    session.KeyPress(ImGuiKey_Escape);
    session.Frame();
    CHECK(G().NavId == session.ItemId("anchor"));
}

TEST_CASE("popup host: the focus never returns to an item that no longer exists")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool showAnchor = true;
    bool open = false;
    session.SetUi([&] {
        if (showAnchor)
        {
            if (ImGui::Button("anchor"))
                fi::OpenPopupLevel(kLevel1, BelowItem());
            session.Track("anchor");
        }
        open = false;
        if (fi::BeginPopupLevel(kLevel1))
        {
            open = true;
            ImGui::Button("first");
            fi::EndPopupLevel();
        }
    });
    session.Frame();
    session.Click(session.ItemCenter("anchor"));
    session.Frames(2);
    REQUIRE(open);
    const ImGuiID anchorId = session.ItemId("anchor");
    showAnchor = false;
    session.Frames(2);
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK_FALSE(open);
    CHECK(G().NavId != anchorId);
}

TEST_CASE("popup host: a level that is no longer submitted closes and leaves nothing behind, next to the host's popups")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool submit = true;
    bool hostPopup = false;
    bool open = false;
    ReasonLog reasons;
    session.SetUi([&] {
        if (ImGui::Button("host"))
            ImGui::OpenPopup("host popup");
        session.Track("host");
        if (ImGui::BeginPopup("host popup"))
        {
            hostPopup = true;
            if (ImGui::Button("ours"))
                fi::OpenPopupLevel(kLevel1, {.rect = LastItemRect(), .side = PopupSide::After});
            session.Track("ours");
            if (submit && fi::BeginPopupLevel(kLevel1))
            {
                ImGui::TextUnformatted("level");
                fi::EndPopupLevel();
            }
            ImGui::EndPopup();
        }
        else
        {
            hostPopup = false;
        }
        open = fi::IsPopupLevelOpen(kLevel1);
        reasons.Note(kLevel1);
    });
    session.Frame();
    session.Click(session.ItemCenter("host"));
    session.Frames(2);
    REQUIRE(hostPopup);
    session.Click(session.ItemCenter("ours"));
    session.Frames(2);
    REQUIRE(open);
    CHECK(G().OpenPopupStack.Size == 2);
    // Escape closes our level, then leaves the host popup to Dear ImGui.
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(open);
    CHECK(hostPopup);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(hostPopup);
    CHECK(G().OpenPopupStack.Size == 0);

    // Open again, then stop submitting our level: it closes, the host popup stays.
    session.Click(session.ItemCenter("host"));
    session.Frames(2);
    session.Click(session.ItemCenter("ours"));
    session.Frames(2);
    REQUIRE(open);
    submit = false;
    session.Frames(2);
    CHECK_FALSE(open);
    CHECK(reasons.last == PopupCloseReason::Closed);
    CHECK(hostPopup);
    CHECK(G().OpenPopupStack.Size == 1);
    CHECK(G().BeginPopupStack.Size == 0);
    CHECK(G().CurrentWindowStack.Size == 0);
}

TEST_CASE("popup host: Shutdown while a level is open leaves Dear ImGui's popup stack and key owners clean")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        if (ImGui::Button("anchor"))
            fi::OpenPopupLevel(kLevel1, BelowItem());
        session.Track("anchor");
        if (fi::BeginPopupLevel(kLevel1))
        {
            ImGui::TextUnformatted("level");
            fi::EndPopupLevel();
        }
    });
    session.Frame();
    session.Click(session.ItemCenter("anchor"));
    session.Frames(3);
    REQUIRE(G().OpenPopupStack.Size == 1);
    REQUIRE(EscapeOwnerNow() != ImGuiKeyOwner_NoOwner);
    facet::Shutdown();
    CHECK(G().OpenPopupStack.Size == 0);
    const ImGuiKeyOwnerData* owner = ImGui::GetKeyOwnerData(&G(), ImGuiKey_Escape);
    CHECK(owner->OwnerCurr == ImGuiKeyOwner_NoOwner);
    CHECK(owner->OwnerNext == ImGuiKeyOwner_NoOwner);
    CHECK(fi::GetPopupHostStats().trackedLevels == 0);
    CHECK(fi::GetPopupHostStats().replayViews == 0);
}

TEST_CASE("popup host: a level takes the size of its content and keeps it steady, also above its anchor")
{
    for (const PopupSide side : {PopupSide::Below, PopupSide::Above})
    {
        CAPTURE(static_cast<int>(side));
        MotionGuard guard;
        facet::test::Session session;
        REQUIRE(session.Ready());
        bool requestOpen = true;
        ImRect content;
        session.SetUi([&] {
            ImGui::SetCursorPos(ImVec2(300.0f, 300.0f));
            ImGui::Button("anchor");
            if (requestOpen)
            {
                fi::OpenPopupLevel(kLevel1, {.rect = LastItemRect(), .side = side}, {.glass = true});
                requestOpen = false;
            }
            if (fi::BeginPopupLevel(kLevel1))
            {
                ImGui::BeginGroup();
                ImGui::Button("first row");
                ImGui::Dummy(ImVec2(120.0f, 40.0f));
                ImGui::Button("last row");
                ImGui::EndGroup();
                content = LastItemRect();
                fi::EndPopupLevel();
            }
        });
        session.Frames(3);
        const ImRect first = fi::PopupLevelRect(kLevel1);
        session.Frames(10);
        const ImRect later = fi::PopupLevelRect(kLevel1);
        CHECK(later.Min.x == first.Min.x);
        CHECK(later.Min.y == first.Min.y);
        CHECK(later.GetHeight() == first.GetHeight());
        // Content plus the padding of a Popup level on each side.
        const float padding = facet::Px(facet::T().spacing.xs);
        CHECK(later.GetHeight() == doctest::Approx(content.GetHeight() + padding * 2.0f).epsilon(0.01));
        CHECK(content.Min.y >= later.Min.y);
        CHECK(content.Max.y <= later.Max.y + 0.5f);
    }
}

TEST_CASE("popup host: a level keeps its side while only its content changes")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool requestOpen = true;
    float contentHeight = 100.0f;
    session.SetUi([&] {
        ImGui::SetCursorPos(ImVec2(300.0f, 380.0f));
        ImGui::Button("anchor");
        if (requestOpen)
        {
            fi::OpenPopupLevel(kLevel1, {.rect = LastItemRect(), .side = PopupSide::Below});
            requestOpen = false;
        }
        if (fi::BeginPopupLevel(kLevel1))
        {
            ImGui::Dummy(ImVec2(120.0f, contentHeight));
            fi::EndPopupLevel();
        }
    });
    session.Frames(3);
    CHECK(fi::PopupLevelSide(kLevel1) == PopupSide::Below);
    // Taller than the room below: it flips above.
    contentHeight = 260.0f;
    session.Frames(3);
    CHECK(fi::PopupLevelSide(kLevel1) == PopupSide::Above);
    // Short again: it would fit below, but it stays where it is while the anchor does not move.
    contentHeight = 100.0f;
    session.Frames(3);
    CHECK(fi::PopupLevelSide(kLevel1) == PopupSide::Above);
    CHECK(fi::PopupLevelRect(kLevel1).GetHeight() == doctest::Approx(100.0f + facet::Px(facet::T().spacing.xs) * 2.0f));
}
// ==== Motion and replay ==============================================================================

TEST_CASE("popup host: the open motion fades in from the anchor side and settles")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        if (ImGui::Button("anchor"))
            fi::OpenPopupLevel(kLevel1, BelowItem());
        session.Track("anchor");
        if (fi::BeginPopupLevel(kLevel1))
        {
            ImGui::TextUnformatted("level");
            fi::EndPopupLevel();
        }
    });
    session.Frame();
    session.Click(session.ItemCenter("anchor"));
    char name[40];
    std::snprintf(name, sizeof(name), "##Popup_%08x", kLevel1);
    const auto maxAlpha = [&] {
        ImGuiWindow* window = ImGui::FindWindowByName(name);
        unsigned int best = 0;
        if (window == nullptr)
            return best;
        for (const ImDrawVert& v : window->DrawList->VtxBuffer)
            best = std::max(best, (v.col >> IM_COL32_A_SHIFT) & 0xFFu);
        return best;
    };
    // The first frame is hidden while Dear ImGui measures; the next one is the start of the fade.
    session.Frames(2);
    const unsigned int early = maxAlpha();
    CHECK(early > 0u);
    CHECK(early < 255u);
    session.Seconds(0.3f);
    CHECK(maxAlpha() == 255u);
}

TEST_CASE("popup host: the close motion replays the last frame, keeps its memory bounded and ends on device and scale changes")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool requestOpen = false;
    bool requestClose = false;
    const facet::Icon icon = facet::FindIcon("chevron-right");
    session.SetUi([&] {
        ImGui::Button("anchor");
        if (requestOpen)
        {
            fi::OpenPopupLevel(kLevel1, BelowItem());
            requestOpen = false;
        }
        if (fi::BeginPopupLevel(kLevel1))
        {
            ImGui::TextUnformatted("level with an icon");
            facet::IconGlyph(icon, facet::Px(16.0f));
            if (requestClose)
            {
                fi::ClosePopupLevel(kLevel1);
                requestClose = false;
            }
            fi::EndPopupLevel();
        }
    });
    session.Frame();
    const auto cycle = [&] {
        requestOpen = true;
        session.Seconds(0.3f);
        requestClose = true;
        session.Frame();
    };
    cycle();
    session.Frame();
    CHECK(fi::IsPopupLevelClosing(kLevel1));
    CHECK(fi::GetPopupHostStats().closingLevels == 1);
    CHECK(fi::GetPopupHostStats().replayViews >= 1);  // the icon atlas view is referenced
    session.Seconds(0.3f);
    CHECK_FALSE(fi::IsPopupLevelClosing(kLevel1));
    CHECK(fi::GetPopupHostStats().closingLevels == 0);
    CHECK(fi::GetPopupHostStats().replayViews == 0);

    // Twenty more cycles: the snapshot buffers do not grow.
    const int capacity = fi::GetPopupHostStats().replayVertices;
    for (int i = 0; i < 20; ++i)
    {
        cycle();
        session.Seconds(0.3f);
    }
    CHECK(fi::GetPopupHostStats().replayVertices == capacity);

    // A device reset during the motion ends it.
    cycle();
    session.Frame();
    REQUIRE(fi::IsPopupLevelClosing(kLevel1));
    facet::InvalidateDeviceObjects();
    CHECK_FALSE(fi::IsPopupLevelClosing(kLevel1));
    CHECK(fi::GetPopupHostStats().replayViews == 0);
    REQUIRE(facet::CreateDeviceObjects());
    session.Frame();

    // A scale change during the motion ends it.
    cycle();
    session.Frame();
    REQUIRE(fi::IsPopupLevelClosing(kLevel1));
    facet::SetUiScale(1.5f);
    session.Frame();
    CHECK_FALSE(fi::IsPopupLevelClosing(kLevel1));
    facet::SetUiScale(1.0f);
    session.Frames(2);

    // Reduced motion: no close motion at all.
    facet::Theme theme = facet::Theme::Light();
    theme.motion.reduceMotion = true;
    facet::SetTheme(theme, false);
    cycle();
    session.Frame();
    CHECK_FALSE(fi::IsPopupLevelClosing(kLevel1));
}

// ==== Context keys, modals, tooltips ===================================================================

TEST_CASE("popup host: context menu requests from a right click, the Menu key and Shift+F10, read through key ownership")
{
    for (const bool navigation : {true, false})
    {
        CAPTURE(navigation);
        MotionGuard guard;
        facet::test::Session session;
        REQUIRE(session.Ready());
        if (!navigation)
            ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
        int itemRequests = 0;
        int windowRequests = 0;
        bool claimMenuKey = false;
        ImVec2 at;
        session.SetUi([&] {
            if (claimMenuKey)
                ImGui::SetKeyOwner(ImGuiKey_Menu, ImHashStr("test.capture"));
            ImGui::Button("target");
            session.Track("target");
            ImVec2 where;
            if (fi::ContextMenuRequested(fi::ContextRequestTarget::LastItem, &where))
            {
                ++itemRequests;
                at = where;
            }
            ImGui::Dummy(ImVec2(10.0f, 200.0f));
            session.Track("space");
            if (fi::ContextMenuRequested(fi::ContextRequestTarget::Window, nullptr))
                ++windowRequests;
        });
        session.Frame();
        const ImVec2 center = session.ItemCenter("target");
        session.Click(center, ImGuiMouseButton_Right);
        CHECK(itemRequests == 1);
        CHECK(windowRequests == 0);
        CHECK(at.x == center.x);
        CHECK(at.y == center.y);
        // A left click gives the target the keyboard focus; the keys ask for its menu.
        session.Click(center);
        session.KeyPress(ImGuiKey_Menu);
        CHECK(itemRequests == 2);
        CHECK(at.x == session.ItemRect("target").Min.x);
        CHECK(at.y == session.ItemRect("target").Max.y);
        session.KeyPress(ImGuiKey_F10 | ImGuiMod_Shift);
        CHECK(itemRequests == 3);
        session.KeyPress(ImGuiKey_F10);
        CHECK(itemRequests == 3);
        CHECK(windowRequests == 0);
        // A key another owner holds asks for nothing.
        claimMenuKey = true;
        session.Frame();
        session.KeyPress(ImGuiKey_Menu);
        CHECK(itemRequests == 3);
        claimMenuKey = false;
        // A right click on empty space asks for the window's menu.
        session.Click(ImVec2(400.0f, 500.0f), ImGuiMouseButton_Right);
        CHECK(windowRequests == 1);
        CHECK(itemRequests == 3);
    }
}

TEST_CASE("popup host: a modal reports scrim clicks, blocks the background and stacks a confirm level")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool requestModal = false;
    bool requestConfirm = false;
    bool modal = false;
    bool confirm = false;
    int scrimClicks = 0;
    int backgroundPresses = 0;
    bool topIsConfirm = false;
    session.SetUi([&] {
        if (ImGui::Button("background"))
            ++backgroundPresses;
        session.Track("background");
        if (requestModal)
        {
            fi::OpenPopupLevel(kLevel1, {}, {.kind = PopupKind::Modal, .size = ImVec2(300.0f, 200.0f)});
            requestModal = false;
        }
        modal = confirm = false;
        if (fi::BeginPopupLevel(kLevel1))
        {
            modal = true;
            ImGui::Button("modal button");
            session.Track("modal button");
            if (requestConfirm)
            {
                fi::OpenPopupLevel(kLevel2, {}, {.kind = PopupKind::Modal});
                requestConfirm = false;
            }
            if (fi::BeginPopupLevel(kLevel2))
            {
                confirm = true;
                ImGui::Button("confirm");
                fi::EndPopupLevel();
            }
            fi::EndPopupLevel();
        }
        if (fi::PopupLevelScrimClicked(kLevel1))
            ++scrimClicks;
        topIsConfirm = fi::TopPopupLevel() == kLevel2;
    });
    session.Frame();
    requestModal = true;
    session.Frames(3);
    REQUIRE(modal);
    // Centered in the work area.
    const ImRect rect = fi::PopupLevelRect(kLevel1);
    CHECK(rect.GetCenter().x == doctest::Approx(400.0f).epsilon(0.01));
    // The background button lies under the scrim: a click there is a scrim click and presses nothing.
    session.Click(session.ItemCenter("background"));
    CHECK(scrimClicks == 1);
    CHECK(backgroundPresses == 0);
    CHECK(modal);
    // The focus stays inside: Tab cannot reach the background.
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(G().NavId != session.ItemId("background"));
    requestConfirm = true;
    session.Frames(3);
    CHECK(confirm);
    CHECK(topIsConfirm);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(confirm);
    CHECK(modal);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(modal);
}

TEST_CASE("popup host: tooltip delay, grace between items, hiding on input, keyboard focus, placement next to the pointer")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool shownA = false;
    bool shownB = false;
    ImGuiID idA = 0;
    session.SetUi([&] {
        ImGui::Button("item a");
        session.Track("a");
        idA = ImGui::GetItemID();
        const bool focusA = ImGui::IsItemFocused() && fi::KeyboardModality();
        shownA = false;
        if (fi::BeginPopupTooltip(idA, LastItemRect(), ImGui::IsItemHovered(), focusA))
        {
            shownA = true;
            ImGui::TextUnformatted("tooltip a");
            fi::EndPopupTooltip();
        }
        ImGui::SameLine();
        ImGui::Button("item b");
        session.Track("b");
        shownB = false;
        if (fi::BeginPopupTooltip(ImGui::GetItemID(), LastItemRect(), ImGui::IsItemHovered(), false))
        {
            shownB = true;
            ImGui::TextUnformatted("tooltip b");
            fi::EndPopupTooltip();
        }
    });
    session.Frame();
    session.MouseMove(session.ItemCenter("a"));
    session.Seconds(0.3f);
    CHECK_FALSE(shownA);
    session.Seconds(0.3f);
    CHECK(shownA);
    // Below and after the pointer: the cursor does not cover it.
    session.Frame();
    const ImRect tip = fi::PopupLevelRect(fi::PopupTooltipLevel(idA));
    CHECK(tip.Min.x > session.ItemCenter("a").x);
    CHECK(tip.Min.y > session.ItemCenter("a").y);
    // The neighbour shows its tooltip at once.
    session.MouseMove(session.ItemCenter("b"));
    CHECK(shownB);
    CHECK_FALSE(shownA);
    // A mouse press hides it until the pointer leaves the item.
    session.MouseDown();
    CHECK_FALSE(shownB);
    session.MouseUp();
    session.Seconds(0.6f);
    CHECK_FALSE(shownB);
    // A key press hides it until the pointer moves.
    session.MouseMove(session.ItemCenter("a"));
    session.Seconds(0.6f);
    REQUIRE(shownA);
    session.KeyPress(ImGuiKey_A);
    session.Seconds(0.6f);
    CHECK_FALSE(shownA);
    session.MouseMove(ImVec2(session.ItemCenter("a").x + 2.0f, session.ItemCenter("a").y));
    session.Frame();
    CHECK(shownA);
    // Keyboard focus shows the tooltip of the focused item, which keeps the focus.
    session.MouseMove(ImVec2(400.0f, 500.0f));
    session.Seconds(1.0f);
    CHECK_FALSE(shownA);
    session.Click(session.ItemCenter("a"));
    session.MouseMove(ImVec2(400.0f, 500.0f));
    session.Seconds(1.0f);
    session.KeyPress(ImGuiKey_Tab);  // shows the focus on the clicked item
    session.Seconds(0.6f);
    CHECK(shownA);
    CHECK(G().NavId == idA);
}

TEST_CASE("popup host: tooltips of many items reuse a bounded set of windows")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    constexpr int kColumns = 20;
    constexpr int kItems = 200;
    int shownItem = -1;
    session.SetUi([&] {
        shownItem = -1;
        for (int i = 0; i < kItems; ++i)
        {
            if (i % kColumns != 0)
                ImGui::SameLine();
            ImGui::PushID(i);
            ImGui::Button("##item", ImVec2(30.0f, 30.0f));
            session.Track("item" + std::to_string(i));
            if (fi::BeginPopupTooltip(ImGui::GetItemID(), LastItemRect(), ImGui::IsItemHovered(), false))
            {
                shownItem = i;
                ImGui::Text("tooltip %d", i);
                fi::EndPopupTooltip();
            }
            ImGui::PopID();
        }
    });
    session.Frame();
    session.MouseMove(session.ItemCenter("item0"));
    session.Seconds(0.6f);
    REQUIRE(shownItem == 0);
    int windowsAfterFirstRow = 0;
    int shownCount = 1;
    for (int i = 1; i < kItems; ++i)
    {
        // Moving straight to the next item keeps the tooltips coming at once.
        session.MouseMove(session.ItemCenter("item" + std::to_string(i)));
        session.Frame();
        shownCount += shownItem == i ? 1 : 0;
        if (i == kColumns * 2)
            windowsAfterFirstRow = G().Windows.Size;
    }
    CHECK(shownCount == kItems);
    CHECK(G().Windows.Size == windowsAfterFirstRow);
}

TEST_CASE("popup host: placement at the screen edges at scale 0.75 and 2, following the anchor and the display size")
{
    for (const float scale : {0.75f, 2.0f})
    {
        CAPTURE(scale);
        MotionGuard guard;
        facet::test::Session session;
        REQUIRE(session.Ready());
        facet::SetUiScale(scale);
        ImRect anchor(ImVec2(760.0f, 560.0f), ImVec2(790.0f, 590.0f));
        bool requestOpen = true;
        ImVec2 inset(0.0f, 0.0f);  // the work area shrinks from the bottom right (a resized host window)
        session.SetUi([&] {
            ImGuiViewportP* viewport = static_cast<ImGuiViewportP*>(ImGui::GetMainViewport());
            viewport->WorkInsetMax = inset;
            viewport->UpdateWorkRect();
            if (requestOpen)
            {
                fi::OpenPopupLevel(kLevel1, {.rect = anchor, .side = PopupSide::Below});
                requestOpen = false;
            }
            fi::SetPopupLevelAnchor(kLevel1, {.rect = anchor, .side = PopupSide::Below});
            if (fi::BeginPopupLevel(kLevel1))
            {
                ImGui::Dummy(facet::Px(ImVec2(120.0f, 80.0f)));
                fi::EndPopupLevel();
            }
        });
        session.Frames(3);
        const float margin = facet::Px(facet::T().overlay.viewportMargin);
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const ImRect bounds(ImVec2(std::ceil(margin), std::ceil(margin)),
                            ImVec2(std::floor(display.x - margin), std::floor(display.y - margin)));
        ImRect rect = fi::PopupLevelRect(kLevel1);
        CHECK(Inside(rect, bounds));
        CHECK(fi::PopupLevelSide(kLevel1) == PopupSide::Above);
        CHECK(rect.Max.y <= anchor.Min.y);
        // The anchor moves up (its list scrolls): the level follows it, placed afresh: below when
        // it fits there now, above otherwise, never over the anchor.
        anchor.Translate(ImVec2(0.0f, -100.0f));
        session.Frame();
        rect = fi::PopupLevelRect(kLevel1);
        CHECK(Inside(rect, bounds));
        CHECK_FALSE(Overlaps(rect, anchor));
        const bool fitsBelow = bounds.Max.y - (anchor.Max.y + facet::Px(facet::T().overlay.anchorGap)) >= rect.GetHeight();
        CHECK(fi::PopupLevelSide(kLevel1) == (fitsBelow ? PopupSide::Below : PopupSide::Above));
        // The work area shrinks to 500 x 400 while the level is open: it stays inside.
        inset = ImVec2(300.0f, 200.0f);
        anchor = ImRect(ImVec2(470.0f, 370.0f), ImVec2(490.0f, 390.0f));
        session.Frames(2);
        rect = fi::PopupLevelRect(kLevel1);
        CHECK(rect.Max.x <= 500.0f - margin + 0.5f);
        CHECK(rect.Max.y <= 400.0f - margin + 0.5f);
        CHECK(rect.Min.x >= margin - 0.5f);
    }
}

TEST_CASE("popup host: a forced hover places the tooltip below its item, and an item out of view shows none")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool shown = false;
    bool hiddenShown = false;
    ImRect item;
    ImRect tip;
    session.SetUi([&] {
        ImGui::SetCursorPos(ImVec2(200.0f, 200.0f));
        ImGui::Button("forced");
        const ImGuiID id = ImGui::GetItemID();
        item = LastItemRect();
        // First an item far below the window's view, hovered as a forced capture state reports
        // it: it must not take the one tooltip of the frame.
        const ImRect below(ImVec2(item.Min.x, item.Min.y + 5000.0f), ImVec2(item.Max.x, item.Max.y + 5000.0f));
        hiddenShown = fi::BeginPopupTooltip(ImGui::GetID("out of view"), below, true, false);
        if (hiddenShown)
            fi::EndPopupTooltip();
        // Then the visible item hovered the same way, with the pointer far away.
        shown = fi::BeginPopupTooltip(id, item, true, false);
        if (shown)
        {
            ImGui::TextUnformatted("tooltip");
            fi::EndPopupTooltip();
        }
        tip = fi::PopupLevelRect(fi::PopupTooltipLevel(id));
    });
    session.MouseMove(ImVec2(2.0f, 2.0f));
    session.Seconds(1.0f);
    REQUIRE(shown);
    CHECK(tip.Min.y >= item.Max.y);
    CHECK(tip.Max.x >= item.Min.x);
    CHECK_FALSE(hiddenShown);
}