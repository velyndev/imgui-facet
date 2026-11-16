// Compile check: the public headers must survive the macros of a full windows.h
// (min, max, small, DrawText, LoadImage and friends), because host applications include it
// before Facet without NOMINMAX or WIN32_LEAN_AND_MEAN.

#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <doctest/doctest.h>

#include <facet/facet.h>

TEST_CASE("The public headers compile after a full windows.h")
{
    CHECK(facet::Version() != nullptr);
}
