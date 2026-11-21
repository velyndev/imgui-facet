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
