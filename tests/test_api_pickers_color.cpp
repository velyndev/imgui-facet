// The public API of the color pickers (ColorWell, ColorPicker, ColorInputs, SwatchPalette,
// SatValSquare, ColorChannelSlider, ColorModeEditor and SampleAnimatedColor in
// facet/widgets/pickers.h): every options struct follows the conventions, and every function is
// safe to call: without input it submits one item, leaves the Dear ImGui stacks as it found them,
// returns false and writes nothing, also inside ImGui::BeginDisabled and outside a frame. The
// behaviour is tested in tests/test_pickers_color.cpp; the other pickers have their own file
// (tests/test_api_pickers_date.cpp).

#include <span>
#include <string>
#include <vector>

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

static_assert(OptionsFollowConventions<facet::ColorWellOptions>());
static_assert(OptionsFollowConventions<facet::ColorPickerOptions>());
static_assert(OptionsFollowConventions<facet::ColorInputsOptions>());
static_assert(OptionsFollowConventions<facet::SwatchPaletteOptions>());
static_assert(OptionsFollowConventions<facet::SatValSquareOptions>());
static_assert(OptionsFollowConventions<facet::ColorChannelSliderOptions>());
static_assert(OptionsFollowConventions<facet::ColorModeEditorOptions>());

// The zero member of every choice enum is the default (rule 12).
static_assert(facet::ColorPickerMode{} == facet::ColorPickerMode::Square);
static_assert(facet::ColorModel{} == facet::ColorModel::Rgb);
static_assert(facet::ColorChannel{} == facet::ColorChannel::Hue);
static_assert(facet::ColorAnimation{} == facet::ColorAnimation::Static);

// Every color picker function once, with the overloads, as an app would call it. Without input
// each one returns false, keeps the stacks and writes nothing.
void CallColorPickers(Values& v)
{
    using namespace facet;
    FACET_CHECK_CALL(false, ColorWell("Well 1", &v.color, {.alpha = true, .showHex = true}));
    FACET_CHECK_CALL(false, ColorPicker("Picker 1", &v.color, {.original = &v.color, .swatches = kSwatches}));
    FACET_CHECK_CALL(false, ColorPicker("Picker 2", &v.color, {.alpha = true, .mode = ColorPickerMode::Wheel}));
    FACET_CHECK_CALL(false, ColorInputs("Inputs 1", &v.color, {.model = ColorModel::Hsv}));
    FACET_CHECK_CALL(false, SwatchPalette("Palette 1", &v.color, std::span(kSwatches), &v.history));
    FACET_CHECK_CALL(false, SwatchPalette("Palette 2", &v.color, &v.swatches, nullptr));
    FACET_CHECK_CALL(false, SwatchPalette("Palette 3", &v.color, std::span<ImVec4>(v.swatchStorage), &v.swatchCount, nullptr));
    FACET_CHECK_CALL(false, SatValSquare("Square 1", &v.color));
    FACET_CHECK_CALL(false, ColorChannelSlider("Channel 1", &v.color, {.channel = ColorChannel::Alpha}));
    FACET_CHECK_CALL(false, ColorChannelSlider("Channel 2", &v.color, {.orientation = Orientation::Vertical}));
    FACET_CHECK_CALL(false, ColorModeEditor("Mode editor 1", &v.animated));
    // A static color is its base color at any time.
    const AnimatedColor still{ColorAnimation::Static, v.animated.color, v.animated.second, v.animated.period};
    CHECK(SampleAnimatedColor(still, 1.0).x == v.animated.color.x);
}

// Number of FACET_CHECK_CALL calls in CallColorPickers.
constexpr int kColorPickerCalls = 11;

} // namespace

TEST_CASE("Every color picker without input returns false, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_CALLS(CallColorPickers, kColorPickerCalls);
}

TEST_CASE("Color pickers called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    CHECK_FALSE(facet::ColorPicker("Picker 1", &values.color));
    CHECK_FALSE(facet::ColorWell("Well 1", &values.color));
    CHECK_FALSE(facet::ColorInputs("Inputs 1", &values.color));
    CHECK_FALSE(facet::SwatchPalette("Palette 1", &values.color, std::span(kSwatches), &values.history));
    CHECK_FALSE(facet::SatValSquare("Square 1", &values.color));
    CHECK_FALSE(facet::ColorChannelSlider("Channel 1", &values.color));
    CHECK_FALSE(facet::ColorModeEditor("Mode editor 1", &values.animated));
    CHECK(Describe(values) == pristine);
}

TEST_CASE("Color pickers with a null value pointer draw read-only and return false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    session.SetUi([&] {
        CHECK_FALSE(facet::ColorWell("Well", nullptr));
        CHECK_FALSE(facet::ColorPicker("Picker", nullptr, {.alpha = true}));
        CHECK_FALSE(facet::ColorInputs("Inputs", nullptr));
        CHECK_FALSE(facet::SwatchPalette("Palette", nullptr, std::span(kSwatches), nullptr));
        CHECK_FALSE(facet::SwatchPalette("Vector", nullptr, static_cast<std::vector<ImVec4>*>(nullptr), nullptr));
        CHECK_FALSE(facet::SatValSquare("Square", nullptr));
        CHECK_FALSE(facet::ColorChannelSlider("Channel", nullptr));
        CHECK_FALSE(facet::ColorModeEditor("Mode", nullptr));
    });
    session.Frames(2);
    const ImVec2 center(200.0f, 20.0f);
    session.Click(center);
    session.Frames(2);
}
