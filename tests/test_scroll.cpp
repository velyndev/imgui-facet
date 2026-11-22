// Scroll core (src/internal/scroll.h): smooth scrolling, wheel routing between nested areas, the
// overlay bars, edge fades, reveal, edge scroll and the scroll area's effect on the host.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "core/core_internal.h"
#include "internal/focus.h"
#include "internal/popup_host.h"
#include "internal/scroll.h"
#include "support/session.h"

namespace fi = facet::internal;

namespace
{

struct MotionGuard
{
    MotionGuard()
    {
        facet::SetFollowSystemMotion(false);
        facet::SetTheme(facet::Theme::Light(), false);
    }
    ~MotionGuard()
    {
        facet::SetFollowSystemMotion(true);
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
};

// The window of an area, captured inside it every frame.
struct AreaProbe
{
    ImGuiID id = 0;
    void Capture() { id = ImGui::GetCurrentWindow()->ID; }
    ImGuiWindow* Window() const { return ImGui::FindWindowByID(id); }
    float ScrollY() const { return Window() != nullptr ? Window()->Scroll.y : -1.0f; }
    float MaxY() const { return Window() != nullptr ? Window()->ScrollMax.y : -1.0f; }
    ImVec2 Center() const { return Window() != nullptr ? Window()->Rect().GetCenter() : ImVec2(0.0f, 0.0f); }
};

// One area of 300 x 200 pixels with `rows` rows of 30 pixels (buttons when `buttons`).
struct SingleArea
{
    AreaProbe area;
    int rows = 50;
    bool buttons = false;
    bool show = true;
    float rowHeight = 30.0f;
    std::function<void()> inside;  // runs inside the area after the rows

    void Ui(facet::test::Session& session)
    {
        if (!show)
            return;
        if (fi::BeginScrollAreaCore("area", {.size = ImVec2(300.0f, 200.0f)}))
        {
            area.Capture();
            char name[32];
            for (int i = 0; i < rows; ++i)
            {
                std::snprintf(name, sizeof(name), "row%d", i);
                if (buttons)
                    ImGui::Button(name, ImVec2(200.0f, rowHeight - ImGui::GetStyle().ItemSpacing.y));
                else
                    ImGui::Dummy(ImVec2(200.0f, rowHeight - ImGui::GetStyle().ItemSpacing.y));
                session.Track(name);
            }
            if (inside)
                inside();
            fi::EndScrollAreaCore();
        }
    }
};

float Notch()
{
    return facet::Px(facet::T().scroll.wheelDistance);
}

} // namespace

TEST_CASE("Scroll area: hides the stock scrollbar without touching the host's style")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    // The style as Facet leaves it without an area (it manages the font scale itself).
    ui.show = false;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    const ImGuiStyle before = ImGui::GetStyle();
    ui.show = true;
    int styleStack = -1;
    int colorStack = -1;
    ImRect clip;
    ImRect rect;
    ui.inside = [&] {
        clip = ImGui::GetCurrentWindow()->ClipRect;
        rect = ImGui::GetCurrentWindow()->Rect();
    };
    session.SetUi([&] {
        ui.Ui(session);
        styleStack = GImGui->StyleVarStack.Size;
        colorStack = GImGui->ColorStack.Size;
    });
    session.Frames(3);
    ImGuiWindow* window = ui.area.Window();
    REQUIRE(window != nullptr);
    CHECK(window->ScrollMax.y > 0.0f);
    CHECK_FALSE(window->ScrollbarY);
    CHECK((window->Flags & ImGuiWindowFlags_NoScrollbar) != 0);
    CHECK(styleStack == 0);
    CHECK(colorStack == 0);
    CHECK(std::memcmp(&before, &ImGui::GetStyle(), sizeof(ImGuiStyle)) == 0);
    // Content is clipped at the area's own rectangle, not inset by half the padding.
    CHECK(clip.Min.x == doctest::Approx(rect.Min.x));
    CHECK(clip.Max.y == doctest::Approx(rect.Max.y));
    // The content starts one focus ring outset in, so a ring at the edge shows whole.
    CHECK(window->WindowPadding.x == doctest::Approx(fi::FocusRingOutset()));
}

TEST_CASE("Scroll area: the wheel moves a target that the position follows smoothly")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Wheel(ui.area.Center(), -1.0f);
    ImGuiWindow* window = ui.area.Window();
    REQUIRE(window != nullptr);
    CHECK(fi::ScrollAreaTarget(window).y == doctest::Approx(Notch()));
    CHECK(fi::IsScrollAreaScrolling(window));
    session.Frame();
    CHECK(ui.area.ScrollY() > 0.0f);
    CHECK(ui.area.ScrollY() < Notch());
    session.Seconds(1.0f);
    CHECK(ui.area.ScrollY() == doctest::Approx(Notch()));
    CHECK_FALSE(fi::IsScrollAreaScrolling(ui.area.Window()));
    // A fraction of a notch (a touchpad) moves the same fraction.
    session.Wheel(ui.area.Center(), -0.25f);
    session.Seconds(1.0f);
    CHECK(ui.area.ScrollY() == doctest::Approx(Notch() * 1.25f).epsilon(0.02));
}

TEST_CASE("Scroll area: Shutdown in the middle of a wheel scroll gives the wheel back to Dear ImGui")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Wheel(ui.area.Center(), -1.0f);
    REQUIRE(fi::IsScrollAreaScrolling(ui.area.Window()));
    ImGuiContext* g = ImGui::GetCurrentContext();
    const ImGuiKeyOwnerData* wheelY = ImGui::GetKeyOwnerData(g, ImGuiKey_MouseWheelY);
    const ImGuiKeyOwnerData* wheelX = ImGui::GetKeyOwnerData(g, ImGuiKey_MouseWheelX);
    REQUIRE(wheelY->OwnerNext != ImGuiKeyOwner_NoOwner);
    facet::Shutdown();
    CHECK(wheelY->OwnerCurr == ImGuiKeyOwner_NoOwner);
    CHECK(wheelY->OwnerNext == ImGuiKeyOwner_NoOwner);
    CHECK(wheelX->OwnerCurr == ImGuiKeyOwner_NoOwner);
    CHECK(wheelX->OwnerNext == ImGuiKeyOwner_NoOwner);
}

TEST_CASE("Scroll area: the same smooth scroll at 30 and 240 frames per second")
{
    MotionGuard guard;
    float scroll[2] = {};
    const float rates[2] = {1.0f / 30.0f, 1.0f / 240.0f};
    for (int i = 0; i < 2; ++i)
    {
        facet::test::Session session({.deltaTime = rates[i]});
        REQUIRE(session.Ready());
        SingleArea ui;
        session.SetUi([&] { ui.Ui(session); });
        session.Frames(2);
        session.Wheel(ui.area.Center(), -3.0f);
        // 0.1 s of motion, counting the frame the wheel turned in (its time step moves it too).
        session.Frames(static_cast<int>(std::lround(0.1f / rates[i])) - 1);
        scroll[i] = ui.area.ScrollY();
    }
    CHECK(scroll[0] > 0.0f);
    CHECK(scroll[0] < Notch() * 3.0f);
    CHECK(scroll[0] == doctest::Approx(scroll[1]).epsilon(0.03));
}

TEST_CASE("Scroll area: reduced motion jumps to the target")
{
    MotionGuard guard;
    facet::Theme theme = facet::Theme::Light();
    theme.motion.reduceMotion = true;
    facet::SetTheme(theme, false);
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Wheel(ui.area.Center(), -1.0f);
    session.Frame();
    CHECK(ui.area.ScrollY() == doctest::Approx(Notch()));
}

TEST_CASE("Scroll area: the wheel goes to the inner area while it can scroll, then to the outer one")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    AreaProbe outer;
    AreaProbe inner;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("outer", {.size = ImVec2(400.0f, 300.0f)}))
        {
            outer.Capture();
            ImGui::Dummy(ImVec2(10.0f, 40.0f));
            if (fi::BeginScrollAreaCore("inner", {.size = ImVec2(300.0f, 150.0f)}))
            {
                inner.Capture();
                ImGui::Dummy(ImVec2(10.0f, 250.0f));
                fi::EndScrollAreaCore();
            }
            ImGui::Dummy(ImVec2(10.0f, 1000.0f));
            fi::EndScrollAreaCore();
        }
    });
    session.Frames(2);
    const float innerMax = inner.MaxY();
    REQUIRE(innerMax > Notch());
    REQUIRE(innerMax < Notch() * 2.0f);
    const ImVec2 at = inner.Center();
    // Down: the inner area takes two notches (the second up to its end), the third goes out.
    session.Wheel(at, -1.0f);
    session.Wheel(at, -1.0f);
    session.Seconds(0.6f);
    CHECK(inner.ScrollY() == doctest::Approx(innerMax));
    CHECK(outer.ScrollY() == 0.0f);
    session.Wheel(at, -1.0f);
    session.Seconds(0.6f);
    CHECK(inner.ScrollY() == doctest::Approx(innerMax));
    CHECK(outer.ScrollY() == doctest::Approx(Notch()));
    // Up: the inner area can scroll up again, so it takes the turn first.
    const ImVec2 back = inner.Center();
    session.Wheel(back, 1.0f);
    session.Seconds(0.6f);
    CHECK(inner.ScrollY() == doctest::Approx(innerMax - Notch()));
    CHECK(outer.ScrollY() == doctest::Approx(Notch()));
    session.Wheel(back, 1.0f);
    session.Seconds(0.6f);
    CHECK(inner.ScrollY() == 0.0f);
    session.Wheel(back, 1.0f);
    session.Seconds(0.6f);
    CHECK(outer.ScrollY() == 0.0f);
}

TEST_CASE("Scroll area: content smaller than the area has no bar and passes the wheel on")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    AreaProbe outer;
    AreaProbe inner;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("outer", {.size = ImVec2(400.0f, 300.0f)}))
        {
            outer.Capture();
            if (fi::BeginScrollAreaCore("small", {.size = ImVec2(300.0f, 150.0f)}))
            {
                inner.Capture();
                ImGui::Dummy(ImVec2(10.0f, 40.0f));
                fi::EndScrollAreaCore();
            }
            ImGui::Dummy(ImVec2(10.0f, 1000.0f));
            fi::EndScrollAreaCore();
        }
    });
    session.Frames(2);
    CHECK(inner.MaxY() == 0.0f);
    CHECK_FALSE(fi::ScrollAreaBarRects(inner.Window(), ImGuiAxis_Y, nullptr, nullptr));
    CHECK(fi::ScrollAreaBarRects(outer.Window(), ImGuiAxis_Y, nullptr, nullptr));
    CHECK(fi::ScrollAreaAt(inner.Center()) == outer.Window());
    session.Wheel(inner.Center(), -1.0f);
    session.Seconds(0.6f);
    CHECK(outer.ScrollY() == doctest::Approx(Notch()));
}

TEST_CASE("Scroll area: the wheel scrolls a stock window around an area once the area is at its end")
{
    MotionGuard guard;
    facet::test::Session session({.window = false});
    REQUIRE(session.Ready());
    AreaProbe inner;
    ImGuiID host = 0;
    session.SetUi([&] {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(500.0f, 400.0f));
        ImGui::Begin("host", nullptr, ImGuiWindowFlags_NoSavedSettings);
        host = ImGui::GetCurrentWindow()->ID;
        if (fi::BeginScrollAreaCore("inner", {.size = ImVec2(300.0f, 150.0f)}))
        {
            inner.Capture();
            ImGui::Dummy(ImVec2(10.0f, 40.0f));
            fi::EndScrollAreaCore();
        }
        ImGui::Dummy(ImVec2(10.0f, 2000.0f));
        ImGui::End();
    });
    session.Frames(2);
    session.Wheel(inner.Center(), -1.0f);
    session.Seconds(0.6f);
    ImGuiWindow* window = ImGui::FindWindowByID(host);
    REQUIRE(window != nullptr);
    CHECK(window->Scroll.y == doctest::Approx(Notch()));
}

TEST_CASE("Scroll area: a control that claimed the wheel keeps it")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    ui.inside = [] {
        // A control under the pointer that owns the wheel, as a focused slider does.
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, ImGui::GetID("control"));
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelX, ImGui::GetID("control"));
    };
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Wheel(ui.area.Center(), -1.0f);
    session.Seconds(0.5f);
    CHECK(ui.area.ScrollY() == 0.0f);
}

TEST_CASE("Scroll area: dragging the thumb scrolls with it and stops a smooth scroll")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    session.Wheel(ui.area.Center(), -5.0f);
    session.Frame();
    REQUIRE(fi::IsScrollAreaScrolling(ui.area.Window()));
    ImRect track;
    ImRect thumb;
    REQUIRE(fi::ScrollAreaBarRects(ui.area.Window(), ImGuiAxis_Y, &track, &thumb));
    const ImVec2 grab = thumb.GetCenter();
    const float travel = track.GetHeight() - thumb.GetHeight();
    REQUIRE(travel > 0.0f);
    // Drag the thumb down by a quarter of its travel, then hold it there.
    session.DragAndHold({grab, ImVec2(grab.x, grab.y + travel * 0.25f)}, 4, 30);
    ImGuiWindow* window = ui.area.Window();
    REQUIRE(fi::ScrollAreaBarRects(window, ImGuiAxis_Y, &track, &thumb));
    const float expected = (thumb.Min.y - track.Min.y) / travel * window->ScrollMax.y;
    CHECK(window->Scroll.y == doctest::Approx(expected).epsilon(0.02));
    CHECK_FALSE(fi::IsScrollAreaScrolling(window));
    CHECK(fi::ScrollAreaTarget(window).y == doctest::Approx(window->Scroll.y));
    // The bar is wide while the pointer is on it.
    session.MouseMove(thumb.GetCenter());
    session.Seconds(0.3f);
    REQUIRE(fi::ScrollAreaBarRects(ui.area.Window(), ImGuiAxis_Y, &track, &thumb));
    CHECK(thumb.GetWidth() == doctest::Approx(facet::Px(facet::T().scroll.barThicknessHover)));
    session.MouseMove(ImVec2(5.0f, 5.0f));
    session.Seconds(0.3f);
    REQUIRE(fi::ScrollAreaBarRects(ui.area.Window(), ImGuiAxis_Y, &track, &thumb));
    CHECK(thumb.GetWidth() == doctest::Approx(facet::Px(facet::T().scroll.barThickness)));
}

TEST_CASE("Scroll area: pressing the track pages toward the pointer until the thumb reaches it")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    ui.rows = 200;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    ImRect track;
    ImRect thumb;
    REQUIRE(fi::ScrollAreaBarRects(ui.area.Window(), ImGuiAxis_Y, &track, &thumb));
    // One press below the thumb: one page.
    const ImVec2 below(track.GetCenter().x, track.Max.y - 4.0f);
    session.Click(below);
    ImGuiWindow* window = ui.area.Window();
    const float page = window->InnerRect.GetHeight() - facet::Px(facet::T().scroll.edgeFade);
    CHECK(fi::ScrollAreaTarget(window).y == doctest::Approx(page));
    session.Seconds(0.6f);
    CHECK(ui.area.ScrollY() == doctest::Approx(page));
    // Held: pages repeat until the thumb is under the pointer.
    session.MouseMove(ImVec2(track.GetCenter().x, track.Min.y + track.GetHeight() * 0.6f));
    session.MouseDown();
    session.Seconds(2.0f);
    session.MouseUp();
    session.Seconds(0.6f);
    REQUIRE(fi::ScrollAreaBarRects(ui.area.Window(), ImGuiAxis_Y, &track, &thumb));
    const float pointer = track.Min.y + track.GetHeight() * 0.6f;
    CHECK(thumb.Min.y <= pointer + 1.0f);
    CHECK(thumb.Max.y >= pointer - facet::Px(facet::T().scroll.edgeFade));
}

TEST_CASE("Scroll area: a stock scroll or keyboard navigation cancels a smooth scroll")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    ui.buttons = true;
    bool setScroll = false;
    bool focusFirst = false;
    ui.inside = [&] {
        if (setScroll)
        {
            ImGui::SetScrollY(10.0f);
            setScroll = false;
        }
    };
    session.SetUi([&] {
        if (focusFirst)
        {
            ImGui::SetKeyboardFocusHere(0);
            focusFirst = false;
        }
        ui.Ui(session);
    });
    session.Frames(2);
    session.Wheel(ui.area.Center(), -3.0f);
    session.Frame();
    setScroll = true;
    session.Frame();
    session.Seconds(1.0f);
    CHECK(ui.area.ScrollY() == doctest::Approx(10.0f));

    // Keyboard navigation to the first row while the wheel scroll runs: navigation wins.
    session.Wheel(ui.area.Center(), -4.0f);
    session.Frame();
    REQUIRE(fi::IsScrollAreaScrolling(ui.area.Window()));
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("area", {.size = ImVec2(300.0f, 200.0f)}))
        {
            ui.area.Capture();
            for (int i = 0; i < ui.rows; ++i)
            {
                if (i == 0 && focusFirst)
                    ImGui::SetKeyboardFocusHere();
                char name[32];
                std::snprintf(name, sizeof(name), "row%d", i);
                ImGui::Button(name, ImVec2(200.0f, 24.0f));
                session.Track(name);
            }
            focusFirst = false;
            fi::EndScrollAreaCore();
        }
    });
    focusFirst = true;
    session.Frame();
    session.Seconds(1.0f);
    CHECK(ui.area.ScrollY() == 0.0f);
    CHECK(session.ItemVisible("row0"));
}

TEST_CASE("Scroll area: revealing rows above and below, at once or animated")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    int reveal = -1;
    fi::ScrollAlign align = fi::ScrollAlign::Nearest;
    bool animate = false;
    ui.inside = [&] {
        if (reveal < 0)
            return;
        char name[32];
        std::snprintf(name, sizeof(name), "row%d", reveal);
        fi::ScrollAreaRevealRect(ImGui::GetCurrentWindow(), session.ItemRect(name), align, animate);
        reveal = -1;
    };
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    reveal = 40;
    session.Frames(3);
    CHECK(session.ItemVisible("row40"));
    ImGuiWindow* window = ui.area.Window();
    // Nearest from above puts it at the bottom edge, inside the padding.
    CHECK(session.ItemRect("row40").Max.y <= window->InnerRect.Max.y - window->WindowPadding.y + 1.0f);
    CHECK(session.ItemRect("row40").Max.y >= window->InnerRect.Max.y - window->WindowPadding.y - 1.0f);
    reveal = 2;
    animate = true;
    session.Frame();
    CHECK_FALSE(session.ItemVisible("row2"));
    session.Seconds(1.0f);
    CHECK(session.ItemVisible("row2"));
    const float top = window->InnerRect.Min.y + window->WindowPadding.y;
    CHECK(session.ItemRect("row2").Min.y == doctest::Approx(top).epsilon(0.01));
    // Center and End.
    reveal = 25;
    align = fi::ScrollAlign::Center;
    animate = false;
    session.Frames(3);
    CHECK(session.ItemRect("row25").GetCenter().y == doctest::Approx(window->InnerRect.GetCenter().y).epsilon(0.02));
    // Nothing moves for a row that is already visible.
    const float before = ui.area.ScrollY();
    reveal = 25;
    align = fi::ScrollAlign::Nearest;
    session.Frames(3);
    CHECK(ui.area.ScrollY() == before);
}

TEST_CASE("Scroll area: edge scroll for a drag is frame-rate independent and stops at the end")
{
    MotionGuard guard;
    float scrolled[2] = {};
    const float rates[2] = {1.0f / 30.0f, 1.0f / 240.0f};
    for (int i = 0; i < 2; ++i)
    {
        facet::test::Session session({.deltaTime = rates[i]});
        REQUIRE(session.Ready());
        SingleArea ui;
        ui.rows = 400;
        bool request = false;
        bool result = false;
        ui.inside = [&] {
            if (!request)
                return;
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            // At the very bottom edge: full speed.
            result = fi::RequestScrollAreaEdgeScroll(window, ImVec2(window->Rect().GetCenter().x, window->InnerRect.Max.y));
        };
        session.SetUi([&] { ui.Ui(session); });
        session.Frames(2);
        request = true;
        session.Seconds(0.5f);
        CHECK(result);
        scrolled[i] = ui.area.ScrollY();
    }
    const float expected = facet::Px(facet::T().drag.autoScrollSpeed) * 0.5f;
    CHECK(scrolled[0] == doctest::Approx(expected).epsilon(0.08));
    CHECK(scrolled[1] == doctest::Approx(expected).epsilon(0.08));

    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    ui.rows = 12;
    bool result = true;
    ui.inside = [&] {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        result = fi::RequestScrollAreaEdgeScroll(window, ImVec2(window->Rect().GetCenter().x, window->InnerRect.Max.y));
    };
    session.SetUi([&] { ui.Ui(session); });
    session.Seconds(2.0f);
    CHECK(ui.area.ScrollY() == doctest::Approx(ui.area.MaxY()));
    CHECK_FALSE(result);
}

TEST_CASE("Scroll area: the position is kept while the area is away")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    bool scroll = false;
    ui.inside = [&] {
        if (scroll)
            fi::ScrollAreaTo(ImGui::GetCurrentWindow(), ImVec2(-1.0f, 120.0f), false);
        scroll = false;
    };
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(2);
    scroll = true;
    session.Frames(2);
    CHECK(ui.area.ScrollY() == doctest::Approx(120.0f));
    ui.show = false;
    session.Frames(300);
    ui.show = true;
    session.Frames(2);
    CHECK(ui.area.ScrollY() == doctest::Approx(120.0f));
}

TEST_CASE("Scroll area: the thumb keeps its minimum length at every scale")
{
    MotionGuard guard;
    for (const float scale : {0.75f, 2.0f})
    {
        facet::SetUiScale(scale);
        facet::test::Session session({.displaySize = ImVec2(1200.0f, 900.0f)});
        REQUIRE(session.Ready());
        SingleArea ui;
        ui.rows = 4000;
        session.SetUi([&] { ui.Ui(session); });
        session.Frames(3);
        ImRect track;
        ImRect thumb;
        REQUIRE(fi::ScrollAreaBarRects(ui.area.Window(), ImGuiAxis_Y, &track, &thumb));
        CHECK(thumb.GetHeight() == doctest::Approx(facet::Px(facet::T().scroll.minThumbLength)));
        CHECK(thumb.GetWidth() == doctest::Approx(facet::Px(facet::T().scroll.barThickness)));
        CHECK(track.Max.x == doctest::Approx(ui.area.Window()->Rect().Max.x - facet::Px(facet::T().scroll.barInset)));
    }
    facet::SetUiScale(1.0f);
}

TEST_CASE("Scroll area: both axes get a bar and the wheel scrolls sideways with Shift")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    AreaProbe area;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("wide", {.size = ImVec2(300.0f, 200.0f), .horizontal = true}))
        {
            area.Capture();
            ImGui::Dummy(ImVec2(900.0f, 900.0f));
            fi::EndScrollAreaCore();
        }
    });
    session.Frames(2);
    ImRect trackX;
    ImRect trackY;
    REQUIRE(fi::ScrollAreaBarRects(area.Window(), ImGuiAxis_X, &trackX, nullptr));
    REQUIRE(fi::ScrollAreaBarRects(area.Window(), ImGuiAxis_Y, &trackY, nullptr));
    // The bars leave the corner to each other.
    CHECK(trackX.Max.x < trackY.Min.x);
    CHECK(trackY.Max.y < trackX.Min.y);
    session.KeyDown(ImGuiMod_Shift);
    session.Wheel(area.Center(), -1.0f);
    session.KeyUp(ImGuiMod_Shift);
    session.Seconds(0.6f);
    CHECK(area.Window()->Scroll.x == doctest::Approx(Notch()));
    CHECK(area.Window()->Scroll.y == 0.0f);
}

TEST_CASE("Scroll area: an idle bar dims and stays visible, and shows at full strength while hovered")
{
    MotionGuard guard;
    struct ContrastGuard
    {
        ~ContrastGuard() { facet::SetIncreasedContrast(false); }
    } contrast;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    int alpha = -1;  // the strongest alpha drawn where the thumb is, -1 when nothing is drawn there
    session.SetUi([&] {
        ui.Ui(session);
        alpha = -1;
        ImGuiWindow* window = ui.area.Window();
        ImRect thumb;
        if (window == nullptr || !fi::ScrollAreaBarRects(window, ImGuiAxis_Y, nullptr, &thumb))
            return;
        thumb.Expand(1.0f);
        for (const ImDrawVert& vertex : window->DrawList->VtxBuffer)
            if (thumb.Contains(vertex.pos))
                alpha = std::max(alpha, static_cast<int>((vertex.col >> IM_COL32_A_SHIFT) & 0xFF));
    });
    const ImVec2 elsewhere(700.0f, 500.0f);
    const float rest = facet::T().scroll.barRestOpacity;
    REQUIRE(rest > 0.0f);
    REQUIRE(rest < 1.0f);

    // The pointer is elsewhere and nothing scrolled: the bar is there, dimmed.
    session.MouseMove(elsewhere);
    session.Seconds(2.0f);
    CHECK(std::fabs(static_cast<float>(alpha) - facet::T().colors.scrollThumb.w * rest * 255.0f) <= 1.0f);
    // Over the area it shows at full strength, and dims again once the pointer has left.
    session.MouseMove(ui.area.Center());
    session.Seconds(1.0f);
    CHECK(std::fabs(static_cast<float>(alpha) - facet::T().colors.scrollThumb.w * 255.0f) <= 1.0f);
    session.MouseMove(elsewhere);
    session.Seconds(2.0f);
    CHECK(std::fabs(static_cast<float>(alpha) - facet::T().colors.scrollThumb.w * rest * 255.0f) <= 1.0f);

    // The increased contrast variant never dims it.
    facet::SetIncreasedContrast(true);
    session.Seconds(1.0f);
    CHECK(std::fabs(static_cast<float>(alpha) - facet::T().colors.scrollThumb.w * 255.0f) <= 1.0f);
    facet::SetIncreasedContrast(false);

    // A rest opacity of 0 hides an idle bar.
    facet::Theme theme = facet::T();
    theme.scroll.barRestOpacity = 0.0f;
    facet::SetTheme(theme, false);
    session.Seconds(1.0f);
    CHECK(alpha == -1);
}

TEST_CASE("Scroll area: content that scrolls keeps the gutter of the bar free")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    SingleArea ui;
    float available = 0.0f;
    float workRight = 0.0f;
    ui.inside = [&] {
        available = ImGui::GetContentRegionAvail().x;
        workRight = ImGui::GetCurrentWindow()->WorkRect.Max.x;
    };
    const facet::ScrollTokens& tokens = facet::T().scroll;
    const float pad = fi::FocusRingOutset();
    const float gutter = fi::ScrollBarGutter();
    CHECK(gutter == doctest::Approx(facet::Px(tokens.barInset + tokens.barThicknessHover + tokens.barGap)));

    // Content that fits takes the whole width inside the padding.
    ui.rows = 3;
    session.SetUi([&] { ui.Ui(session); });
    session.Frames(3);
    ImGuiWindow* window = ui.area.Window();
    REQUIRE(window != nullptr);
    CHECK(available == doctest::Approx(300.0f - pad * 2.0f));
    CHECK(fi::ScrollAreaGutter(window).x == 0.0f);

    // Content that scrolls ends before the bar at its hovered width, with barGap between them.
    ui.rows = 50;
    session.Frames(3);
    const float right = window->Rect().Max.x;
    CHECK(available == doctest::Approx(300.0f - pad - gutter));
    CHECK(workRight == doctest::Approx(right - gutter));
    CHECK(fi::ScrollAreaGutter(window).x == doctest::Approx(gutter - pad));
    CHECK(fi::ScrollAreaGutter(window).y == 0.0f);
    ImRect track;
    REQUIRE(fi::ScrollAreaBarRects(window, ImGuiAxis_Y, &track, nullptr));
    const float hoveredBarLeft = right - facet::Px(tokens.barInset) - facet::Px(tokens.barThicknessHover);
    CHECK(workRight == doctest::Approx(hoveredBarLeft - facet::Px(tokens.barGap)));
    CHECK(track.Min.x > workRight);

    // The gutter goes with the bar.
    ui.rows = 3;
    session.Frames(3);
    CHECK(available == doctest::Approx(300.0f - pad * 2.0f));
    CHECK(fi::ScrollAreaGutter(window).x == 0.0f);
    CHECK(fi::ScrollAreaGutter(nullptr).x == 0.0f);
}

TEST_CASE("Scroll area: the padding lies inside the gutter, and an area can leave the gutter to its caller")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const float gutter = fi::ScrollBarGutter();
    const float wide = gutter + 4.0f;
    float padded = 0.0f;
    float own = 0.0f;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("padded", {.size = ImVec2(300.0f, 200.0f), .padding = wide}))
        {
            padded = ImGui::GetContentRegionAvail().x;
            ImGui::Dummy(ImVec2(100.0f, 900.0f));
            fi::EndScrollAreaCore();
        }
        if (fi::BeginScrollAreaCore("own", {.size = ImVec2(300.0f, 200.0f), .gutter = false}))
        {
            own = ImGui::GetContentRegionAvail().x;
            ImGui::Dummy(ImVec2(100.0f, 900.0f));
            fi::EndScrollAreaCore();
        }
    });
    session.Frames(3);
    CHECK(padded == doctest::Approx(300.0f - wide * 2.0f));
    CHECK(own == doctest::Approx(300.0f - fi::FocusRingOutset() * 2.0f));
}

TEST_CASE("Scroll area: scrolled to its end, content stops before the bars on both axes")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    AreaProbe area;
    bool toEnd = false;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("wide", {.size = ImVec2(300.0f, 200.0f), .horizontal = true}))
        {
            area.Capture();
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (toEnd)
                fi::ScrollAreaTo(window, window->ScrollMax, false);
            ImGui::Dummy(ImVec2(900.0f, 900.0f));
            session.Track("block");
            fi::EndScrollAreaCore();
        }
    });
    session.Frames(3);
    toEnd = true;
    session.Frames(3);
    ImGuiWindow* window = area.Window();
    REQUIRE(window != nullptr);
    const float gutter = fi::ScrollBarGutter();
    const ImRect block = session.ItemRect("block");
    CHECK(window->Scroll.x == doctest::Approx(window->ScrollMax.x));
    CHECK(window->Scroll.y == doctest::Approx(window->ScrollMax.y));
    CHECK(block.Max.x == doctest::Approx(window->Rect().Max.x - gutter));
    CHECK(block.Max.y == doctest::Approx(window->Rect().Max.y - gutter));
    CHECK(fi::ScrollAreaGutter(window).y == doctest::Approx(gutter - fi::FocusRingOutset()));
}

TEST_CASE("Scroll area: a revealed rect stops before the horizontal bar")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    AreaProbe area;
    bool reveal = false;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("wide", {.size = ImVec2(300.0f, 200.0f), .horizontal = true}))
        {
            area.Capture();
            for (int i = 0; i < 20; ++i)
            {
                ImGui::Dummy(ImVec2(900.0f, 26.0f));
                if (i == 10)
                    session.Track("row");
            }
            if (reveal)
                fi::ScrollAreaRevealRect(ImGui::GetCurrentWindow(), session.ItemRect("row"), fi::ScrollAlign::Nearest, false);
            reveal = false;
            fi::EndScrollAreaCore();
        }
    });
    session.Frames(3);
    reveal = true;
    session.Frames(3);
    ImGuiWindow* window = area.Window();
    REQUIRE(window != nullptr);
    CHECK(window->Scroll.y > 0.0f);
    CHECK(session.ItemRect("row").Max.y == doctest::Approx(window->Rect().Max.y - fi::ScrollBarGutter()));
}

TEST_CASE("Scroll area: content that fits only beside the gutter keeps it instead of alternating")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    AreaProbe area;
    float height = 200.0f;
    float available = 0.0f;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("tiles", {.size = ImVec2(300.0f, height)}))
        {
            area.Capture();
            // A block whose height follows its width, as tiles that keep their proportions do:
            // taller than the area at the full width, shorter once the gutter has narrowed it.
            available = ImGui::GetContentRegionAvail().x;
            ImGui::Dummy(ImVec2(available, available * 0.675f));
            fi::EndScrollAreaCore();
        }
    });
    const float pad = fi::FocusRingOutset();
    const float narrow = 300.0f - pad - fi::ScrollBarGutter();
    REQUIRE((300.0f - pad * 2.0f) * 0.675f > height - pad * 2.0f);
    REQUIRE(narrow * 0.675f < height - pad * 2.0f);
    session.Frames(5);
    for (int i = 0; i < 8; ++i)
    {
        session.Frame();
        CHECK(available == doctest::Approx(narrow));
    }
    // No bar: the content fits beside the gutter.
    CHECK_FALSE(fi::ScrollAreaBarRects(area.Window(), ImGuiAxis_Y, nullptr, nullptr));
    // A taller area holds the content at the full width: the gutter is given up.
    height = 260.0f;
    session.Frames(4);
    CHECK(available == doctest::Approx(300.0f - pad * 2.0f));
}

TEST_CASE("Scroll area: content that fits at the full width does not keep a bar because of the gutter")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    AreaProbe area;
    int tiles = 30;
    float available = 0.0f;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("tiles", {.size = ImVec2(300.0f, 200.0f)}))
        {
            area.Capture();
            // Tiles of a fixed size in as many columns as fit: narrower content has more rows.
            available = ImGui::GetContentRegionAvail().x;
            const int columns = std::max(1, static_cast<int>(available / 49.0f));
            const int rows = (tiles + columns - 1) / columns;
            ImGui::Dummy(ImVec2(static_cast<float>(columns) * 49.0f, static_cast<float>(rows) * 90.0f));
            fi::EndScrollAreaCore();
        }
    });
    const float pad = fi::FocusRingOutset();
    const float full = 300.0f - pad * 2.0f;
    const float narrow = 300.0f - pad - fi::ScrollBarGutter();
    REQUIRE(static_cast<int>(full / 49.0f) == 6);
    REQUIRE(static_cast<int>(narrow / 49.0f) == 5);
    // Thirty tiles scroll in either layout: the gutter stays.
    session.Frames(6);
    CHECK(available == doctest::Approx(narrow));
    CHECK(fi::ScrollAreaBarRects(area.Window(), ImGuiAxis_Y, nullptr, nullptr));
    // Twelve tiles take three rows beside the gutter, which scroll, and two rows at the full
    // width, which fit: the area ends up at the full width without a bar and stays there.
    tiles = 12;
    session.Frames(6);
    for (int i = 0; i < 6; ++i)
    {
        session.Frame();
        CHECK(available == doctest::Approx(full));
    }
    CHECK_FALSE(fi::ScrollAreaBarRects(area.Window(), ImGuiAxis_Y, nullptr, nullptr));
    // Eighteen tiles scroll in either layout again; after one look at the full width the gutter
    // stays for good.
    tiles = 18;
    session.Frames(8);
    for (int i = 0; i < 6; ++i)
    {
        session.Frame();
        CHECK(available == doctest::Approx(narrow));
    }
}

TEST_CASE("Scroll area: edge fades draw the surface color where content continues")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImVec4 surface(0.25f, 0.5f, 0.75f, 1.0f);
    const ImU32 solid = facet::ToU32(surface);
    SingleArea ui;
    bool push = true;
    int solidVertices = 0;
    session.SetUi([&] {
        if (push)
            fi::PushScrollFadeSurface(surface);
        ui.Ui(session);
        if (push)
            fi::PopScrollFadeSurface();
        solidVertices = 0;
        if (ImGuiWindow* window = ui.area.Window())
            for (const ImDrawVert& vertex : window->DrawList->VtxBuffer)
                solidVertices += vertex.col == solid ? 1 : 0;
    });
    session.Frames(2);
    CHECK(solidVertices > 0);  // the bottom edge: content continues below
    // At the end of the content the bottom fade is gone; the top one shows the surface.
    session.SetUi([&] {
        fi::PushScrollFadeSurface(surface);
        ui.Ui(session);
        fi::PopScrollFadeSurface();
        if (ImGuiWindow* window = ui.area.Window())
        {
            fi::ScrollAreaTo(window, ImVec2(-1.0f, window->ScrollMax.y), false);
            solidVertices = 0;
            for (const ImDrawVert& vertex : window->DrawList->VtxBuffer)
                solidVertices += vertex.col == solid && vertex.pos.y > window->Rect().GetCenter().y ? 1 : 0;
        }
    });
    session.Frames(3);
    CHECK(solidVertices == 0);
    // Unbalanced pops are reported.
    const int errors = fi::UsageErrorCount();
    session.SetUi([&] { fi::PopScrollFadeSurface(); });
    session.Frame();
    CHECK(fi::UsageErrorCount() == errors + 1);
}

TEST_CASE("Scroll area: a popup opened inside a pushed surface fades into the popup surface")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImVec4 page(0.25f, 0.5f, 0.75f, 1.0f);
    const ImU32 pageColor = facet::ToU32(page);
    const ImU32 elevated = facet::ToU32(facet::T().colors.bgElevated);
    REQUIRE(pageColor != elevated);
    int pageVertices = 0;
    int elevatedVertices = 0;
    int insideVertices = 0;
    bool open = false;
    session.SetUi([&] {
        // The page pushes its surface around a control that opens a popup with a scrolling list.
        fi::PushScrollFadeSurface(page);
        if (!open)
        {
            ImGui::OpenPopup("list");
            open = true;
        }
        if (ImGui::BeginPopup("list"))
        {
            if (fi::BeginScrollAreaCore("rows", {.size = ImVec2(200.0f, 120.0f)}))
            {
                ImGui::Dummy(ImVec2(180.0f, 900.0f));
                ImGuiWindow* window = ImGui::GetCurrentWindow();
                fi::EndScrollAreaCore();  // draws the fades
                pageVertices = elevatedVertices = 0;
                for (const ImDrawVert& vertex : window->DrawList->VtxBuffer)
                {
                    pageVertices += vertex.col == pageColor ? 1 : 0;
                    elevatedVertices += vertex.col == elevated ? 1 : 0;
                }
            }
            ImGui::EndPopup();
        }
        // An area of the page itself still takes the pushed surface.
        if (fi::BeginScrollAreaCore("page rows", {.size = ImVec2(200.0f, 120.0f)}))
        {
            ImGui::Dummy(ImVec2(180.0f, 900.0f));
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            fi::EndScrollAreaCore();
            insideVertices = 0;
            for (const ImDrawVert& vertex : window->DrawList->VtxBuffer)
                insideVertices += vertex.col == pageColor ? 1 : 0;
        }
        fi::PopScrollFadeSurface();
    });
    session.Frames(3);
    CHECK(pageVertices == 0);
    CHECK(elevatedVertices > 0);  // the bottom fade, where the rows continue
    CHECK(insideVertices > 0);
}

TEST_CASE("Scroll area: inside a forced popup level or a panel the fades take the level's surface")
{
    MotionGuard guard;
    // In the dark theme the page background and the elevated surfaces differ.
    facet::SetTheme(facet::Theme::Dark(), false);
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::Colors& c = facet::T().colors;
    const ImU32 page = facet::ToU32(c.bg);
    const ImU32 popup = facet::ToU32(c.ElevatedSurface(2));
    const ImU32 dialog = facet::ToU32(c.ElevatedSurface(3));
    REQUIRE(page != popup);
    REQUIRE(popup != dialog);
    struct Count
    {
        int page = 0;
        int popup = 0;
        int dialog = 0;
    };
    Count forced;
    Count panel;
    const auto rows = [&](const char* id, Count* count) {
        if (fi::BeginScrollAreaCore(id, {.size = ImVec2(200.0f, 120.0f)}))
        {
            ImGui::Dummy(ImVec2(180.0f, 900.0f));
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            fi::EndScrollAreaCore();  // draws the fades
            *count = Count{};
            for (const ImDrawVert& vertex : window->DrawList->VtxBuffer)
            {
                count->page += vertex.col == page ? 1 : 0;
                count->popup += vertex.col == popup ? 1 : 0;
                count->dialog += vertex.col == dialog ? 1 : 0;
            }
        }
    };
    bool opened = false;
    session.SetUi([&] {
        ImGui::Button("anchor");
        fi::PopupAnchor anchor;
        anchor.rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        // A capture shows a level for one frame as a plain window, not a Dear ImGui popup.
        const ImGuiID forcedId = ImGui::GetID("forced");
        fi::ForcePopupLevelOpen(forcedId, anchor);
        if (fi::BeginPopupLevel(forcedId))
        {
            rows("forced rows", &forced);
            fi::EndPopupLevel();
        }
        // A panel is a plain window too, here at the dialog elevation.
        const ImGuiID panelId = ImGui::GetID("panel");
        if (!opened)
        {
            fi::OpenPopupLevel(panelId, anchor, {.kind = fi::PopupKind::Panel, .takeFocus = false, .elevation = 3});
            opened = true;
        }
        if (fi::BeginPopupLevel(panelId))
        {
            rows("panel rows", &panel);
            fi::EndPopupLevel();
        }
    });
    session.Frames(4);
    CHECK(forced.page == 0);
    CHECK(forced.popup > 0);  // the bottom fade, where the rows continue
    CHECK(panel.page == 0);
    CHECK(panel.dialog > 0);
}

TEST_CASE("Scroll area: an End without its Begin is reported")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const int errors = fi::UsageErrorCount();
    session.SetUi([&] { fi::EndScrollAreaCore(); });
    session.Frame();
    CHECK(fi::UsageErrorCount() == errors + 1);
}

TEST_CASE("Scroll area: a gesture stays with its window while an area slides under the resting pointer")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    AreaProbe outer;
    AreaProbe inner;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("outer", {.size = ImVec2(400.0f, 300.0f)}))
        {
            outer.Capture();
            ImGui::Dummy(ImVec2(10.0f, 200.0f));
            if (fi::BeginScrollAreaCore("inner", {.size = ImVec2(300.0f, 150.0f)}))
            {
                inner.Capture();
                ImGui::Dummy(ImVec2(10.0f, 600.0f));
                fi::EndScrollAreaCore();
            }
            ImGui::Dummy(ImVec2(10.0f, 1000.0f));
            fi::EndScrollAreaCore();
        }
    });
    session.Frames(2);
    // The pointer rests above the inner area; two notches bring the inner area under it.
    const ImVec2 at(outer.Window()->Rect().Min.x + 100.0f, outer.Window()->Rect().Min.y + 100.0f);
    session.Wheel(at, -1.0f);
    session.Wheel(at, -1.0f);
    session.Wheel(at, -1.0f);
    session.Seconds(0.6f);
    REQUIRE(inner.Window()->Rect().Contains(at));
    CHECK(outer.ScrollY() == doctest::Approx(Notch() * 3.0f));
    CHECK(inner.ScrollY() == 0.0f);
    // A turn the other way ends the gesture; the next turn down starts at the inner area.
    session.Wheel(at, 1.0f);
    session.Wheel(at, -1.0f);
    session.Seconds(0.6f);
    CHECK(inner.ScrollY() == doctest::Approx(Notch()));
}

TEST_CASE("Scroll area: fades and bars are drawn above child windows inside the area")
{
    MotionGuard guard;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImVec4 surface(0.75f, 0.5f, 0.25f, 1.0f);
    const ImU32 solid = facet::ToU32(surface);
    AreaProbe area;
    ImGuiID child = 0;
    int inChild = 0;
    int inArea = 0;
    session.SetUi([&] {
        if (fi::BeginScrollAreaCore("area", {.size = ImVec2(300.0f, 200.0f), .fadeSurface = surface}))
        {
            area.Capture();
            ImGui::Dummy(ImVec2(10.0f, 100.0f));
            ImGui::BeginChild("child", ImVec2(200.0f, 400.0f));
            child = ImGui::GetCurrentWindow()->ID;
            ImGui::TextUnformatted("Child window content");
            ImGui::EndChild();
            fi::EndScrollAreaCore();
        }
        inChild = 0;
        inArea = 0;
        if (ImGuiWindow* window = ImGui::FindWindowByID(child))
            for (const ImDrawVert& vertex : window->DrawList->VtxBuffer)
                inChild += vertex.col == solid ? 1 : 0;
        if (ImGuiWindow* window = area.Window())
            for (const ImDrawVert& vertex : window->DrawList->VtxBuffer)
                inArea += vertex.col == solid ? 1 : 0;
    });
    session.Frames(3);
    CHECK(inChild > 0);
    CHECK(inArea == 0);
}
