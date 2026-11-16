#pragma once

// Atlas of baked shadow corner tiles on Direct3D 11.
//
// Tiles are baked on the CPU (shadow_math.h) the first time a key is used and uploaded into
// RGBA8 pages: white with the shadow value in alpha, so the Dear ImGui renderer tints them
// with the vertex color and they batch with any other textured drawing. Each tile sits in a
// slot with one transparent texel on its top and left side and one copy of its last row and
// column on the other side, so bilinear filtering at the 9-slice seams reads the edge profile.

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <wrl/client.h>

#include "effects/shadow_math.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

namespace facet::internal
{

inline constexpr int kShadowAtlasPageSize = 1024;
inline constexpr int kShadowAtlasMaxPages = 4;

struct ShadowTile
{
    ImTextureID texture = 0;     // the page's shader resource view
    ImVec2 uv;                   // UV of the tile's top-left texel corner
    ImVec2 texelUv;              // UV size of one texel
    ShadowTileLayout layout;
};

class ShadowAtlas
{
public:
    ShadowAtlas() = default;
    ~ShadowAtlas();
    ShadowAtlas(const ShadowAtlas&) = delete;
    ShadowAtlas& operator=(const ShadowAtlas&) = delete;

    // Starts using the device. Pages are created when the first tile needs one.
    void SetDevice(ID3D11Device* device, ID3D11DeviceContext* context);
    // Releases every page and forgets every tile (device lost or shutdown).
    void ReleaseDevice();

    // The tile of `key`, baked and uploaded on first use. Returns nullptr when no device is
    // set, the tile does not fit, every page is full, or a page could not be created.
    const ShadowTile* Acquire(const ShadowKey& key);

    // Forgets every tile and empties the pages without releasing them. Call at the start of
    // a frame, never between recording and rendering one (recorded draws read the pages).
    void Clear();

    int PageCount() const { return static_cast<int>(pages.size()); }
    int TileCount() const { return static_cast<int>(tiles.size()); }
    std::uint64_t BakeCount() const { return bakes; }
    // True when the pages are more than kRefillThreshold full, so a frame start should Clear.
    bool NearlyFull() const;

private:
    struct Page
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        ShelfPacker packer;
    };

    bool AddPage();
    bool Upload(Page& page, int slotX, int slotY, const ShadowTileLayout& layout);

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    std::vector<Page> pages;
    std::unordered_map<std::uint64_t, ShadowTile> tiles;
    std::vector<float> tileScratch;
    std::vector<std::uint8_t> pixelScratch;
    std::uint64_t bakes = 0;
    bool failed = false;  // page creation failed; stays off until the next SetDevice
};

} // namespace facet::internal
