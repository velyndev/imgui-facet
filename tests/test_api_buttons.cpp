// The public API of the buttons family (facet/widgets/buttons.h): every options struct follows
// the conventions, and every widget that is still a placeholder is safe to call, submits one item
// that cannot be operated, leaves the Dear ImGui stacks as it found them, returns the neutral
// result and writes nothing. A widget that is implemented leaves the placeholder list below and
// gets its behaviour tests in tests/test_buttons.cpp.

#include <span>

#include <doctest/doctest.h>
#include <imgui.h>

#include <facet/facet.h>

#include "support/controls_stub_check.h"
#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;
using namespace facet::test::controls;

static_assert(OptionsFollowConventions<facet::ButtonOptions>());
static_assert(OptionsFollowConventions<facet::IconButtonOptions>());
static_assert(OptionsFollowConventions<facet::CopyButtonOptions>());
static_assert(OptionsFollowConventions<facet::LinkOptions>());
static_assert(OptionsFollowConventions<facet::ToggleButtonGroupOptions>());

// The zero member of every choice enum is the default (rule 12).
static_assert(facet::ButtonVariant{} == facet::ButtonVariant::Secondary);

// Every placeholder of the family once, with the overloads, as an app would call it.
void CallButtonStubs(Values& v)
{
    using namespace facet;
    const std::span<bool> flags(v.flags);
    FACET_CHECK_STUB(false, IconButton("Icon button 1", Icon("settings"), {.selected = true}));
    FACET_CHECK_STUB(false, CopyButton("Copy 1", "Sample", {.iconOnly = false}));
    FACET_CHECK_STUB(false, Link("Link 1", {.url = "about:blank"}));
    FACET_CHECK_STUB(false, ToggleButtonGroup("Toggles 1", flags, kChoices));
    FACET_CHECK_STUB(false, ToggleButtonGroup("Toggles 2", &v.index, kChoices, {.allowEmpty = true}));
    FACET_CHECK_STUB(false, ToggleButtonGroup("Toggles 3", flags, kLabels));
    FACET_CHECK_STUB(false, ToggleButtonGroup("Toggles 4", &v.index, kLabels, {.mixed = true}));
}

// Number of FACET_CHECK_STUB calls in CallButtonStubs.
constexpr int kButtonStubCalls = 7;

} // namespace

TEST_CASE("Every buttons placeholder returns the neutral result, writes nothing and keeps the stacks")
{
    FACET_CHECK_FAMILY_STUBS(CallButtonStubs, kButtonStubCalls);
}

TEST_CASE("Buttons placeholders called outside a frame return the neutral result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    // Between frames: a context exists but no frame and no window.
    CHECK_FALSE(facet::IconButton("Pin", facet::Icon("pin")));
    CHECK_FALSE(facet::Link("Link 1"));
}
