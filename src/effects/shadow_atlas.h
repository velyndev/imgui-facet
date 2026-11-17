#pragma once

// Atlas of baked shadow corner tiles on Direct3D 11.
//
// Tiles are baked on the CPU (shadow_math.h) the first time a key is used and uploaded into
// RGBA8 pages: white with the shadow value in alpha, so the Dear ImGui renderer tints them
// with the vertex color and they batch with any other textured drawing. Each tile sits in a
// slot with one transparent texel on its top and left side and one copy of its last row and
// column on the other side, so bilinear filtering at the 9-slice seams reads the edge profile.
//
// Recording a frame never touches the device context: a new tile is baked into an upload
// record, and the caller queues that record as a draw callback in front of the draw that
// samples the tile. The texels therefore reach the page while the frame is rendered, through
// the immediate context given to facet::Init, in the same order as the draws that read them.
// Rendering happens on the frame thread (see context.h); the done list is atomic only so a
// confirmation is never read half written, not to support another render thread. A tile stays
// pending until a callback reports its upload as done, so a draw list that is recorded but
// never rendered (a window hidden for its first frame) cannot leave a tile empty.

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
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

// The start of an upload record. The slot's RGBA8 texels (size * size * 4 bytes) follow it.
struct ShadowUploadHeader
{
    ID3D11Texture2D* texture = nullptr;                  // the page; owned by the atlas
    std::atomic<std::uint64_t>* confirmations = nullptr; // the atlas's done list
    std::uint64_t id = 0;
    std::uint32_t confirmationCount = 0;
    std::uint32_t left = 0;
    std::uint32_t top = 0;
    std::uint32_t size = 0;  // slot width and height in texels
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

    // The tile of `key`, baked into an upload record on first use. Returns nullptr when no
    // device is set, the tile does not fit, every page is full, or a page could not be created.
    const ShadowTile* Acquire(const ShadowKey& key);

    // The upload record the caller must queue before its first draw with the tile of `key`,
    // or an empty span when the upload is done or `drawList` already queued it in `frame`.
    // The span stays valid until the next call on the atlas.
    std::span<const std::uint8_t> PendingUpload(const ShadowKey& key, const void* drawList, std::uint64_t frame);

    // Runs an upload record on `context` and marks it done. Called from the draw callback.
    static void RunUpload(ID3D11DeviceContext* context, const std::uint8_t* record, size_t size);

    // Uploads every pending tile at once through the atlas's own context.
    void FlushUploads();

    // Tiles whose upload has not been reported done yet.
    int PendingCount() const;

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
        std::uint64_t uploadId = 0;
        std::vector<std::uint8_t> record;  // empty once the upload is done
        const void* queuedList = nullptr;
        std::uint64_t queuedFrame = 0;
    };

    // Done uploads, stored by id modulo the count. An id pushed out by a later one only
    // makes its tile upload again.
    static constexpr std::uint32_t kConfirmationCount = 64;

    bool AddPage();
    void BuildRecord(Entry& entry, const Page& page, int slotX, int slotY, const ShadowTileLayout& layout);
    bool UploadDone(const Entry& entry) const;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    std::vector<Page> pages;
    std::unordered_map<std::uint64_t, Entry> tiles;
    std::vector<float> tileScratch;
    std::unique_ptr<std::atomic<std::uint64_t>[]> confirmations;
    std::uint64_t nextUploadId = 1;
    std::uint64_t bakes = 0;
    bool failed = false;  // page creation failed; stays off until the next SetDevice
};

} // namespace facet::internal
