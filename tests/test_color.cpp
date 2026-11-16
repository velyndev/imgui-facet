// Color math: transfer curve, OKLab and OKLCH round trips, WCAG contrast.

#include <cmath>

#include <doctest/doctest.h>

#include <facet/facet.h>

namespace
{

constexpr float kTolerance = 1e-3f;

bool Near(const ImVec4& a, const ImVec4& b, float tolerance = kTolerance)
{
    return std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance &&
           std::fabs(a.z - b.z) <= tolerance && std::fabs(a.w - b.w) <= tolerance;
}

// A spread of colors including the primaries, greys and the gamut corners.
const ImVec4 kSamples[] = {
    facet::Rgb(0x000000), facet::Rgb(0xFFFFFF), facet::Rgb(0xFF0000), facet::Rgb(0x00FF00),
    facet::Rgb(0x0000FF), facet::Rgb(0x00FFFF), facet::Rgb(0xFF00FF), facet::Rgb(0xFFFF00),
    facet::Rgb(0x777777), facet::Rgb(0x1C1C1E), facet::Rgb(0xF2F2F7), facet::Rgb(0x0066CC),
    facet::Rgb(0xD70015), facet::Rgb(0x123456, 0.5f), facet::Rgb(0xC0FFEE, 0.25f),
};

} // namespace

TEST_CASE("Rgb unpacks 0xRRGGBB into sRGB channels")
{
    const ImVec4 color = facet::Rgb(0xFF8000, 0.5f);
    CHECK(color.x == doctest::Approx(1.0f));
    CHECK(color.y == doctest::Approx(128.0f / 255.0f));
    CHECK(color.z == doctest::Approx(0.0f));
    CHECK(color.w == doctest::Approx(0.5f));
}

TEST_CASE("U32 packing round-trips")
{
    for (const ImVec4& sample : kSamples)
        CHECK(Near(facet::FromU32(facet::ToU32(sample)), sample, 0.5f / 255.0f + 1e-6f));
}

TEST_CASE("The sRGB transfer curve round-trips and hits its reference points")
{
    CHECK(facet::SrgbToLinear(0.0f) == doctest::Approx(0.0f));
    CHECK(facet::SrgbToLinear(1.0f) == doctest::Approx(1.0f));
    CHECK(facet::SrgbToLinear(0.5f) == doctest::Approx(0.21404f).epsilon(1e-4));
    for (int i = 0; i <= 255; ++i)
    {
        const float value = static_cast<float>(i) / 255.0f;
        CHECK(facet::LinearToSrgb(facet::SrgbToLinear(value)) == doctest::Approx(value).epsilon(1e-5));
    }
}

TEST_CASE("OKLab matches published reference values")
{
    const facet::Oklab white = facet::ToOklab(facet::Rgb(0xFFFFFF));
    CHECK(white.L == doctest::Approx(1.0f).epsilon(1e-4));
    CHECK(white.a == doctest::Approx(0.0f).epsilon(1e-4));
    CHECK(white.b == doctest::Approx(0.0f).epsilon(1e-4));

    const facet::Oklab red = facet::ToOklab(facet::Rgb(0xFF0000));
    CHECK(red.L == doctest::Approx(0.62796f).epsilon(1e-3));
    CHECK(red.a == doctest::Approx(0.22486f).epsilon(1e-3));
    CHECK(red.b == doctest::Approx(0.12585f).epsilon(1e-3));

    const facet::Oklab blue = facet::ToOklab(facet::Rgb(0x0000FF));
    CHECK(blue.L == doctest::Approx(0.45201f).epsilon(1e-3));
    CHECK(blue.a == doctest::Approx(-0.03246f).epsilon(1e-2));
    CHECK(blue.b == doctest::Approx(-0.31153f).epsilon(1e-3));
}

TEST_CASE("OKLab and OKLCH conversions round-trip")
{
    for (const ImVec4& sample : kSamples)
    {
        CAPTURE(sample.x);
        CAPTURE(sample.y);
        CAPTURE(sample.z);
        CHECK(Near(facet::FromOklab(facet::ToOklab(sample), sample.w), sample));
        CHECK(Near(facet::FromOklch(facet::ToOklch(sample), sample.w), sample));
        CHECK(Near(facet::LinearToSrgb(facet::OklabToLinear(
                       facet::LinearToOklab(facet::SrgbToLinear(sample)), sample.w)),
                   sample));
    }
}

TEST_CASE("Greys have no chroma and out-of-gamut OKLCH colors are mapped into sRGB")
{
    CHECK(facet::ToOklch(facet::Rgb(0x777777)).C == doctest::Approx(0.0f).epsilon(1e-4));

    const facet::Oklch tooVivid = {0.7f, 0.4f, 250.0f};
    const ImVec4 mapped = facet::FromOklch(tooVivid);
    const float channels[] = {mapped.x, mapped.y, mapped.z};
    for (const float channel : channels)
    {
        CHECK(channel >= 0.0f);
        CHECK(channel <= 1.0f);
    }
    const facet::Oklch back = facet::ToOklch(mapped);
    CHECK(back.L == doctest::Approx(0.7f).epsilon(0.01));
    CHECK(back.h == doctest::Approx(250.0f).epsilon(0.01));
    CHECK(back.C < tooVivid.C);
}

TEST_CASE("Mix interpolates in OKLab and keeps the endpoints")
{
    const ImVec4 a = facet::Rgb(0x0066CC, 1.0f);
    const ImVec4 b = facet::Rgb(0xFFD60A, 0.0f);
    CHECK(Near(facet::Mix(a, b, 0.0f), a));
    CHECK(Near(facet::Mix(a, b, 1.0f), b));
    const ImVec4 mid = facet::Mix(facet::Rgb(0x000000), facet::Rgb(0xFFFFFF), 0.5f);
    CHECK(facet::ToOklab(mid).L == doctest::Approx(0.5f).epsilon(1e-3));
    CHECK(facet::Mix(a, b, 0.5f).w == doctest::Approx(0.5f));
}

TEST_CASE("WithAlpha replaces or scales alpha")
{
    CHECK(facet::WithAlpha(facet::Rgb(0x112233), 0.25f).w == doctest::Approx(0.25f));
    CHECK(facet::WithAlpha(IM_COL32(10, 20, 30, 200), 0.5f) == IM_COL32(10, 20, 30, 100));
    CHECK(facet::WithAlpha(IM_COL32(10, 20, 30, 200), 2.0f) == IM_COL32(10, 20, 30, 255));
}

TEST_CASE("Over blends a translucent color onto an opaque one")
{
    const ImVec4 result = facet::Over(facet::Rgb(0xFFFFFF, 0.5f), facet::Rgb(0x000000));
    CHECK(Near(result, ImVec4(0.5f, 0.5f, 0.5f, 1.0f)));
    CHECK(Near(facet::Over(facet::Rgb(0x0066CC), facet::Rgb(0xFFFFFF)), facet::Rgb(0x0066CC)));
}

TEST_CASE("ContrastRatio matches known WCAG values")
{
    CHECK(facet::ContrastRatio(facet::Rgb(0x000000), facet::Rgb(0xFFFFFF)) == doctest::Approx(21.0f));
    CHECK(facet::ContrastRatio(facet::Rgb(0xFFFFFF), facet::Rgb(0x000000)) == doctest::Approx(21.0f));
    CHECK(facet::ContrastRatio(facet::Rgb(0x808080), facet::Rgb(0x808080)) == doctest::Approx(1.0f));
    // #777777 on white is the classic just-below-AA grey, #767676 just reaches it.
    CHECK(facet::ContrastRatio(facet::Rgb(0x777777), facet::Rgb(0xFFFFFF)) == doctest::Approx(4.48f).epsilon(0.01));
    CHECK(facet::ContrastRatio(facet::Rgb(0x767676), facet::Rgb(0xFFFFFF)) >= 4.5f);
    // A translucent foreground is composed over the background first.
    CHECK(facet::ContrastRatio(facet::Rgb(0x000000, 0.0f), facet::Rgb(0xFFFFFF)) == doctest::Approx(1.0f));
    CHECK(facet::RelativeLuminance(facet::Rgb(0xFFFFFF)) == doctest::Approx(1.0f));
    CHECK(facet::RelativeLuminance(facet::Rgb(0xFF0000)) == doctest::Approx(0.2126f).epsilon(1e-4));
}

TEST_CASE("ReadableOn picks the higher contrast of white and black")
{
    CHECK(Near(facet::ReadableOn(facet::Rgb(0x000000)), facet::Rgb(0xFFFFFF)));
    CHECK(Near(facet::ReadableOn(facet::Rgb(0xFFFFFF)), facet::Rgb(0x000000)));
    CHECK(Near(facet::ReadableOn(facet::Rgb(0xFFD60A)), facet::Rgb(0x000000)));
    for (int i = 0; i < 64; ++i)
    {
        const facet::Oklch lch = {static_cast<float>(i % 8) / 7.0f, 0.15f, static_cast<float>(i) * 45.0f};
        const ImVec4 background = facet::FromOklch(lch);
        CHECK(facet::ContrastRatio(facet::ReadableOn(background), background) >= 4.5f);
    }
}
