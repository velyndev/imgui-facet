// Behaviour of the sliders family (facet/widgets/sliders.h).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks are in tests/test_api_sliders.cpp.

#include <algorithm>
#include <cmath>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/session.h"
#include "widgets/sliders/pad_core.h"
#include "widgets/sliders/range_core.h"
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


// ---- RangeSlider ---------------------------------------------------------------------------

namespace
{

// The track of an int range slider over 0 to 100 without a visible label, from its tracked rect:
// the knob centres travel from half a knob after the start to half a knob before the value column.
struct RangeTrack
{
    float x0 = 0.0f;
    float x1 = 0.0f;
    float y = 0.0f;
    float X(double value) const { return x0 + static_cast<float>(value / 100.0) * (x1 - x0); }
    ImVec2 At(double value) const { return ImVec2(X(value), y); }
};

RangeTrack TrackOf(const ImRect& rect)
{
    const facet::internal::SliderMetrics m = facet::internal::GetSliderMetrics(facet::ControlSize::Regular);
    facet::internal::ControlTextScope text(m.control);
    const float valueWidth = std::ceil(std::max(ImGui::CalcTextSize("0").x, ImGui::CalcTextSize("100").x));
    const float column = std::ceil(facet::internal::RangeValueColumnWidth(valueWidth)) + m.valueGap;
    RangeTrack t;
    t.x0 = std::round(rect.Min.x) + m.knob * 0.5f;
    t.x1 = std::round(rect.Min.x) + std::round(rect.GetWidth()) - column - m.knob * 0.5f;
    t.y = std::round(rect.GetCenter().y);
    return t;
}

// One int range slider over 0 to 100 under test, with the counts the tests read.
struct RangeRig
{
    facet::test::Session session;
    int low = 20;
    int high = 60;
    int changes = 0;
    int commits = 0;
    facet::RangeSliderOptions options = {.width = 400.0f};
    RangeTrack track;
    ImGuiID lastItem = 0;
    ImGuiID widgetId = 0;
    bool activeAfter = false;

    RangeRig()
    {
        session.SetUi([this] {
            ImGui::Button("before");
            session.Track("before");
            widgetId = ImGui::GetID("##range");
            if (facet::RangeSlider("##range", &low, &high, 0, 100, options))
                ++changes;
            lastItem = ImGui::GetItemID();
            activeAfter = ImGui::IsItemActive();
            if (facet::IsItemEditCommitted())
                ++commits;
            session.Track("range");
            track = TrackOf(session.ItemRect("range"));
        });
        session.Frame();
        session.Frame();
    }
    // Tab from the stock button to the first handle.
    void FocusLow()
    {
        session.Click(session.ItemCenter("before"));
        session.KeyPress(ImGuiKey_Tab);
        session.KeyPress(ImGuiKey_Tab);
    }
};

} // namespace

TEST_CASE("Each range handle is a Tab stop that the arrows move and that stops at the gap")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    rig.options.minGap = 10.0;
    rig.FocusLow();
    rig.session.KeyPress(ImGuiKey_RightArrow);
    CHECK(rig.low == 21);
    rig.session.KeyPress(ImGuiKey_End);
    CHECK(rig.low == 50);  // high 60 minus the gap
    rig.session.KeyPress(ImGuiKey_RightArrow);
    CHECK(rig.low == 50);  // a step at the gap limit writes nothing
    rig.session.KeyPress(ImGuiKey_Tab);
    rig.session.KeyPress(ImGuiKey_Home);
    CHECK(rig.high == 60); // the high handle cannot go below low plus the gap
    rig.session.KeyPress(ImGuiKey_End);
    CHECK(rig.high == 100);
    rig.session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Ctrl);
    CHECK(rig.high == 90); // Ctrl steps ten times
    CHECK(rig.low == 50);
}

TEST_CASE("A range handle with Push moves its neighbour along until the range ends")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    rig.options.minGap = 5.0;
    rig.options.collision = facet::HandleCollision::Push;
    rig.FocusLow();
    rig.session.KeyPress(ImGuiKey_End);
    CHECK(rig.low == 95);
    CHECK(rig.high == 100);
}

TEST_CASE("A range handle drag never crosses the other handle and returns exactly from outside the window")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    const ImVec2 grab = rig.track.At(20.0);
    rig.session.MouseMove(grab);
    rig.session.MouseDown();
    rig.session.MouseMove(ImVec2(grab.x + 60.0f, grab.y));
    CHECK(rig.low > 20);
    CHECK(rig.activeAfter);
    rig.session.MouseMove(ImVec2(2000.0f, grab.y));
    CHECK(rig.low == 60);  // stopped at the high handle (no gap)
    CHECK(rig.high == 60);
    rig.session.MouseMove(ImVec2(-2000.0f, grab.y));
    CHECK(rig.low == 0);
    rig.session.MouseMove(grab);
    CHECK(rig.low == 20);
    rig.session.MouseUp();
    rig.session.Frame();
    CHECK(rig.commits == 1);
    CHECK(rig.high == 60);
}

TEST_CASE("Dragging the range span moves both handles and keeps their distance")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    const ImVec2 middle = rig.track.At(40.0);
    rig.session.MouseMove(middle);
    rig.session.MouseDown();
    rig.session.MouseMove(ImVec2(middle.x + rig.track.X(30.0) - rig.track.X(0.0), middle.y));
    CHECK(rig.high - rig.low == 40);
    CHECK(rig.low > 20);
    rig.session.MouseMove(ImVec2(3000.0f, middle.y));
    CHECK(rig.high == 100);
    CHECK(rig.low == 60);
    rig.session.MouseMove(middle);
    rig.session.MouseUp();
    CHECK(rig.low == 20);
    CHECK(rig.high == 60);
}

TEST_CASE("A click on the empty range track moves the nearer handle, a click between the handles grabs the span")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    rig.session.Click(rig.track.At(5.0));
    CHECK(rig.low == 5);
    CHECK(rig.high == 60);
    rig.session.Click(rig.track.At(80.0));
    CHECK(rig.high == 80);
    // Exactly between the handles: the span takes the press and nothing moves.
    rig.session.Click(rig.track.At(42.5));
    CHECK(rig.low == 5);
    CHECK(rig.high == 80);
    // Without span dragging the nearer handle moves there.
    rig.options.dragSpan = false;
    rig.session.Click(rig.track.At(30.0));
    CHECK(rig.low == 30);
    CHECK(rig.high == 80);
}

TEST_CASE("Range handles at one value separate in the direction of the first movement")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    rig.low = 50;
    rig.high = 50;
    rig.session.Frame();
    const ImVec2 at = rig.track.At(50.0);
    rig.session.Drag({at, ImVec2(at.x + 80.0f, at.y)});
    CHECK(rig.low == 50);
    CHECK(rig.high > 50);
    rig.high = 50;
    rig.session.Frame();
    rig.session.Drag({at, ImVec2(at.x - 80.0f, at.y)});
    CHECK(rig.low < 50);
    CHECK(rig.high == 50);
}

TEST_CASE("A range minimum gap larger than the range, a range of zero and a range of one stay valid")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int lowA = 3, highA = 4;
    int lowB = 5, highB = 5;
    int lowC = 0, highC = 1;
    float lowD = 0.0f, highD = 0.0f;
    bool written[4] = {};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        written[0] |= facet::RangeSlider("Gap", &lowA, &highA, 0, 10, {.minGap = 50.0});
        session.Track("gap");
        written[1] |= facet::RangeSlider("Zero", &lowB, &highB, 5, 5);
        session.Track("zero");
        written[2] |= facet::RangeSlider("One", &lowC, &highC, 0, 1);
        session.Track("one");
        written[3] |= facet::RangeSlider("Log", &lowD, &highD, 0.0f, 100.0f, {.scale = facet::ValueScale::Logarithmic});
        session.Track("log");
    });
    session.Frame();
    // Drawing repairs nothing in the caller's variables.
    CHECK(lowA == 3);
    CHECK(highA == 4);
    // The gap is clamped to the range: the handles are drawn at the two ends and cannot move, so
    // the keys write nothing.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_Home);
    CHECK_FALSE(written[0]);
    CHECK(lowA == 3);
    CHECK(highA == 4);
    // A range of zero is read-only.
    const ImRect zero = session.ItemRect("zero");
    session.Drag({ImVec2(zero.Min.x + 60.0f, zero.GetCenter().y), ImVec2(zero.Max.x - 10.0f, zero.GetCenter().y)});
    CHECK_FALSE(written[1]);
    CHECK(lowB == 5);
    // A range of one: whole numbers only, never crossing.
    const ImRect one = session.ItemRect("one");
    for (float x = one.Min.x + 40.0f; x < one.Max.x; x += 23.0f)
    {
        session.Drag({ImVec2(one.Min.x + 50.0f, one.GetCenter().y), ImVec2(x, one.GetCenter().y)});
        REQUIRE(lowC >= 0);
        REQUIRE(highC <= 1);
        REQUIRE(lowC <= highC);
    }
    // Logarithmic with a minimum of zero falls back to linear and stays finite.
    const ImRect log = session.ItemRect("log");
    session.Drag({ImVec2(log.Min.x + 60.0f, log.GetCenter().y), ImVec2(log.Max.x - 60.0f, log.GetCenter().y)});
    CHECK(std::isfinite(lowD));
    CHECK(std::isfinite(highD));
    CHECK(highD > 0.0f);
    CHECK(lowD <= highD);
}

TEST_CASE("A mirrored range moves the other handle the same distance the other way")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    rig.options.mirror = true;
    rig.low = 30;
    rig.high = 80;  // off center
    rig.FocusLow();
    rig.session.KeyPress(ImGuiKey_RightArrow);
    // The pair is drawn symmetric around 50, kept from the high handle, and moves from there.
    CHECK(rig.low == 21);
    CHECK(rig.high == 79);
    rig.session.KeyPress(ImGuiKey_Home);
    CHECK(rig.low == 0);
    CHECK(rig.high == 100);
}

TEST_CASE("A fixed span range moves both handles and stops as a whole at the ends")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    rig.options.fixedSpan = true;
    rig.FocusLow();
    rig.session.KeyPress(ImGuiKey_End);
    CHECK(rig.high == 100);
    CHECK(rig.low == 60);
    rig.session.KeyPress(ImGuiKey_Home);
    CHECK(rig.low == 0);
    CHECK(rig.high == 40);
    const ImVec2 grab = rig.track.At(40.0);
    rig.session.Drag({grab, ImVec2(4000.0f, grab.y)});
    CHECK(rig.high - rig.low == 40);
    CHECK(rig.high == 100);
}

TEST_CASE("A vertical range moves its handles with Up and Down and by dragging up")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float low = 0.2f, high = 0.8f;
    ImGuiID handles[2] = {};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        ImGui::PushOverrideID(ImGui::GetID("Band"));
        handles[0] = ImGui::GetID(0);
        handles[1] = ImGui::GetID(1);
        ImGui::PopID();
        facet::RangeSlider("Band", &low, &high, 0.0f, 1.0f, {.orientation = facet::Orientation::Vertical});
        session.Track("band");
    });
    session.Frame();
    const ImRect band = session.ItemRect("band");
    CHECK(band.GetWidth() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    CHECK(band.GetHeight() == doctest::Approx(std::round(facet::Px(facet::T().metrics.faderLength))));
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(low == doctest::Approx(0.21f));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == handles[1]);
    // 0.8f is not 0.8 as a double; the step still lands on 0.79, the next point of the grid.
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(high == 0.79f);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(high == 0.8f);
}

TEST_CASE("A read-only or disabled range writes nothing and a read-only one keeps its Tab stops")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int low = 20, high = 60;
    int dLow = 20, dHigh = 60;
    bool any = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        any |= facet::RangeSlider("##ro", &low, &high, 0, 100, {.readOnly = true});
        session.Track("ro");
        ImGui::BeginDisabled();
        any |= facet::RangeSlider("##off", &dLow, &dHigh, 0, 100);
        session.Track("off");
        ImGui::EndDisabled();
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    for (const char* name : {"ro", "off"})
    {
        const ImRect r = session.ItemRect(name);
        session.Drag({ImVec2(r.Min.x + 30.0f, r.GetCenter().y), ImVec2(r.Max.x - 30.0f, r.GetCenter().y)});
        session.Click(ImVec2(r.Min.x + 12.0f, r.GetCenter().y));
    }
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    // Two handle stops of the read-only range, then the disabled range is skipped.
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    CHECK_FALSE(any);
    CHECK(low == 20);
    CHECK(high == 60);
    CHECK(dLow == 20);
    CHECK(dHigh == 60);
}

TEST_CASE("Enter on a range handle types its value, which keeps the order unless out of range is allowed")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    rig.options.minGap = 5.0;
    rig.FocusLow();
    rig.session.KeyPress(ImGuiKey_Enter);
    rig.session.Frames(2);
    rig.session.Type("35");
    rig.session.KeyPress(ImGuiKey_Enter);
    rig.session.Frames(2);
    CHECK(rig.low == 35);
    // Past the other handle: stopped one gap before it.
    rig.session.KeyPress(ImGuiKey_Enter);
    rig.session.Frames(2);
    rig.session.Type("90");
    rig.session.KeyPress(ImGuiKey_Enter);
    rig.session.Frames(2);
    CHECK(rig.low == 55);
    CHECK(rig.high == 60);
    // Out of range: clamped, or written as typed with allowOutOfRange.
    rig.session.KeyPress(ImGuiKey_Enter);
    rig.session.Frames(2);
    rig.session.Type("-20");
    rig.session.KeyPress(ImGuiKey_Enter);
    rig.session.Frames(2);
    CHECK(rig.low == 0);
    rig.options.allowOutOfRange = true;
    rig.session.KeyPress(ImGuiKey_Enter);
    rig.session.Frames(2);
    rig.session.Type("-20");
    rig.session.KeyPress(ImGuiKey_Enter);
    rig.session.Frames(2);
    CHECK(rig.low == -20);
    // Editing the other handle leaves the out-of-range value alone.
    rig.session.KeyPress(ImGuiKey_Tab);
    rig.session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(rig.high == 59);
    CHECK(rig.low == -20);
}

TEST_CASE("A double click on a range handle with doubleClickReset sends it to its end")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    rig.options.doubleClickReset = true;
    rig.session.DoubleClick(rig.track.At(60.0));
    CHECK(rig.high == 100);
    CHECK(rig.low == 20);
    rig.session.DoubleClick(rig.track.At(20.0));
    CHECK(rig.low == 0);
}

TEST_CASE("A range slider is one item for the queries while idle and keeps the width it is given")
{
    RangeRig rig;
    REQUIRE(rig.session.Ready());
    CHECK(rig.lastItem == rig.widgetId);
    for (const float scale : {0.75f, 2.0f})
    {
        CAPTURE(scale);
        facet::SetUiScale(scale);
        rig.options.width = 5.0f;
        rig.session.Frames(2);
        CHECK(std::fabs(rig.session.ItemRect("range").GetWidth() - facet::Px(5.0f)) <= 1.0f);
        rig.options.width = 200.0f;
        rig.session.Frames(2);
        CHECK(std::fabs(rig.session.ItemRect("range").GetWidth() - facet::Px(200.0f)) <= 1.0f);
    }
    facet::SetUiScale(1.0f);
    CHECK(rig.low == 20);
    CHECK(rig.high == 60);
}

// ---- SliderStepped -------------------------------------------------------------------------

TEST_CASE("A stepped slider moves from stop to stop with the keys and rests on stops when dragged")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float kStops[] = {1.0f, 2.0f, 5.0f, 10.0f, 50.0f};
    float value = 2.0f;
    float magnetic = 2.0f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::SliderStepped("##s", &value, kStops, {.width = kSliderWidth});
        session.Track("stepped");
        facet::SliderStepped("##m", &magnetic, kStops, {.magnetic = true, .width = kSliderWidth});
        session.Track("magnetic");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(value == 5.0f);
    session.KeyPress(ImGuiKey_End);
    CHECK(value == 50.0f);
    session.KeyPress(ImGuiKey_Home);
    CHECK(value == 1.0f);
    // Typed: the nearest stop.
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    session.Type("8");
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(value == 10.0f);
    // A drag anywhere lands on a stop.
    const ImRect rect = session.ItemRect("stepped");
    for (float x = rect.Min.x + 15.0f; x < rect.Max.x - 40.0f; x += 17.0f)
    {
        session.Drag({ImVec2(rect.Min.x + 15.0f, rect.GetCenter().y), ImVec2(x, rect.GetCenter().y)});
        const bool onStop = value == 1.0f || value == 2.0f || value == 5.0f || value == 10.0f || value == 50.0f;
        REQUIRE(onStop);
    }
    // Magnetic: free between the stops, the keys still go to the next stop. After the clicks the
    // first Tab shows the focus where it is, the second moves it on.
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(magnetic == 5.0f);
    const ImRect m = session.ItemRect("magnetic");
    const float knob = KnobSize();
    session.Drag({ImVec2(m.Min.x + knob * 0.5f, m.GetCenter().y), ImVec2(m.Min.x + 90.0f, m.GetCenter().y)});
    const bool between = magnetic != 1.0f && magnetic != 2.0f && magnetic != 5.0f && magnetic != 10.0f;
    CHECK(between);
}

TEST_CASE("A stepped slider with fewer than two stops is read-only, and stop labels add one line")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float kOne[] = {3.0f};
    static const float kStops[] = {0.0f, 0.5f, 1.0f};
    static const char* const kLabels[] = {"Low", "Middle", "High"};
    float value = 3.0f;
    float labelled = 0.5f;
    bool written = false;
    session.SetUi([&] {
        written |= facet::SliderStepped("##one", &value, kOne, {.width = kSliderWidth});
        session.Track("one");
        facet::SliderStepped("##plain", &labelled, kStops, {.width = kSliderWidth});
        session.Track("plain");
        facet::SliderStepped("##labels", &labelled, kStops, {.stopLabels = kLabels, .width = kSliderWidth});
        session.Track("labels");
    });
    session.Frame();
    const ImRect one = session.ItemRect("one");
    session.Drag({ImVec2(one.Min.x + 10.0f, one.GetCenter().y), ImVec2(one.Max.x - 10.0f, one.GetCenter().y)});
    CHECK_FALSE(written);
    CHECK(value == 3.0f);
    CHECK(session.ItemRect("labels").GetHeight() > session.ItemRect("plain").GetHeight());
}
// ---- SliderWithInput -----------------------------------------------------------------------

TEST_CASE("A slider with a field has two Tab stops, and the field writes a typed value on Enter")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int value = 40;
    float free = 0.5f;
    int changes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::SliderWithInput("Amount", &value, 0, 100, {.width = 400.0f}))
            ++changes;
        session.Track("amount");
        facet::SliderWithInput("Free", &free, 0.0f, 1.0f, {.allowOutOfRange = true, .width = 400.0f});
        session.Track("free");
    });
    session.Frame();
    CHECK(session.ItemRect("amount").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(value == 41);
    // The field: the next Tab stop takes the keyboard with the number selected.
    session.KeyPress(ImGuiKey_Tab);
    session.Type("75");
    CHECK(value == 41);  // typed numbers write on Enter
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == 75);
    // Clamped to the range without allowOutOfRange.
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    session.KeyPress(ImGuiKey_Tab);
    session.Type("500");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(value == 100);
    // allowOutOfRange: the field writes 3, the slider shows it clamped and does not touch it.
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.Type("3");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(free == 3.0f);
    session.Frames(3);
    CHECK(free == 3.0f);
    CHECK(changes >= 3);
}

TEST_CASE("A narrow slider with a field puts the field under the track")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.5f;
    session.SetUi([&] {
        facet::SliderWithInput("Amount", &value, 0.0f, 1.0f, {.width = 150.0f});
        session.Track("narrow");
    });
    session.Frame();
    const float height = facet::ControlHeight(facet::ControlSize::Regular);
    CHECK(session.ItemRect("narrow").GetHeight() > height * 1.5f);
    CHECK(std::fabs(session.ItemRect("narrow").GetWidth() - facet::Px(150.0f)) <= 1.0f);
}
// ---- RangeInput ----------------------------------------------------------------------------

TEST_CASE("A range input has four Tab stops and a typed value follows the handle rules")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int low = 20, high = 60;
    int pLow = 20, pHigh = 60;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::RangeInput("Range", &low, &high, 0, 100, {.minGap = 10.0, .width = 500.0f});
        session.Track("range");
        facet::RangeInput("Pushing", &pLow, &pHigh, 0, 100,
                          {.minGap = 10.0, .collision = facet::HandleCollision::Push, .slider = false, .width = 500.0f});
        session.Track("push");
    });
    session.Frame();
    CHECK(session.ItemRect("range").GetHeight() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    // Low field: a value past the high one stops one gap before it.
    session.KeyPress(ImGuiKey_Tab);
    session.Type("90");
    session.KeyPress(ImGuiKey_Tab);
    CHECK(low == 50);
    CHECK(high == 60);
    // Low handle, then high handle.
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(low == 49);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(high == 61);
    // High field.
    session.KeyPress(ImGuiKey_Tab);
    session.Type("5");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(high == 59);
    CHECK(low == 49);
    // Fields only, with Push: a typed low pushes the high value.
    session.KeyPress(ImGuiKey_Tab);
    session.Type("95");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(pLow == 90);
    CHECK(pHigh == 100);
}

TEST_CASE("A narrow range input puts its fields under the track")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float low = 0.2f, high = 0.8f;
    session.SetUi([&] {
        facet::RangeInput("Range", &low, &high, 0.0f, 1.0f, {.width = 220.0f});
        session.Track("narrow");
    });
    session.Frame();
    CHECK(session.ItemRect("narrow").GetHeight() > facet::ControlHeight(facet::ControlSize::Regular) * 1.5f);
    CHECK(std::fabs(session.ItemRect("narrow").GetWidth() - facet::Px(220.0f)) <= 1.0f);
}
// ---- MultiSlider ---------------------------------------------------------------------------

TEST_CASE("Multi slider handles are Tab stops in order, keep the gap and never pass each other")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float values[3] = {0.2f, 0.5f, 0.8f};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::MultiSlider("##m", std::span<float>(values), 0.0f, 1.0f, {.minGap = 0.1, .width = kSliderWidth});
        session.Track("multi");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_End);
    CHECK(values[0] == doctest::Approx(0.4f));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Home);
    CHECK(values[1] == doctest::Approx(0.5f));
    session.KeyPress(ImGuiKey_End);
    CHECK(values[1] == doctest::Approx(0.7f));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Home);
    CHECK(values[2] == doctest::Approx(0.8f));
    session.KeyPress(ImGuiKey_End);
    CHECK(values[2] == 1.0f);
    CHECK(values[0] < values[1]);
    CHECK(values[1] < values[2]);
}

TEST_CASE("A growable multi slider adds a handle on a double click and removes one with Delete")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::vector<float> values = {0.2f, 0.8f};
    float storage[3] = {0.3f, 0.6f, 0.0f};
    int count = 2;
    int added = -2;
    int removed = -2;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::MultiSlider("##v", &values, 0.0f, 1.0f,
                           {.allowAdd = true, .allowRemove = true, .minHandles = 1, .width = kSliderWidth});
        session.Track("vector");
        if (facet::MultiSliderAddedHandle() >= 0)
            added = facet::MultiSliderAddedHandle();
        if (facet::MultiSliderRemovedHandle() >= 0)
            removed = facet::MultiSliderRemovedHandle();
        facet::MultiSlider("##s", std::span<float>(storage), &count, 0.0f, 1.0f,
                           {.allowAdd = true, .allowRemove = true, .width = kSliderWidth});
        session.Track("storage");
    });
    session.Frame();
    // A double click on the empty track after the last handle: a third handle at the end.
    const ImRect rect = session.ItemRect("vector");
    session.DoubleClick(ImVec2(rect.Max.x - KnobSize(), rect.GetCenter().y));
    REQUIRE(values.size() == 3);
    CHECK(added == 2);
    CHECK(values[0] == 0.2f);
    CHECK(values[1] == 0.8f);
    CHECK(values[2] > 0.9f);
    // Delete on the first handle removes it; the minimum of one handle stops further removals.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Delete);
    REQUIRE(values.size() == 2);
    CHECK(removed == 0);
    CHECK(values[0] == 0.8f);
    session.KeyPress(ImGuiKey_Delete);
    session.KeyPress(ImGuiKey_Delete);
    CHECK(values.size() == 1);
    // The storage overload stops at its capacity.
    const ImRect s = session.ItemRect("storage");
    session.DoubleClick(ImVec2(s.Min.x + KnobSize(), s.GetCenter().y));
    CHECK(count == 3);
    session.DoubleClick(ImVec2(s.Max.x - KnobSize(), s.GetCenter().y));
    CHECK(count == 3);
    CHECK(storage[0] < storage[1]);
    CHECK(storage[1] < storage[2]);
}

TEST_CASE("A multi slider handle dragged off the track and released there is removed")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::vector<float> values = {0.0f, 1.0f};
    session.SetUi([&] {
        facet::MultiSlider("##v", &values, 0.0f, 1.0f, {.allowRemove = true, .width = kSliderWidth});
        session.Track("vector");
    });
    session.Frame();
    const ImRect rect = session.ItemRect("vector");
    const ImVec2 first(rect.Min.x + KnobSize() * 0.5f, rect.GetCenter().y);
    session.Drag({first, ImVec2(first.x + 20.0f, first.y + 30.0f)});
    CHECK(values.size() == 2);  // still within two control heights: kept
    session.Drag({first, ImVec2(first.x, first.y + facet::ControlHeight(facet::ControlSize::Regular) * 3.0f)});
    REQUIRE(values.size() == 1);
    CHECK(values[0] == 1.0f);
}
// ---- Knob ----------------------------------------------------------------------------------

TEST_CASE("A knob turns up with an upward drag, steps with Up and Down and restores on a double click")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.5f;
    int whole = 3;
    int commits = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::Knob("Gain", &value, 0.0f, 1.0f);
        if (facet::IsItemEditCommitted())
            ++commits;
        session.Track("knob");
        facet::Knob("Steps", &whole, 0, 10, {.variant = facet::KnobVariant::Dot});
        session.Track("whole");
    });
    session.Frame();
    const ImRect knob = session.ItemRect("knob");
    const ImVec2 center(knob.GetCenter().x, knob.Min.y + knob.GetWidth() * 0.5f);
    // A press grabs without moving the value; up increases, back to the press point restores it.
    session.MouseMove(center);
    session.MouseDown();
    CHECK(value == 0.5f);
    session.MouseMove(ImVec2(center.x, center.y - 40.0f));
    CHECK(value > 0.5f);
    session.MouseMove(ImVec2(center.x, center.y + 4000.0f));
    CHECK(value == 0.0f);
    session.MouseMove(center);
    CHECK(value == 0.5f);
    session.MouseMove(ImVec2(center.x, center.y + 30.0f));
    session.MouseUp();
    session.Frame();
    CHECK(value < 0.5f);
    CHECK(commits == 1);
    // A double click restores the value from before its first click.
    const float moved = value;
    session.Seconds(0.5f);  // past the double click time of the drag's press
    session.DoubleClick(center);
    CHECK(value == moved);
    // Keys: Tab reaches it, Up steps, End reaches the maximum.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_End);
    CHECK(value == 1.0f);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(value == doctest::Approx(0.99f));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(whole == 4);
}

TEST_CASE("A circular knob follows the pointer angle, and a read-only knob writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.5f;
    float fixed = 0.5f;
    session.SetUi([&] {
        facet::Knob("Pan", &value, 0.0f, 1.0f, {.circularDrag = true});
        session.Track("knob");
        facet::Knob("Fixed", &fixed, 0.0f, 1.0f, {.readOnly = true});
        session.Track("fixed");
    });
    session.Frame();
    const ImRect knob = session.ItemRect("knob");
    const ImVec2 center(knob.GetCenter().x, knob.Min.y + knob.GetWidth() * 0.5f);
    // The pointer to the right of the centre is three quarters of the default 270 degree sweep:
    // the sweep starts at the bottom left and runs clockwise over the top.
    session.Drag({center, ImVec2(center.x + 60.0f, center.y)});
    CHECK(value == doctest::Approx(5.0 / 6.0).epsilon(0.02));
    session.Drag({center, ImVec2(center.x - 60.0f, center.y)});
    CHECK(value == doctest::Approx(1.0 / 6.0).epsilon(0.02));
    const ImRect f = session.ItemRect("fixed");
    session.Drag({f.GetCenter(), ImVec2(f.GetCenter().x, f.GetCenter().y - 80.0f)});
    CHECK(fixed == 0.5f);
}

TEST_CASE("A focused knob steps with Left and Right like Down and Up and keeps the focus")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float value = 0.5f;
    int commits = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::Knob("Gain", &value, 0.0f, 1.0f);
        if (facet::IsItemEditCommitted())
            ++commits;
        session.Track("knob");
        facet::Knob("Next", &value, 0.0f, 1.0f);
        session.Track("next");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("knob"));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(value == doctest::Approx(0.51f));
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(value == doctest::Approx(0.49f));
    session.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    CHECK(value == doctest::Approx(0.5f));
    session.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    CHECK(value == doctest::Approx(0.6f));
    CHECK(commits == 5);
    // The arrows never move the focus to the next knob.
    CHECK(ImGui::GetFocusID() == session.ItemId("knob"));
}
// ---- XYPad ---------------------------------------------------------------------------------

namespace
{

// The screen point of fractions (fx, fy) of a pad tracked as `name`, fy from the bottom: the knob
// centres travel in the frame inset by half a knob.
ImVec2 PadAt(const facet::test::Session& session, const char* name, double fx, double fy)
{
    const ImRect r = session.ItemRect(name);
    const float half = KnobSize() * 0.5f;
    const ImRect area(r.Min.x + half, r.Min.y + half, r.Max.x - half, r.Max.y - half);
    return ImVec2(area.Min.x + static_cast<float>(fx) * area.GetWidth(), area.Max.y - static_cast<float>(fy) * area.GetHeight());
}

} // namespace

TEST_CASE("An XY pad drags its point on both axes, steps with the arrows and restores with Escape")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 point(0.25f, 0.75f);
    int changes = 0;
    int commits = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::XYPad("Position", &point, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.width = 200.0f}))
            ++changes;
        if (facet::IsItemEditCommitted())
            ++commits;
        session.Track("pad");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    CHECK(session.ItemRect("pad").GetWidth() == doctest::Approx(200.0f));
    CHECK(session.ItemRect("pad").GetHeight() == doctest::Approx(200.0f));
    // A press on the point grabs it; a drag moves both axes.
    const ImVec2 at = PadAt(session, "pad", 0.25, 0.75);
    session.MouseMove(at);
    session.MouseDown();
    CHECK(point.x == 0.25f);
    CHECK(point.y == 0.75f);
    const float span = session.ItemRect("pad").GetWidth() - KnobSize();
    session.MouseMove(ImVec2(at.x + span * 0.5f, at.y + span * 0.25f));
    CHECK(point.x == doctest::Approx(0.75f).epsilon(0.01));
    CHECK(point.y == doctest::Approx(0.5f).epsilon(0.01));
    session.KeyPress(ImGuiKey_Escape);
    CHECK(point.x == 0.25f);
    CHECK(point.y == 0.75f);
    session.MouseUp();
    CHECK(commits == 0);
    // A click elsewhere moves the point there.
    session.Click(PadAt(session, "pad", 0.6, 0.2));
    CHECK(point.x == doctest::Approx(0.6f).epsilon(0.01));
    CHECK(point.y == doctest::Approx(0.2f).epsilon(0.01));
    CHECK(commits == 1);
    // One Tab stop; the arrows move both axes, Tab goes on.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("pad"));
    point = ImVec2(0.5f, 0.5f);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(point.x == doctest::Approx(0.51f));
    CHECK(point.y == doctest::Approx(0.51f));
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Ctrl);
    CHECK(point.y == doctest::Approx(0.5f));
    session.KeyPress(ImGuiKey_End);
    CHECK(point.x == 1.0f);
    session.KeyPress(ImGuiKey_PageDown);
    CHECK(point.y == doctest::Approx(0.4f));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    CHECK(changes > 0);
}

TEST_CASE("An XY pad snaps to its steps, flips y with yDown, and shows a value out of range clamped")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 grid(2.0f, 2.0f);
    ImVec2 outside(5.0f, -3.0f);
    session.SetUi([&] {
        facet::XYPad("Grid", &grid, ImVec2(0.0f, 0.0f), ImVec2(10.0f, 10.0f),
                     {.step = {1.0f, 2.0f}, .grid = true, .yDown = true, .width = 200.0f});
        session.Track("grid");
        facet::XYPad("Outside", &outside, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.width = 100.0f});
        session.Track("outside");
    });
    session.Frame();
    // yDown: fraction 0 of y is at the top, so a click near the top writes a small y.
    session.Click(PadAt(session, "grid", 0.33, 0.85));
    CHECK(grid.x == 3.0f);
    CHECK(grid.y == 2.0f);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(grid.y == 4.0f);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(grid.x == 4.0f);
    // Nothing is written until an edit.
    session.Frames(3);
    CHECK(outside.x == 5.0f);
    CHECK(outside.y == -3.0f);
}

TEST_CASE("An XY pad read-only, disabled or mixed: written only where it may be")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 fixed(0.5f, 0.5f);
    ImVec2 off(0.5f, 0.5f);
    ImVec2 mixed(0.5f, 0.5f);
    ImVec2 line(0.5f, 0.5f);
    bool mixedSet = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::XYPad("Fixed", &fixed, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.width = 100.0f, .readOnly = true});
        session.Track("fixed");
        ImGui::BeginDisabled();
        facet::XYPad("Off", &off, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.width = 100.0f});
        ImGui::EndDisabled();
        session.Track("off");
        if (facet::XYPad("Mixed", &mixed, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.width = 100.0f, .mixed = !mixedSet}))
            mixedSet = true;
        session.Track("mixed");
        // A y range of zero: the point moves along x only.
        facet::XYPad("Line", &line, ImVec2(0.0f, 0.5f), ImVec2(1.0f, 0.5f), {.width = 100.0f});
        session.Track("line");
    });
    session.Frame();
    session.Drag({session.ItemCenter("fixed"), session.ItemRect("fixed").Max});
    session.Drag({session.ItemCenter("off"), session.ItemRect("off").Max});
    CHECK(fixed.x == 0.5f);
    CHECK(off.x == 0.5f);
    // The read-only pad is a Tab stop, the disabled one is not.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("fixed"));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(fixed.x == 0.5f);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("mixed"));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(mixedSet);
    CHECK(mixed.x == doctest::Approx(0.51f));
    session.Click(PadAt(session, "line", 0.9, 0.9));
    CHECK(line.x == doctest::Approx(0.9f).epsilon(0.02));
    CHECK(line.y == 0.5f);
}

// ---- BoxRangePad ---------------------------------------------------------------------------

TEST_CASE("A box range pad drags corners on both axes and edges on one, keeping the gap per axis")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 low(0.2f, 0.2f);
    ImVec2 high(0.8f, 0.8f);
    int commits = 0;
    session.SetUi([&] {
        facet::BoxRangePad("Crop", &low, &high, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                           {.minGap = {0.1f, 0.2f}, .width = 200.0f});
        if (facet::IsItemEditCommitted())
            ++commits;
        session.Track("box");
    });
    session.Frame();
    // The top right corner (max.y is at the top) toward the lower left: both axes stop one gap short.
    session.Drag({PadAt(session, "box", 0.8, 0.8), PadAt(session, "box", 0.0, 0.0)});
    CHECK(high.x == doctest::Approx(0.3f));
    CHECK(high.y == doctest::Approx(0.4f));
    CHECK(low.x == 0.2f);
    CHECK(low.y == 0.2f);
    CHECK(commits == 1);
    // The left edge: x only, whatever the vertical movement.
    low = ImVec2(0.2f, 0.2f);
    high = ImVec2(0.8f, 0.8f);
    session.Frame();
    session.Drag({PadAt(session, "box", 0.2, 0.5), PadAt(session, "box", 0.5, 0.9)});
    CHECK(low.x == doctest::Approx(0.5f).epsilon(0.01));
    CHECK(low.y == 0.2f);
    CHECK(high.y == 0.8f);
    // A press outside the box moves the nearest handle there: the bottom edge, down.
    session.Click(PadAt(session, "box", 0.6, 0.05));
    CHECK(low.y == doctest::Approx(0.05f).epsilon(0.02));
    CHECK(low.x == doctest::Approx(0.5f).epsilon(0.01));
}

TEST_CASE("A box range pad body drags the whole box, keeping its size, and Escape restores it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 low(0.2f, 0.2f);
    ImVec2 high(0.6f, 0.5f);
    session.SetUi([&] {
        facet::BoxRangePad("Crop", &low, &high, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.width = 200.0f});
        session.Track("box");
    });
    session.Frame();
    session.Drag({PadAt(session, "box", 0.4, 0.35), PadAt(session, "box", 2.0, 2.0)});
    CHECK(high.x == doctest::Approx(1.0f));
    CHECK(high.y == doctest::Approx(1.0f));
    CHECK(high.x - low.x == doctest::Approx(0.4f));
    CHECK(high.y - low.y == doctest::Approx(0.3f));
    const ImVec2 at = PadAt(session, "box", 0.8, 0.85);
    session.MouseMove(at);
    session.MouseDown();
    session.MouseMove(ImVec2(at.x - 60.0f, at.y + 40.0f));
    CHECK(low.x < 0.6f);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(low.x == doctest::Approx(0.6f));
    CHECK(low.y == doctest::Approx(0.7f));
    CHECK(high.x == doctest::Approx(1.0f));
    CHECK(high.y == doctest::Approx(1.0f));
    session.MouseUp();
}

TEST_CASE("A box range pad has eight Tab stops, the arrows move the focused handle and Push moves the other side")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 low(0.2f, 0.2f);
    ImVec2 high(0.8f, 0.8f);
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::BoxRangePad("Crop", &low, &high, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                           {.minGap = {0.1f, 0.1f}, .collision = facet::HandleCollision::Push, .width = 200.0f});
        session.Track("box");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    const ImGuiID widget = session.ItemId("box");
    // The id of part n under the widget id, as ImGui::GetID(n) inside PushOverrideID(widget).
    const auto handleId = [&](int part) { return ImHashData(&part, sizeof(part), widget); };
    // A click on an edge focuses that edge.
    session.Click(PadAt(session, "box", 0.2, 0.5));
    CHECK(ImGui::GetFocusID() == handleId(0));
    session.Click(PadAt(session, "box", 0.5, 0.8));
    CHECK(ImGui::GetFocusID() == handleId(1));
    low = ImVec2(0.2f, 0.2f);
    high = ImVec2(0.8f, 0.8f);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    for (int part = 0; part < 8; ++part)
    {
        CAPTURE(part);
        session.KeyPress(ImGuiKey_Tab);
        CHECK(ImGui::GetFocusID() == handleId(part));
    }
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    // Back to the left edge: Right moves low.x, Up is left to navigation for an edge of x.
    for (int i = 0; i < 8; ++i)
        session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    REQUIRE(ImGui::GetFocusID() == handleId(0));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(low.x == doctest::Approx(0.21f));
    // End sends the left edge right, pushing the right edge to the end one gap later.
    session.KeyPress(ImGuiKey_End);
    CHECK(low.x == doctest::Approx(0.9f));
    CHECK(high.x == 1.0f);
    // The top edge (part 1) moves y high; Down pushes nothing until the gap.
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == handleId(1));
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(high.y == doctest::Approx(0.79f));
    // A corner (part 6, bottom right) moves both axes.
    for (int i = 0; i < 5; ++i)
        session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == handleId(6));
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(low.y == doctest::Approx(0.19f));
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(high.x == doctest::Approx(0.99f));
}

TEST_CASE("A box range pad read-only, disabled or mixed: written only where it may be")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 fixedLow(0.2f, 0.2f), fixedHigh(0.8f, 0.8f);
    ImVec2 offLow(0.2f, 0.2f), offHigh(0.8f, 0.8f);
    ImVec2 mixLow(0.2f, 0.2f), mixHigh(0.8f, 0.8f);
    ImVec2 outLow(-1.0f, 0.2f), outHigh(2.0f, 0.8f);
    bool mixedSet = false;
    session.SetUi([&] {
        facet::BoxRangePad("Fixed", &fixedLow, &fixedHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                           {.width = 100.0f, .readOnly = true});
        session.Track("fixed");
        ImGui::BeginDisabled();
        facet::BoxRangePad("Off", &offLow, &offHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.width = 100.0f});
        ImGui::EndDisabled();
        session.Track("off");
        if (facet::BoxRangePad("Mixed", &mixLow, &mixHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                               {.width = 100.0f, .mixed = !mixedSet}))
            mixedSet = true;
        session.Track("mixed");
        facet::BoxRangePad("Out", &outLow, &outHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.width = 100.0f});
        session.Track("out");
    });
    session.Frame();
    session.Drag({PadAt(session, "fixed", 0.8, 0.8), PadAt(session, "fixed", 0.5, 0.5)});
    session.Drag({PadAt(session, "off", 0.8, 0.8), PadAt(session, "off", 0.5, 0.5)});
    CHECK(fixedHigh.x == 0.8f);
    CHECK(offHigh.x == 0.8f);
    session.Drag({PadAt(session, "mixed", 0.8, 0.8), PadAt(session, "mixed", 0.6, 0.6)});
    CHECK(mixedSet);
    CHECK(mixHigh.x < 0.8f);
    // Values out of range and out of order are drawn repaired and written only when edited.
    session.Frames(3);
    CHECK(outLow.x == -1.0f);
    CHECK(outHigh.x == 2.0f);
    // The right edge of the repaired box (at max.x) moves left: only high.x is written.
    session.Drag({PadAt(session, "out", 1.0, 0.5), PadAt(session, "out", 0.7, 0.5)});
    CHECK(outHigh.x == doctest::Approx(0.7f).epsilon(0.02));
    CHECK(outLow.x == -1.0f);
    CHECK(outLow.y == 0.2f);
}

// ---- AllocationBar -------------------------------------------------------------------------

namespace
{

// A point on the track row of an allocation bar without a visible label, at share fraction f.
ImVec2 BarAt(const facet::test::Session& session, const char* name, double f)
{
    const ImRect r = session.ItemRect(name);
    const float half = KnobSize() * 0.5f;
    const float row = facet::ControlHeight(facet::ControlSize::Regular);
    return ImVec2(r.Min.x + half + static_cast<float>(f) * (r.GetWidth() - KnobSize()), r.Min.y + row * 0.5f);
}

float ShareSum(const float* shares, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += shares[i];
    return static_cast<float>(sum);
}

} // namespace

TEST_CASE("An allocation bar divider moves share between its neighbours only and keeps the total and the minimum")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float shares[3] = {0.5f, 0.3f, 0.2f};
    static const char* const kParts[] = {"A", "B", "C"};
    int commits = 0;
    session.SetUi([&] {
        facet::AllocationBar("##bar", shares, kParts, {.minGap = 0.1, .width = 400.0f});
        if (facet::IsItemEditCommitted())
            ++commits;
        session.Track("bar");
    });
    session.Frame();
    session.Drag({BarAt(session, "bar", 0.5), BarAt(session, "bar", 1.0)});
    CHECK(shares[0] == doctest::Approx(0.7f));
    CHECK(shares[1] == doctest::Approx(0.1f));
    CHECK(shares[2] == doctest::Approx(0.2f));
    CHECK(ShareSum(shares, 3) == 1.0f);
    CHECK(commits == 1);
    // The second divider toward the end stops one minimum short of it.
    session.Drag({BarAt(session, "bar", 0.8), BarAt(session, "bar", 1.2)});
    CHECK(shares[0] == doctest::Approx(0.7f));
    CHECK(shares[1] == doctest::Approx(0.2f));
    CHECK(shares[2] == doctest::Approx(0.1f));
    CHECK(ShareSum(shares, 3) == 1.0f);
}

TEST_CASE("An allocation bar steps the focused divider with the keys and Push moves the dividers beyond it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float shares[3] = {0.5f, 0.3f, 0.2f};
    static const char* const kParts[] = {"A", "B", "C"};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::AllocationBar("##bar", shares, kParts,
                             {.step = 0.01, .minGap = 0.1, .collision = facet::HandleCollision::Push, .width = 400.0f});
        session.Track("bar");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(shares[0] == doctest::Approx(0.51f));
    CHECK(shares[1] == doctest::Approx(0.29f));
    CHECK(shares[2] == doctest::Approx(0.2f));
    session.KeyPress(ImGuiKey_End);
    CHECK(shares[0] == doctest::Approx(0.8f));
    CHECK(shares[1] == doctest::Approx(0.1f));
    CHECK(shares[2] == doctest::Approx(0.1f));
    CHECK(ShareSum(shares, 3) == 1.0f);
    session.KeyPress(ImGuiKey_Home);
    CHECK(shares[0] == doctest::Approx(0.1f));
    CHECK(shares[1] == doctest::Approx(0.8f));
    // The second divider is the next Tab stop.
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(shares[1] == doctest::Approx(0.79f));
    CHECK(shares[2] == doctest::Approx(0.11f));
}

TEST_CASE("An allocation bar types the share of a segment from a divider or a click on its caption")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float shares[3] = {0.5f, 0.3f, 0.2f};
    static const char* const kParts[] = {"A", "B", "C"};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::AllocationBar("##bar", shares, kParts, {.minGap = 0.05, .width = 400.0f});
        session.Track("bar");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    session.Type("40");
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(shares[0] == doctest::Approx(0.4f));
    CHECK(shares[1] == doctest::Approx(0.4f));
    CHECK(shares[2] == doctest::Approx(0.2f));
    // A click on the caption of the last segment types its share; the divider before it moves.
    const ImRect r = session.ItemRect("bar");
    session.Click(ImVec2(BarAt(session, "bar", 0.9).x, r.Max.y - 4.0f));
    session.Frames(2);
    session.Type("50");
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(shares[2] == doctest::Approx(0.5f));
    CHECK(shares[1] == doctest::Approx(0.1f));
    CHECK(ShareSum(shares, 3) == 1.0f);
}

TEST_CASE("An allocation bar draws shares that do not add up normalized and writes them only on an edit")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float shares[3] = {2.0f, 1.0f, 1.0f};
    float fixed[2] = {0.25f, 0.75f};
    float single[1] = {1.0f};
    static const char* const kParts[] = {"A", "B", "C"};
    session.SetUi([&] {
        facet::AllocationBar("##bar", shares, kParts, {.width = 400.0f});
        session.Track("bar");
        facet::AllocationBar("##fixed", fixed, kParts, {.width = 400.0f, .readOnly = true});
        session.Track("fixed");
        facet::AllocationBar("##single", single, kParts, {.width = 400.0f});
        session.Track("single");
    });
    session.Frames(3);
    CHECK(shares[0] == 2.0f);
    // The first divider sits at 0.5 of the normalized total; a drag from there writes all shares.
    session.Drag({BarAt(session, "bar", 0.5), BarAt(session, "bar", 0.6)});
    CHECK(shares[0] == doctest::Approx(0.6f).epsilon(0.02));
    CHECK(shares[1] == doctest::Approx(0.15f).epsilon(0.05));
    CHECK(shares[2] == doctest::Approx(0.25f));
    CHECK(ShareSum(shares, 3) == 1.0f);
    session.Drag({BarAt(session, "fixed", 0.25), BarAt(session, "fixed", 0.6)});
    CHECK(fixed[0] == 0.25f);
    session.Drag({BarAt(session, "single", 0.5), BarAt(session, "single", 0.9)});
    CHECK(single[0] == 1.0f);
}

// ---- ArcRangeSlider ------------------------------------------------------------------------

namespace
{

// The point at fraction f of the arc of a 200 unit arc range tracked as `name`, `sweep` degrees
// starting at the bottom (open in the middle of the bottom for less than a full turn).
ImVec2 ArcAt(const facet::test::Session& session, const char* name, double f, float sweep = 270.0f, float radiusShift = 0.0f)
{
    const ImRect r = session.ItemRect(name);
    const float diameter = 200.0f;
    const ImVec2 center(r.Min.x + diameter * 0.5f, r.Min.y + diameter * 0.5f);
    const float radius = diameter * 0.5f - KnobSize() * 0.5f + radiusShift;
    const float pi = 3.14159265358979f;
    const float sweepRad = sweep * pi / 180.0f;
    const float start = pi * 0.5f + (2.0f * pi - sweepRad) * 0.5f;
    const float angle = start + sweepRad * static_cast<float>(f);
    return ImVec2(center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius);
}

} // namespace

TEST_CASE("An arc range has two Tab stops; all four arrows step, the gap holds and Push turns the other handle")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float low = 20.0f;
    float high = 60.0f;
    float pushLow = 20.0f;
    float pushHigh = 60.0f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::ArcRangeSlider("Band", &low, &high, 0.0f, 100.0f, {.minGap = 10.0, .diameter = 200.0f});
        session.Track("arc");
        facet::ArcRangeSlider("Push", &pushLow, &pushHigh, 0.0f, 100.0f,
                              {.minGap = 10.0, .collision = facet::HandleCollision::Push, .diameter = 200.0f});
        session.Track("push");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    const ImGuiID widget = session.ItemId("arc");
    const auto handleId = [&](ImGuiID w, int part) { return ImHashData(&part, sizeof(part), w); };
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == handleId(widget, 0));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(low == doctest::Approx(21.0f));
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(low == doctest::Approx(22.0f));
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(low == doctest::Approx(20.0f));
    session.KeyPress(ImGuiKey_End);
    CHECK(low == doctest::Approx(50.0f));
    CHECK(ImGui::GetFocusID() == handleId(widget, 0));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == handleId(widget, 1));
    session.KeyPress(ImGuiKey_Home);
    CHECK(high == doctest::Approx(60.0f));
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == handleId(session.ItemId("push"), 0));
    session.KeyPress(ImGuiKey_End);
    CHECK(pushLow == doctest::Approx(90.0f));
    CHECK(pushHigh == doctest::Approx(100.0f));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("A wrapping arc range runs through max to min and its handles never pass each other")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float from = 300.0f;
    float to = 60.0f;
    float pushFrom = 50.0f;
    float pushTo = 60.0f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::ArcRangeSlider("Sector", &from, &to, 0.0f, 360.0f,
                              {.step = 5.0, .minGap = 10.0, .wrap = true, .sweep = 360.0f, .diameter = 200.0f});
        session.Track("arc");
        facet::ArcRangeSlider("Push", &pushFrom, &pushTo, 0.0f, 360.0f,
                              {.minGap = 10.0, .collision = facet::HandleCollision::Push, .wrap = true, .sweep = 360.0f,
                               .diameter = 200.0f});
        session.Track("push");
    });
    session.Frame();
    // A value out of order is drawn as the band through max: nothing is written.
    session.Frames(2);
    CHECK(from == 300.0f);
    CHECK(to == 60.0f);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    // The high handle steps past max and on from min.
    session.KeyPress(ImGuiKey_Tab);
    for (int i = 0; i < 3; ++i)
        session.KeyPress(ImGuiKey_DownArrow);
    CHECK(to == doctest::Approx(45.0f));
    // Moving the high handle back toward the low one stops one gap after it, through min.
    session.HoldKey(ImGuiKey_DownArrow, 3.0f);
    CHECK(to == doctest::Approx(310.0f));
    CHECK(from == 300.0f);
    // Push: the low handle turning into the high one carries it along.
    session.KeyPress(ImGuiKey_Tab);
    for (int i = 0; i < 4; ++i)
        session.KeyPress(ImGuiKey_RightArrow);
    // Continuous keys move on the grid of hundredths of the track: 50.4, 54, 57.6, 61.2.
    CHECK(pushFrom == doctest::Approx(61.2f));
    CHECK(pushTo == doctest::Approx(pushFrom + 10.0f));
}

TEST_CASE("An arc range follows the pointer angle, the band turns both handles, Escape restores")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float low = 25.0f;
    float high = 50.0f;
    int commits = 0;
    session.SetUi([&] {
        facet::ArcRangeSlider("Band", &low, &high, 0.0f, 100.0f, {.diameter = 200.0f});
        if (facet::IsItemEditCommitted())
            ++commits;
        session.Track("arc");
    });
    session.Frame();
    // The high handle at half of the sweep (the top) to three quarters.
    session.Drag({ArcAt(session, "arc", 0.5), ArcAt(session, "arc", 0.62), ArcAt(session, "arc", 0.75)});
    CHECK(high == doctest::Approx(75.0f).epsilon(0.02));
    CHECK(low == 25.0f);
    CHECK(commits == 1);
    // The band between them turns both by the same amount.
    const float bandBefore = high - low;
    session.Drag({ArcAt(session, "arc", 0.5), ArcAt(session, "arc", 0.6)});
    CHECK(high - low == doctest::Approx(bandBefore));
    CHECK(low == doctest::Approx(35.0f).epsilon(0.03));
    // A press on the ring away from both handles moves the nearer one there.
    session.Click(ArcAt(session, "arc", 0.1));
    CHECK(low == doctest::Approx(10.0f).epsilon(0.05));
    // Escape during a drag restores both values.
    const float before = high;
    const ImVec2 at = ArcAt(session, "arc", high / 100.0f);
    session.MouseMove(at);
    session.MouseDown();
    session.MouseMove(ArcAt(session, "arc", 0.95));
    CHECK(high != before);
    session.KeyPress(ImGuiKey_Escape);
    CHECK(high == before);
    session.MouseUp();
}

TEST_CASE("An arc range at a small size keeps its parts inside, and read-only, disabled or mixed write only where they may")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float low = 0.2f, high = 0.6f;
    float fixedLow = 0.2f, fixedHigh = 0.6f;
    float offLow = 0.2f, offHigh = 0.6f;
    float mixLow = 0.2f, mixHigh = 0.6f;
    bool mixedSet = false;
    session.SetUi([&] {
        facet::ArcRangeSlider("Tiny", &low, &high, 0.0f, 1.0f, {.format = "%.2f", .diameter = 24.0f});
        session.Track("tiny");
        facet::ArcRangeSlider("Fixed", &fixedLow, &fixedHigh, 0.0f, 1.0f, {.diameter = 200.0f, .readOnly = true});
        session.Track("fixed");
        ImGui::BeginDisabled();
        facet::ArcRangeSlider("Off", &offLow, &offHigh, 0.0f, 1.0f, {.diameter = 200.0f});
        ImGui::EndDisabled();
        session.Track("off");
        if (facet::ArcRangeSlider("Mixed", &mixLow, &mixHigh, 0.0f, 1.0f, {.diameter = 200.0f, .mixed = !mixedSet}))
            mixedSet = true;
        session.Track("mixed");
    });
    session.Frame();
    // The tiny dial puts its values under it: the widget is taller than the dial and its label.
    const ImRect tiny = session.ItemRect("tiny");
    CHECK(tiny.GetWidth() >= 24.0f);
    CHECK(tiny.GetHeight() > 24.0f + facet::T().spacing.xs);
    session.Drag({ArcAt(session, "fixed", 0.6), ArcAt(session, "fixed", 0.9)});
    session.Drag({ArcAt(session, "off", 0.6), ArcAt(session, "off", 0.9)});
    CHECK(fixedHigh == 0.6f);
    CHECK(offHigh == 0.6f);
    session.Drag({ArcAt(session, "mixed", 0.6), ArcAt(session, "mixed", 0.8)});
    CHECK(mixedSet);
    CHECK(mixHigh > 0.7f);
}

// ---- JoystickPad ---------------------------------------------------------------------------

namespace
{

// The point of stick deflection (x, y), y up, on a 200 unit joystick tracked as `name`.
ImVec2 StickAt(const facet::test::Session& session, const char* name, float x, float y)
{
    const ImRect r = session.ItemRect(name);
    const float travel = (200.0f - KnobSize()) * 0.5f;
    return ImVec2(r.Min.x + 100.0f + x * travel, r.Min.y + 100.0f - y * travel);
}

// Drags a 200 unit joystick from its centre to the right edge and releases it, in a session with
// time step `dt`; returns the drawn stick offset after `seconds` more.
float StickAfterRelease(float dt, float seconds)
{
    facet::test::Session session({.deltaTime = dt});
    REQUIRE(session.Ready());
    ImVec2 value(0.0f, 0.0f);
    ImGuiID id = 0;
    session.SetUi([&] {
        facet::JoystickPad("Stick", &value, {.diameter = 200.0f});
        id = ImGui::GetItemID();
        session.Track("stick");
    });
    session.Frame();
    session.MouseMove(StickAt(session, "stick", 0.0f, 0.0f));
    session.MouseDown();
    session.MouseMove(StickAt(session, "stick", 1.0f, 0.0f));
    REQUIRE(value.x == doctest::Approx(1.0f));
    // The release frame is the first step of the return: `seconds` in all.
    session.MouseUp();
    CHECK(value.x == 0.0f);
    const int frames = static_cast<int>(std::lround(seconds / dt)) - 1;
    if (frames > 0)
        session.Frames(frames);
    return facet::internal::JoystickPadStickOffset(id).x;
}

} // namespace

TEST_CASE("A joystick follows a drag inside the unit circle and reports zero after the release")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 value(0.0f, 0.0f);
    int changes = 0;
    int commits = 0;
    session.SetUi([&] {
        if (facet::JoystickPad("Stick", &value, {.diameter = 200.0f}))
            ++changes;
        if (facet::IsItemEditCommitted())
            ++commits;
        session.Track("stick");
    });
    session.Frame();
    session.MouseMove(StickAt(session, "stick", 0.0f, 0.0f));
    session.MouseDown();
    CHECK(value.x == 0.0f);
    session.MouseMove(StickAt(session, "stick", 0.5f, 0.25f));
    CHECK(value.x == doctest::Approx(0.5f).epsilon(0.02));
    CHECK(value.y == doctest::Approx(0.25f).epsilon(0.02));
    // Far out to the upper right: on the circle, in the pointer's direction.
    session.MouseMove(StickAt(session, "stick", 3.0f, 3.0f));
    CHECK(value.x == doctest::Approx(std::sqrt(0.5f)).epsilon(0.01));
    CHECK(value.y == doctest::Approx(std::sqrt(0.5f)).epsilon(0.01));
    const int before = changes;
    session.MouseUp();
    CHECK(value.x == 0.0f);
    CHECK(value.y == 0.0f);
    CHECK(changes == before + 1);
    CHECK(commits == 1);
    // At rest nothing more is written.
    session.Frames(30);
    CHECK(changes == before + 1);
}

TEST_CASE("A joystick springs back the same way at any frame rate and at once with reduced motion")
{
    const float slow = StickAfterRelease(1.0f / 30.0f, 0.1f);
    const float fast = StickAfterRelease(1.0f / 240.0f, 0.1f);
    CHECK(slow > 0.0f);
    CHECK(slow < 1.0f);
    CHECK(slow == doctest::Approx(fast).epsilon(0.01));
    CHECK(StickAfterRelease(1.0f / 60.0f, 3.0f) == 0.0f);

    const facet::Theme saved = facet::T();
    facet::Theme reduced = saved;
    reduced.motion.reduceMotion = true;
    facet::SetTheme(reduced, false);
    CHECK(StickAfterRelease(1.0f / 60.0f, 0.0f) == 0.0f);
    facet::SetTheme(saved, false);
}

TEST_CASE("A joystick's arrows hold it at the edge while down, and step it without springReturn")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 spring(0.0f, 0.0f);
    ImVec2 free(0.0f, 0.0f);
    ImVec2 square(0.0f, 0.0f);
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::JoystickPad("Spring", &spring, {.diameter = 120.0f});
        session.Track("spring");
        facet::JoystickPad("Free", &free, {.springReturn = false, .diameter = 120.0f});
        session.Track("free");
        facet::JoystickPad("Square", &square, {.circular = false, .diameter = 120.0f});
        session.Track("square");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("spring"));
    session.KeyDown(ImGuiKey_RightArrow);
    CHECK(spring.x == 1.0f);
    CHECK(spring.y == 0.0f);
    session.KeyDown(ImGuiKey_UpArrow);
    CHECK(spring.x == doctest::Approx(std::sqrt(0.5f)));
    CHECK(spring.y == doctest::Approx(std::sqrt(0.5f)));
    session.KeyUp(ImGuiKey_UpArrow);
    session.KeyUp(ImGuiKey_RightArrow);
    session.Frame();
    CHECK(spring.x == 0.0f);
    CHECK(spring.y == 0.0f);
    session.KeyDown(ImGuiKey_LeftArrow | ImGuiMod_Shift);
    CHECK(spring.x == doctest::Approx(-0.5f));
    session.KeyUp(ImGuiKey_LeftArrow | ImGuiMod_Shift);
    session.Frame();
    CHECK(spring.x == 0.0f);
    // Without springReturn the arrows step a hundredth of the range and the stick stays.
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("free"));
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    session.Frames(10);
    CHECK(free.x == doctest::Approx(0.02f));
    CHECK(free.y == doctest::Approx(0.02f));
    // A square stick holds the diagonal in the corner.
    session.KeyPress(ImGuiKey_Tab);
    session.KeyDown(ImGuiKey_RightArrow);
    session.KeyDown(ImGuiKey_DownArrow);
    CHECK(square.x == 1.0f);
    CHECK(square.y == -1.0f);
    session.KeyUp(ImGuiKey_DownArrow);
    session.KeyUp(ImGuiKey_RightArrow);
}

TEST_CASE("A joystick's deadzone keeps small deflections at zero and the value continuous past it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 value(0.0f, 0.0f);
    ImVec2 fixed(0.5f, 0.0f);
    ImVec2 off(0.0f, 0.5f);
    session.SetUi([&] {
        facet::JoystickPad("Stick", &value, {.deadzone = 0.2f, .springReturn = false, .showDeadzone = true, .diameter = 200.0f});
        session.Track("stick");
        facet::JoystickPad("Fixed", &fixed, {.diameter = 120.0f, .readOnly = true});
        session.Track("fixed");
        ImGui::BeginDisabled();
        facet::JoystickPad("Off", &off, {.diameter = 120.0f});
        ImGui::EndDisabled();
        session.Track("off");
    });
    session.Frame();
    session.Drag({StickAt(session, "stick", 0.0f, 0.0f), StickAt(session, "stick", 0.1f, 0.0f)});
    CHECK(value.x == 0.0f);
    session.Drag({StickAt(session, "stick", 0.1f, 0.0f), StickAt(session, "stick", 0.6f, 0.0f)});
    CHECK(value.x == doctest::Approx(0.5f).epsilon(0.02));
    CHECK(value.y == 0.0f);
    const ImRect f = session.ItemRect("fixed");
    session.Drag({f.GetCenter(), f.Max});
    CHECK(fixed.x == 0.5f);
    const ImRect o = session.ItemRect("off");
    session.Drag({o.GetCenter(), o.Max});
    CHECK(off.y == 0.5f);
}

// ---- RangeSliderHistogram ------------------------------------------------------------------

TEST_CASE("A histogram range stands on its bars and its handles follow the range rules")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const float kBins[] = {1.0f, 3.0f, 0.0f, 2.0f};
    int low = 20, high = 60;
    float emptyLow = 0.2f, emptyHigh = 0.4f;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::RangeSliderHistogram("Spread", &low, &high, 0, 100, kBins, {.minGap = 10.0, .width = 400.0f});
        session.Track("histogram");
        facet::RangeSliderHistogram("Empty", &emptyLow, &emptyHigh, 0.0f, 1.0f, {}, {.width = 400.0f});
        session.Track("empty");
    });
    session.Frame();
    const float row = facet::ControlHeight(facet::ControlSize::Regular);
    const float barsHeight = std::round(facet::Px(facet::T().metrics.histogramHeight));
    CHECK(session.ItemRect("histogram").GetHeight() == doctest::Approx(row + barsHeight));
    CHECK(session.ItemRect("empty").GetHeight() == doctest::Approx(row + barsHeight));
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_End);
    CHECK(low == 50);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_End);
    CHECK(high == 100);
    // A drag on the bars does not move the handles: only the track row takes the mouse.
    const ImRect rect = session.ItemRect("histogram");
    session.Drag({ImVec2(rect.GetCenter().x, rect.Min.y + 4.0f), ImVec2(rect.Max.x - 60.0f, rect.Min.y + 4.0f)});
    CHECK(low == 50);
    CHECK(high == 100);
}