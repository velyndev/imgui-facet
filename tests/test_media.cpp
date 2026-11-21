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

// ---- AvatarGroup -------------------------------------------------------------------------------

TEST_CASE("An avatar group shows maxVisible avatars and a chip, and fits the width when maxVisible is 0")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::AvatarItem members[] = {{"Placeholder 1"}, {"Placeholder 2"}, {"Placeholder 3"},
                                                {"Placeholder 4"}, {"Placeholder 5"}, {"Placeholder 6"}};
    int activations = 0;
    float narrowWidth = 0.0f;
    session.SetUi([&] {
        facet::AvatarGroup("all", members);
        session.Track("all");
        facet::AvatarGroup("two", members, {.maxVisible = 2});
        session.Track("two");
        if (facet::AvatarGroup("one", std::span(members, 1), {}))
            ++activations;
        session.Track("one");
        ImGui::BeginChild("narrow", ImVec2(facet::ControlHeight(facet::ControlSize::Regular) * 3.0f, 0.0f));
        facet::AvatarGroup("fit", members);
        narrowWidth = ImGui::GetItemRectSize().x;
        ImGui::EndChild();
        facet::AvatarGroup("none", std::span<const facet::AvatarItem>());
        session.Track("none");
    });
    session.Frame();
    const float d = facet::ControlHeight(facet::ControlSize::Regular);
    const float step = d - std::round(d * facet::T().media.avatarOverlap);
    CHECK(session.ItemRect("all").GetWidth() == doctest::Approx(d + 5.0f * step));
    CHECK(session.ItemRect("two").GetWidth() < session.ItemRect("all").GetWidth());
    CHECK(session.ItemRect("two").GetWidth() > d + step);  // the chip follows the two avatars
    CHECK(session.ItemRect("one").GetWidth() == doctest::Approx(d));
    CHECK(narrowWidth <= d * 3.0f);
    CHECK(session.ItemRect("none").GetWidth() == doctest::Approx(d));
    session.Click(session.ItemCenter("one"));
    session.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus
    session.KeyPress(ImGuiKey_Space);
    CHECK(activations == 2);
}

// ---- ThumbnailStrip ----------------------------------------------------------------------------

TEST_CASE("A thumbnail strip chooses by click and by the arrows, Home and End, and never in read-only")
{
    ReducedMotion reduced;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::ImageItem items[] = {{"Image 1"}, {"Image 2"}, {"Image 3"}, {"Image 4"}};
    int selected = -1;
    int other = 1;
    int writes = 0;
    session.SetUi([&] {
        if (facet::ThumbnailStrip("strip", &selected, items, {.thumbnailHeight = 40.0f}))
            ++writes;
        session.Track("strip");
        facet::ThumbnailStrip("fixed", &other, items, {.thumbnailHeight = 40.0f, .readOnly = true});
        session.Track("fixed");
    });
    session.Frame();
    const ImRect r = session.ItemRect("strip");
    const float inset = std::ceil(facet::internal::FocusRingOutset());
    const float thumb = facet::PxSnapped(40.0f);
    const float stride = thumb + facet::PxSnapped(facet::T().spacing.sm);
    const auto thumbCenter = [&](const ImRect& strip, int i) {
        return ImVec2(strip.Min.x + inset + stride * static_cast<float>(i) + thumb * 0.5f, strip.Min.y + inset + thumb * 0.5f);
    };
    session.Click(thumbCenter(r, 2));
    CHECK(selected == 2);
    CHECK(writes == 1);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(selected == 3);
    session.KeyPress(ImGuiKey_RightArrow);  // at the end it stays
    CHECK(selected == 3);
    session.KeyPress(ImGuiKey_Home);
    CHECK(selected == 0);
    session.KeyPress(ImGuiKey_End);
    CHECK(selected == 3);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(selected == 2);
    CHECK(writes == 5);
    session.Click(thumbCenter(session.ItemRect("fixed"), 3));
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(other == 1);
}

TEST_CASE("A thumbnail strip scrolls by drag and by the wheel without choosing")
{
    ReducedMotion reduced;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static facet::ImageItem items[12];
    for (facet::ImageItem& item : items)
        item.label = "Placeholder";
    int selected = 0;
    session.SetUi([&] {
        facet::ThumbnailStrip("strip", &selected, items, {.thumbnailHeight = 60.0f, .width = 200.0f});
        session.Track("strip");
    });
    session.Frame();
    const ImRect r = session.ItemRect("strip");
    // Dragging from the second thumbnail to the left scrolls, and the release does not choose.
    session.Drag({At(r, 0.6f, 0.5f), At(r, 0.1f, 0.5f)}, 6);
    session.Frames(2);
    CHECK(selected == 0);
    // The wheel scrolls once the strip owned it for a frame; the selection stays.
    session.MouseMove(At(r, 0.5f, 0.5f));
    session.Wheel(At(r, 0.5f, 0.5f), -1.0f);
    session.Wheel(At(r, 0.5f, 0.5f), -1.0f);
    session.Frames(2);
    CHECK(selected == 0);
    // End scrolls the last thumbnail into view: a click near the right edge now picks it.
    session.Click(At(r, 0.02f, 0.5f));
    session.KeyPress(ImGuiKey_End);
    session.Frames(2);
    CHECK(selected == 11);
    session.Click(ImVec2(r.Max.x - std::ceil(facet::internal::FocusRingOutset()) - 4.0f, r.GetCenter().y));
    CHECK(selected == 11);
}

TEST_CASE("A thumbnail strip with no items, loading or failed is not a Tab stop and writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::ImageItem items[] = {{"Image 1"}};
    int selected = -1;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::ThumbnailStrip("empty", &selected, std::span<const facet::ImageItem>());
        facet::ThumbnailStrip("loading", &selected, items, {.state = facet::DataState::Loading});
        facet::ThumbnailStrip("error", &selected, items, {.state = facet::DataState::Error});
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
    CHECK(selected == -1);
}

TEST_CASE("A thumbnail strip from a getter skips the entries it refuses")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int selected = -1;
    int calls = 0;
    struct User
    {
        int* calls;
    } user{&calls};
    session.SetUi([&] {
        facet::ThumbnailStrip("strip", &selected, 1000, [](void* u, int index, facet::ImageItem* out) {
            ++*static_cast<User*>(u)->calls;
            out->label = "Generated";
            return index % 2 == 0;
        }, &user, {.thumbnailHeight = 40.0f, .width = 300.0f});
    });
    session.Frame();
    // Only the visible entries are asked for.
    CHECK(calls > 0);
    CHECK(calls < 20);
}

// ---- Carousel ----------------------------------------------------------------------------------

TEST_CASE("A carousel pages with its buttons, the arrows, Home and End, and wraps when it loops")
{
    ReducedMotion reduced;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::ImageItem slides[] = {{"Slide 1"}, {"Slide 2"}, {"Slide 3"}};
    int index = 0;
    int looping = 0;
    session.SetUi([&] {
        facet::Carousel("slides", &index, slides, {.width = 320.0f});
        session.Track("slides");
        facet::Carousel("loop", &looping, slides, {.loop = true, .width = 320.0f});
        session.Track("loop");
    });
    session.Frame();
    const ImRect r = session.ItemRect("slides");
    const float edge = facet::PxSnapped(facet::T().spacing.sm);
    const float button = facet::ControlHeight(facet::ControlSize::Regular);
    const float slideHeight = std::round(r.GetWidth() * 9.0f / 16.0f);
    const ImVec2 nextAt(r.Max.x - edge - button * 0.5f, r.Min.y + slideHeight * 0.5f);
    const ImVec2 previousAt(r.Min.x + edge + button * 0.5f, r.Min.y + slideHeight * 0.5f);
    session.Click(nextAt);
    CHECK(index == 1);
    session.Click(nextAt);
    session.Click(nextAt);  // disabled at the last slide without loop
    CHECK(index == 2);
    session.Click(previousAt);
    CHECK(index == 1);
    session.Click(ImVec2(r.GetCenter().x, r.Min.y + slideHeight * 0.5f));  // the slide itself takes the focus
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(index == 2);
    session.KeyPress(ImGuiKey_Home);
    CHECK(index == 0);
    session.KeyPress(ImGuiKey_End);
    CHECK(index == 2);
    const ImRect l = session.ItemRect("loop");
    session.Click(ImVec2(l.GetCenter().x, l.Min.y + slideHeight * 0.5f));
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(looping == 2);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(looping == 0);
    // A sideways wheel pages; the plain wheel is left to the window.
    const ImVec2 middle(l.GetCenter().x, l.Min.y + slideHeight * 0.5f);
    session.MouseMove(middle);
    session.Wheel(middle, -1.0f);
    session.Wheel(middle, -1.0f);
    CHECK(looping == 0);
    session.Wheel(middle, 0.0f, -1.0f);
    session.Wheel(middle, 0.0f, -1.0f);
    CHECK(looping != 0);
}

TEST_CASE("A carousel follows a drag and pages on a flick or past half a slide")
{
    ReducedMotion reduced;
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::ImageItem slides[] = {{"Slide 1"}, {"Slide 2"}, {"Slide 3"}};
    int index = 0;
    session.SetUi([&] {
        facet::Carousel("slides", &index, slides, {.width = 320.0f, .height = 120.0f});
        session.Track("slides");
    });
    session.Frame();
    const ImRect r = session.ItemRect("slides");
    const float y = r.Min.y + facet::Px(60.0f);
    // A slow drag of less than half a slide springs back.
    session.Drag({ImVec2(r.GetCenter().x, y), ImVec2(r.GetCenter().x - r.GetWidth() * 0.2f, y)}, 30);
    session.Frames(2);
    CHECK(index == 0);
    // A drag past half a slide pages.
    session.Drag({ImVec2(r.Max.x - 60.0f, y), ImVec2(r.Max.x - 60.0f - r.GetWidth() * 0.6f, y)}, 30);
    CHECK(index == 1);
    // A short fast flick pages too.
    session.Drag({ImVec2(r.GetCenter().x, y), ImVec2(r.GetCenter().x - r.GetWidth() * 0.15f, y)}, 1);
    CHECK(index == 2);
    // A drag past the last slide does not leave the range.
    session.Drag({ImVec2(r.GetCenter().x, y), ImVec2(r.Min.x + 4.0f, y)}, 4);
    CHECK(index == 2);
}

TEST_CASE("A carousel with one slide or none has no buttons and no slide keys")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static const facet::ImageItem one[] = {{"Slide 1"}};
    int index = 0;
    int none = 0;
    int writes = 0;
    session.SetUi([&] {
        if (facet::Carousel("one", &index, one, {.width = 320.0f}))
            ++writes;
        session.Track("one");
        if (facet::Carousel("none", &none, std::span<const facet::ImageItem>(), {.width = 320.0f}))
            ++writes;
    });
    session.Frame();
    session.Click(session.ItemCenter("one"));
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_End);
    CHECK(index == 0);
    CHECK(writes == 0);
}

TEST_CASE("Carousel autoplay advances, pauses while hovered and with its button, and is off with reduced motion")
{
    ThemeGuard guard;
    facet::Theme theme = facet::T();
    theme.motion.reduceMotion = false;
    facet::SetTheme(theme, false);
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::SetFollowSystemMotion(false);
    static const facet::ImageItem slides[] = {{"Slide 1"}, {"Slide 2"}, {"Slide 3"}};
    int index = 0;
    session.SetUi([&] {
        facet::Carousel("slides", &index, slides, {.autoplay = 0.5f, .width = 320.0f});
        session.Track("slides");
    });
    session.Frame();
    session.MouseMove(ImVec2(700.0f, 580.0f));
    session.Seconds(0.6f);
    CHECK(index == 1);
    const ImRect r = session.ItemRect("slides");
    session.MouseMove(At(r, 0.5f, 0.3f));
    session.Seconds(1.0f);
    CHECK(index == 1);
    // The pause button at the end of the row stops it.
    const float row = facet::ControlHeight(facet::ControlSize::Small);
    session.Click(ImVec2(r.Max.x - row * 0.5f, r.Max.y - row * 0.5f));
    session.MouseMove(ImVec2(700.0f, 580.0f));
    session.Seconds(1.2f);
    CHECK(index == 1);
    theme.motion.reduceMotion = true;
    facet::SetTheme(theme, false);
    session.Click(ImVec2(r.Max.x - row * 0.5f, r.Max.y - row * 0.5f));  // no button any more: nothing
    session.MouseMove(ImVec2(700.0f, 580.0f));
    session.Seconds(1.2f);
    CHECK(index == 1);
    facet::SetFollowSystemMotion(true);
}

// ---- ImageViewer -------------------------------------------------------------------------------

TEST_CASE("An image viewer zooms around the pointer within its limits and fits again with 0")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ImageHandle image = MakeImage(200, 100);
    facet::ImageViewerState view;
    session.SetUi([&] {
        facet::ImageViewer("viewer", image, &view, {.maxZoom = 4.0f, .width = 200.0f, .height = 200.0f});
        session.Track("viewer");
    });
    session.Frame();
    const ImRect r = session.ItemRect("viewer");
    const float unit = facet::Px(1.0f);
    // Fit: 200 by 100 in a 200-unit square viewport.
    CHECK(view.zoom == 0.0f);
    // The wheel zooms in around the pointer: the image point under it stays put.
    const ImVec2 pointer = At(r, 0.75f, 0.5f);
    const float fit = r.GetWidth() / 200.0f;
    const ImVec2 before(100.0f + (pointer.x - r.GetCenter().x) / fit, 50.0f);
    session.MouseMove(pointer);
    session.Wheel(pointer, 1.0f);
    session.Wheel(pointer, 1.0f);
    REQUIRE(view.zoom > 0.0f);
    const float scale = view.zoom * unit;
    const float after = view.center.x + (pointer.x - r.GetCenter().x) / scale;
    CHECK(after == doctest::Approx(before.x).epsilon(0.01));
    // The closest zoom is maxZoom.
    for (int i = 0; i < 20; ++i)
        session.Wheel(pointer, 1.0f);
    CHECK(view.zoom == doctest::Approx(4.0f));
    // Zooming out stops at fit (or 100 percent when that is smaller).
    for (int i = 0; i < 40; ++i)
        session.Wheel(pointer, -1.0f);
    CHECK(view.zoom * unit == doctest::Approx(std::min(fit, unit)));
    // Keys: 1 shows 100 percent, + and - zoom, 0 fits.
    session.Click(r.GetCenter());
    session.KeyPress(ImGuiKey_1);
    CHECK(view.zoom == doctest::Approx(1.0f));
    session.KeyPress(ImGuiKey_Equal);
    CHECK(view.zoom == doctest::Approx(1.25f));
    session.KeyPress(ImGuiKey_Minus);
    CHECK(view.zoom == doctest::Approx(1.0f));
    session.KeyPress(ImGuiKey_0);
    CHECK(view.zoom == 0.0f);
    facet::ReleaseImage(image);
}

TEST_CASE("An image viewer pans with the arrows and a drag, clamped so the image never leaves the viewport")
{
    ReducedMotion reduced;
    facet::test::Session session;
    REQUIRE(session.Ready());
    const facet::ImageHandle image = MakeImage(400, 400);
    facet::ImageViewerState view{.zoom = 1.0f, .center = ImVec2(200.0f, 200.0f)};
    session.SetUi([&] {
        facet::ImageViewer("viewer", image, &view, {.width = 100.0f, .height = 100.0f});
        session.Track("viewer");
    });
    session.Frame();
    const ImRect r = session.ItemRect("viewer");
    const float unit = facet::Px(1.0f);
    const float half = r.GetWidth() * 0.5f / unit;
    session.Click(r.GetCenter());
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(view.center.x > 200.0f);
    session.KeyPress(ImGuiKey_End);  // not a viewer key: nothing
    for (int i = 0; i < 40; ++i)
        session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(view.center.x == doctest::Approx(half));
    // A drag to the left moves the image left, so the view's center moves right.
    session.Drag({r.GetCenter(), ImVec2(r.GetCenter().x - 30.0f, r.GetCenter().y)}, 6);
    CHECK(view.center.x == doctest::Approx(half + 30.0f / unit).epsilon(0.05));
    // A double click toggles to fit, and again to 100 percent.
    session.Seconds(0.6f);  // so the next clicks start a new count
    session.DoubleClick(r.GetCenter());
    CHECK(view.zoom == 0.0f);
    session.Seconds(0.6f);
    session.DoubleClick(r.GetCenter());
    CHECK(view.zoom == doctest::Approx(1.0f));
    facet::ReleaseImage(image);
}

TEST_CASE("An image viewer without a ready image writes no view")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::ImageViewerState view;
    int writes = 0;
    session.SetUi([&] {
        if (facet::ImageViewer("viewer", facet::ImageHandle{}, &view, {.height = 100.0f}))
            ++writes;
        session.Track("viewer");
    });
    session.Frame();
    session.Wheel(session.ItemCenter("viewer"), 1.0f);
    session.Wheel(session.ItemCenter("viewer"), 1.0f);
    session.Click(session.ItemCenter("viewer"));
    session.KeyPress(ImGuiKey_1);
    CHECK(writes == 0);
    CHECK(view.zoom == 0.0f);
}
