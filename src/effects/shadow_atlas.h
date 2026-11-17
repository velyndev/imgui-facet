#pragma once

// Atlas of baked shadow corner tiles on Direct3D 11.
//
// Tiles are baked on the CPU (shadow_math.h) the first time a key is used and uploaded into
// RGBA8 pages: white with the shadow value in alpha, so the Dear ImGui renderer tints them
// with the vertex color and they batch with any other textured drawing. Each tile sits in a
// slot with one transparent texel on its top and left side and one copy of its last row and
// column on the other side, so bilinear filtering at the 9-slice seams reads the edge profile.
//
// Recording a frame never touches the device context. A new tile's texels stay in the atlas
// until they are uploaded, and the caller puts a small upload record (the upload id) as a
// draw callback in front of every draw of a tile that is still pending. Whichever of those
// callbacks the renderer reaches first writes the texels to the page, so the upload precedes
// the first draw that samples the tile however the draw list's channels were split and
// merged afterwards. Every later callback for the same id finds the upload done and returns.
// The record holds no pointer into the atlas: it is looked up by id, so a record that
// outlives its tile (the atlas was cleared or released its device) does nothing. Rendering
// happens on the frame thread (see context.h). A tile stays pending until a callback runs
// its upload, so a draw list that is recorded but never rendered (a window hidden for its
// first frame) cannot leave a tile empty: the next draw queues the upload again.

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

// What an upload callback carries in the draw list: the id of a pending upload.
struct ShadowUploadRecord
{
    std::uint64_t id = 0;
};

class ShadowAtlas
{
public:
    ShadowAtlas();
    ~ShadowAtlas();
    ShadowAtlas(const ShadowAtlas&) = delete;
    ShadowAtlas& operator=(const ShadowAtlas&) = delete;

    // Starts using the device. Pages are created when the first tile needs one.
    void SetDevice(ID3D11Device* device, ID3D11DeviceContext* context);
    // Releases every page and forgets every tile (device lost or shutdown).
    void ReleaseDevice();

    // The tile of `key`, baked on first use and pending until its upload runs. Returns
    // nullptr when no device is set, the tile does not fit, every page is full, or a page
    // could not be created.
    const ShadowTile* Acquire(const ShadowKey& key);

    // Fills `record` and returns true while the tile of `key` waits for its upload. The
    // caller queues the record in front of each draw of the tile; there is no deduplication,
    // because a draw list's channels may be merged in any order.
    bool PendingUpload(const ShadowKey& key, ShadowUploadRecord& record) const;

    // Writes the texels of `record`'s upload to its page on `context` and frees them. Does
    // nothing when the upload already ran or its tile is gone. Called from the draw callback.
    void RunUpload(ID3D11DeviceContext* context, const ShadowUploadRecord& record);

    // Uploads every pending tile at once through the atlas's own context.
    void FlushUploads();

    // Tiles whose upload has not run yet.
    int PendingCount() const { return static_cast<int>(pending.size()); }

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

    struct Entry
    {
        ShadowTile tile;
        std::vector<std::uint8_t> texels;  // the slot's RGBA8 texels; empty once uploaded
        std::uint64_t uploadId = 0;        // the pending upload; 0 once uploaded
        int page = 0;
        std::uint32_t left = 0;
        std::uint32_t top = 0;
        std::uint32_t size = 0;  // slot width and height in texels
    };

    bool AddPage();
    void BuildTexels(Entry& entry, const ShadowTileLayout& layout);
    void Upload(ID3D11DeviceContext* target, Entry& entry);

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    std::vector<Page> pages;
    std::unordered_map<std::uint64_t, Entry> tiles;            // by packed key
    std::unordered_map<std::uint64_t, std::uint64_t> pending;  // upload id to packed key
    std::vector<float> tileScratch;
    std::uint64_t nextUploadId = 1;
    std::uint64_t bakes = 0;
    bool failed = false;  // page creation failed; stays off until the next SetDevice
};

} // namespace facet::internal
