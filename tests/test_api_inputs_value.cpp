// The public API of the number-like inputs of facet/widgets/inputs.h (NumberInput, DragNumber,
// UnitInput, VectorInput, SizeInput, HexInput, DurationInput, KeybindCapture and Rating): every
// options struct follows the conventions, and every widget that is still a placeholder is safe to
// call, submits one item that cannot be operated, leaves the Dear ImGui stacks as it found them,
// returns the neutral result and writes nothing. Behaviour tests of implemented widgets go into
// tests/test_inputs_value.cpp; the text-like inputs have tests/test_api_inputs_text.cpp.

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

static_assert(OptionsFollowConventions<facet::NumberInputOptions>());
static_assert(OptionsFollowConventions<facet::DragNumberOptions>());
static_assert(OptionsFollowConventions<facet::UnitInputOptions>());
static_assert(OptionsFollowConventions<facet::VectorInputOptions>());
static_assert(OptionsFollowConventions<facet::SizeInputOptions>());
static_assert(OptionsFollowConventions<facet::HexInputOptions>());
static_assert(OptionsFollowConventions<facet::DurationInputOptions>());
static_assert(OptionsFollowConventions<facet::KeybindCaptureOptions>());
static_assert(OptionsFollowConventions<facet::RatingOptions>());

static_assert(HasFormatMembers<facet::NumberInputOptions>());
static_assert(HasFormatMembers<facet::DragNumberOptions>());
static_assert(HasFormatMembers<facet::UnitInputOptions>());
static_assert(HasFormatMembers<facet::VectorInputOptions>());
static_assert(HasFormatMembers<facet::SizeInputOptions>());

// Every ready widget once, with the overloads, as an app would call it. A frame without input
// gives the same result as a placeholder: false, one item, the stacks kept, nothing written.
void CallReadyValueInputs(Values& v)
{
    using namespace facet;
    FACET_CHECK_STUB(false, NumberInput("Number 1", &v.integer));
    FACET_CHECK_STUB(false, NumberInput("Number 2", &v.number, {.min = 0.0, .max = 1.0, .step = 0.1}));
    FACET_CHECK_STUB(false, NumberInput("Number 3", &v.real, {.unit = "ms"}));
    FACET_CHECK_STUB(false, DragNumber("Drag 1", &v.integer));
    FACET_CHECK_STUB(false, DragNumber("Drag 2", &v.number, {.speed = 0.01}));
    FACET_CHECK_STUB(false, DragNumber("Drag 3", &v.real, {.wrap = true}));
    FACET_CHECK_STUB(false, KeybindCapture("Keybind 1", &v.bind, {.showMode = true, .showAlternate = true}));
    FACET_CHECK_STUB(false, UnitInput("Unit 1", &v.real, &v.unit, kUnits));
    FACET_CHECK_STUB(false, UnitInput("Unit 2", &v.real, nullptr, kUnits));
    FACET_CHECK_STUB(false, VectorInput("Vector 1", std::span<float>(v.vector), {.linkToggle = true}));
    FACET_CHECK_STUB(false, VectorInput("Vector 3", &v.point));
    FACET_CHECK_STUB(false, VectorInput("Vector 4", &v.color));
    FACET_CHECK_STUB(false, VectorInput("Vector 2", std::span<int>(v.vectorInt)));
    FACET_CHECK_STUB(false, Rating("Rating 1", &v.rating, {.halfSteps = true}));
    FACET_CHECK_STUB(false, Rating("Rating 2", &v.ratingInt));
    FACET_CHECK_STUB(false, SizeInput("Size 1", &v.size));
    FACET_CHECK_STUB(false, SizeInput("Size 2", &v.sizeX, &v.sizeY, {.swapButton = true}));
    FACET_CHECK_STUB(false, DurationInput("Duration 1", &v.seconds, {.showMilliseconds = true}));
}

// Number of FACET_CHECK_STUB calls in CallReadyValueInputs.
constexpr int kReadyValueInputCalls = 18;

// Every placeholder of these widgets once, with the overloads, as an app would call it.
void CallValueInputStubs(Values& v)
{
    using namespace facet;
    FACET_CHECK_STUB(false, HexInput("Hex 1", &v.hex32, {.prefix = true}));
    FACET_CHECK_STUB(false, HexInput("Hex 2", &v.hex64));
}

// Number of FACET_CHECK_STUB calls in CallValueInputStubs.
constexpr int kValueInputStubCalls = 2;

} // namespace

TEST_CASE("Every ready value input without input returns false, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_STUBS(CallReadyValueInputs, kReadyValueInputCalls);
}

TEST_CASE("Every value input placeholder returns the neutral result, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_STUBS(CallValueInputStubs, kValueInputStubCalls);
}

TEST_CASE("Value input placeholders called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    CHECK_FALSE(facet::NumberInput("Number 1", &values.integer));
    CHECK_FALSE(facet::KeybindCapture("Keybind 1", &values.bind));
    CHECK(Describe(values) == pristine);
}
