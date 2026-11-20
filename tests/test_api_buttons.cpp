// The public API of the buttons family (facet/widgets/buttons.h): every options struct follows
// the conventions, and the widgets give the neutral result outside a frame. Every widget of the
// family is implemented; their behaviour tests are in tests/test_buttons.cpp.

#include <span>

#include <doctest/doctest.h>
#include <imgui.h>

#include <facet/facet.h>

#include "support/options_check.h"
#include "support/session.h"

namespace
{

using facet::test::OptionsFollowConventions;

static_assert(OptionsFollowConventions<facet::ButtonOptions>());
static_assert(OptionsFollowConventions<facet::IconButtonOptions>());
static_assert(OptionsFollowConventions<facet::CopyButtonOptions>());
static_assert(OptionsFollowConventions<facet::LinkOptions>());
static_assert(OptionsFollowConventions<facet::ToggleButtonGroupOptions>());

// The zero member of every choice enum is the default (rule 12).
static_assert(facet::ButtonVariant{} == facet::ButtonVariant::Secondary);

} // namespace

TEST_CASE("Buttons widgets called outside a frame return the neutral result and write nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    // Between frames: a context exists but no frame and no window.
    static const char* const kItems[] = {"First", "Second"};
    bool flags[2] = {false, true};
    int index = 1;
    CHECK_FALSE(facet::IconButton("Pin", facet::Icon("pin")));
    CHECK_FALSE(facet::Link("Link 1"));
    CHECK_FALSE(facet::CopyButton("Copy", "Sample"));
    CHECK_FALSE(facet::ToggleButtonGroup("Toggles", std::span<bool>(flags), kItems));
    CHECK_FALSE(facet::ToggleButtonGroup("Toggles", &index, kItems));
    CHECK_FALSE(flags[0]);
    CHECK(flags[1]);
    CHECK(index == 1);
}
