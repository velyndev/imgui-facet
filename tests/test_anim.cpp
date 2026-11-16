// Motion: easing curves and the first-query rule of the animation functions.

#include <doctest/doctest.h>

#include <facet/facet.h>

TEST_CASE("Every easing curve maps 0 to 0 and 1 to 1 and clamps its input")
{
    const facet::Ease eases[] = {facet::Ease::Linear,     facet::Ease::InCubic,  facet::Ease::OutCubic,
                                 facet::Ease::InOutCubic, facet::Ease::OutQuint, facet::Ease::OutBack};
    for (const facet::Ease ease : eases)
    {
        CAPTURE(static_cast<int>(ease));
        CHECK(facet::EaseValue(ease, 0.0f) == doctest::Approx(0.0f));
        CHECK(facet::EaseValue(ease, 1.0f) == doctest::Approx(1.0f));
        CHECK(facet::EaseValue(ease, -1.0f) == doctest::Approx(0.0f));
        CHECK(facet::EaseValue(ease, 2.0f) == doctest::Approx(1.0f));
    }
    CHECK(facet::EaseValue(facet::Ease::InOutCubic, 0.5f) == doctest::Approx(0.5f));
}

TEST_CASE("The first query of an animation key returns the target")
{
    CHECK(facet::Smooth(1, 0, 3.0f, 10.0f) == doctest::Approx(3.0f));
    CHECK(facet::Spring(2, 0, 4.0f, facet::SpringSpec{}) == doctest::Approx(4.0f));
    CHECK(facet::Tween(3, 0, 5.0f, 0.2f) == doctest::Approx(5.0f));
    const ImVec4 color = facet::SmoothColor(4, 0, ImVec4(0.1f, 0.2f, 0.3f, 1.0f), 10.0f);
    CHECK(color.z == doctest::Approx(0.3f));
}
