#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace facet::internal
{

// One file from assets/, compiled into the library by cmake/FacetEmbed.cmake.
struct EmbeddedAsset
{
    const char* name;            // path relative to assets/, forward slashes, e.g. "icons/check.svg"
    const unsigned char* data;   // file bytes followed by one zero byte (not counted in size)
    std::size_t size;            // byte count of the file
};

// All embedded assets sorted by name. Empty when no asset files exist.
// Defined in the generated file facet_embedded_assets.cpp in the build tree.
std::span<const EmbeddedAsset> EmbeddedAssets();

// Returns the asset with exactly this name, or nullptr.
const EmbeddedAsset* FindEmbeddedAsset(std::string_view name);

} // namespace facet::internal
