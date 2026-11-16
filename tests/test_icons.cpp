// Icon identifiers and image handles: value semantics that need no device.

#include <string>

#include <doctest/doctest.h>

#include <facet/facet.h>

// Icon names hash at compile time.
static_assert(facet::Icon("check").IsValid());
static_assert(facet::Icon("check") == facet::Icon("check"));
static_assert(!(facet::Icon("check") == facet::Icon("chevron")));
static_assert(!facet::Icon().IsValid());
static_assert(!facet::Icon("").IsValid());
// FNV-1a reference value for "a".
static_assert(facet::Icon::Hash("a") == 0xE40C292Cu);

TEST_CASE("Icons built from runtime strings equal their compile-time constants")
{
    const std::string name = std::string("chev") + "ron";
    CHECK(facet::Icon(name) == facet::Icon("chevron"));
}

TEST_CASE("Default image handles are invalid and compare equal")
{
    const facet::ImageHandle a;
    const facet::ImageHandle b;
    CHECK_FALSE(a.IsValid());
    CHECK(a == b);
    CHECK((facet::ImageHandle{3, 1} == facet::ImageHandle{3, 2}) == false);
}
