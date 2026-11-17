// Library skeleton: version string, lifetime guards and the embedded asset table.

#include <cstdio>
#include <cstring>

#include <doctest/doctest.h>
#include <imgui.h>

#include <facet/facet.h>

#include "internal/embedded_assets.h"

TEST_CASE("Version is a non-empty major.minor.patch string")
{
    const char* version = facet::Version();
    REQUIRE(version != nullptr);
    CHECK(std::strlen(version) > 0);
    CHECK(std::strchr(version, '.') != nullptr);
}

TEST_CASE("The version comes from version.h alone")
{
    // The library returns the header's string, built from the three numbers.
    CHECK(std::strcmp(facet::Version(), FACET_VERSION) == 0);
    char expected[32];
    std::snprintf(expected, sizeof(expected), "%d.%d.%d", FACET_VERSION_MAJOR, FACET_VERSION_MINOR,
                  FACET_VERSION_PATCH);
    CHECK(std::strcmp(FACET_VERSION, expected) == 0);
    CHECK(FACET_VERSION_NUM == FACET_VERSION_MAJOR * 10000 + FACET_VERSION_MINOR * 100 + FACET_VERSION_PATCH);
    // CMake read the same numbers for project(VERSION).
    CHECK(std::strcmp(FACET_TEST_PROJECT_VERSION, FACET_VERSION) == 0);
}

TEST_CASE("Init rejects a missing device and context")
{
    ImGui::CreateContext();
    CHECK_FALSE(facet::Init(facet::InitDesc{}));
    CHECK_FALSE(facet::IsInitialized());
    facet::Shutdown();
    CHECK_FALSE(facet::IsInitialized());
    ImGui::DestroyContext();
}

TEST_CASE("Embedded assets contain the icons of the built-in set")
{
    const auto assets = facet::internal::EmbeddedAssets();
    CHECK(assets.size() >= 2);

    for (const char* name : {"icons/check.svg", "icons/chevron-right.svg"})
    {
        CAPTURE(name);
        const facet::internal::EmbeddedAsset* asset = facet::internal::FindEmbeddedAsset(name);
        REQUIRE(asset != nullptr);
        CHECK(asset->size > 0);
        // The data is zero terminated and is an SVG document.
        CHECK(asset->data[asset->size] == 0);
        CHECK(std::strstr(reinterpret_cast<const char*>(asset->data), "<svg") != nullptr);
    }

    CHECK(facet::internal::FindEmbeddedAsset("icons/does-not-exist.svg") == nullptr);
}

TEST_CASE("Embedded assets are sorted by name")
{
    const auto assets = facet::internal::EmbeddedAssets();
    for (size_t i = 1; i < assets.size(); ++i)
        CHECK(std::strcmp(assets[i - 1].name, assets[i].name) < 0);
}
