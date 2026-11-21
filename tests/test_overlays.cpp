// Behaviour of the overlays family (facet/widgets/overlays.h), and the parts of the popup host
// (src/internal/popup_host.h) that the family added: Panel levels, levels at a fixed rect that slide
// in, and interactive tooltips.
//
// One test case per behaviour; docs/writing-a-widget.md describes the pattern. The API checks, and
// the placeholder checks of the widgets that are still planned, are in tests/test_api_overlays.cpp.

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
            fi::OpenPopupLevel(panel, {.rect = ImRect(300.0f, 100.0f, 340.0f, 120.0f)}, {.kind = fi::PopupKind::Panel});
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
    // Escape does not close a panel by itself (its widget claims Escape).
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
