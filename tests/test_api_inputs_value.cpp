// The public API of the number-like inputs of facet/widgets/inputs.h (NumberInput, DragNumber,
// UnitInput, VectorInput, SizeInput, HexInput, DurationInput, KeybindCapture and Rating): every
// options struct follows the conventions, and every widget called in a frame without input, or
// outside a frame, returns false, submits one item, leaves the Dear ImGui stacks as it found them
// and writes nothing. Behaviour tests are in tests/test_inputs_value.cpp; the text-like inputs
// have tests/test_api_inputs_text.cpp.

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

// Every widget once, with the overloads, as an app would call it. A frame without input gives
// the neutral result: false, one item, the stacks kept, nothing written (FACET_CHECK_STUB checks
// exactly that, for placeholders and ready widgets alike).
void CallValueInputs(Values& v)
{
    using namespace facet;
    FACET_CHECK_STUB(false, NumberInput("Number 1", &v.integer));
    FACET_CHECK_STUB(false, NumberInput("Number 2", &v.number, {.min = 0.0, .max = 1.0, .step = 0.1}));
    FACET_CHECK_STUB(false, NumberInput("Number 3", &v.real, {.unit = "ms"}));
    FACET_CHECK_STUB(false, DragNumber("Drag 1", &v.integer));
    FACET_CHECK_STUB(false, DragNumber("Drag 2", &v.number, {.speed = 0.01}));
    FACET_CHECK_STUB(false, DragNumber("Drag 3", &v.real, {.wrap = true}));
    FACET_CHECK_STUB(false, UnitInput("Unit 1", &v.real, &v.unit, kUnits));
    FACET_CHECK_STUB(false, UnitInput("Unit 2", &v.real, nullptr, kUnits));
    FACET_CHECK_STUB(false, VectorInput("Vector 1", std::span<float>(v.vector), {.linkToggle = true}));
    FACET_CHECK_STUB(false, VectorInput("Vector 2", std::span<int>(v.vectorInt)));
    FACET_CHECK_STUB(false, VectorInput("Vector 3", &v.point));
    FACET_CHECK_STUB(false, VectorInput("Vector 4", &v.color));
    FACET_CHECK_STUB(false, SizeInput("Size 1", &v.size));
    FACET_CHECK_STUB(false, SizeInput("Size 2", &v.sizeX, &v.sizeY, {.swapButton = true}));
    FACET_CHECK_STUB(false, HexInput("Hex 1", &v.hex32, {.prefix = true}));
    FACET_CHECK_STUB(false, HexInput("Hex 2", &v.hex64));
    FACET_CHECK_STUB(false, DurationInput("Duration 1", &v.seconds, {.showMilliseconds = true}));
    FACET_CHECK_STUB(false, KeybindCapture("Keybind 1", &v.bind, {.showMode = true, .showAlternate = true}));
    FACET_CHECK_STUB(false, Rating("Rating 1", &v.rating, {.halfSteps = true}));
    FACET_CHECK_STUB(false, Rating("Rating 2", &v.ratingInt));
}

// Number of FACET_CHECK_STUB calls in CallValueInputs.
constexpr int kValueInputCalls = 20;

} // namespace

TEST_CASE("Every value input without input returns false, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_STUBS(CallValueInputs, kValueInputCalls);
}

TEST_CASE("Value inputs called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Values values;
    const std::string pristine = Describe(values);
    CHECK_FALSE(facet::NumberInput("Number 1", &values.integer));
    CHECK_FALSE(facet::DragNumber("Drag 1", &values.real));
    CHECK_FALSE(facet::UnitInput("Unit 1", &values.real, &values.unit, kUnits));
    CHECK_FALSE(facet::VectorInput("Vector 1", &values.point));
    CHECK_FALSE(facet::SizeInput("Size 1", &values.size));
    CHECK_FALSE(facet::HexInput("Hex 1", &values.hex32));
    CHECK_FALSE(facet::DurationInput("Duration 1", &values.seconds));
    CHECK_FALSE(facet::KeybindCapture("Keybind 1", &values.bind));
    CHECK_FALSE(facet::Rating("Rating 1", &values.rating));
    CHECK(Describe(values) == pristine);
}

TEST_CASE("Value inputs given null value pointers draw read-only and return false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool any = false;
    session.SetUi([&] {
        any |= facet::NumberInput("Number", static_cast<double*>(nullptr));
        any |= facet::DragNumber("Drag", static_cast<int*>(nullptr));
        any |= facet::UnitInput("Unit", nullptr, nullptr, kUnits);
        any |= facet::VectorInput("Vector", static_cast<ImVec2*>(nullptr));
        any |= facet::SizeInput("Size", static_cast<ImVec2*>(nullptr));
        any |= facet::HexInput("Hex", static_cast<std::uint32_t*>(nullptr));
        any |= facet::DurationInput("Duration", nullptr);
        any |= facet::KeybindCapture("Keybind", nullptr);
        any |= facet::Rating("Rating", static_cast<int*>(nullptr));
    });
    session.Frames(2);
    session.Click(ImVec2(200.0f, 20.0f));
    session.KeyPress(ImGuiKey_UpArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK_FALSE(any);
}
