// Behaviour of the media family (facet/widgets/media.h).
//
// One test case per behaviour; docs/writing-a-widget.md describes the pattern. The API checks are in
// tests/test_api_media.cpp. Images are created in code from plain pixels.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/drag_drop.h"
#include "internal/focus.h"
#include "support/session.h"
#include "widgets/media/media_look.h"

namespace
{

// Restores the theme a test changes.
struct ThemeGuard
{
    facet::Theme saved = facet::T();
    ~ThemeGuard() { facet::SetTheme(saved, false); }
};

// Turns animations off for the guard's lifetime, so motion lands at once.
struct ReducedMotion : ThemeGuard
{
    ReducedMotion()
    {
        facet::Theme theme = facet::T();
        theme.motion.reduceMotion = true;
        facet::SetTheme(theme, false);
    }
};

// A plain image of `width` by `height` pixels in one color.
facet::ImageHandle MakeImage(int width, int height, std::uint8_t gray = 128)
{
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4u, gray);
    for (std::size_t i = 3; i < pixels.size(); i += 4)
        pixels[i] = 255;
    return facet::CreateImageRGBA(pixels.data(), width, height);
}

ImVec2 At(const ImRect& r, float fx, float fy)
{
    return ImVec2(r.Min.x + r.GetWidth() * fx, r.Min.y + r.GetHeight() * fy);
}

} // namespace

// ---- Shared parts ------------------------------------------------------------------------------

TEST_CASE("Media initials take the first letter of the first and the last word")
{
    using facet::internal::MediaInitialsOf;
    CHECK(std::string(MediaInitialsOf("placeholder name").text) == "PN");
    CHECK(std::string(MediaInitialsOf("Placeholder middle last").text) == "PL");
    CHECK(std::string(MediaInitialsOf("  single  ").text) == "S");
    CHECK(std::string(MediaInitialsOf("").text).empty());
    CHECK(std::string(MediaInitialsOf("   ").text).empty());
    CHECK(std::string(MediaInitialsOf(nullptr).text).empty());
    // A letter of more than one byte is kept whole.
    CHECK(std::string(MediaInitialsOf("\xC3\xA9t\xC3\xA9 \xC3\xBC" "ber").text) == "\xC3\xA9\xC3\xBC");
}

TEST_CASE("Media fit rectangles land on whole pixels and keep an image at its own size exact")
{
    using facet::internal::MediaFitRect;
    ImVec2 uv0;
    ImVec2 uv1;
    // Contain: a 40 by 20 image in a 100 by 100 box is letterboxed, centered, on whole pixels.
    ImRect r = MediaFitRect(ImRect(10.3f, 10.6f, 110.3f, 110.6f), ImVec2(40.0f, 20.0f), facet::ImageFit::Contain, &uv0, &uv1);
    CHECK(r.Min.x == std::round(r.Min.x));
    CHECK(r.Min.y == std::round(r.Min.y));
    CHECK(r.GetWidth() == doctest::Approx(100.0f));
    CHECK(r.GetHeight() == doctest::Approx(50.0f));
    // An image exactly as large as the box keeps its texel size.
    r = MediaFitRect(ImRect(5.0f, 5.0f, 45.0f, 25.0f), ImVec2(40.0f, 20.0f), facet::ImageFit::Contain, &uv0, &uv1);
    CHECK(r.GetWidth() == 40.0f);
    CHECK(r.GetHeight() == 20.0f);
    // Cover: the box, cropped by the UV range; portrait and landscape crop the other axis.
    r = MediaFitRect(ImRect(0.0f, 0.0f, 100.0f, 100.0f), ImVec2(200.0f, 100.0f), facet::ImageFit::Cover, &uv0, &uv1);
    CHECK(r.GetWidth() == 100.0f);
    CHECK(uv0.x == doctest::Approx(0.25f));
    CHECK(uv1.x == doctest::Approx(0.75f));
    CHECK(uv0.y == doctest::Approx(0.0f));
    r = MediaFitRect(ImRect(0.0f, 0.0f, 100.0f, 100.0f), ImVec2(50.0f, 100.0f), facet::ImageFit::Cover, &uv0, &uv1);
    CHECK(uv0.y == doctest::Approx(0.25f));
    CHECK(uv0.x == doctest::Approx(0.0f));
    // A tiny image is scaled up to the box, never below one pixel.
    r = MediaFitRect(ImRect(0.0f, 0.0f, 64.0f, 32.0f), ImVec2(1.0f, 1.0f), facet::ImageFit::Contain, &uv0, &uv1);
    CHECK(r.GetWidth() == 32.0f);
    CHECK(r.GetHeight() == 32.0f);
}

TEST_CASE("A media spring settles on its target in the same time at any frame rate")
{
    ThemeGuard guard;
    facet::Theme theme = facet::T();
    theme.motion.reduceMotion = false;
    facet::SetTheme(theme, false);
    facet::SetFollowSystemMotion(false);
    const auto run = [&](float dt) {
        facet::internal::MediaSpringValue v;
        facet::internal::MediaSpringStep(v, 0.0f, theme.motion.standard, dt);  // starts at 0
        float at = 0.0f;
        for (float time = 0.0f; time < 0.2f - 1e-4f; time += dt)
        {
            facet::internal::MediaSpringStep(v, 100.0f, theme.motion.standard, dt);
            at = v.position;
        }
        for (int i = 0; i < static_cast<int>(2.0f / dt); ++i)
            facet::internal::MediaSpringStep(v, 100.0f, theme.motion.standard, dt);
        return std::pair<float, float>(at, v.position);
    };
    const auto slow = run(1.0f / 30.0f);
    const auto fast = run(1.0f / 240.0f);
    CHECK(slow.first == doctest::Approx(fast.first).epsilon(0.02));
    CHECK(slow.second == 100.0f);
    CHECK(fast.second == 100.0f);
    facet::SetFollowSystemMotion(true);
}

// ---- ImageView ---------------------------------------------------------------------------------

TEST_CASE("An image view takes its image's size, keeps the aspect ratio and lands on whole pixels")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ImageHandle landscape = MakeImage(40, 20);
    const facet::ImageHandle portrait = MakeImage(10, 30);
    REQUIRE(facet::IsImageReady(landscape));
    session.SetUi([&] {
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 0.4f, ImGui::GetCursorPosY() + 0.3f));
        facet::ImageView("own", landscape);
        session.Track("own");
        facet::ImageView("wide", landscape, {.width = 80.0f});
        session.Track("wide");
        facet::ImageView("tall", portrait, {.height = 60.0f});
        session.Track("tall");
        facet::ImageView("box", portrait, {.fit = facet::ImageFit::Cover, .width = 50.0f, .height = 50.0f});
        session.Track("box");
        facet::ImageView("fill", landscape, {.width = facet::kFill});
        session.Track("fill");
    });
    session.Frame();
    const float s = facet::Px(1.0f);
    CHECK(session.ItemRect("own").GetWidth() == 40.0f);
    CHECK(session.ItemRect("own").GetHeight() == 20.0f);
    CHECK(session.ItemRect("own").Min.x == std::round(session.ItemRect("own").Min.x));
    CHECK(session.ItemRect("own").Min.y == std::round(session.ItemRect("own").Min.y));
    CHECK(session.ItemRect("wide").GetWidth() == doctest::Approx(std::round(80.0f * s)));
    CHECK(session.ItemRect("wide").GetHeight() == doctest::Approx(std::round(40.0f * s)));
    CHECK(session.ItemRect("tall").GetWidth() == doctest::Approx(std::round(20.0f * s)));
    CHECK(session.ItemRect("box").GetWidth() == doctest::Approx(std::round(50.0f * s)));
    CHECK(session.ItemRect("box").GetHeight() == doctest::Approx(std::round(50.0f * s)));
    CHECK(session.ItemRect("fill").GetHeight() == doctest::Approx(session.ItemRect("fill").GetWidth() * 0.5f).epsilon(0.01));
    facet::ReleaseImage(landscape);
    facet::ReleaseImage(portrait);
}

TEST_CASE("An image view keeps its box when its handle becomes invalid while shown")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::ImageHandle image = MakeImage(60, 30);
    session.SetUi([&] {
        facet::ImageView("picture", image);
        session.Track("picture");
        facet::ImageView("never", facet::ImageHandle{});
        session.Track("never");
    });
    session.Frame();
    CHECK(session.ItemRect("picture").GetWidth() == 60.0f);
    facet::ReleaseImage(image);
    session.Frames(2);
    CHECK(facet::ImageFailed(image));
    CHECK(session.ItemRect("picture").GetWidth() == 60.0f);
    CHECK(session.ItemRect("picture").GetHeight() == 30.0f);
    // Without a size ever known: the large control height square.
    const float side = facet::Px(facet::T().metrics.controlHeightLarge);
    CHECK(session.ItemRect("never").GetWidth() == doctest::Approx(std::round(side)));
}

TEST_CASE("An activatable image view is a Tab stop that a click, Space and Enter activate")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ImageHandle image = MakeImage(32, 32);
    int activations = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::ImageView("plain", image);
        session.Track("plain");
        if (facet::ImageView("active", image, {.activatable = true}))
            ++activations;
        session.Track("active");
    });
    session.Frame();
    session.Click(session.ItemCenter("plain"));
    CHECK(activations == 0);
    session.Click(session.ItemCenter("active"));
    CHECK(activations == 1);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("active"));
    session.KeyPress(ImGuiKey_Space);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(activations == 3);
    facet::ReleaseImage(image);
}

TEST_CASE("The retry button of a failed image view is reported by its own query, not by the result")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int activations = 0;
    int retries = 0;
    session.SetUi([&] {
        if (facet::ImageView("failed", facet::ImageHandle{}, {.activatable = true, .retryLabel = "Retry", .width = 160.0f,
                                                             .height = 120.0f}))
            ++activations;
        session.Track("failed");
        if (facet::ImageViewRetryPressed())
            ++retries;
    });
    session.Frame();
    const ImRect r = session.ItemRect("failed");
    // The button sits at the bottom of the box, in the middle.
    session.Click(ImVec2(r.GetCenter().x, r.Max.y - facet::ControlHeight(facet::ControlSize::Small) * 0.5f -
                                              facet::Px(facet::T().spacing.xs)));
    CHECK(retries == 1);
    CHECK(activations == 0);
    // Tab from the image reaches the button, and Space presses it.
    session.Click(At(r, 0.5f, 0.2f));
    CHECK(activations == 1);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Space);
    CHECK(retries == 2);
    CHECK(facet::ImageViewRetryPressed() == false);
}

TEST_CASE("A disabled activatable image view is not a Tab stop and returns false")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int activations = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        ImGui::BeginDisabled();
        if (facet::ImageView("off", facet::ImageHandle{}, {.activatable = true}))
            ++activations;
        session.Track("off");
        ImGui::EndDisabled();
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("off"));
    CHECK(activations == 0);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

// ---- Avatar ------------------------------------------------------------------------------------

TEST_CASE("An avatar takes the control height of its size, even, and activates only when activatable")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int activations = 0;
    session.SetUi([&] {
        facet::Avatar("Placeholder name", facet::ImageHandle{});
        session.Track("plain");
        facet::Avatar("Large", facet::ImageHandle{}, {.size = facet::ControlSize::Large});
        session.Track("large");
        facet::Avatar("Odd", facet::ImageHandle{}, {.diameter = 33.0f});
        session.Track("odd");
        if (facet::Avatar("Active", facet::ImageHandle{}, {.activatable = true}))
            ++activations;
        session.Track("active");
    });
    session.Frame();
    CHECK(session.ItemRect("plain").GetWidth() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Regular)));
    CHECK(session.ItemRect("large").GetWidth() == doctest::Approx(facet::ControlHeight(facet::ControlSize::Large)));
    const float odd = session.ItemRect("odd").GetWidth();
    CHECK(std::fmod(odd, 2.0f) == 0.0f);
    session.Click(session.ItemCenter("plain"));
    CHECK(activations == 0);
    session.Click(session.ItemCenter("active"));
    CHECK(activations == 1);
    session.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus
    session.KeyPress(ImGuiKey_Enter);
    CHECK(activations == 2);
}
