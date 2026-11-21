// Behaviour of the color pickers (ColorWell, ColorPicker, ColorInputs, SwatchPalette,
// SatValSquare, ColorChannelSlider, ColorModeEditor and SampleAnimatedColor in facet/widgets/pickers.h)
// and of their shared color math (src/widgets/pickers/color_model.h).
//
// One test case per behaviour; docs/writing-a-widget.md describes the pattern. The API checks are in
// tests/test_api_pickers_color.cpp.

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/popup_host.h"
#include "support/session.h"
#include "widgets/pickers/color_model.h"

namespace
{

using facet::internal::ColorHsv;

// Colors that are not on the 8-bit grid, greys and the corners of the cube.
const ImVec4 kSamples[] = {
    {0.2f, 0.4f, 0.8f, 1.0f},     {0.8123f, 0.3001f, 0.2f, 0.5f}, {0.5f, 0.5f, 0.5f, 1.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},     {1.0f, 1.0f, 1.0f, 1.0f},       {1.0f, 0.0f, 0.0f, 1.0f},
    {0.123f, 0.987f, 0.456f, 1.0f}, {0.9f, 0.9f, 0.1f, 0.25f},
};

bool Same(const ImVec4& a, const ImVec4& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

bool Near(const ImVec4& a, const ImVec4& b, float tolerance)
{
    return std::fabs(a.x - b.x) <= tolerance && std::fabs(a.y - b.y) <= tolerance && std::fabs(a.z - b.z) <= tolerance &&
           std::fabs(a.w - b.w) <= tolerance;
}

[[maybe_unused]] ImGuiID PartId(ImGuiID widget, const char* part)
{
    return ImHashStr(part, 0, widget);
}

// The motion settings a test changes, restored at the end.
struct MotionGuard
{
    facet::Theme saved = facet::T();
    bool follow = facet::FollowsSystemMotion();
    MotionGuard() { facet::SetFollowSystemMotion(false); }
    ~MotionGuard()
    {
        facet::SetTheme(saved, false);
        facet::SetFollowSystemMotion(follow);
    }
    static void Reduce(bool reduce)
    {
        facet::Theme theme = facet::T();
        theme.motion.reduceMotion = reduce;
        facet::SetTheme(theme, false);
    }
};

} // namespace

// ---- Color math ---------------------------------------------------------------------------------

TEST_CASE("HSV and HSL conversions give back the exact color")
{
    for (const ImVec4& c : kSamples)
    {
        CAPTURE(c.x);
        CAPTURE(c.y);
        CAPTURE(c.z);
        CHECK(Same(facet::internal::ColorFromHsv(facet::internal::ColorToHsv(c), c.w), c));
        CHECK(Same(facet::internal::ColorFromHsl(facet::internal::ColorToHsl(c), c.w), c));
    }
}

TEST_CASE("Hex, RGB, HSV, HSL and OKLCH entries of one color agree")
{
    ImVec4 fromHex, fromRgb, fromHsv, fromHsl, fromBare;
    REQUIRE(facet::internal::ColorParseText("#3366FF", &fromHex, nullptr));
    REQUIRE(facet::internal::ColorParseText("rgb(51, 102, 255)", &fromRgb, nullptr));
    REQUIRE(facet::internal::ColorParseText("51 102 255", &fromBare, nullptr));
    REQUIRE(facet::internal::ColorParseText("hsv(225, 80%, 100%)", &fromHsv, nullptr));
    REQUIRE(facet::internal::ColorParseText("hsl(225, 100%, 60%)", &fromHsl, nullptr));
    CHECK(Same(fromHex, fromRgb));
    CHECK(Same(fromHex, fromBare));
    CHECK(Near(fromHex, fromHsv, 1.0f / 255.0f));
    CHECK(Near(fromHex, fromHsl, 1.0f / 255.0f));
    // OKLCH of the same color goes back to it.
    CHECK(Near(facet::FromOklch(facet::ToOklch(fromHex), 1.0f), fromHex, 1e-4f));
    char hex[16];
    facet::internal::ColorFormatHex(hex, sizeof(hex), fromRgb, false);
    CHECK(std::string(hex) == "#3366FF");
    facet::internal::ColorFormatHex(hex, sizeof(hex), facet::internal::ColorFromHsv(facet::internal::ColorToHsv(fromHex), 0.5f), true);
    CHECK(std::string(hex) == "#3366FF80");
}

TEST_CASE("Color text reads the short and alpha forms and rejects single numbers")
{
    ImVec4 c;
    bool alpha = false;
    REQUIRE(facet::internal::ColorParseText("#36f", &c, &alpha));
    CHECK(c.x == 0x33 / 255.0f);
    CHECK_FALSE(alpha);
    REQUIRE(facet::internal::ColorParseText("  #3366ff80 ", &c, &alpha));
    CHECK(alpha);
    CHECK(c.w == 0x80 / 255.0f);
    REQUIRE(facet::internal::ColorParseText("0.2, 0.4, 1", &c, &alpha));
    CHECK(c.x == 0.2f);
    CHECK(c.z == 1.0f);
    REQUIRE(facet::internal::ColorParseText("rgba(51, 102, 255, 0.5)", &c, &alpha));
    CHECK(c.w == 0.5f);
    CHECK(alpha);
    REQUIRE(facet::internal::ColorParseText("aabbcc", &c, &alpha));
    CHECK(c.x == 0xAA / 255.0f);
    const ImVec4 untouched(0.1f, 0.2f, 0.3f, 0.4f);
    for (const char* text : {"128", "123456", "hello", "", "rgb(1, 2)", "#12345", "1 2 3 4 5"})
    {
        CAPTURE(text);
        c = untouched;
        CHECK_FALSE(facet::internal::ColorParseText(text, &c, nullptr));
        CHECK(Same(c, untouched));
    }
}

TEST_CASE("The hue memory keeps the hue at grey and the saturation at black")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ColorHsv grey, black, outside;
    session.SetUi([&] {
        const ImGuiID id = ImGui::GetID("memory");
        const ImVec4 blue(0.2f, 0.4f, 0.8f, 1.0f);
        const ColorHsv start = facet::internal::ColorTrackHsv(id, blue);
        ColorHsv meant = start;
        meant.v = 0.0;
        facet::internal::ColorRememberHsv(id, facet::internal::ColorFromHsv(meant, 1.0f), meant);
        black = facet::internal::ColorTrackHsv(id, facet::internal::ColorFromHsv(meant, 1.0f));
        // A grey set from outside keeps the remembered hue.
        grey = facet::internal::ColorTrackHsv(id, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        outside = facet::internal::ColorTrackHsv(ImGui::GetID("fresh"), ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    });
    session.Frame();
    const ColorHsv blue = facet::internal::ColorToHsv(ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
    CHECK(black.h == doctest::Approx(blue.h));
    CHECK(black.s == doctest::Approx(blue.s));
    CHECK(black.v == 0.0);
    CHECK(grey.h == doctest::Approx(blue.h));
    CHECK(grey.s == 0.0);
    CHECK(outside.h == 0.0);
}
