// The shared test session (tests/support/session.h): frames, tracked items and every kind of
// scripted input, checked against stock Dear ImGui widgets.

#include <cstdint>
#include <cstring>

#include <doctest/doctest.h>
#include <imgui.h>

#include <facet/facet.h>

#include "support/session.h"

TEST_CASE("A session starts Facet and steps frames at the fixed time step")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    CHECK(facet::IsInitialized());
    const std::uint64_t before = facet::FrameIndex();
    session.Frames(3);
    CHECK(facet::FrameIndex() == before + 3);
    CHECK(session.FrameCount() == 3);
    CHECK(facet::DeltaTime() == doctest::Approx(1.0f / 60.0f));
    session.Seconds(0.5f);
    CHECK(session.FrameCount() == 33);
}

TEST_CASE("Click, drag and release drive a stock slider")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.0f;
    session.SetUi([&] {
        ImGui::SetNextItemWidth(400.0f);
        ImGui::SliderFloat("##value", &value, 0.0f, 1.0f);
        session.Track("slider");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("slider");
    REQUIRE(rect.GetWidth() > 300.0f);

    const auto at = [&](float fraction) { return ImVec2(rect.Min.x + rect.GetWidth() * fraction, rect.GetCenter().y); };
    session.Click(at(0.5f));
    CHECK(value == doctest::Approx(0.5f).epsilon(0.03));

    // Through an intermediate point, then out of the window to the right: the slider clamps.
    session.Drag({at(0.2f), at(0.8f), ImVec2(2000.0f, rect.GetCenter().y)});
    CHECK(value == 1.0f);
    CHECK_FALSE(ImGui::IsAnyItemActive());
}

TEST_CASE("Keys with modifiers, held keys and typed text reach stock widgets")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    char text[32] = "";
    float value = 0.5f;
    session.SetUi([&] {
        ImGui::InputText("##text", text, sizeof(text));
        session.Track("text");
        ImGui::SliderFloat("##value", &value, 0.0f, 1.0f, "%.2f");
        session.Track("slider");
    });
    session.Frame();

    session.Click(session.ItemCenter("text"));
    session.Type("hello");
    CHECK(std::strcmp(text, "hello") == 0);
    // Ctrl+A selects everything, so the next text replaces it.
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.Type("hi");
    CHECK(std::strcmp(text, "hi") == 0);

    // Tab moves on to the slider, Shift+Tab back.
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("slider"));
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    CHECK(ImGui::GetFocusID() == session.ItemId("text"));
    // Tabbing into a slider opens its text input; Enter leaves it with the value unchanged.
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == 0.5f);

    // One press of the right arrow on the activated slider moves one step; holding it repeats.
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_RightArrow);
    const float oneStep = value - 0.5f;
    CHECK(oneStep > 0.0f);
    session.HoldKey(ImGuiKey_RightArrow, 1.0f);
    CHECK(value - 0.5f > oneStep * 5.0f);
}

TEST_CASE("Right clicks, double clicks and the Menu key reach Dear ImGui")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int rightClicks = 0;
    int doubleClicks = 0;
    int menuPresses = 0;
    session.SetUi([&] {
        ImGui::Button("target", ImVec2(120.0f, 40.0f));
        session.Track("target");
        rightClicks += ImGui::IsItemClicked(ImGuiMouseButton_Right) ? 1 : 0;
        doubleClicks += ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) ? 1 : 0;
        menuPresses += ImGui::IsKeyPressed(ImGuiKey_Menu, false) ? 1 : 0;
    });
    session.Frame();
    session.Click(session.ItemCenter("target"), ImGuiMouseButton_Right);
    CHECK(rightClicks == 1);
    CHECK(doubleClicks == 0);
    session.Click(session.ItemCenter("target"));
    CHECK(doubleClicks == 0);  // one click is not a double click
    session.Frames(30);        // longer than the double click time
    session.DoubleClick(session.ItemCenter("target"));
    CHECK(doubleClicks == 1);
    session.KeyPress(ImGuiKey_Menu);
    CHECK(menuPresses == 1);
}

TEST_CASE("Shift and the arrows select text, and copy and paste use the session's own clipboard")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    char text[64] = "";
    session.SetUi([&] {
        ImGui::InputText("##text", text, sizeof(text));
        session.Track("text");
    });
    session.Frame();
    CHECK(session.Clipboard().empty());

    session.Click(session.ItemCenter("text"));
    session.Type("hello world");  // character input, as typed without an IME
    CHECK(std::strcmp(text, "hello world") == 0);
    for (int i = 0; i < 5; ++i)
        session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);
    session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    CHECK(session.Clipboard() == "world");
    session.KeyPress(ImGuiKey_Home);
    session.SetClipboard("say ");
    session.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    CHECK(std::strcmp(text, "say hello world") == 0);
    // A double click on the first word selects it; typing replaces it.
    const ImRect field = session.ItemRect("text");
    session.DoubleClick(ImVec2(field.Min.x + ImGui::GetStyle().FramePadding.x + 2.0f, field.GetCenter().y));
    session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    CHECK(session.Clipboard() == "say");
}

TEST_CASE("A drag held still keeps the item active until the release")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.0f;
    int activeFrames = 0;
    session.SetUi([&] {
        ImGui::SetNextItemWidth(400.0f);
        ImGui::SliderFloat("##value", &value, 0.0f, 1.0f);
        session.Track("slider");
        activeFrames += ImGui::IsItemActive() ? 1 : 0;
    });
    session.Frame();
    const ImRect rect = session.ItemRect("slider");
    const auto at = [&](float fraction) { return ImVec2(rect.Min.x + rect.GetWidth() * fraction, rect.GetCenter().y); };
    session.Drag({at(0.2f), at(0.6f)}, 4);
    const int plainDrag = activeFrames;
    activeFrames = 0;
    session.DragAndHold({at(0.2f), at(0.6f)}, 4, 20);
    CHECK(activeFrames == plainDrag + 20);
    CHECK(value == doctest::Approx(0.6f).epsilon(0.03));
    CHECK_FALSE(ImGui::IsAnyItemActive());
}

TEST_CASE("Tracked items report whether they lie in the visible part of their window")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        ImGui::BeginChild("list", ImVec2(200.0f, 100.0f));
        for (int i = 0; i < 50; ++i)
        {
            ImGui::Text("Row %d", i);
            if (i == 0)
                session.Track("first");
            if (i == 48)
                session.Track("far");
        }
        ImGui::EndChild();
        session.Track("list");
    });
    session.Frame();
    CHECK(session.ItemVisible("first"));
    CHECK_FALSE(session.ItemVisible("far"));
    CHECK_FALSE(session.ItemVisible("never tracked"));
    session.Wheel(session.ItemCenter("list"), -20.0f);  // to the end of the list
    session.Frames(2);
    CHECK_FALSE(session.ItemVisible("first"));
    CHECK(session.ItemVisible("far"));
}

TEST_CASE("The mouse wheel scrolls the window under the mouse")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float scroll = 0.0f;
    session.SetUi([&] {
        ImGui::BeginChild("list", ImVec2(200.0f, 100.0f));
        for (int i = 0; i < 50; ++i)
            ImGui::Text("Row %d", i);
        scroll = ImGui::GetScrollY();
        ImGui::EndChild();
        session.Track("list");
    });
    session.Frame();
    session.Wheel(session.ItemCenter("list"), -2.0f);
    session.Frames(2);
    CHECK(scroll > 0.0f);
}
