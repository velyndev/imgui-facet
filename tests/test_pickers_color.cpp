// Behaviour of the color pickers (ColorWell, ColorPicker, ColorInputs, SwatchPalette,
// SatValSquare, ColorChannelSlider, ColorModeEditor and SampleAnimatedColor in facet/widgets/pickers.h)
// and of their shared color math (src/widgets/pickers/color_model.h).
//
// One test case per behaviour; docs/writing-a-widget.md describes the pattern. The API checks are in
// tests/test_api_pickers_color.cpp.

#include <algorithm>
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

// ---- SwatchPalette --------------------------------------------------------------------------------

TEST_CASE("SwatchPalette picks by click and by keys in two dimensions, and records the history")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const ImVec4 kSwatches[] = {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1}, {0, 1, 1, 1}, {1, 0, 1, 1}};
    ImVec4 color(0.5f, 0.5f, 0.5f, 0.25f);
    facet::ColorHistory history;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        (void)facet::SwatchPalette("Palette", &color, std::span<const ImVec4>(kSwatches), &history, {.columns = 3});
        session.Track("palette");
    });
    session.Frame();
    const ImRect r = session.ItemRect("palette");
    const float side = facet::ControlHeight(facet::ControlSize::Inherit);
    // The second swatch of the first row.
    session.Click(ImVec2(r.Min.x + side * 1.5f + facet::Px(4.0f), r.Min.y + side * 0.5f));
    CHECK(color.y == 1.0f);
    CHECK(color.x == 0.0f);
    CHECK(color.w == 0.25f);  // without alpha, the alpha stays
    CHECK(history.count == 1);
    // The history row now sits above the grid. Keys: Down from the history goes to the grid.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("palette"));
    session.KeyPress(ImGuiKey_Home);
    session.KeyPress(ImGuiKey_DownArrow);  // grid row 1, column 1
    session.KeyPress(ImGuiKey_DownArrow);  // grid row 2, column 1
    session.KeyPress(ImGuiKey_RightArrow); // the fifth swatch
    session.KeyPress(ImGuiKey_Enter);
    CHECK(color.x == 0.0f);
    CHECK(color.y == 1.0f);
    CHECK(color.z == 1.0f);
    CHECK(history.count == 2);
    CHECK(Same(history.colors[0], ImVec4(0, 1, 1, 0.25f)));
}

TEST_CASE("SwatchPalette shows a swatch's name on hover and on keyboard focus")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const ImVec4 kSwatches[] = {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}};
    static const char* const kNames[] = {"Placeholder red", nullptr, "Placeholder blue"};
    ImVec4 color(0.5f, 0.5f, 0.5f, 1.0f);
    bool tooltip = false;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        // No visible label, so the palette has no tooltip of its own.
        (void)facet::SwatchPalette("##palette", &color, std::span<const ImVec4>(kSwatches), nullptr,
                                   {.columns = 3, .names = kNames});
        session.Track("palette");
        tooltip = facet::internal::IsPopupLevelOpen(facet::internal::PopupTooltipLevel(session.ItemId("palette")));
    });
    session.Frame();
    const ImRect r = session.ItemRect("palette");
    const float side = facet::ControlHeight(facet::ControlSize::Inherit);
    // Hover: the first swatch has a name, the second none.
    session.MouseMove(ImVec2(r.Min.x + side * 0.5f, r.Min.y + side * 0.5f));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK(tooltip);
    session.MouseMove(ImVec2(r.Min.x + side * 1.5f + facet::Px(4.0f), r.Min.y + side * 0.5f));
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK_FALSE(tooltip);
    // Keyboard focus on the third swatch shows its name without hovering.
    session.MouseMove(ImVec2(r.Max.x + facet::Px(200.0f), r.Max.y + facet::Px(200.0f)));
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("palette"));
    session.KeyPress(ImGuiKey_End);
    session.Seconds(facet::T().overlay.tooltipDelay + 0.2f);
    CHECK(tooltip);
}

TEST_CASE("The editable SwatchPalette adds, removes and moves swatches in the caller's storage")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::array<ImVec4, 3> storage = {ImVec4(1, 0, 0, 1), ImVec4(0, 1, 0, 1), ImVec4()};
    int count = 2;
    ImVec4 color(0.0f, 0.0f, 1.0f, 1.0f);
    std::vector<ImVec4> vector = {ImVec4(1, 0, 0, 1)};
    session.SetUi([&] {
        (void)facet::SwatchPalette("Palette", &color, std::span<ImVec4>(storage), &count, nullptr, {.columns = 4});
        session.Track("palette");
        (void)facet::SwatchPalette("Vector", &color, &vector, nullptr, {.columns = 4});
        session.Track("vector");
    });
    session.Frame();
    const ImRect r = session.ItemRect("palette");
    const float side = facet::ControlHeight(facet::ControlSize::Inherit);
    const float step = side + facet::Px(4.0f);
    // The + cell is third: it adds the color.
    session.Click(ImVec2(r.Min.x + step * 2.0f + side * 0.5f, r.GetCenter().y));
    CHECK(count == 3);
    CHECK(Same(storage[2], color));
    // Full: no + cell any more, so a click there picks nothing new.
    session.Click(ImVec2(r.Min.x + step * 3.0f + side * 0.5f, r.GetCenter().y));
    CHECK(count == 3);
    // Alt+Left moves the focused swatch, Delete removes it.
    session.Click(ImVec2(r.Min.x + step * 2.0f + side * 0.5f, r.GetCenter().y));
    session.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Alt);
    CHECK(Same(storage[1], color));
    session.KeyPress(ImGuiKey_Delete);
    CHECK(count == 2);
    CHECK(Same(storage[1], ImVec4(0, 1, 0, 1)));
    // The vector form grows.
    const ImRect v = session.ItemRect("vector");
    session.Click(ImVec2(v.Min.x + step + side * 0.5f, v.GetCenter().y));
    CHECK(vector.size() == 2);
}

// ---- ColorPicker ------------------------------------------------------------------------------------

TEST_CASE("ColorPicker parts are Tab stops: square, hue and alpha bars, then the compare swatch")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 color(0.2f, 0.4f, 0.8f, 0.5f);
    const ImVec4 original(0.9f, 0.1f, 0.1f, 1.0f);
    ImGuiID picker = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        picker = ImGui::GetID("Picker");
        (void)facet::ColorPicker("Picker", &color, {.alpha = true, .inputs = false, .original = &original});
        session.Track("picker");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == PartId(picker, "##sv"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == PartId(picker, "##hue"));
    // The hue moves, saturation and value stay.
    const ColorHsv before = facet::internal::ColorToHsv(color);
    session.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    const ColorHsv after = facet::internal::ColorToHsv(color);
    CHECK(after.h != doctest::Approx(before.h));
    CHECK(after.s == doctest::Approx(before.s).epsilon(1e-4));
    CHECK(after.v == doctest::Approx(before.v).epsilon(1e-4));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == PartId(picker, "##alpha"));
    session.KeyPress(ImGuiKey_Home);
    CHECK(color.w == 0.0f);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == PartId(picker, "##original"));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(Same(color, original));
}

TEST_CASE("ColorPicker in the Wheel mode turns the hue on the ring and picks on the triangle")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 color = facet::internal::ColorFromHsv({0.25, 0.5, 0.5}, 1.0f);
    ImGuiID picker = 0;
    session.SetUi([&] {
        picker = ImGui::GetID("Wheel");
        (void)facet::ColorPicker("Wheel", &color, {.mode = facet::ColorPickerMode::Wheel, .inputs = false, .width = 200.0f});
        session.Track("wheel");
    });
    session.Frame();
    const ImRect r = session.ItemRect("wheel");
    const float side = r.GetWidth();
    const ImVec2 center(r.Min.x + side * 0.5f, r.Min.y + side * 0.5f);
    // The top of the ring is hue 0; the triangle around the centre keeps the hue.
    session.Click(ImVec2(center.x, r.Min.y + facet::Px(4.0f)));
    CHECK(facet::internal::ColorToHsv(color).h == doctest::Approx(0.0).epsilon(0.02));
    CHECK(ImGui::GetFocusID() == PartId(picker, "##hue"));
    session.Click(center);
    CHECK(ImGui::GetFocusID() == PartId(picker, "##sv"));
    const ColorHsv hsv = facet::internal::ColorToHsv(color);
    CHECK(hsv.h == doctest::Approx(0.0).epsilon(0.02));
    CHECK(hsv.v > 0.2);
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    CHECK(ImGui::GetFocusID() == PartId(picker, "##hue"));
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(facet::internal::ColorToHsv(color).h == doctest::Approx(0.99).epsilon(0.02));
}

// ---- ColorWell ------------------------------------------------------------------------------------

TEST_CASE("ColorWell opens its picker, follows it, and Escape restores the color and the focus")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const ImVec4 start(0.2f, 0.4f, 0.8f, 1.0f);
    ImVec4 color = start;
    ImGuiID well = 0;
    bool open = false;
    ImRect levelRect;
    session.SetUi([&] {
        well = ImGui::GetID("Well");
        (void)facet::ColorWell("Well", &color);
        session.Track("well");
        // The host answers inside a frame only.
        const ImGuiID level = PartId(well, "##popover");
        open = facet::internal::IsPopupLevelOpen(level);
        levelRect = facet::internal::PopupLevelRect(level);
    });
    session.Frame();
    // The swatch frame after the label.
    const ImRect wellRect = session.ItemRect("well");
    session.Click(ImVec2(wellRect.Max.x - 8.0f, wellRect.GetCenter().y));
    session.Frames(2);
    CHECK(open);
    // The level sits below the well.
    CHECK(levelRect.Min.y >= session.ItemRect("well").Max.y);
    // The focus went into the picker: the square takes the arrows.
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK_FALSE(Same(color, start));
    session.KeyPress(ImGuiKey_Escape);
    session.Frames(2);
    CHECK_FALSE(open);
    CHECK(Same(color, start));
    CHECK(ImGui::GetFocusID() == well);
    // Enter opens it again from the keyboard.
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(open);
}

TEST_CASE("ColorWell copies and pastes hex text on Ctrl+C and Ctrl+V")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    ImVec4 color(0.2f, 0.4f, 1.0f, 1.0f);
    ImVec4 readOnly(0.2f, 0.4f, 1.0f, 1.0f);
    ImGuiID top = 0;
    session.SetUi([&] {
        top = facet::internal::TopPopupLevel();
        ImGui::Button("before");
        session.Track("before");
        (void)facet::ColorWell("Well", &color);
        session.Track("well");
        (void)facet::ColorWell("Read-only", &readOnly, {.readOnly = true});
        session.Track("readOnly");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("well"));
    session.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    CHECK(session.Clipboard() == "#3366FF");
    session.SetClipboard("#FF8000");
    session.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    CHECK(color.x == 1.0f);
    CHECK(color.y == 128.0f / 255.0f);
    CHECK(color.z == 0.0f);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("readOnly"));
    session.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    CHECK(readOnly.x == 0.2f);
    session.KeyPress(ImGuiKey_Enter);
    session.Frames(2);
    CHECK(top == 0);
}

// ---- ColorModeEditor and SampleAnimatedColor --------------------------------------------------------

TEST_CASE("SampleAnimatedColor pulses, sweeps and cycles, and stands still while motion is reduced")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    MotionGuard guard;
    MotionGuard::Reduce(false);
    const ImVec4 base(0.2f, 0.4f, 0.8f, 1.0f);
    const ImVec4 second(0.9f, 0.6f, 0.1f, 1.0f);
    facet::AnimatedColor pulse{facet::ColorAnimation::Pulse, base, second, 2.0f};
    CHECK(Near(facet::SampleAnimatedColor(pulse, 0.0), base, 1e-5f));
    CHECK(Near(facet::SampleAnimatedColor(pulse, 1.0), second, 1e-4f));
    CHECK(Near(facet::SampleAnimatedColor(pulse, 2.0), base, 1e-5f));
    facet::AnimatedColor sweep{facet::ColorAnimation::Gradient, base, second, 4.0f};
    CHECK(Near(facet::SampleAnimatedColor(sweep, 2.0), second, 1e-4f));
    CHECK(Near(facet::SampleAnimatedColor(sweep, 1.0), facet::Mix(base, second, 0.5f), 1e-4f));
    facet::AnimatedColor cycle{facet::ColorAnimation::Cycle, base, second, 3.0f};
    const facet::Oklch start = facet::ToOklch(base);
    const facet::Oklch half = facet::ToOklch(facet::SampleAnimatedColor(cycle, 1.5));
    CHECK(half.L == doctest::Approx(start.L).epsilon(0.02));
    CHECK(std::fabs(std::fmod(half.h - start.h + 360.0f, 360.0f) - 180.0f) < 20.0f);
    MotionGuard::Reduce(true);
    CHECK(Same(facet::SampleAnimatedColor(pulse, 1.0), base));
    CHECK(Same(facet::SampleAnimatedColor(cycle, 1.5), base));
}

TEST_CASE("ColorModeEditor switches the animation by keys and shows the fields it needs")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::AnimatedColor animated{facet::ColorAnimation::Static, ImVec4(0.2f, 0.4f, 0.8f, 1.0f), ImVec4(1, 1, 1, 1), 2.0f};
    ImGuiID editor = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        editor = ImGui::GetID("Mode");
        (void)facet::ColorModeEditor("Mode", &animated);
        session.Track("editor");
    });
    session.Frame();
    const float staticHeight = session.ItemRect("editor").GetHeight();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(animated.animation == facet::ColorAnimation::Pulse);
    // Pulse adds the second well and the period field.
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == PartId(editor, "##color"));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == PartId(editor, "##second"));
    session.KeyPress(ImGuiKey_Tab);
    char period[64];
    std::snprintf(period, sizeof(period), "%s##period", facet::GetStrings().period);
    CHECK(ImGui::GetFocusID() == PartId(editor, period));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(animated.period > 2.0f);
    CHECK(session.ItemRect("editor").GetHeight() >= staticHeight);
}

TEST_CASE("ColorModeEditor makes room in its period field for the unit of Strings::seconds")
{
    // The width of the focused period field with `unit` as the seconds string.
    const auto fieldWidth = [](const char* unit) {
        facet::Strings strings;
        strings.seconds = unit;
        facet::SetStrings(strings);
        float width = 0.0f;
        {
            facet::test::Session session;
            REQUIRE(session.Ready());
            facet::AnimatedColor animated{facet::ColorAnimation::Pulse, ImVec4(0.2f, 0.4f, 0.8f, 1.0f), ImVec4(1, 1, 1, 1),
                                          2.0f};
            ImGuiID editor = 0;
            session.SetUi([&] {
                ImGui::Button("before");
                session.Track("before");
                editor = ImGui::GetID("Mode");
                (void)facet::ColorModeEditor("Mode", &animated);
            });
            session.Frame();
            session.Click(session.ItemCenter("before"));
            // As in the test above: two presses reach the animation, then the two wells, then the period.
            for (int i = 0; i < 5; ++i)
                session.KeyPress(ImGuiKey_Tab);
            char period[64];
            std::snprintf(period, sizeof(period), "%s##period", facet::GetStrings().period);
            REQUIRE(ImGui::GetFocusID() == PartId(editor, period));
            width = GImGui->NavWindow->NavRectRel[0].GetWidth();
        }
        facet::SetStrings(facet::Strings{});
        return width;
    };
    const float shortUnit = fieldWidth("s");
    const float longUnit = fieldWidth("seconds%");
    CHECK(shortUnit > 0.0f);
    CHECK(longUnit > shortUnit);
}

TEST_CASE("Color handles are solid knobs: the knob color with the picked color inside, hover changes only the outline color")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::Theme saved = facet::T();
    struct Restore
    {
        facet::Theme theme;
        ~Restore() { facet::SetTheme(theme, false); }
    } restore{saved};
    facet::Theme theme = facet::Theme::Dark();
    // A knob color nothing else on the square uses, so its vertices can be counted.
    theme.colors.knob = facet::Rgb(0x00FF7F);
    facet::SetTheme(theme, false);
    ImVec4 color(0.2f, 0.4f, 0.8f, 1.0f);
    std::vector<ImDrawVert> drawn;
    session.SetUi([&] {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const int from = drawList->VtxBuffer.Size;
        (void)facet::SatValSquare("Square", &color, {.width = 160.0f});
        session.Track("square");
        drawn.assign(drawList->VtxBuffer.Data + from, drawList->VtxBuffer.Data + drawList->VtxBuffer.Size);
    });
    session.Frames(2);
    const ImRect rect = session.ItemRect("square");
    const float settle = facet::T().motion.durationFast + 0.1f;
    const auto count = [&](ImU32 color32) {
        int n = 0;
        for (const ImDrawVert& v : drawn)
            n += v.col == color32 ? 1 : 0;
        return n;
    };
    session.MouseMove(ImVec2(rect.Max.x + 200.0f, rect.Max.y + 100.0f));
    session.Seconds(settle);
    const std::vector<ImDrawVert> rest = drawn;
    CHECK(count(facet::ToU32(facet::T().colors.knob)) > 0);
    CHECK(count(facet::ToU32(facet::T().colors.controlBorder)) > 0);
    // Hover anywhere on the square: the handle keeps its size, its outline turns controlBorderHover.
    session.MouseMove(ImVec2(rect.Min.x + 10.0f, rect.Min.y + 10.0f));
    session.Seconds(settle);
    REQUIRE(drawn.size() == rest.size());
    bool samePlaces = true;
    for (std::size_t i = 0; i < rest.size(); ++i)
        samePlaces = samePlaces && std::fabs(drawn[i].pos.x - rest[i].pos.x) < 0.01f && std::fabs(drawn[i].pos.y - rest[i].pos.y) < 0.01f;
    CHECK(samePlaces);
    CHECK(count(facet::ToU32(facet::T().colors.controlBorderHover)) > 0);
}
TEST_CASE("A color handle keeps an edge of 3:1 against white and black under it in both themes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::Theme saved = facet::T();
    const ImVec4 planes[] = {ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec4(0.0f, 0.0f, 0.0f, 1.0f)};
    for (const bool dark : {false, true})
    {
        CAPTURE(dark);
        facet::SetTheme(dark ? facet::Theme::Dark() : facet::Theme::Light(), false);
        const facet::Colors& c = facet::T().colors;
        for (const ImVec4& plane : planes)
        {
            CAPTURE(plane.x);
            for (const bool disabled : {false, true})
            {
                CAPTURE(disabled);
                // The outermost marks of the handle: the rim in the knob token and the hairline.
                const ImVec4 edge = facet::internal::ColorHandleEdge(plane, 0.0f, disabled);
                const float best = std::max(facet::ContrastRatio(edge, plane), facet::ContrastRatio(c.knob, plane));
                CHECK(best >= 3.0f);
            }
            // The drawn handle carries that hairline.
            const ImU32 edge = facet::ToU32(facet::internal::ColorHandleEdge(plane, 0.0f, false));
            int edgeVertices = 0;
            session.SetUi([&] {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const int from = drawList->VtxBuffer.Size;
                const ImVec2 origin = ImGui::GetCursorScreenPos();
                const ImVec2 center(origin.x + 40.0f, origin.y + 40.0f);
                facet::internal::DrawColorHandle(drawList, center, 10.0f, plane, 0.0f, 0.0f, 0.0f, false);
                edgeVertices = 0;
                for (int i = from; i < drawList->VtxBuffer.Size; ++i)
                    edgeVertices += drawList->VtxBuffer[i].col == edge ? 1 : 0;
            });
            session.Frames(2);
            CHECK(edgeVertices > 0);
        }
    }
    facet::SetTheme(saved, false);
}