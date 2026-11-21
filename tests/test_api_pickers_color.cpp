// The public API of the color pickers (ColorWell, ColorPicker, ColorInputs, SwatchPalette,
// SatValSquare, ColorChannelSlider, ColorModeEditor and SampleAnimatedColor in
// facet/widgets/pickers.h) while they are planned: every options struct follows the conventions,
// and every function is safe to call, submits one placeholder item that cannot be operated, leaves
// the Dear ImGui stacks as it found them, returns the neutral result and writes nothing. The other
// pickers have their own file (tests/test_api_pickers_date.cpp).

#include <span>
#include <string>

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

// Every color picker function once, with the overloads, as an app would call it.
void CallColorPickerStubs(Values& v)
{
    using namespace facet;
    FACET_CHECK_STUB(false, ColorWell("Well 1", &v.color, {.alpha = true, .showHex = true}));
    FACET_CHECK_STUB(false, ColorPicker("Picker 1", &v.color, {.original = &v.color, .swatches = kSwatches}));
    FACET_CHECK_STUB(false, ColorInputs("Inputs 1", &v.color, {.model = ColorModel::Hsv}));
    FACET_CHECK_STUB(false, SwatchPalette("Palette 1", &v.color, std::span(kSwatches), &v.history));
    FACET_CHECK_STUB(false, SwatchPalette("Palette 2", &v.color, &v.swatches, nullptr));
    FACET_CHECK_STUB(false, SwatchPalette("Palette 3", &v.color, std::span<ImVec4>(v.swatchStorage), &v.swatchCount, nullptr));
    FACET_CHECK_STUB(false, SatValSquare("Square 1", &v.color));
    FACET_CHECK_STUB(false, ColorChannelSlider("Channel 1", &v.color, {.channel = ColorChannel::Alpha}));
    FACET_CHECK_STUB(false, ColorModeEditor("Mode editor 1", &v.animated));
    CHECK(SampleAnimatedColor(v.animated, 1.0).x == v.animated.color.x);
}

// Number of FACET_CHECK_STUB calls in CallColorPickerStubs.
constexpr int kColorPickerStubCalls = 9;

} // namespace

TEST_CASE("Every color picker stub returns the neutral result, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_STUBS(CallColorPickerStubs, kColorPickerStubCalls);
}

TEST_CASE("Color picker stubs called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    CHECK_FALSE(facet::ColorPicker("Picker 1", &values.color));
    CHECK_FALSE(facet::ColorWell("Well 1", &values.color));
    CHECK(Describe(values) == pristine);
}
