// Icon identifiers, the registry and the built-in glyph set. None of this needs a device.

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

#include <doctest/doctest.h>

#include <facet/facet.h>

#include "image/image_internal.h"

// Icon names hash at compile time.
static_assert(facet::Icon("check").IsValid());
static_assert(facet::Icon("check") == facet::Icon("check"));
static_assert(!(facet::Icon("check") == facet::Icon("chevron-right")));
static_assert(!facet::Icon().IsValid());
static_assert(!facet::Icon("").IsValid());
// FNV-1a reference value for "a".
static_assert(facet::Icon::Hash("a") == 0xE40C292Cu);

namespace
{

// Every name the framework's controls rely on.
constexpr const char* kRequired[] = {
    "check", "minus", "plus", "close", "chevron-up", "chevron-down", "chevron-left", "chevron-right",
    "chevrons-up-down", "arrow-up", "arrow-down", "arrow-left", "arrow-right", "search", "clear",
    "more-horizontal", "more-vertical", "grip", "eye", "eye-off", "copy", "paste", "edit", "trash", "folder",
    "file", "image", "image-off", "download", "upload", "external-link", "link", "calendar", "clock", "info",
    "warning", "error", "success", "help", "lock", "unlock", "pin", "unpin", "star", "star-filled", "heart",
    "settings", "sliders", "filter", "sort-ascending", "sort-descending", "refresh", "undo", "redo", "reset",
    "play", "pause", "stop", "zoom-in", "zoom-out", "expand", "collapse", "maximize", "minimize", "restore",
    "sidebar", "grid", "list", "keyboard", "mouse", "palette", "eyedropper", "layers", "user", "bell", "sun",
    "moon", "monitor", "menu",
};

int CoveredPixels(const facet::internal::RgbaImage& image)
{
    int covered = 0;
    for (std::size_t i = 3; i < image.pixels.size(); i += 4)
        covered += image.pixels[i] != 0 ? 1 : 0;
    return covered;
}

} // namespace

TEST_CASE("Icons built from runtime strings equal their compile-time constants")
{
    const std::string name = std::string("chev") + "ron-right";
    CHECK(facet::Icon(name) == facet::Icon("chevron-right"));
}

TEST_CASE("Default image handles are invalid and compare equal")
{
    const facet::ImageHandle a;
    const facet::ImageHandle b;
    CHECK_FALSE(a.IsValid());
    CHECK(a == b);
    CHECK((facet::ImageHandle{3, 1} == facet::ImageHandle{3, 2}) == false);
}

TEST_CASE("Every required icon is built in and found by name")
{
    for (const char* name : kRequired)
    {
        const std::string nameText = name;
        CAPTURE(nameText);
        const facet::Icon icon = facet::FindIcon(name);
        CHECK(icon.IsValid());
        CHECK(icon == facet::Icon(name));
        REQUIRE(facet::IconName(icon) != nullptr);
        CHECK(std::strcmp(facet::IconName(icon), name) == 0);
    }
    CHECK_FALSE(facet::FindIcon("no-such-icon").IsValid());
    CHECK_FALSE(facet::FindIcon("").IsValid());
    CHECK_FALSE(facet::FindIcon(nullptr).IsValid());
    CHECK(facet::IconName(facet::Icon("no-such-icon")) == nullptr);
}

TEST_CASE("Icon names are listed in byte order")
{
    const int count = facet::IconCount();
    CHECK(count >= static_cast<int>(std::size(kRequired)));
    for (int i = 1; i < count; ++i)
        CHECK(std::strcmp(facet::IconNameAt(i - 1), facet::IconNameAt(i)) < 0);
    CHECK(facet::IconNameAt(-1) == nullptr);
    CHECK(facet::IconNameAt(count) == nullptr);
}

TEST_CASE("Every built-in icon parses and rasterizes to a white mask inside its margin")
{
    for (int i = 0; i < facet::IconCount(); ++i)
    {
        const char* name = facet::IconNameAt(i);
        const std::string nameText = name;
        CAPTURE(nameText);
        for (const int side : {16, 20, 24, 32})
        {
            CAPTURE(side);
            facet::internal::RgbaImage image;
            REQUIRE(facet::internal::RasterizeIcon(facet::Icon(name).id, side, image));
            CHECK(image.width == side);
            CHECK(image.height == side);
            CHECK(CoveredPixels(image) > side);  // more than a stray dot

            for (std::size_t p = 0; p < image.pixels.size(); p += 4)
            {
                if (image.pixels[p + 3] == 0)
                    continue;
                // The mask stores white; the color comes from the tint when drawn.
                CHECK(image.pixels[p] == 255);
            }
        }

        // At 16 px the 1 unit safe margin keeps the outermost pixel ring nearly empty
        // (anti-aliasing of an edge exactly on the margin may leave a trace).
        facet::internal::RgbaImage image;
        REQUIRE(facet::internal::RasterizeIcon(facet::Icon(name).id, 16, image));
        int ringMax = 0;
        for (int y = 0; y < 16; ++y)
        {
            for (int x = 0; x < 16; ++x)
            {
                if (x != 0 && y != 0 && x != 15 && y != 15)
                    continue;
                ringMax = std::max(ringMax, static_cast<int>(image.pixels[(y * 16 + x) * 4 + 3]));
            }
        }
        CHECK(ringMax <= 32);
    }
}

TEST_CASE("Icons registered at runtime join the registry")
{
    const int before = facet::IconCount();
    const char* svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\" viewBox=\"0 0 16 16\">"
                      "<circle cx=\"8\" cy=\"8\" r=\"5\"/></svg>";
    const facet::Icon dot = facet::RegisterIcon("test-runtime-dot", svg);
    REQUIRE(dot.IsValid());
    CHECK(dot == facet::Icon("test-runtime-dot"));
    CHECK(facet::FindIcon("test-runtime-dot") == dot);
    CHECK(facet::IconCount() == before + 1);

    facet::internal::RgbaImage image;
    REQUIRE(facet::internal::RasterizeIcon(dot.id, 16, image));
    CHECK(CoveredPixels(image) > 16);

    // Registering the same name again replaces the drawing and keeps the count.
    const char* square = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\" viewBox=\"0 0 16 16\">"
                         "<rect width=\"16\" height=\"16\"/></svg>";
    CHECK(facet::RegisterIcon("test-runtime-dot", square) == dot);
    CHECK(facet::IconCount() == before + 1);
    REQUIRE(facet::internal::RasterizeIcon(dot.id, 16, image));
    CHECK(CoveredPixels(image) == 16 * 16);

    // Unparsable sources and empty names are refused.
    CHECK_FALSE(facet::RegisterIcon("test-runtime-bad", "not an svg").IsValid());
    CHECK_FALSE(facet::RegisterIcon("", svg).IsValid());
    CHECK_FALSE(facet::RegisterIcon(nullptr, svg).IsValid());
    CHECK_FALSE(facet::FindIcon("test-runtime-bad").IsValid());
    CHECK(facet::IconCount() == before + 1);
}
