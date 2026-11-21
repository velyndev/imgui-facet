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

// ---- SatValSquare -------------------------------------------------------------------------------

TEST_CASE("SatValSquare moves on both axes with the arrows, fine with Shift and coarse with Ctrl")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 color = facet::internal::ColorFromHsv({0.6, 0.5, 0.5}, 0.75f);
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::SatValSquare("Square", &color, {.width = 160.0f}))
            ++writes;
        session.Track("square");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("square"));
    session.KeyPress(ImGuiKey_RightArrow);
    ColorHsv hsv = facet::internal::ColorToHsv(color);
    CHECK(hsv.s == doctest::Approx(0.51).epsilon(1e-4));
    CHECK(hsv.v == doctest::Approx(0.5).epsilon(1e-4));
    session.KeyPress(ImGuiKey_UpArrow);
    hsv = facet::internal::ColorToHsv(color);
    CHECK(hsv.v == doctest::Approx(0.51).epsilon(1e-4));
    session.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Shift);
    hsv = facet::internal::ColorToHsv(color);
    CHECK(hsv.v == doctest::Approx(0.509).epsilon(1e-4));
    session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Ctrl);
    hsv = facet::internal::ColorToHsv(color);
    CHECK(hsv.s == doctest::Approx(0.5).epsilon(1e-4));
    // Hue and alpha stay.
    CHECK(hsv.h == doctest::Approx(0.6).epsilon(1e-4));
    CHECK(color.w == 0.75f);
    CHECK(writes == 4);
}

TEST_CASE("SatValSquare keeps hue and saturation when the value goes to zero and back")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 color = facet::internal::ColorFromHsv({0.3, 0.7, 0.6}, 1.0f);
    session.SetUi([&] {
        (void)facet::SatValSquare("Square", &color, {.width = 160.0f});
        session.Track("square");
    });
    session.Frame();
    const ImRect r = session.ItemRect("square");
    // Down to the bottom edge (value 0, black), then up again.
    session.Drag({r.GetCenter(), ImVec2(r.GetCenter().x, r.Max.y + 20.0f), ImVec2(r.GetCenter().x, r.Min.y + r.GetHeight() * 0.25f)});
    const ColorHsv hsv = facet::internal::ColorToHsv(color);
    CHECK(hsv.h == doctest::Approx(0.3).epsilon(1e-3));
    CHECK(hsv.s > 0.3);
    CHECK(hsv.v > 0.5);
}

TEST_CASE("SatValSquare: a click puts the handle under the pointer and Escape restores the color")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImVec4 start = facet::internal::ColorFromHsv({0.1, 0.2, 0.3}, 1.0f);
    ImVec4 color = start;
    bool canceled = false;
    session.SetUi([&] {
        (void)facet::SatValSquare("Square", &color, {.width = 160.0f});
        canceled = canceled || facet::IsItemEditCanceled();
        session.Track("square");
    });
    session.Frame();
    const ImRect r = session.ItemRect("square");
    // The top right corner of the gradient: full saturation and value.
    session.Click(ImVec2(r.Max.x - 2.0f, r.Min.y + 2.0f));
    ColorHsv hsv = facet::internal::ColorToHsv(color);
    CHECK(hsv.s == doctest::Approx(1.0));
    CHECK(hsv.v == doctest::Approx(1.0));
    CHECK(hsv.h == doctest::Approx(0.1).epsilon(1e-4));
    // A drag, then Escape while the button is still down.
    const ImVec4 before = color;
    session.MouseMove(r.GetCenter());
    session.MouseDown();
    session.MouseMove(ImVec2(r.Min.x + 10.0f, r.Max.y - 10.0f));
    CHECK_FALSE(Same(color, before));
    session.KeyPress(ImGuiKey_Escape);
    session.MouseUp();
    CHECK(Same(color, before));
    CHECK(canceled);
}

// ---- ColorChannelSlider ---------------------------------------------------------------------------

TEST_CASE("ColorChannelSlider writes only its channel and steps with the slider keys")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 color(0.2f, 0.4f, 0.8f, 0.5f);
    session.SetUi([&] {
        (void)facet::ColorChannelSlider("Red", &color, {.channel = facet::ColorChannel::Red});
        session.Track("red");
        (void)facet::ColorChannelSlider("Alpha", &color, {.channel = facet::ColorChannel::Alpha});
        session.Track("alpha");
    });
    session.Frame();
    session.Click(session.ItemCenter("red"));
    session.KeyPress(ImGuiKey_Home);
    CHECK(color.x == 0.0f);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(color.x == doctest::Approx(0.01f));
    session.KeyPress(ImGuiKey_End);
    CHECK(color.x == 1.0f);
    CHECK(color.y == 0.4f);
    CHECK(color.z == 0.8f);
    CHECK(color.w == 0.5f);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("alpha"));
    session.KeyPress(ImGuiKey_PageUp);
    CHECK(color.w == doctest::Approx(0.6f));
    CHECK(color.x == 1.0f);
}

TEST_CASE("ColorChannelSlider wraps the hue and keeps it while the saturation passes through zero")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 color = facet::internal::ColorFromHsv({0.995, 0.8, 0.9}, 1.0f);
    session.SetUi([&] {
        (void)facet::ColorChannelSlider("Hue", &color, {.channel = facet::ColorChannel::Hue});
        session.Track("hue");
        (void)facet::ColorChannelSlider("Saturation", &color, {.channel = facet::ColorChannel::Saturation});
        session.Track("saturation");
    });
    session.Frame();
    session.Click(session.ItemCenter("hue"));
    // The hue was moved to the click; step from the end over the wrap.
    session.KeyPress(ImGuiKey_End);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(facet::internal::ColorToHsv(color).h == doctest::Approx(0.01).epsilon(1e-3));
    const double hue = facet::internal::ColorToHsv(color).h;
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("saturation"));
    session.KeyPress(ImGuiKey_Home);
    CHECK(facet::internal::ColorToHsv(color).s == 0.0);
    session.KeyPress(ImGuiKey_End);
    const ColorHsv back = facet::internal::ColorToHsv(color);
    CHECK(back.s == doctest::Approx(1.0));
    CHECK(back.h == doctest::Approx(hue).epsilon(1e-4));
}

TEST_CASE("A read-only or disabled ColorChannelSlider never writes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImVec4 start(0.2f, 0.4f, 0.8f, 1.0f);
    ImVec4 readOnly = start, disabled = start;
    session.SetUi([&] {
        (void)facet::ColorChannelSlider("Read-only", &readOnly, {.channel = facet::ColorChannel::Green, .readOnly = true});
        session.Track("readOnly");
        ImGui::BeginDisabled();
        (void)facet::ColorChannelSlider("Disabled", &disabled, {.channel = facet::ColorChannel::Green});
        session.Track("disabled");
        ImGui::EndDisabled();
    });
    session.Frame();
    session.Click(ImVec2(session.ItemRect("readOnly").Max.x - 4.0f, session.ItemRect("readOnly").GetCenter().y));
    session.KeyPress(ImGuiKey_End);
    session.Click(ImVec2(session.ItemRect("disabled").Max.x - 4.0f, session.ItemRect("disabled").GetCenter().y));
    CHECK(Same(readOnly, start));
    CHECK(Same(disabled, start));
}

// ---- ColorInputs ----------------------------------------------------------------------------------

TEST_CASE("ColorInputs writes a typed channel and leaves the others exactly as they were")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 color(0.2001f, 0.4002f, 0.8003f, 1.0f);
    ImGuiID inputs = 0;
    session.SetUi([&] {
        (void)facet::ColorInputs("##inputs", &color, {.modeSwitch = false});
        inputs = ImGui::GetItemID();
        session.Track("inputs");
    });
    session.Frame();
    // Every channel is a Tab stop: the red, green and blue fields in order.
    const ImRect r = session.ItemRect("inputs");
    session.Click(ImVec2(r.Min.x + 30.0f, r.GetCenter().y));
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.Type("128");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(color.x == 128.0f / 255.0f);
    CHECK(color.y == 0.4002f);
    CHECK(color.z == 0.8003f);
    // After a click the first Tab shows the focus where it is, the next ones move it.
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == PartId(inputs, "R##r"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == PartId(inputs, "G##g"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == PartId(inputs, "B##b"));
}

TEST_CASE("ColorInputs parses pasted color text into the whole color")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 color(0.0f, 0.0f, 0.0f, 1.0f);
    session.SetUi([&] {
        (void)facet::ColorInputs("##inputs", &color, {.model = facet::ColorModel::Hsv, .modeSwitch = false});
        session.Track("inputs");
    });
    session.Frame();
    const ImRect r = session.ItemRect("inputs");
    session.Click(ImVec2(r.Max.x - 10.0f, r.GetCenter().y));
    session.KeyPress(ImGuiKey_Escape);
    session.SetClipboard("rgb(51, 102, 255)");
    session.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    CHECK(color.x == 51.0f / 255.0f);
    CHECK(color.y == 102.0f / 255.0f);
    CHECK(color.z == 1.0f);
    // A single number is not a color: nothing changes.
    session.SetClipboard("42");
    session.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    CHECK(color.z == 1.0f);
}

TEST_CASE("ColorInputs in the hex model writes the typed digits and keeps unchanged channels")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 color(0.2001f, 0.4f, 1.0f, 1.0f);
    session.SetUi([&] {
        (void)facet::ColorInputs("##inputs", &color, {.model = facet::ColorModel::Hex, .modeSwitch = false});
        session.Track("inputs");
    });
    session.Frame();
    session.Click(session.ItemCenter("inputs"));
    session.KeyPress(ImGuiKey_Home);
    session.Type("FF");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(color.x == 1.0f);
    CHECK(color.y == 0.4f);
    CHECK(color.z == 1.0f);
}
