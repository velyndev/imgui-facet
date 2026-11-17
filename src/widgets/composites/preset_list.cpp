// PresetList (facet/widgets/composites.h): planned. A placeholder until Phase 2 implements it.

#include <algorithm>

#include "facet/widgets/composites.h"
#include "widgets/content_stub.h"

namespace facet
{

PresetListResult PresetList(const char* id, int*, std::span<const PresetItem> presets, const PresetListOptions& options)
{
    namespace stub = internal::content;
    const float rows = static_cast<float>(std::max<std::size_t>(1, presets.size()));
    stub::Stub("PresetList", id, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::ControlRows(rows)));
    return {};
}

} // namespace facet