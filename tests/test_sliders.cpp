// Behaviour of the sliders family (facet/widgets/sliders.h).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_sliders.cpp.

#include <cmath>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/session.h"
#include "widgets/sliders/slider_core.h"

namespace
{

// Sliders in these tests have no visible label, so their control starts at the item's left edge.
constexpr float kSliderWidth = 300.0f;

float KnobSize()
{
    return facet::internal::GetSliderMetrics(facet::ControlSize::Regular).knob;
}

} // namespace

TEST_CASE("A slider press on the knob grabs it, and a drag out and back restores the exact value")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.0f;
    int changes = 0;
    int commits = 0;
    session.SetUi([&] {
        if (facet::Slider("##s", &value, 0.0f, 1.0f, {.width = kSliderWidth}))
            ++changes;
        if (facet::IsItemEditCommitted())
            ++commits;
        session.Track("slider");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("slider");
    // Value 0: the knob sits at the start of the control.
    const ImVec2 knob(rect.Min.x + KnobSize() * 0.5f + 2.0f, rect.GetCenter().y);
    session.MouseMove(knob);
    session.MouseDown();
    CHECK(value == 0.0f);
    session.MouseMove(ImVec2(knob.x + 120.0f, knob.y));
    session.MouseMove(ImVec2(knob.x + 160.0f, knob.y));
    CHECK(value > 0.2f);
    // Out of the window to the left, and back to the press point.
    session.MouseMove(ImVec2(-500.0f, knob.y));
    CHECK(value == 0.0f);
    session.MouseMove(ImVec2(knob.x + 60.0f, knob.y));
    session.MouseMove(knob);
    CHECK(value == 0.0f);
    session.MouseUp();
    session.Frame();
    CHECK(commits == 1);
    CHECK(changes >= 2);
}

TEST_CASE("A slider click on the track moves the value under the pointer")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int value = 0;
    session.SetUi([&] {
        facet::Slider("##s", &value, 0, 10, {.width = kSliderWidth});
        session.Track("slider");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("slider");
    session.Click(ImVec2(rect.Min.x + KnobSize() * 0.5f + 100.0f, rect.GetCenter().y));
    CHECK(value > 0);
    CHECK(value < 10);
}

TEST_CASE("A slider integer drag writes whole numbers inside the range only")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int value = 5;
    session.SetUi([&] {
        facet::Slider("##s", &value, 0, 7, {.width = kSliderWidth});
        session.Track("slider");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("slider");
    session.MouseMove(ImVec2(rect.Min.x + 20.0f, rect.GetCenter().y));
    session.MouseDown();
    bool seen[8] = {};
    for (int i = 0; i < 40; ++i)
    {
        session.MouseMove(ImVec2(rect.Min.x + 20.0f + i * 7.3f, rect.GetCenter().y));
        REQUIRE(value >= 0);
        REQUIRE(value <= 7);
        seen[value] = true;
    }
    session.MouseUp();
    CHECK(seen[0]);
    CHECK(seen[7]);
}

TEST_CASE("A slider is a Tab stop and the keys step it, Home and End reach the ends, Escape restores")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.5f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::Slider("Level", &value, 0.0f, 1.0f);
        session.Track("slider");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("slider"));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(value == doctest::Approx(0.51f));
    session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);
    CHECK(value == doctest::Approx(0.509f));
    session.KeyPress(ImGuiKey_End);
    CHECK(value == 1.0f);
    session.KeyPress(ImGuiKey_Home);
    CHECK(value == 0.0f);
    session.KeyPress(ImGuiKey_PageUp);
    CHECK(value == doctest::Approx(0.1f));
}

TEST_CASE("Escape during a slider drag restores the value from the press and reports a cancel")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.25f;
    int canceled = 0;
    session.SetUi([&] {
        facet::Slider("##s", &value, 0.0f, 1.0f, {.width = kSliderWidth});
        if (facet::IsItemEditCanceled())
            ++canceled;
        session.Track("slider");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("slider");
    session.MouseMove(ImVec2(rect.Min.x + 30.0f, rect.GetCenter().y));
    session.MouseDown();
    session.MouseMove(ImVec2(rect.Min.x + 200.0f, rect.GetCenter().y));
    CHECK(value != 0.25f);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(value == 0.25f);
    CHECK(canceled == 1);
    session.MouseUp();
    CHECK(value == 0.25f);
}

TEST_CASE("A read-only, a disabled, an empty-range and a null slider write nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float readOnly = 0.5f;
    float disabled = 0.5f;
    float empty = 3.0f;
    bool any = false;
    session.SetUi([&] {
        any |= facet::Slider("##readOnly", &readOnly, 0.0f, 1.0f, {.width = kSliderWidth, .readOnly = true});
        session.Track("readOnly");
        ImGui::BeginDisabled();
        any |= facet::Slider("##disabled", &disabled, 0.0f, 1.0f, {.width = kSliderWidth});
        session.Track("disabled");
        ImGui::EndDisabled();
        any |= facet::Slider("##empty", &empty, 3.0f, 3.0f, {.width = kSliderWidth});
        session.Track("empty");
        any |= facet::Slider("##null", static_cast<float*>(nullptr), 0.0f, 1.0f, {.width = kSliderWidth});
    });
    session.Frame();
    for (const char* name : {"readOnly", "disabled", "empty"})
    {
        const ImRect rect = session.ItemRect(name);
        session.Drag({ImVec2(rect.Min.x + 4.0f, rect.GetCenter().y), ImVec2(rect.Max.x - 4.0f, rect.GetCenter().y)});
    }
    // The read-only slider still takes the focus, and its keys write nothing.
    session.Click(session.ItemCenter("readOnly"));
    CHECK(ImGui::GetFocusID() == session.ItemId("readOnly"));
    session.KeyPress(ImGuiKey_End);
    CHECK_FALSE(any);
    CHECK(readOnly == 0.5f);
    CHECK(disabled == 0.5f);
    CHECK(empty == 3.0f);
}

TEST_CASE("A slider drag commits once on release")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.0f;
    int changes = 0;
    int commits = 0;
    session.SetUi([&] {
        if (facet::Slider("##s", &value, 0.0f, 1.0f, {.width = kSliderWidth}))
            ++changes;
        if (facet::IsItemEditCommitted())
            ++commits;
        session.Track("slider");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("slider");
    session.Drag({ImVec2(rect.Min.x + 40.0f, rect.GetCenter().y), ImVec2(rect.Min.x + 200.0f, rect.GetCenter().y)}, 8);
    CHECK(changes >= 2);
    CHECK(commits == 1);
}

TEST_CASE("Enter on a focused slider types a value that is written on Enter")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.5f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::Slider("Level", &value, 0.0f, 10.0f);
        session.Track("slider");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("slider"));
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    session.Type("7.5");
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(value == 7.5f);
    // The focus is back on the slider.
    CHECK(ImGui::GetFocusID() == session.ItemId("slider"));
    // A typed value outside the range is clamped.
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    session.Type("40");
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(value == 10.0f);
    // Escape while typing keeps the value.
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    session.Type("3");
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK(value == 10.0f);
}

TEST_CASE("A slider keeps the width it is given, down to a few pixels, at scale 0.75 and 2")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.5f;
    for (const float scale : {0.75f, 2.0f})
    {
        CAPTURE(scale);
        facet::SetUiScale(scale);
        session.SetUi([&] {
            facet::Slider("A long label for a narrow slider", &value, 0.0f, 1.0f, {.width = 6.0f});
            session.Track("narrow");
            facet::Slider("Level", &value, 0.0f, 1.0f, {.width = 200.0f});
            session.Track("wide");
        });
        session.Frames(2);
        CHECK(std::fabs(session.ItemRect("narrow").GetWidth() - facet::Px(6.0f)) <= 1.0f);
        CHECK(session.ItemRect("wide").GetWidth() == doctest::Approx(std::round(facet::Px(200.0f))));
        CHECK(session.ItemRect("wide").GetHeight() ==
              doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    }
    facet::SetUiScale(1.0f);
    CHECK(value == 0.5f);
}
