// Behaviour of the overlays family (facet/widgets/overlays.h), and the parts of the popup host
// (src/internal/popup_host.h) that the family added: Panel levels, levels at a fixed rect that slide
// in, and interactive tooltips.
//
// One test case per behaviour; docs/writing-a-widget.md describes the pattern. The API checks are in
// tests/test_api_overlays.cpp.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/scope_stack.h"
#include "support/session.h"

namespace fi = facet::internal;

namespace
{

// Rests the pointer at `pos` long enough for a tooltip to show.
void HoverFor(facet::test::Session& session, ImVec2 pos, float seconds)
{
    session.MouseMove(pos);
    session.Seconds(seconds);
}

float TooltipWait()
{
    return facet::T().overlay.tooltipDelay * facet::T().motion.scale + 0.2f;
}

const ImVec2 kAway(4.0f, 590.0f);

// Motion that does not follow the system setting, restored afterwards; `reduce` turns it off.
struct OverlayMotionGuard
{
    explicit OverlayMotionGuard(bool reduce = false)
    {
        facet::SetFollowSystemMotion(false);
        facet::Theme theme = facet::Theme::Light();
        theme.motion.reduceMotion = reduce;
        facet::SetTheme(theme, false);
    }
    ~OverlayMotionGuard()
    {
        facet::SetFollowSystemMotion(true);
        facet::SetTheme(facet::Theme::Light(), false);
    }
};

bool Inside(const ImRect& inner, const ImRect& outer)
{
    return inner.Min.x >= outer.Min.x - 0.5f && inner.Min.y >= outer.Min.y - 0.5f && inner.Max.x <= outer.Max.x + 0.5f &&
           inner.Max.y <= outer.Max.y + 0.5f;
}

ImRect DisplayMinusMargin()
{
    const float margin = facet::Px(facet::T().overlay.viewportMargin);
    const ImVec2 size = ImGui::GetIO().DisplaySize;
    return ImRect(margin, margin, size.x - margin, size.y - margin);
}

const ImGuiWindow* ToastWindow(int slot)
{
    char name[32];
    std::snprintf(name, sizeof(name), "##FacetToast%02d", slot);
    return ImGui::FindWindowByName(name);
}

int ActiveToastWindows()
{
    int count = 0;
    for (int i = 0; i < 16; ++i)
    {
        const ImGuiWindow* window = ToastWindow(i);
        count += window != nullptr && window->Active ? 1 : 0;
    }
    return count;
}

} // namespace

// ---- Popup host additions ----------------------------------------------------------------------

TEST_CASE("A panel level takes input, survives a click outside and gives the focus back when closed")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImGuiID panel = ImHashStr("test.panel");
    bool open = false;
    int insidePresses = 0;
    int outsidePresses = 0;
    bool panelOpen = false;
    session.SetUi([&] {
        if (ImGui::Button("opener"))
            open = true;
        session.Track("opener");
        if (ImGui::Button("outside"))
            ++outsidePresses;
        session.Track("outside");
        if (open && !fi::IsPopupLevelOpen(panel))
            fi::OpenPopupLevel(panel, {.rect = ImRect(300.0f, 100.0f, 340.0f, 120.0f)},
                               {.kind = fi::PopupKind::Panel, .closeOnEscape = false});
        if (!open && fi::IsPopupLevelOpen(panel))
            fi::ClosePopupLevel(panel);
        if (fi::BeginPopupLevel(panel))
        {
            if (ImGui::Button("inside"))
                ++insidePresses;
            session.Track("inside");
            fi::EndPopupLevel();
        }
        panelOpen = fi::IsPopupLevelOpen(panel);
    });
    session.Frame();
    session.Click(session.ItemCenter("opener"));
    session.Frames(2);
    REQUIRE(panelOpen);
    // The focus moved into the panel.
    const ImGuiContext& g = *ImGui::GetCurrentContext();
    REQUIRE(g.NavWindow != nullptr);
    CHECK(g.NavWindow->Name != std::string("test"));
    session.Click(session.ItemCenter("inside"));
    CHECK(insidePresses == 1);
    // A click on the page reaches its item and leaves the panel open and in front: background input
    // is not blocked.
    session.Click(session.ItemCenter("outside"));
    CHECK(outsidePresses == 1);
    CHECK(panelOpen);
    session.Click(session.ItemCenter("inside"));
    CHECK(insidePresses == 2);
    // Without closeOnEscape, Escape does not close it.
    session.KeyPress(ImGuiKey_Escape);
    CHECK(panelOpen);
    // Closed while the focus is inside, the focus goes back to the page window's last item.
    open = false;
    session.Frames(2);
    CHECK_FALSE(panelOpen);
    REQUIRE(g.NavWindow != nullptr);
    CHECK(g.NavWindow->Name == std::string("test"));
    CHECK(g.NavId == session.ItemId("outside"));
}

TEST_CASE("Escape closes a panel after the lists opened from it and before the dialog it opened over")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImGuiID modal = ImHashStr("test.modal");
    const ImGuiID panel = ImHashStr("test.panel");
    const ImGuiID list = ImHashStr("test.list");
    int step = 0;
    bool modalOpen = false;
    bool panelOpen = false;
    bool listOpen = false;
    session.SetUi([&] {
        if (step == 1)
            fi::OpenPopupLevel(modal, {}, {.kind = fi::PopupKind::Modal});
        if (fi::BeginPopupLevel(modal))
        {
            ImGui::Button("in modal");
            if (step == 2)
                fi::OpenPopupLevel(panel, {.rect = ImRect(100.0f, 100.0f, 140.0f, 120.0f)}, {.kind = fi::PopupKind::Panel});
            if (fi::BeginPopupLevel(panel))
            {
                ImGui::Button("in panel");
                if (step == 3)
                    fi::OpenPopupLevel(list, {.rect = ImRect(100.0f, 300.0f, 140.0f, 320.0f)});
                if (fi::BeginPopupLevel(list))
                {
                    ImGui::Button("in list");
                    fi::EndPopupLevel();
                }
                fi::EndPopupLevel();
            }
            fi::EndPopupLevel();
        }
        modalOpen = fi::IsPopupLevelOpen(modal);
        panelOpen = fi::IsPopupLevelOpen(panel);
        listOpen = fi::IsPopupLevelOpen(list);
        step = 0;
    });
    session.Frame();
    for (int s = 1; s <= 3; ++s)
    {
        step = s;
        session.Frames(2);
    }
    REQUIRE(modalOpen);
    REQUIRE(panelOpen);
    REQUIRE(listOpen);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(modalOpen);
    CHECK(panelOpen);
    CHECK_FALSE(listOpen);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(modalOpen);
    CHECK_FALSE(panelOpen);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(modalOpen);
}

TEST_CASE("A panel that closes on a click outside lets the press reach the page")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImGuiID panel = ImHashStr("test.panel");
    bool open = true;
    int pagePresses = 0;
    bool panelOpen = false;
    fi::PopupCloseReason reason = fi::PopupCloseReason::None;
    session.SetUi([&] {
        if (ImGui::Button("page"))
            ++pagePresses;
        session.Track("page");
        const fi::PopupAnchor anchor{.rect = ImRect(400.0f, 100.0f, 440.0f, 120.0f)};
        if (open && !fi::IsPopupLevelOpen(panel))
            fi::OpenPopupLevel(panel, anchor, {.kind = fi::PopupKind::Panel, .closeOnClickOutside = true});
        open = false;
        if (fi::PopupLevelClosedReason(panel) != fi::PopupCloseReason::None)
            reason = fi::PopupLevelClosedReason(panel);
        if (fi::BeginPopupLevel(panel))
        {
            ImGui::Button("inside");
            session.Track("inside");
            fi::EndPopupLevel();
        }
        panelOpen = fi::IsPopupLevelOpen(panel);
    });
    session.Frames(3);
    REQUIRE(panelOpen);
    // A press inside keeps it, and so does a press on its anchor rect.
    session.Click(session.ItemCenter("inside"));
    CHECK(panelOpen);
    session.Click(ImVec2(420.0f, 110.0f));
    CHECK(panelOpen);
    // A press on the page closes it, and the page's button still takes the click.
    session.Click(session.ItemCenter("page"));
    CHECK_FALSE(panelOpen);
    CHECK(pagePresses == 1);
    CHECK(reason == fi::PopupCloseReason::ClickOutside);
}

TEST_CASE("A level with a fixed rect sits there and slides in from its edge")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImGuiID level = ImHashStr("test.slide");
    const ImRect place(600.0f, 0.0f, 800.0f, 600.0f);
    session.SetUi([&] {
        if (ImGui::GetFrameCount() == 3)
            fi::OpenPopupLevel(level, {}, {.kind = fi::PopupKind::Modal, .placeRect = place, .slideIn = true,
                                           .slideEdge = fi::PopupSide::After});
        if (fi::BeginPopupLevel(level))
        {
            ImGui::TextUnformatted("content");
            fi::EndPopupLevel();
        }
    });
    session.Frames(5);
    const ImRect rect = fi::PopupLevelRect(level);
    CHECK(rect.Min.x == doctest::Approx(place.Min.x));
    CHECK(rect.Max.y == doctest::Approx(place.Max.y));
    // Once settled, the window is at the rect, its full size.
    session.Seconds(0.5f);
    char name[32];
    std::snprintf(name, sizeof(name), "##Popup_%08x", level);
    ImGuiWindow* window = ImGui::FindWindowByName(name);
    REQUIRE(window != nullptr);
    CHECK(window->Pos.x == doctest::Approx(place.Min.x));
    CHECK(window->Size.y == doctest::Approx(place.GetHeight()));
}

TEST_CASE("A sliding level starts outside its rect and reaches it, at once under reduced motion")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImGuiID level = ImHashStr("test.slide.motion");
    const ImRect place(600.0f, 0.0f, 800.0f, 600.0f);
    bool openNow = false;
    session.SetUi([&] {
        // A Popup level: no scrim, so every vertex belongs to the sliding surface and content.
        if (openNow)
            fi::OpenPopupLevel(level, {}, {.kind = fi::PopupKind::Popup, .placeRect = place, .slideIn = true,
                                           .slideEdge = fi::PopupSide::After});
        openNow = false;
        if (fi::BeginPopupLevel(level))
        {
            ImGui::TextUnformatted("content");
            fi::EndPopupLevel();
        }
    });
    session.Frame();
    // The content vertices of the first shown frame lie right of the rect's start: the level slides in.
    openNow = true;
    session.Frames(2);
    char name[32];
    std::snprintf(name, sizeof(name), "##Popup_%08x", level);
    const ImGuiWindow* window = ImGui::FindWindowByName(name);
    REQUIRE(window != nullptr);
    float minX = 1.0e9f;
    for (const ImDrawVert& v : window->DrawList->VtxBuffer)
        minX = std::min(minX, v.pos.x);
    CHECK(minX > place.Min.x + 1.0f);
}

TEST_CASE("An interactive tooltip stays while the pointer moves into it and takes its clicks")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    bool tipOpen = false;
    session.SetUi([&] {
        ImGui::Button("anchor");
        session.Track("anchor");
        const ImGuiID item = ImGui::GetItemID();
        if (fi::BeginPopupTooltip(item, session.ItemRect("anchor"), ImGui::IsItemHovered(), false,
                                  fi::PopupTooltipDesc{.interactive = true}))
        {
            if (ImGui::Button("in tip"))
                ++presses;
            session.Track("tip button");
            fi::EndPopupTooltip();
        }
        tipOpen = fi::IsPopupLevelOpen(fi::PopupTooltipLevel(item));
    });
    session.Frame();
    HoverFor(session, session.ItemCenter("anchor"), TooltipWait());
    REQUIRE(tipOpen);
    // Move into the tooltip in a few steps, as a pointer travels, then click its button.
    const ImVec2 from = session.ItemCenter("anchor");
    const ImVec2 to = session.ItemCenter("tip button");
    for (int i = 1; i <= 4; ++i)
        session.MouseMove(ImVec2(from.x + (to.x - from.x) * i / 4.0f, from.y + (to.y - from.y) * i / 4.0f));
    CHECK(tipOpen);
    session.Click(session.ItemCenter("tip button"));
    CHECK(presses == 1);
    CHECK(tipOpen);
    // Leaving both closes it.
    session.MouseMove(kAway);
    session.Frames(2);
    CHECK_FALSE(tipOpen);
}

// ---- Tooltips ------------------------------------------------------------------------------------

TEST_CASE("ItemTooltip shows after the hover delay, hides when the pointer leaves and takes no focus")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int presses = 0;
    bool shown = false;
    session.SetUi([&] {
        if (facet::Button("Save"))
            ++presses;
        session.Track("save");
        facet::ItemTooltip("Saves the %s", "file");
        shown = fi::IsPopupLevelOpen(fi::PopupTooltipLevel(ImGui::GetItemID()));
        ImGui::Button("other");
        session.Track("other");
    });
    session.Frame();
    session.MouseMove(session.ItemCenter("save"));
    session.Seconds(facet::T().overlay.tooltipDelay * 0.5f);
    CHECK_FALSE(shown);
    session.Seconds(facet::T().overlay.tooltipDelay);
    CHECK(shown);
    // The tooltip window lets input through and never holds the keyboard focus.
    const ImGuiContext& g = *ImGui::GetCurrentContext();
    const ImGuiID focusBefore = g.NavId;
    session.Click(session.ItemCenter("save"));
    CHECK(presses == 1);
    CHECK(g.NavWindow != nullptr);
    CHECK((g.NavWindow->Flags & ImGuiWindowFlags_Tooltip) == 0);
    (void)focusBefore;
    session.MouseMove(kAway);
    session.Frames(2);
    CHECK_FALSE(shown);
}

TEST_CASE("ItemTooltip shows on keyboard focus and never for a disabled item")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool shownEnabled = false;
    bool shownDisabled = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::Button("Enabled");
        session.Track("enabled");
        facet::ItemTooltip("Enabled tip");
        shownEnabled = fi::IsPopupLevelOpen(fi::PopupTooltipLevel(ImGui::GetItemID()));
        ImGui::BeginDisabled();
        facet::Button("Disabled");
        session.Track("disabled");
        facet::ItemTooltip("Disabled tip");
        shownDisabled = fi::IsPopupLevelOpen(fi::PopupTooltipLevel(ImGui::GetItemID()));
        ImGui::EndDisabled();
    });
    session.Frame();
    HoverFor(session, session.ItemCenter("disabled"), TooltipWait());
    CHECK_FALSE(shownDisabled);
    session.MouseMove(kAway);
    session.Click(session.ItemCenter("before"));
    session.MouseMove(kAway);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("enabled"));
    session.Seconds(TooltipWait());
    CHECK(shownEnabled);
    // Tab does not land on the disabled button, so the focus leaves and the tooltip with it.
    session.KeyPress(ImGuiKey_Tab);
    session.Frames(2);
    CHECK_FALSE(shownEnabled);
    CHECK_FALSE(shownDisabled);
}

TEST_CASE("A rich tooltip submits its content only while it shows and keeps the scopes balanced")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int contentFrames = 0;
    session.SetUi([&] {
        ImGui::Button("item");
        session.Track("item");
        if (facet::BeginRichTooltip({.maxWidth = 120.0f}))
        {
            ++contentFrames;
            ImGui::TextUnformatted("A long rich tooltip text that wraps at its width");
            facet::EndRichTooltip();
        }
        // The scope class ends only what began.
        facet::RichTooltipScope scope;
    });
    session.Frames(3);
    CHECK(contentFrames == 0);
    HoverFor(session, session.ItemCenter("item"), TooltipWait());
    CHECK(contentFrames > 0);
    CHECK(fi::OpenScopeCount() == 0);
    // The text wraps at maxWidth: the tooltip is no wider than it plus its padding.
    const ImRect tip = fi::PopupLevelRect(fi::PopupTooltipLevel(session.ItemId("item")));
    CHECK(tip.GetWidth() <= facet::Px(120.0f) + facet::Px(facet::T().spacing.sm) * 2.0f + 1.0f);
}

TEST_CASE("A forced capture state shows a tooltip below its item without input")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool shown = false;
    ImRect tip;
    session.SetUi([&] {
        ImGui::SetCursorPos(ImVec2(100.0f, 100.0f));
        facet::Button("Item");
        session.Track("item");
        fi::ForceNextItemState(fi::ForcedItemState::Hovered);
        facet::ItemTooltip("Forced");
        shown = fi::IsPopupLevelOpen(fi::PopupTooltipLevel(ImGui::GetItemID()));
        tip = fi::PopupLevelRect(fi::PopupTooltipLevel(ImGui::GetItemID()));
    });
    session.Frames(3);
    CHECK(shown);
    CHECK(tip.Min.y >= session.ItemRect("item").Max.y);
    int visibleTooltips = 0;
    for (const ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
        visibleTooltips += window->Active && !window->Hidden && (window->Flags & ImGuiWindowFlags_Tooltip) != 0 ? 1 : 0;
    CHECK(visibleTooltips == 1);
}
// ---- Popover -------------------------------------------------------------------------------------

TEST_CASE("A popover opens below its anchor, takes the focus, and Escape closes it and gives the focus back")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = false;
    ImRect card;
    ImRect anchor;
    session.SetUi([&] {
        if (facet::Button("Filters"))
            open = !open;
        session.Track("anchor");
        anchor = session.ItemRect("anchor");
        if (facet::BeginPopover("filters", &open, {.placement = facet::PopoverPlacement::Below}))
        {
            facet::Button("Inside");
            session.Track("inside");
            facet::EndPopover();
        }
        card = fi::PopupLevelRect(ImGui::GetID("filters"));
    });
    session.Frame();
    session.Click(session.ItemCenter("anchor"));
    session.Seconds(0.3f);
    REQUIRE(open);
    CHECK(card.Min.y >= anchor.Max.y);
    CHECK(card.GetWidth() > 0.0f);
    const ImGuiContext& g = *ImGui::GetCurrentContext();
    CHECK(g.NavId == session.ItemId("inside"));
    session.KeyPress(ImGuiKey_Escape);
    session.Frame();
    CHECK_FALSE(open);
    CHECK(g.NavId == session.ItemId("anchor"));
}

TEST_CASE("A click outside a popover closes it and still presses the item there; the anchor toggles it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = false;
    int otherPresses = 0;
    session.SetUi([&] {
        if (facet::Button("Anchor"))
            open = !open;
        session.Track("anchor");
        if (facet::BeginPopover("pop", &open))
        {
            ImGui::TextUnformatted("Content");
            facet::EndPopover();
        }
        ImGui::SetCursorPos(ImVec2(500.0f, 500.0f));
        if (facet::Button("Other"))
            ++otherPresses;
        session.Track("other");
    });
    session.Frame();
    session.Click(session.ItemCenter("anchor"));
    session.Seconds(0.3f);
    REQUIRE(open);
    // The page under a popover keeps taking input: the press closes the popover and reaches the button.
    session.Click(session.ItemCenter("other"));
    CHECK_FALSE(open);
    CHECK(otherPresses == 1);
    // A second click on the anchor closes the popover instead of opening it again.
    session.Click(session.ItemCenter("anchor"));
    session.Seconds(0.3f);
    REQUIRE(open);
    session.Click(session.ItemCenter("anchor"));
    session.Frame();
    CHECK_FALSE(open);
}

TEST_CASE("A popover stays inside the display and off its anchor at every edge and corner, at scale 0.75 and 2")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    const ImVec2 corners[] = {{0.0f, 0.0f},   {380.0f, 0.0f},   {740.0f, 0.0f},  {0.0f, 280.0f},  {740.0f, 280.0f},
                              {0.0f, 560.0f}, {380.0f, 560.0f}, {740.0f, 560.0f}};
    const float scales[] = {0.75f, 2.0f};
    const facet::PopoverPlacement placements[] = {facet::PopoverPlacement::Auto, facet::PopoverPlacement::Below,
                                                  facet::PopoverPlacement::End};
    for (const float scale : scales)
    {
        facet::SetUiScale(scale);
        for (const facet::PopoverPlacement placement : placements)
        {
            for (const ImVec2 at : corners)
            {
                bool open = true;
                ImRect card;
                ImRect anchor;
                session.SetUi([&] {
                    ImGui::SetCursorScreenPos(at);
                    ImGui::Button("A", ImVec2(40.0f, 24.0f));
                    anchor = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                    if (facet::BeginPopover("edge", &open, {.placement = placement, .width = 160.0f}))
                    {
                        ImGui::Dummy(ImVec2(facet::Px(140.0f), facet::Px(80.0f)));
                        facet::EndPopover();
                    }
                    card = fi::PopupLevelRect(ImGui::GetID("edge"));
                });
                session.Seconds(0.3f);
                INFO("scale " << scale << " anchor at " << at.x << ", " << at.y);
                CHECK(Inside(card, DisplayMinusMargin()));
                CHECK_FALSE(card.Overlaps(anchor));
                open = false;
                session.Frames(2);
            }
        }
    }
    facet::SetUiScale(1.0f);
}

TEST_CASE("A popover follows its anchor when the window scrolls and when the anchor moves")

{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = true;
    float scroll = 0.0f;
    float x = 300.0f;
    ImRect card;
    ImRect anchor;
    session.SetUi([&] {
        ImGui::SetScrollY(scroll);
        ImGui::SetCursorPos(ImVec2(x, 200.0f));
        ImGui::Button("anchor");
        anchor = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        if (facet::BeginPopover("follow", &open, {.placement = facet::PopoverPlacement::Below}))
        {
            ImGui::TextUnformatted("Content");
            facet::EndPopover();
        }
        card = fi::PopupLevelRect(ImGui::GetID("follow"));
        ImGui::Dummy(ImVec2(10.0f, 1200.0f));
    });
    session.Seconds(0.3f);
    const float gapBefore = card.Min.y - anchor.Max.y;
    scroll = 80.0f;
    session.Frames(3);
    REQUIRE(open);
    CHECK(card.Min.y - anchor.Max.y == doctest::Approx(gapBefore).epsilon(0.01));
    // An anchor that moves next to the display edge (a window resized under it) takes the card along,
    // and the card stays inside the display.
    x = 760.0f;
    session.Frames(3);
    CHECK(card.Max.x <= 800.0f - facet::Px(facet::T().overlay.viewportMargin) + 0.5f);
    CHECK(card.Min.y > anchor.Max.y);
}

TEST_CASE("A popover that is not dismissible stays on Escape and on a click outside")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = true;
    session.SetUi([&] {
        ImGui::Button("anchor");
        if (facet::BeginPopover("stay", &open, {.dismissible = false}))
        {
            ImGui::TextUnformatted("Content");
            facet::EndPopover();
        }
        ImGui::SetCursorPos(ImVec2(500.0f, 500.0f));
        ImGui::Button("away");
        session.Track("away");
    });
    session.Seconds(0.3f);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(open);
    session.Click(session.ItemCenter("away"));
    CHECK(open);
}

TEST_CASE("A popover with openOnHover opens after the delay and closes when the pointer leaves both")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = false;
    ImRect card;
    session.SetUi([&] {
        ImGui::SetCursorPos(ImVec2(100.0f, 100.0f));
        ImGui::Button("hover me");
        session.Track("anchor");
        if (facet::BeginPopover("hover", &open, {.placement = facet::PopoverPlacement::Below, .openOnHover = true}))
        {
            ImGui::TextUnformatted("Content");
            facet::EndPopover();
        }
        card = fi::PopupLevelRect(ImGui::GetID("hover"));
    });
    session.Frame();
    session.MouseMove(session.ItemCenter("anchor"));
    session.Frame();
    CHECK_FALSE(open);
    session.Seconds(facet::T().overlay.submenuDelay + 0.2f);
    CHECK(open);
    // Into the card and back: it stays.
    session.MouseMove(card.GetCenter());
    session.Frames(2);
    CHECK(open);
    session.MouseMove(kAway);
    session.Frames(2);
    CHECK_FALSE(open);
}

TEST_CASE("Popover content taller than the display scrolls inside the card")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = true;
    ImRect card;
    session.SetUi([&] {
        ImGui::Button("anchor");
        if (facet::BeginPopover("tall", &open))
        {
            for (int i = 0; i < 80; ++i)
                ImGui::Text("Row %d", i);
            facet::EndPopover();
        }
        card = fi::PopupLevelRect(ImGui::GetID("tall"));
    });
    session.Seconds(0.3f);
    CHECK(Inside(card, DisplayMinusMargin()));
    CHECK(card.GetHeight() > facet::Px(facet::T().metrics.controlHeight));
}

// ---- Context menu --------------------------------------------------------------------------------

TEST_CASE("A context menu opens on a right click at the pointer; a row runs and closes it; Escape closes it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    int ran = 0;
    bool menuOpen = false;
    ImRect menu;
    session.SetUi([&] {
        ImGui::SetCursorPos(ImVec2(200.0f, 150.0f));
        facet::Button("Target");
        session.Track("target");
        if (facet::BeginContextMenu("menu"))
        {
            const ImGuiID level = fi::CurrentPopupLevel();
            if (fi::MenuRow(level, "Rename").pressed)
                ++ran;
            session.Track("row");
            facet::EndContextMenu();
        }
        menuOpen = fi::IsPopupLevelOpen(ImGui::GetID("menu"));
        menu = fi::PopupLevelRect(ImGui::GetID("menu"));
    });
    session.Frame();
    const ImVec2 at = session.ItemCenter("target");
    session.Click(at, ImGuiMouseButton_Right);
    session.Seconds(0.3f);
    REQUIRE(menuOpen);
    CHECK(menu.Min.y >= at.y - 1.0f);
    CHECK(menu.GetWidth() >= facet::Px(facet::T().overlay.menuMinWidth) - 1.0f);
    session.Click(session.ItemCenter("row"));
    session.Frame();
    CHECK(ran == 1);
    CHECK_FALSE(menuOpen);
    session.Click(at, ImGuiMouseButton_Right);
    session.Seconds(0.3f);
    REQUIRE(menuOpen);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(menuOpen);
}

TEST_CASE("The Menu key and Shift+F10 open a context menu at the focused item; the focus comes back")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool menuOpen = false;
    ImRect menu;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::Button("Target");
        session.Track("target");
        if (facet::BeginContextMenu("menu"))
        {
            (void)fi::MenuRow(fi::CurrentPopupLevel(), "Copy");
            facet::EndContextMenu();
        }
        menuOpen = fi::IsPopupLevelOpen(ImGui::GetID("menu"));
        menu = fi::PopupLevelRect(ImGui::GetID("menu"));
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("target"));
    session.KeyPress(ImGuiKey_Menu);
    session.Seconds(0.3f);
    REQUIRE(menuOpen);
    CHECK(menu.Min.y >= session.ItemRect("target").Max.y - 1.0f);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(menuOpen);
    CHECK(ImGui::GetFocusID() == session.ItemId("target"));
    session.KeyPress(ImGuiKey_F10 | ImGuiMod_Shift);
    session.Seconds(0.3f);
    CHECK(menuOpen);
}

TEST_CASE("A window context menu opens on a right click on empty space")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool menuOpen = false;
    session.SetUi([&] {
        ImGui::Button("item");
        if (facet::BeginContextMenu("space", {.target = facet::ContextTarget::Window}))
        {
            (void)fi::MenuRow(fi::CurrentPopupLevel(), "Paste");
            facet::EndContextMenu();
        }
        menuOpen = fi::IsPopupLevelOpen(ImGui::GetID("space"));
    });
    session.Frame();
    session.Click(ImVec2(400.0f, 400.0f), ImGuiMouseButton_Right);
    session.Frames(2);
    CHECK(menuOpen);
}

// ---- Modal ---------------------------------------------------------------------------------------

TEST_CASE("A modal takes the focus, keeps Tab inside, blocks the page and gives the focus back on Escape")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = false;
    int pagePresses = 0;
    ImRect dialog;
    session.SetUi([&] {
        if (facet::Button("Open"))
            open = true;
        session.Track("opener");
        if (facet::Button("Page"))
            ++pagePresses;
        session.Track("page");
        if (facet::BeginModal("Settings", &open))
        {
            facet::Button("First");
            session.Track("first");
            facet::Button("Second");
            session.Track("second");
            facet::EndModal();
        }
        dialog = fi::PopupLevelRect(ImGui::GetID("Settings"));
    });
    session.Frame();
    session.Click(session.ItemCenter("opener"));
    session.Seconds(0.3f);
    REQUIRE(open);
    // Centered in the display.
    CHECK(std::fabs(dialog.GetCenter().x - 400.0f) < 2.0f);
    CHECK(ImGui::GetFocusID() == session.ItemId("first"));
    // Tab cycles inside the dialog: the first two stops, then the close button, then back.
    const ImGuiContext& g = *ImGui::GetCurrentContext();
    for (int i = 0; i < 6; ++i)
    {
        session.KeyPress(ImGuiKey_Tab);
        REQUIRE(g.NavWindow != nullptr);
        CHECK(g.NavId != session.ItemId("page"));
        CHECK(g.NavId != session.ItemId("opener"));
    }
    session.Click(session.ItemCenter("first"));
    session.KeyPress(ImGuiKey_Escape);
    session.Frame();
    CHECK_FALSE(open);
    CHECK(ImGui::GetFocusID() == session.ItemId("opener"));
    // The page under it takes no click: a click there lands on the scrim, which dismisses the
    // dialog, and the focus goes back to the opener all the same.
    session.Click(session.ItemCenter("opener"));
    session.Seconds(0.3f);
    REQUIRE(open);
    session.Click(session.ItemCenter("page"));
    session.Frame();
    CHECK(pagePresses == 0);
    CHECK_FALSE(open);
    CHECK(ImGui::GetFocusID() == session.ItemId("opener"));
}

TEST_CASE("A modal closes on the scrim and on Escape; one that is not dismissible ignores both")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = true;
    bool fixed = false;
    session.SetUi([&] {
        if (facet::BeginModal("Dismissible", &open))
        {
            ImGui::TextUnformatted("Content");
            facet::EndModal();
        }
        if (facet::BeginModal("Must answer###fixed", &fixed, {.dismissible = false, .closeButton = false}))
        {
            if (facet::Button("Done"))
                fixed = false;
            session.Track("done");
            facet::EndModal();
        }
    });
    session.Seconds(0.3f);
    session.Click(ImVec2(10.0f, 10.0f));
    session.Frame();
    CHECK_FALSE(open);
    open = true;
    session.Seconds(0.3f);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(open);
    fixed = true;
    session.Seconds(0.3f);
    session.KeyPress(ImGuiKey_Escape);
    session.Click(ImVec2(10.0f, 10.0f));
    session.Frame();
    CHECK(fixed);
    session.Click(session.ItemCenter("done"));
    session.Frame();
    CHECK_FALSE(fixed);
}

TEST_CASE("Modal content taller than the display scrolls inside a dialog that fits the display")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = true;
    ImRect dialog;
    session.SetUi([&] {
        if (facet::BeginModal("Long", &open))
        {
            for (int i = 0; i < 100; ++i)
                ImGui::Text("Line %d", i);
            facet::EndModal();
        }
        dialog = fi::PopupLevelRect(ImGui::GetID("Long"));
    });
    session.Seconds(0.3f);
    CHECK(Inside(dialog, DisplayMinusMargin()));
    CHECK(dialog.GetWidth() == doctest::Approx(facet::Px(facet::T().overlay.modalWidth)));
}

// ---- Confirm dialog ------------------------------------------------------------------------------

TEST_CASE("A confirm dialog over a modal closes first on Escape, and each gives the focus back in turn")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool modal = false;
    bool confirm = false;
    facet::DialogResult last = facet::DialogResult::None;
    session.SetUi([&] {
        if (facet::Button("Open"))
            modal = true;
        session.Track("opener");
        if (facet::BeginModal("Outer", &modal))
        {
            if (facet::Button("Delete"))
                confirm = true;
            session.Track("delete");
            const facet::DialogResult result = facet::ConfirmDialog("Delete it?", &confirm, "It cannot be restored.");
            if (result != facet::DialogResult::None)
                last = result;
            facet::EndModal();
        }
    });
    session.Frame();
    session.Click(session.ItemCenter("opener"));
    session.Seconds(0.3f);
    session.Click(session.ItemCenter("delete"));
    session.Seconds(0.3f);
    REQUIRE(confirm);
    session.KeyPress(ImGuiKey_Escape);
    session.Frame();
    CHECK_FALSE(confirm);
    CHECK(modal);
    CHECK(last == facet::DialogResult::Canceled);
    CHECK(ImGui::GetFocusID() == session.ItemId("delete"));
    session.KeyPress(ImGuiKey_Escape);
    session.Frame();
    CHECK_FALSE(modal);
    CHECK(ImGui::GetFocusID() == session.ItemId("opener"));
}

TEST_CASE("Enter confirms a confirm dialog once and writes the flag; Escape cancels")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = false;
    int confirmed = 0;
    int canceled = 0;
    session.SetUi([&] {
        if (facet::Button("Apply"))
            open = true;
        session.Track("opener");
        const facet::DialogResult result = facet::ConfirmDialog("Apply?", &open, "Apply the changes.");
        confirmed += result == facet::DialogResult::Confirmed ? 1 : 0;
        canceled += result == facet::DialogResult::Canceled ? 1 : 0;
    });
    session.Frame();
    session.Click(session.ItemCenter("opener"));
    session.Seconds(0.3f);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(confirmed == 1);
    CHECK_FALSE(open);
    CHECK(ImGui::GetFocusID() == session.ItemId("opener"));
    session.Click(session.ItemCenter("opener"));
    session.Seconds(0.3f);
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK(canceled == 1);
    CHECK(confirmed == 1);
    CHECK_FALSE(open);
}

TEST_CASE("A destructive confirm dialog starts on Cancel, so Enter cancels; the scrim cancels too")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = false;
    facet::DialogResult last = facet::DialogResult::None;
    session.SetUi([&] {
        if (facet::Button("Delete"))
            open = true;
        session.Track("opener");
        const facet::DialogResult result =
            facet::ConfirmDialog("Delete?", &open, "It cannot be restored.", {.confirmLabel = "Delete", .destructive = true});
        if (result != facet::DialogResult::None)
            last = result;
    });
    session.Frame();
    session.Click(session.ItemCenter("opener"));
    session.Seconds(0.3f);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(last == facet::DialogResult::Canceled);
    CHECK_FALSE(open);
    last = facet::DialogResult::None;
    session.Click(session.ItemCenter("opener"));
    session.Seconds(0.3f);
    session.Click(ImVec2(5.0f, 500.0f));
    session.Frames(2);
    CHECK(last == facet::DialogResult::Canceled);
}

TEST_CASE("A hold to confirm dialog confirms only after the button was held long enough")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = false;
    int confirmed = 0;
    session.SetUi([&] {
        if (facet::Button("Reset"))
            open = true;
        session.Track("opener");
        const facet::DialogResult result =
            facet::ConfirmDialog("Reset?", &open, "Every setting returns to its default.", {.holdToConfirm = true});
        confirmed += result == facet::DialogResult::Confirmed ? 1 : 0;
    });
    session.Frame();
    session.Click(session.ItemCenter("opener"));
    session.Seconds(0.3f);
    // Enter does not bypass the hold.
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(confirmed == 0);
    CHECK(open);
    // A short press does nothing.
    session.KeyPress(ImGuiKey_Tab);  // shows the focus, which starts on the confirm button
    session.KeyPress(ImGuiKey_Space);
    session.Frames(2);
    CHECK(confirmed == 0);
    // Space held on the focused confirm button for longer than motion.durationSlow confirms.
    session.HoldKey(ImGuiKey_Space, facet::T().motion.durationSlow + 0.2f);
    session.Frames(2);
    CHECK(confirmed == 1);
    CHECK_FALSE(open);
}

// ---- Toasts --------------------------------------------------------------------------------------

TEST_CASE("ShowToast queues inside a frame only, and the queries follow the toast")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    CHECK_FALSE(facet::ShowToast("outside a frame").IsValid());
    facet::ToastId toast;
    bool show = true;
    session.SetUi([&] {
        if (show)
            toast = facet::ShowToast("Saved", {.tone = facet::Tone::Success, .duration = -1.0f});
        show = false;
    });
    session.Frame();
    REQUIRE(toast.IsValid());
    CHECK(facet::IsToastVisible(toast));
    session.Frames(2);
    CHECK(ActiveToastWindows() == 1);
    facet::DismissToast(toast);
    CHECK_FALSE(facet::IsToastVisible(toast));
    session.Seconds(0.5f);
    CHECK(ActiveToastWindows() == 0);
}

TEST_CASE("At most toastMaxCount toasts show; the rest wait and show in queue order as others leave")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    facet::ToastId toasts[5];
    bool show = true;
    session.SetUi([&] {
        if (show)
        {
            char text[16];
            for (int i = 0; i < 5; ++i)
            {
                std::snprintf(text, sizeof(text), "Toast %d", i);
                toasts[i] = facet::ShowToast(text, {.duration = -1.0f});
            }
        }
        show = false;
    });
    session.Frames(3);
    const int max = facet::T().overlay.toastMaxCount;
    CHECK(ActiveToastWindows() == max);
    for (const facet::ToastId& toast : toasts)
        CHECK(facet::IsToastVisible(toast));
    // The oldest leaves first; then the first waiting one (the fourth queued) shows.
    facet::DismissToast(toasts[0]);
    session.Seconds(0.5f);
    CHECK(ActiveToastWindows() == max);
    CHECK_FALSE(facet::IsToastVisible(toasts[0]));
    // The windows show toasts 1, 2 and 3 (the newest queued, 4, still waits).
    CHECK(facet::IsToastVisible(toasts[4]));
}

TEST_CASE("A full toast queue drops its oldest toast")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::ToastId first;
    facet::ToastId last;
    bool show = true;
    session.SetUi([&] {
        if (show)
        {
            first = facet::ShowToast("first", {.duration = -1.0f});
            for (int i = 0; i < 16; ++i)
                last = facet::ShowToast("more", {.duration = -1.0f});
        }
        show = false;
    });
    session.Frame();
    CHECK_FALSE(facet::IsToastVisible(first));
    CHECK(facet::IsToastVisible(last));
}

TEST_CASE("A toast's timer pauses while the pointer is over it and runs again when it leaves")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    facet::ToastId toast;
    bool show = true;
    session.SetUi([&] {
        if (show)
            toast = facet::ShowToast("Copied", {.duration = 1.0f});
        show = false;
    });
    session.Frames(3);
    const ImGuiWindow* window = ToastWindow(0);
    REQUIRE(window != nullptr);
    session.MouseMove(ImVec2(window->Pos.x + window->Size.x * 0.3f, window->Pos.y + window->Size.y * 0.5f));
    session.Seconds(2.0f);
    CHECK(facet::IsToastVisible(toast));
    session.MouseMove(kAway);
    session.Seconds(1.3f);
    CHECK_FALSE(facet::IsToastVisible(toast));
}

TEST_CASE("Ctrl+Tab reaches a toast; its timer pauses while it has focus and Escape dismisses it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    facet::ToastId toast;
    bool show = true;
    session.SetUi([&] {
        ImGui::Button("page");
        session.Track("page");
        if (show)
            toast = facet::ShowToast("Exported", {.actionLabel = "Open", .duration = 1.0f});
        show = false;
    });
    session.Frames(2);
    session.Click(session.ItemCenter("page"));
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Ctrl);
    session.Frames(2);
    const ImGuiContext& g = *ImGui::GetCurrentContext();
    REQUIRE(g.NavWindow != nullptr);
    CHECK(std::string(g.NavWindow->RootWindow->Name).rfind("##FacetToast", 0) == 0);
    session.Seconds(1.5f);
    CHECK(facet::IsToastVisible(toast));
    session.KeyPress(ImGuiKey_Escape);
    session.Frame();
    CHECK_FALSE(facet::IsToastVisible(toast));
}

TEST_CASE("A toast's action reports on the next frame once, and the toast leaves")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    facet::ToastId toast;
    bool show = true;
    int reported = 0;
    session.SetUi([&] {
        if (show)
            toast = facet::ShowToast("Item deleted", {.actionLabel = "Undo", .duration = -1.0f});
        show = false;
        reported += facet::ToastActionPressed(toast) ? 1 : 0;
    });
    session.Seconds(0.5f);
    const ImGuiWindow* window = ToastWindow(0);
    REQUIRE(window != nullptr);
    // Click where the action sits: left of the close button at the end of the first line.
    const float side = facet::ControlHeight(facet::ControlSize::Small);
    const float padding = facet::Px(facet::T().spacing.md);
    const ImVec2 close(window->Pos.x + window->Size.x - padding - side * 0.5f, window->Pos.y + padding + side * 0.5f);
    session.Click(ImVec2(close.x - side - facet::Px(facet::T().spacing.sm) - 4.0f, close.y));
    session.Frames(3);
    CHECK(reported == 1);
    CHECK_FALSE(facet::IsToastVisible(toast));
}

TEST_CASE("When a toast leaves, the others move into its room without a jump")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    facet::ToastId toasts[2];
    bool show = true;
    session.SetUi([&] {
        if (show)
        {
            toasts[0] = facet::ShowToast("Older", {.duration = -1.0f});
            toasts[1] = facet::ShowToast("Newer", {.duration = -1.0f});
        }
        show = false;
    });
    session.Seconds(0.5f);
    // The older toast sits above the newer one (bottom corner): dismissing the newer moves the older down.
    const ImGuiWindow* older = ToastWindow(0);
    REQUIRE(older != nullptr);
    const float before = older->Pos.y;
    facet::DismissToast(toasts[1]);
    float last = before;
    float largestStep = 0.0f;
    for (int i = 0; i < 40; ++i)
    {
        session.Frame();
        largestStep = std::max(largestStep, std::fabs(older->Pos.y - last));
        last = older->Pos.y;
    }
    CHECK(last > before + 10.0f);
    CHECK(largestStep < (last - before) * 0.5f);
}

TEST_CASE("Under reduced motion a toast appears, moves and leaves at once")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion(true);
    facet::ToastId toast;
    bool show = true;
    session.SetUi([&] {
        if (show)
            toast = facet::ShowToast("Now", {.duration = -1.0f});
        show = false;
    });
    session.Frames(2);
    CHECK(ActiveToastWindows() == 1);
    facet::DismissToast(toast);
    session.Frames(2);
    CHECK(ActiveToastWindows() == 0);
}

TEST_CASE("A toast dragged toward the edge past half its width is dismissed; a short drag springs back")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    facet::ToastId toast;
    bool show = true;
    session.SetUi([&] {
        if (show)
            toast = facet::ShowToast("Swipe me", {.description = "A line under the text", .duration = -1.0f});
        show = false;
    });
    session.Seconds(0.5f);
    const ImGuiWindow* window = ToastWindow(0);
    REQUIRE(window != nullptr);
    const ImVec2 start(window->Pos.x + facet::Px(30.0f), window->Pos.y + window->Size.y - facet::Px(8.0f));
    const float x0 = window->Pos.x;
    session.Drag({start, ImVec2(start.x + 20.0f, start.y)}, 12);
    session.Seconds(0.5f);
    CHECK(facet::IsToastVisible(toast));
    CHECK(window->Pos.x == doctest::Approx(x0).epsilon(0.01));
    session.Drag({start, ImVec2(start.x + window->Size.x * 0.7f, start.y)}, 12);
    session.Frames(2);
    CHECK_FALSE(facet::IsToastVisible(toast));
}

TEST_CASE("UpdateToast replaces the text and restarts the timer; SetToastCorner moves the stack")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    facet::ToastId toast;
    bool show = true;
    session.SetUi([&] {
        if (show)
            toast = facet::ShowToast("Exporting", {.duration = 0.6f, .progressBar = true, .progress = 0.0f});
        show = false;
    });
    session.Seconds(0.4f);
    facet::UpdateToast(toast, "Exporting", {.duration = 0.6f, .progressBar = true, .progress = 0.5f});
    session.Seconds(0.4f);
    CHECK(facet::IsToastVisible(toast));
    facet::SetToastCorner(facet::ScreenCorner::TopLeft);
    session.Frames(2);
    const ImGuiWindow* window = ToastWindow(0);
    REQUIRE(window != nullptr);
    CHECK(window->Pos.x < 100.0f);
    facet::SetToastCorner(facet::ScreenCorner::BottomRight);
    session.Seconds(0.6f);
    CHECK_FALSE(facet::IsToastVisible(toast));
}

// ---- Drawer --------------------------------------------------------------------------------------

TEST_CASE("A modal drawer sits flush with its edge over the whole side, blocks the page and closes on Escape")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = false;
    int pagePresses = 0;
    ImRect rect;
    session.SetUi([&] {
        if (facet::Button("Open"))
            open = true;
        session.Track("opener");
        if (facet::Button("Page"))
            ++pagePresses;
        session.Track("page");
        if (facet::BeginDrawer("Details", &open))
        {
            facet::Button("Inside");
            session.Track("inside");
            facet::EndDrawer();
        }
        rect = fi::PopupLevelRect(ImGui::GetID("Details"));
    });
    session.Frame();
    session.Click(session.ItemCenter("opener"));
    session.Seconds(0.4f);
    REQUIRE(open);
    CHECK(rect.Max.x == doctest::Approx(800.0f));
    CHECK(rect.Min.y == doctest::Approx(0.0f));
    CHECK(rect.Max.y == doctest::Approx(600.0f));
    CHECK(rect.GetWidth() == doctest::Approx(facet::Px(facet::T().overlay.drawerWidth)));
    CHECK(ImGui::GetFocusID() == session.ItemId("inside"));
    session.KeyPress(ImGuiKey_Escape);
    session.Frame();
    CHECK_FALSE(open);
    CHECK(ImGui::GetFocusID() == session.ItemId("opener"));
    // A click on the page lands on the scrim: nothing under it is pressed, the drawer closes.
    session.Click(session.ItemCenter("opener"));
    session.Seconds(0.4f);
    REQUIRE(open);
    session.Click(session.ItemCenter("page"));
    session.Frame();
    CHECK(pagePresses == 0);
    CHECK_FALSE(open);
    CHECK(ImGui::GetFocusID() == session.ItemId("opener"));
}

TEST_CASE("A non-modal drawer leaves the page usable beside it; Escape and a bottom sheet flick close it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = true;
    int pagePresses = 0;
    ImRect rect;
    session.SetUi([&] {
        if (facet::Button("Page"))
            ++pagePresses;
        session.Track("page");
        if (facet::BeginDrawer("Sheet", &open, {.side = facet::DrawerSide::Bottom, .modal = false}))
        {
            ImGui::TextUnformatted("Content");
            facet::EndDrawer();
        }
        rect = fi::PopupLevelRect(ImGui::GetID("Sheet"));
    });
    session.Seconds(0.4f);
    REQUIRE(open);
    CHECK(rect.Max.y == doctest::Approx(600.0f));
    CHECK(rect.GetHeight() == doctest::Approx(facet::Px(facet::T().overlay.drawerHeight)));
    session.Click(session.ItemCenter("page"));
    CHECK(pagePresses == 1);
    CHECK(open);
    // A fast drag down on the title row closes it.
    const ImVec2 title(rect.GetCenter().x, rect.Min.y + facet::Px(facet::T().spacing.xl) + 6.0f);
    session.Drag({title, ImVec2(title.x, title.y + 60.0f)}, 2);
    session.Frames(2);
    CHECK_FALSE(open);
    open = true;
    session.Seconds(0.4f);
    session.Click(ImVec2(rect.GetCenter().x, rect.GetCenter().y));
    session.KeyPress(ImGuiKey_Escape);
    session.Frame();
    CHECK_FALSE(open);
}

TEST_CASE("A resizable drawer's inner edge drags its width")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = true;
    ImRect rect;
    session.SetUi([&] {
        if (facet::BeginDrawer("Resize", &open, {.side = facet::DrawerSide::Start, .resizable = true}))
        {
            ImGui::TextUnformatted("Content");
            facet::EndDrawer();
        }
        rect = fi::PopupLevelRect(ImGui::GetID("Resize"));
    });
    session.Seconds(0.4f);
    const float before = rect.GetWidth();
    const ImVec2 edge(rect.Max.x - facet::Px(facet::T().layout.dividerBand) * 0.5f, 300.0f);
    session.Drag({edge, ImVec2(edge.x + 60.0f, edge.y)}, 6);
    session.Frames(2);
    CHECK(rect.GetWidth() == doctest::Approx(before + 60.0f).epsilon(0.02));
    CHECK(rect.Min.x == doctest::Approx(0.0f));
}

// ---- Loading overlay -----------------------------------------------------------------------------

TEST_CASE("A loading overlay keeps the region's size and blocks clicks and Tab while it loads")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool loading = true;
    int presses = 0;
    ImRect region;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::BeginLoadingOverlay("region", {.loading = loading});
        if (facet::Button("Inside"))
            ++presses;
        session.Track("inside");
        ImGui::TextUnformatted("More content in the region");
        facet::EndLoadingOverlay();
        region = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImGui::Button("after");
        session.Track("after");
    });
    session.Seconds(0.3f);
    const ImRect loadingRegion = region;
    session.Click(session.ItemCenter("inside"));
    CHECK(presses == 0);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    loading = false;
    session.Seconds(0.3f);
    CHECK(region.GetWidth() == doctest::Approx(loadingRegion.GetWidth()));
    CHECK(region.GetHeight() == doctest::Approx(loadingRegion.GetHeight()));
    session.Click(session.ItemCenter("inside"));
    CHECK(presses == 1);
    CHECK(fi::OpenScopeCount() == 0);
}

// ---- Command palette -----------------------------------------------------------------------------

namespace
{

const facet::ChoiceItem kPaletteCommands[] = {
    {.label = "New file", .group = "File"},
    {.label = "Open file", .group = "File"},
    {.label = "Toggle sidebar", .group = "View"},
    {.label = "Zoom in", .group = "View"},
};

} // namespace

TEST_CASE("The command palette opens with its shortcut, filters as one types and Enter picks")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    const facet::ShortcutId shortcut = facet::RegisterShortcut("test.palette", ImGuiMod_Ctrl | ImGuiKey_K);
    bool open = false;
    int picked = -1;
    int picks = 0;
    session.SetUi([&] {
        ImGui::Button("page");
        session.Track("page");
        if (facet::CommandPalette("Commands", &open, &picked, kPaletteCommands, {.openShortcut = shortcut}))
            ++picks;
    });
    session.Frame();
    session.Click(session.ItemCenter("page"));
    session.KeyPress(ImGuiKey_K | ImGuiMod_Ctrl);
    session.Seconds(0.3f);
    REQUIRE(open);
    session.Type("zoom");
    session.Frames(2);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(picks == 1);
    CHECK(picked == 3);
    CHECK_FALSE(open);
    CHECK(ImGui::GetFocusID() == session.ItemId("page"));
    facet::UnregisterShortcut(shortcut);
}

TEST_CASE("The command palette moves with Down, closes on Escape without a pick and lists recent picks first")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    bool open = true;
    int picked = -1;
    int picks = 0;
    session.SetUi([&] {
        if (facet::CommandPalette("##palette", &open, &picked, kPaletteCommands))
            ++picks;
    });
    session.Seconds(0.3f);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(picks == 1);
    CHECK(picked == 2);
    // Opened again with an empty field, the recent pick is the first row: Enter picks it.
    open = true;
    session.Seconds(0.3f);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(picks == 2);
    CHECK(picked == 2);
    open = true;
    session.Seconds(0.3f);
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK_FALSE(open);
    CHECK(picks == 2);
}

TEST_CASE("The shortcut palette triggers the picked shortcut")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    const facet::ShortcutId save = facet::RegisterShortcut("test.save", ImGuiMod_Ctrl | ImGuiKey_S, {.label = "Save the file"});
    bool open = true;
    facet::ShortcutId picked;
    int fired = 0;
    session.SetUi([&] {
        fired += facet::ShortcutPressed(save) ? 1 : 0;
        (void)facet::CommandPalette("Shortcuts", &open, &picked);
    });
    session.Seconds(0.3f);
    session.Type("save");
    session.Frames(2);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(3);
    CHECK(picked == save);
    CHECK(fired == 1);
    facet::UnregisterShortcut(save);
}

// ---- Unsaved changes bar -------------------------------------------------------------------------

TEST_CASE("The unsaved changes bar shows while there are changes, applies, reverts and reverts on Escape")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    OverlayMotionGuard motion;
    int changes = 2;
    int applied = 0;
    int reverted = 0;
    session.SetUi([&] {
        ImGui::Button("page");
        session.Track("page");
        switch (facet::UnsavedChangesBar("changes", changes, {.applyLabel = "Apply##bar", .revertLabel = "Revert##bar"}))
        {
        case facet::UnsavedChangesAction::Apply:
            ++applied;
            break;
        case facet::UnsavedChangesAction::Revert:
            ++reverted;
            break;
        default:
            break;
        }
    });
    session.Seconds(0.4f);
    // The buttons sit at the bottom right of the window: Tab reaches them from the page.
    session.Click(session.ItemCenter("page"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Space);
    CHECK(reverted == 1);
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK(reverted == 2);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Space);
    CHECK(applied == 1);
    changes = 0;
    session.Seconds(0.4f);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(reverted == 2);
}
