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
