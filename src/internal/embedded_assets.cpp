#include "internal/embedded_assets.h"

namespace facet::internal
{

const EmbeddedAsset* FindEmbeddedAsset(std::string_view name)
{
    for (const EmbeddedAsset& asset : EmbeddedAssets())
    {
        if (name == asset.name)
            return &asset;
    }
    return nullptr;
}

} // namespace facet::internal
