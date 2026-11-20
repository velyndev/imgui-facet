// The public API of the sliders family (facet/widgets/sliders.h): every options struct follows
// the conventions and the multi-handle model, and every widget that is still a placeholder is safe
// to call, submits one item that cannot be operated, leaves the Dear ImGui stacks as it found
// them, returns the neutral result and writes nothing. Behaviour tests of implemented widgets go
// into tests/test_sliders.cpp.

#include <concepts>
#include <span>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/controls_stub_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::controls;

static_assert(OptionsFollowConventions<facet::SliderOptions>());
static_assert(OptionsFollowConventions<facet::SliderSteppedOptions>());
static_assert(OptionsFollowConventions<facet::SliderWithInputOptions>());
static_assert(OptionsFollowConventions<facet::RangeSliderOptions>());
static_assert(OptionsFollowConventions<facet::RangeInputOptions>());
static_assert(OptionsFollowConventions<facet::RangeSliderHistogramOptions>());
static_assert(OptionsFollowConventions<facet::MultiSliderOptions>());
static_assert(OptionsFollowConventions<facet::KnobOptions>());
static_assert(OptionsFollowConventions<facet::XYPadOptions>());
static_assert(OptionsFollowConventions<facet::JoystickPadOptions>());
static_assert(OptionsFollowConventions<facet::ArcRangeSliderOptions>());
static_assert(OptionsFollowConventions<facet::BoxRangePadOptions>());
static_assert(OptionsFollowConventions<facet::AllocationBarOptions>());

// The multi-handle model: every widget of it has the same state members, and the two-handle
// linear ones share the typed-value and reset members.
template <class Options>
constexpr bool HasHandleModelState()
{
    return requires(Options o) {
        { o.readOnly } -> std::same_as<bool&>;
        { o.mixed } -> std::same_as<bool&>;
        { o.error } -> std::same_as<const char*&>;
        { o.helpText } -> std::same_as<const char*&>;
    };
}
template <class Options>
constexpr bool HasTwoHandleMembers()
{
    return requires(Options o) {
        { o.allowOutOfRange } -> std::same_as<bool&>;
        { o.doubleClickReset } -> std::same_as<bool&>;
    };
}
static_assert(HasHandleModelState<facet::RangeSliderOptions>());
static_assert(HasHandleModelState<facet::RangeInputOptions>());
static_assert(HasHandleModelState<facet::RangeSliderHistogramOptions>());
static_assert(HasHandleModelState<facet::MultiSliderOptions>());
static_assert(HasHandleModelState<facet::ArcRangeSliderOptions>());
static_assert(HasHandleModelState<facet::BoxRangePadOptions>());
static_assert(HasHandleModelState<facet::AllocationBarOptions>());
static_assert(HasTwoHandleMembers<facet::RangeSliderOptions>());
static_assert(HasTwoHandleMembers<facet::RangeInputOptions>());
static_assert(HasTwoHandleMembers<facet::RangeSliderHistogramOptions>());

static_assert(HasFormatMembers<facet::SliderOptions>());
static_assert(HasFormatMembers<facet::SliderSteppedOptions>());
static_assert(HasFormatMembers<facet::SliderWithInputOptions>());
static_assert(HasFormatMembers<facet::RangeSliderOptions>());
static_assert(HasFormatMembers<facet::RangeInputOptions>());
static_assert(HasFormatMembers<facet::RangeSliderHistogramOptions>());
static_assert(HasFormatMembers<facet::MultiSliderOptions>());
static_assert(HasFormatMembers<facet::KnobOptions>());
static_assert(HasFormatMembers<facet::XYPadOptions>());
static_assert(HasFormatMembers<facet::ArcRangeSliderOptions>());
static_assert(HasFormatMembers<facet::BoxRangePadOptions>());
static_assert(HasFormatMembers<facet::AllocationBarOptions>());

// The zero member of every choice enum is the default (rule 12).
static_assert(facet::HandleCollision{} == facet::HandleCollision::Clamp);
static_assert(facet::KnobVariant{} == facet::KnobVariant::Arc);

// Every placeholder of the family once, with the overloads, as an app would call it.
void CallSliderStubs(Values& v)
{
    using namespace facet;
    FACET_CHECK_STUB(false, RangeSliderHistogram("Histogram 1", &v.low, &v.high, 0.0f, 1.0f, kBins));
    FACET_CHECK_STUB(false, RangeSliderHistogram("Histogram 2", &v.lowInt, &v.highInt, 0, 10, kBins));
    FACET_CHECK_STUB(false, XYPad("Pad 1", &v.point, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.grid = true}));
    FACET_CHECK_STUB(false, JoystickPad("Stick 1", &v.point, {.deadzone = 0.1f}));
    FACET_CHECK_STUB(false, ArcRangeSlider("Arc 1", &v.low, &v.high, 0.0f, 360.0f, {.wrap = true, .sweep = 360.0f}));
    FACET_CHECK_STUB(false, BoxRangePad("Box 1", &v.boxLow, &v.boxHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)));
    FACET_CHECK_STUB(false, AllocationBar("Allocation 1", std::span<float>(v.shares), kChoices));
    FACET_CHECK_STUB(false, AllocationBar("Allocation 2", std::span<float>(v.shares), kLabels, {.minGap = 0.1}));
}

// Number of FACET_CHECK_STUB calls in CallSliderStubs.
constexpr int kSliderStubCalls = 8;

} // namespace

TEST_CASE("Every sliders placeholder returns the neutral result, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_STUBS(CallSliderStubs, kSliderStubCalls);
}

TEST_CASE("Sliders placeholders called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    CHECK_FALSE(facet::Slider("Slider 1", &values.number, 0.0f, 1.0f));
    CHECK_FALSE(facet::RangeSlider("Range 1", &values.low, &values.high, 0.0f, 1.0f));
    CHECK(Describe(values) == pristine);
}

TEST_CASE("A sliders placeholder is one item under the label's id that cannot be operated")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 stick(0.25f, 0.5f);
    float gain = 0.5f;
    float gainHigh = 0.75f;
    ImGuiID expected = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        expected = ImGui::GetID("Volume##value");
        facet::JoystickPad("Volume##value", &stick);
        session.Track("slider");
        facet::ArcRangeSlider("Gain", &gain, &gainHigh, 0.0f, 1.0f);
        session.Track("knob");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();

    CHECK(session.ItemId("slider") == expected);
    // Tab goes from the stock button before straight to the one after: no placeholder is a Tab stop.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("before"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    // Clicks and drags on the placeholders activate nothing and write nothing.
    session.Click(session.ItemCenter("knob"));
    CHECK_FALSE(ImGui::IsAnyItemActive());
    const ImRect slider = session.ItemRect("slider");
    session.Drag({ImVec2(slider.Min.x + 4.0f, slider.GetCenter().y), ImVec2(slider.Max.x - 4.0f, slider.GetCenter().y)});
    CHECK(stick.x == 0.25f);
    CHECK(stick.y == 0.5f);
    CHECK(gain == 0.5f);
    CHECK(gainHigh == 0.75f);
}

TEST_CASE("Pads keep their default size in a wide row and fit a short row")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec2 point(0.5f, 0.5f);
    ImVec2 low(0.25f, 0.25f);
    ImVec2 high(0.75f, 0.75f);
    float available = 0.0f;
    session.SetUi([&] {
        facet::XYPad("Pad##wide", &point, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        session.Track("xy wide");
        facet::JoystickPad("Stick##wide", &point);
        session.Track("stick wide");
        // A row far narrower than the default pad sizes.
        ImGui::BeginChild("short row", ImVec2(facet::Px(40.0f), facet::Px(400.0f)));
        available = ImGui::GetContentRegionAvail().x;
        facet::XYPad("Pad", &point, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        session.Track("xy");
        facet::BoxRangePad("Box", &low, &high, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
        session.Track("box");
        facet::JoystickPad("Stick", &point);
        session.Track("stick");
        // A size the caller asks for is kept.
        facet::XYPad("Pad##asked", &point, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.width = 100.0f});
        session.Track("xy asked");
        facet::JoystickPad("Stick##asked", &point, {.diameter = 100.0f});
        session.Track("stick asked");
        ImGui::EndChild();
    });
    session.Frame();

    REQUIRE(available > 1.0f);
    CHECK(session.ItemRect("xy wide").GetWidth() > available);
    CHECK(session.ItemRect("stick wide").GetWidth() > available);
    for (const char* name : {"xy", "box", "stick"})
    {
        CAPTURE(name);
        const ImRect rect = session.ItemRect(name);
        CHECK(rect.GetWidth() == doctest::Approx(available));
        // Square: as high as it is wide.
        CHECK(rect.GetHeight() == doctest::Approx(available));
    }
    CHECK(session.ItemRect("xy asked").GetWidth() == doctest::Approx(facet::Px(100.0f)));
    CHECK(session.ItemRect("stick asked").GetWidth() == doctest::Approx(facet::Px(100.0f)));
}
