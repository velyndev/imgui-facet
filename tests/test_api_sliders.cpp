// The public API of the sliders family (facet/widgets/sliders.h): every options struct follows
// the conventions and the multi-handle model, a call outside a frame writes nothing, and the pads
// keep their size rule. Every widget of the family is implemented; their behaviour tests are in
// tests/test_sliders.cpp.

#include <concepts>
#include <span>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "support/controls_api_check.h"
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

} // namespace

TEST_CASE("Sliders called outside a frame return the neutral result and write nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    CHECK_FALSE(facet::Slider("Slider 1", &values.number, 0.0f, 1.0f));
    CHECK_FALSE(facet::RangeSlider("Range 1", &values.low, &values.high, 0.0f, 1.0f));
    CHECK_FALSE(facet::XYPad("Pad 1", &values.point, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)));
    CHECK_FALSE(facet::JoystickPad("Stick 1", &values.point));
    CHECK_FALSE(facet::ArcRangeSlider("Arc 1", &values.low, &values.high, 0.0f, 1.0f));
    CHECK_FALSE(facet::BoxRangePad("Box 1", &values.boxLow, &values.boxHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f)));
    CHECK_FALSE(facet::AllocationBar("Allocation 1", std::span<float>(values.shares), kLabels));
    CHECK(Describe(values) == pristine);
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
