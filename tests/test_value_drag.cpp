// Value drag core (src/internal/value_drag.h): mapping, keyboard steps, drag accumulation and
// the input behaviour with its edit sessions.

#include <cmath>
#include <limits>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/item.h"
#include "internal/value_drag.h"
#include "support/session.h"

using facet::internal::DragTracker;
using facet::internal::FractionToValue;
using facet::internal::ItemOptions;
using facet::internal::KeyboardStep;
using facet::internal::NormalizeMapping;
using facet::internal::PageStep;
using facet::internal::PressMode;
using facet::internal::SnapValue;
using facet::internal::StepSize;
using facet::internal::ValueMapping;
using facet::internal::ValueToFraction;

namespace
{

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// A bare value control built from the item behaviour and the value drag core, as the sliders
// of Phase 2 will be: a 200 x 24 pixel track whose knob centre runs from its left to its right
// edge.
struct TestSlider
{
    double value = 0.5;
    facet::internal::ValueDragOptions options;
    bool disabled = false;
    int changes = 0;
    int commits = 0;
    int cancels = 0;
    int activations = 0;
    int afterEdit = 0;

    TestSlider() { options.grabRadius = 10.0f; }

    bool leadingButton = false;  // a stock button before the slider takes the window's first focus
    std::vector<int> cancelFrames;

    void Ui(facet::test::Session& session)
    {
        if (leadingButton)
            ImGui::Button("first");
        if (disabled)
            ImGui::BeginDisabled();
        ItemOptions item;
        item.press = PressMode::Click;
        const facet::internal::ItemState state = facet::internal::ItemBehavior(ImGui::GetID("slider"), ImVec2(200.0f, 24.0f), item);
        const facet::internal::ValueDragResult result = facet::internal::ValueDragBehavior(state, &value, state.rect, options);
        changes += result.changed ? 1 : 0;
        commits += facet::IsItemEditCommitted() ? 1 : 0;
        if (facet::IsItemEditCanceled())
        {
            ++cancels;
            cancelFrames.push_back(ImGui::GetFrameCount());
        }
        activations += ImGui::IsItemActivated() ? 1 : 0;
        afterEdit += ImGui::IsItemDeactivatedAfterEdit() ? 1 : 0;
        session.Track("slider");
        if (disabled)
            ImGui::EndDisabled();
    }

    // The screen point at `fraction` of the track, on its centre line.
    static ImVec2 At(const facet::test::Session& session, double fraction)
    {
        const ImRect r = session.ItemRect("slider");
        return ImVec2(r.Min.x + static_cast<float>(fraction) * r.GetWidth(), r.GetCenter().y);
    }
};

void Install(facet::test::Session& session, TestSlider& slider)
{
    session.SetUi([&] { slider.Ui(session); });
    session.Frames(2);
}

} // namespace

// ---- Mapping ----------------------------------------------------------------------------------

TEST_CASE("Value mapping: linear fractions with exact ends")
{
    const ValueMapping m{.min = -2.0, .max = 6.0};
    CHECK(ValueToFraction(m, -2.0) == 0.0);
    CHECK(ValueToFraction(m, 6.0) == 1.0);
    CHECK(ValueToFraction(m, 2.0) == doctest::Approx(0.5));
    CHECK(ValueToFraction(m, 100.0) == 1.0);  // clamped
    CHECK(ValueToFraction(m, kNaN) == 0.0);
    CHECK(FractionToValue(m, 0.0) == -2.0);
    CHECK(FractionToValue(m, 1.0) == 6.0);
    CHECK(FractionToValue(m, 0.25) == doctest::Approx(0.0));
    CHECK(FractionToValue(m, -3.0) == -2.0);
    CHECK(FractionToValue(m, kNaN) == -2.0);
}

TEST_CASE("Value mapping: invalid ranges are repaired")
{
    const ValueMapping swapped = NormalizeMapping({.min = 5.0, .max = 1.0});
    CHECK(swapped.min == 1.0);
    CHECK(swapped.max == 5.0);
    const ValueMapping nanMin = NormalizeMapping({.min = kNaN, .max = 3.0});
    CHECK(nanMin.min == 3.0);
    CHECK(NormalizeMapping({.step = -1.0}).step == 0.0);
    CHECK(NormalizeMapping({.step = kNaN}).step == 0.0);
    const ValueMapping integer = NormalizeMapping({.min = 0.4, .max = 9.6, .step = 2.4, .integer = true});
    CHECK(integer.min == 0.0);
    CHECK(integer.max == 10.0);
    CHECK(integer.step == 2.0);
    CHECK(NormalizeMapping({.integer = true}).step == 1.0);
    // An empty range maps everything to its one value.
    const ValueMapping empty{.min = 3.0, .max = 3.0};
    CHECK(ValueToFraction(empty, 3.0) == 0.0);
    CHECK(FractionToValue(empty, 0.7) == 3.0);
}

TEST_CASE("Value mapping: logarithmic, and Linear when min is not above zero")
{
    const ValueMapping log{.min = 10.0, .max = 10000.0, .scale = facet::ValueScale::Logarithmic};
    CHECK(ValueToFraction(log, 100.0) == doctest::Approx(1.0 / 3.0));
    CHECK(ValueToFraction(log, 1000.0) == doctest::Approx(2.0 / 3.0));
    CHECK(FractionToValue(log, 0.5) == doctest::Approx(std::sqrt(10.0 * 10000.0)));
    CHECK(FractionToValue(log, 0.0) == 10.0);
    CHECK(FractionToValue(log, 1.0) == 10000.0);
    // Values at or below zero are clamped to min.
    CHECK(ValueToFraction(log, 0.0) == 0.0);
    CHECK(ValueToFraction(log, -5.0) == 0.0);
    // Monotonic over the whole track.
    double previous = 0.0;
    for (int i = 0; i <= 1000; ++i)
    {
        const double v = FractionToValue(log, i / 1000.0);
        CHECK(v >= previous);
        previous = v;
    }
    // min <= 0 falls back to Linear (common.h).
    const ValueMapping zero{.min = 0.0, .max = 100.0, .scale = facet::ValueScale::Logarithmic};
    CHECK(NormalizeMapping(zero).scale == facet::ValueScale::Linear);
    CHECK(ValueToFraction(zero, 50.0) == doctest::Approx(0.5));
    const ValueMapping negative{.min = -10.0, .max = 10.0, .scale = facet::ValueScale::Logarithmic};
    CHECK(FractionToValue(negative, 0.5) == doctest::Approx(0.0));
}

TEST_CASE("Value mapping: steps land exactly on the grid and max stays reachable")
{
    const ValueMapping m{.min = 0.0, .max = 10.0, .step = 3.0};
    CHECK(SnapValue(m, 4.0) == 3.0);
    CHECK(SnapValue(m, 4.6) == 6.0);
    CHECK(SnapValue(m, 9.4) == 9.0);   // nearer the last grid point
    CHECK(SnapValue(m, 9.6) == 10.0);  // nearer max, which is not on the grid
    CHECK(SnapValue(m, 42.0) == 10.0);
    CHECK(SnapValue(m, kNaN) == 0.0);
    CHECK(FractionToValue(m, 1.0) == 10.0);
    CHECK(FractionToValue(m, 0.49) == 6.0);
    // Integers round to whole numbers.
    const ValueMapping integer{.min = 0.0, .max = 100.0, .integer = true};
    CHECK(SnapValue(integer, 41.5) == 42.0);
    CHECK(FractionToValue(integer, 0.333) == 33.0);
    // Snap stops pull a drag within their radius.
    const double stops[] = {0.0, 5.0};
    const ValueMapping withStops{.min = -10.0, .max = 10.0, .stops = stops};
    CHECK(facet::internal::ApplySnapStops(withStops, 0.3, 0.515, 0.02) == 0.0);
    CHECK(facet::internal::ApplySnapStops(withStops, 1.0, 0.55, 0.02) == 1.0);
}

TEST_CASE("Keyboard steps: a hundred presses cross the track exactly")
{
    const ValueMapping m{.min = 0.0, .max = 1.0};
    double v = 0.0;
    for (int i = 0; i < 100; ++i)
        v = KeyboardStep(m, v, 1, StepSize::Normal);
    CHECK(v == 1.0);
    for (int i = 0; i < 100; ++i)
        v = KeyboardStep(m, v, -1, StepSize::Normal);
    CHECK(v == 0.0);
    // Count presses at once, fine and coarse sizes.
    CHECK(KeyboardStep(m, 0.0, 1, StepSize::Normal, 25) == doctest::Approx(0.25));
    CHECK(KeyboardStep(m, 0.5, 1, StepSize::Fine) == doctest::Approx(0.501));
    CHECK(KeyboardStep(m, 0.5, 1, StepSize::Coarse) == doctest::Approx(0.6));
    // Between grid points: first to the next one in the direction of travel.
    CHECK(KeyboardStep(m, 0.505, 1, StepSize::Normal) == doctest::Approx(0.51));
    CHECK(KeyboardStep(m, 0.505, -1, StepSize::Normal) == doctest::Approx(0.50));
    // Clamped at the ends.
    CHECK(KeyboardStep(m, 1.0, 1, StepSize::Coarse, 3) == 1.0);
    CHECK(KeyboardStep(m, 0.0, -1, StepSize::Normal) == 0.0);
    // Logarithmic tracks step evenly in fraction space.
    const ValueMapping log{.min = 1.0, .max = 1000.0, .scale = facet::ValueScale::Logarithmic};
    CHECK(KeyboardStep(log, 1.0, 1, StepSize::Coarse) == doctest::Approx(std::pow(1000.0, 0.1)));
    double lv = 1.0;
    for (int i = 0; i < 10; ++i)
        lv = KeyboardStep(log, lv, 1, StepSize::Coarse);
    CHECK(lv == 1000.0);
}

TEST_CASE("Keyboard steps: stepped and integer values move by whole steps")
{
    const ValueMapping m{.min = 0.0, .max = 10.0, .step = 3.0};
    CHECK(KeyboardStep(m, 0.0, 1, StepSize::Normal) == 3.0);
    CHECK(KeyboardStep(m, 0.0, 1, StepSize::Fine) == 3.0);  // never finer than a step
    CHECK(KeyboardStep(m, 9.0, 1, StepSize::Normal) == 10.0);  // max is the last stop
    CHECK(KeyboardStep(m, 10.0, -1, StepSize::Normal) == 9.0);
    CHECK(KeyboardStep(m, 4.0, 1, StepSize::Normal) == 6.0);
    CHECK(KeyboardStep(m, 4.0, -1, StepSize::Normal) == 3.0);
    CHECK(KeyboardStep(m, 0.0, 1, StepSize::Coarse) == 10.0);
    const ValueMapping integer{.min = 0.0, .max = 100.0, .integer = true};
    CHECK(KeyboardStep(integer, 10.0, 1, StepSize::Normal) == 11.0);
    CHECK(KeyboardStep(integer, 10.0, 1, StepSize::Coarse) == 20.0);
    CHECK(PageStep(integer, 10.0, 1) == 20.0);
    CHECK(PageStep(integer, 95.0, 1) == 100.0);
    CHECK(PageStep({.min = 0.0, .max = 1.0}, 0.5, -1) == doctest::Approx(0.4));
    // NaN values step in from the end the direction starts at.
    CHECK(KeyboardStep(integer, kNaN, 1, StepSize::Normal) == 1.0);
}

TEST_CASE("Drag tracker: travel turns into a fraction without rounding")
{
    DragTracker tracker;
    tracker.Begin(0.25, 200.0);
    tracker.Move(50.0);
    CHECK(tracker.Fraction() == doctest::Approx(0.5));
    tracker.Move(100.0, facet::internal::kFineDragFactor);
    CHECK(tracker.Fraction() == doctest::Approx(0.55));
    tracker.Move(-10.0, facet::internal::kFineDragFactor);
    tracker.Move(-50.0);
    CHECK(tracker.Fraction() == doctest::Approx(0.25 + 9.0 / 200.0));
    CHECK(tracker.StartFraction() == 0.25);
    // Beyond the ends: unclamped.
    tracker.Move(-1000.0);
    CHECK(tracker.Fraction() < 0.0);
    tracker.Move(std::numeric_limits<double>::infinity());
    CHECK(std::isfinite(tracker.Fraction()));
}

// ---- Behaviour --------------------------------------------------------------------------------

TEST_CASE("Value drag: a press on the knob grabs it without a jump")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    Install(session, slider);
    const ImVec2 knob = TestSlider::At(session, 0.5);
    session.MouseMove(ImVec2(knob.x + 6.0f, knob.y));
    session.MouseDown();
    CHECK(slider.value == 0.5);
    CHECK(slider.activations == 1);
    session.MouseMove(ImVec2(knob.x + 56.0f, knob.y));
    CHECK(slider.value == doctest::Approx(0.75));
    session.MouseUp();
    CHECK(slider.commits == 1);
    CHECK(slider.afterEdit == 1);
    CHECK(slider.cancels == 0);
}

TEST_CASE("Value drag: a press on the track moves the value under the pointer")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    Install(session, slider);
    session.Click(TestSlider::At(session, 0.2));
    CHECK(slider.value == doctest::Approx(0.2));
    CHECK(slider.commits == 1);
    // Beyond the end and back: the value waits at the end until the pointer returns.
    session.Drag({TestSlider::At(session, 0.2), TestSlider::At(session, 1.3), TestSlider::At(session, 0.9)});
    CHECK(slider.value == doctest::Approx(0.9));
}

TEST_CASE("Value drag: Shift drags finer, Ctrl snaps to the coarse grid")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    Install(session, slider);
    const ImVec2 knob = TestSlider::At(session, 0.5);
    session.MouseMove(knob);
    session.MouseDown();
    session.KeyDown(ImGuiMod_Shift);
    session.MouseMove(ImVec2(knob.x + 100.0f, knob.y));
    CHECK(slider.value == doctest::Approx(0.55));
    session.KeyUp(ImGuiMod_Shift);
    session.KeyDown(ImGuiMod_Ctrl);
    session.MouseMove(ImVec2(knob.x + 107.0f, knob.y));
    CHECK(slider.value == doctest::Approx(0.6));  // 0.585 snapped to tenths
    session.KeyUp(ImGuiMod_Ctrl);
    session.MouseUp();
}

TEST_CASE("Value drag: no drift after moving back to the start")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    // 1.3 / 9.9 * 9.9 is not exactly 1.3 in double precision, so only returning the start value is exact.
    slider.options.mapping = {.min = 0.0, .max = 9.9};
    slider.value = 1.3;
    Install(session, slider);
    const ImVec2 knob = TestSlider::At(session, 1.3 / 9.9);
    const ImVec2 start(knob.x + 3.0f, knob.y);
    session.Drag({start, ImVec2(start.x + 37.0f, start.y), ImVec2(start.x - 53.0f, start.y),
                  ImVec2(start.x + 11.0f, start.y), start},
                 3);
    CHECK(slider.value == 1.3);
    CHECK(slider.changes > 0);
}

TEST_CASE("Value drag: Escape during a drag restores the value and cancels the edit")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    Install(session, slider);
    const ImVec2 knob = TestSlider::At(session, 0.5);
    session.MouseMove(knob);
    session.MouseDown();
    session.MouseMove(ImVec2(knob.x + 60.0f, knob.y));
    CHECK(slider.value == doctest::Approx(0.8));
    session.KeyPress(ImGuiKey_Escape);
    CHECK(slider.value == 0.5);
    CHECK(slider.cancels == 1);
    CHECK(slider.commits == 0);
    // The drag is over: moving and releasing change nothing.
    session.MouseMove(ImVec2(knob.x + 90.0f, knob.y));
    session.MouseUp();
    CHECK(slider.value == 0.5);
    CHECK(slider.commits == 0);
}

TEST_CASE("Value drag: keys step while focused, one edit session per press")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    Install(session, slider);
    session.Click(TestSlider::At(session, 0.5));  // focuses without moving the value
    REQUIRE(slider.value == 0.5);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("slider"));
    const int commits = slider.commits;

    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(slider.value == doctest::Approx(0.51));
    CHECK(slider.commits == commits + 1);
    session.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Shift);
    CHECK(slider.value == doctest::Approx(0.511));
    session.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    CHECK(slider.value == doctest::Approx(0.6));
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(slider.value == doctest::Approx(0.59));
    session.KeyPress(ImGuiKey_End);
    CHECK(slider.value == 1.0);
    session.KeyPress(ImGuiKey_PageDown);
    CHECK(slider.value == doctest::Approx(0.9));
    session.KeyPress(ImGuiKey_Home);
    CHECK(slider.value == 0.0);
    CHECK(slider.commits == commits + 7);
    // The cross-axis arrows are left to navigation.
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(slider.value == 0.0);
}

TEST_CASE("Value drag: a held key repeats by time, independent of the frame rate")
{
    double results[2] = {};
    const float rates[2] = {1.0f / 30.0f, 1.0f / 120.0f};
    for (int i = 0; i < 2; ++i)
    {
        facet::test::Session session({.deltaTime = rates[i]});
        REQUIRE(session.Ready());
        TestSlider slider;
        slider.value = 0.0;
        Install(session, slider);
        session.Click(TestSlider::At(session, 0.0));
        // Held for 0.5 s of frame time at either rate: the first press, then repeats from
        // io.KeyRepeatDelay (0.275 s) on every io.KeyRepeatRate (0.05 s): 1 + 5 steps.
        session.KeyDown(ImGuiKey_RightArrow);
        session.Frames(static_cast<int>(std::lround(0.5 / rates[i])));
        results[i] = slider.value;
        session.KeyUp(ImGuiKey_RightArrow);
        CHECK(slider.commits == 1);  // the click on the knob edits nothing; the held key is one session
    }
    CHECK(results[0] == doctest::Approx(0.06));
    CHECK(results[0] == results[1]);
}

TEST_CASE("Value drag: the same drag path gives the same value at any frame rate")
{
    double results[2] = {};
    const float rates[2] = {1.0f / 30.0f, 1.0f / 144.0f};
    const int framesPerSegment[2] = {3, 14};
    for (int i = 0; i < 2; ++i)
    {
        facet::test::Session session({.deltaTime = rates[i]});
        REQUIRE(session.Ready());
        TestSlider slider;
        Install(session, slider);
        const ImVec2 knob = TestSlider::At(session, 0.5);
        session.Drag({knob, ImVec2(knob.x + 41.0f, knob.y), ImVec2(knob.x - 23.0f, knob.y + 30.0f),
                      ImVec2(knob.x + 17.0f, knob.y)},
                     framesPerSegment[i]);
        results[i] = slider.value;
    }
    CHECK(results[0] == doctest::Approx(0.5 + 17.0 / 200.0));
    CHECK(results[0] == results[1]);
}

TEST_CASE("Value drag: Escape during a held key restores the value")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    Install(session, slider);
    session.Click(TestSlider::At(session, 0.5));
    session.KeyDown(ImGuiKey_RightArrow);
    CHECK(slider.value == doctest::Approx(0.51));
    const int escapeFrame = session.FrameCount();
    session.KeyPress(ImGuiKey_Escape);
    CHECK(slider.value == 0.5);
    CHECK(slider.cancels == 1);
    // Reported on the frame Escape went down, and on no other.
    REQUIRE(slider.cancelFrames.size() == 1);
    CHECK(slider.cancelFrames[0] == escapeFrame + 1);
    session.KeyUp(ImGuiKey_RightArrow);
    CHECK(slider.value == 0.5);
}

TEST_CASE("Value drag: the wheel steps only when focused or with Alt")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    slider.leadingButton = true;
    Install(session, slider);
    REQUIRE(ImGui::GetFocusID() != session.ItemId("slider"));
    const ImVec2 knob = TestSlider::At(session, 0.5);
    // Not focused: the wheel is left to the window.
    session.Wheel(knob, 1.0f);
    CHECK(slider.value == 0.5);
    // Alt held over the control.
    session.KeyDown(ImGuiMod_Alt);
    session.Wheel(knob, 1.0f);
    CHECK(slider.value == doctest::Approx(0.51));
    session.KeyUp(ImGuiMod_Alt);
    // Focused.
    session.Click(knob);
    session.Wheel(knob, -2.0f);
    CHECK(slider.value == doctest::Approx(0.49));
}

TEST_CASE("Value drag: fractions of a wheel notch add up to steps")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    Install(session, slider);
    const ImVec2 knob = TestSlider::At(session, 0.5);
    session.Click(knob);
    REQUIRE(slider.value == 0.5);
    for (int i = 0; i < 8; ++i)
        session.Wheel(knob, 0.25f);
    CHECK(slider.value == doctest::Approx(0.52));
    // Ten tenths make one notch as well.
    for (int i = 0; i < 10; ++i)
        session.Wheel(knob, 0.1f);
    CHECK(slider.value == doctest::Approx(0.53));
    // The other direction starts its own count.
    session.Wheel(knob, 0.5f);
    session.Wheel(knob, -0.75f);
    CHECK(slider.value == doctest::Approx(0.53));
    session.Wheel(knob, -0.25f);
    CHECK(slider.value == doctest::Approx(0.52));
}

TEST_CASE("Value drag: the clamp limits stop an edit, and an edit that cannot move writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    slider.options.clampLow = 0.2;
    slider.options.clampHigh = 0.6;
    Install(session, slider);
    session.Drag({TestSlider::At(session, 0.5), TestSlider::At(session, 0.9)});
    CHECK(slider.value == 0.6);
    CHECK(slider.commits == 1);
    // At the limit: a drag and a key press further on are sessions without an edit.
    const int changes = slider.changes;
    session.Drag({TestSlider::At(session, 0.6), TestSlider::At(session, 0.95)});
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_End);
    CHECK(slider.value == 0.6);
    CHECK(slider.changes == changes);
    CHECK(slider.commits == 1);
    CHECK(slider.afterEdit == 1);
    session.KeyPress(ImGuiKey_Home);
    CHECK(slider.value == 0.2);
}

TEST_CASE("Value drag: disabled and read-only controls never write")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TestSlider slider;
    slider.disabled = true;
    Install(session, slider);
    session.Drag({TestSlider::At(session, 0.5), TestSlider::At(session, 0.9)});
    session.Click(TestSlider::At(session, 0.1));
    CHECK(slider.value == 0.5);
    CHECK(slider.activations == 0);

    slider.disabled = false;
    slider.options.readOnly = true;
    session.Drag({TestSlider::At(session, 0.5), TestSlider::At(session, 0.9)});
    session.Click(TestSlider::At(session, 0.1));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(slider.value == 0.5);
    CHECK(slider.changes == 0);
}
