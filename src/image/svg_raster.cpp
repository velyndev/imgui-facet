// SVG images rasterized per scale.
//
// SVG rasterizing does not exist yet: both loaders return an invalid handle.

#include "facet/core/image.h"

namespace facet
{

ImageHandle LoadSvgFile(const char*, ImVec2)
{
    return {};
}

ImageHandle LoadSvgMemory(const char*, std::size_t, ImVec2)
{
    return {};
}

} // namespace facet
