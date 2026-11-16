// Soft shadows and glow, drawn as 9-slices of baked corner tiles from the shadow atlas.

#include "facet/core/effects.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <d3d11.h>

#include "effects/shadow_atlas.h"
#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/device.h"
#include "internal/subsystem.h"

namespace facet
{
namespace internal
{
namespace
{

// A frame start empties the atlas when its last page is filled beyond this fraction, so a
// changing set of shadow sizes cannot run out of space in the middle of a frame.
constexpr float kRefillThreshold = 0.75f;

} // namespace

ShadowAtlas::ShadowAtlas()
    : confirmations(std::make_unique<std::atomic<std::uint64_t>[]>(kConfirmationCount))
{
    for (std::uint32_t i = 0; i < kConfirmationCount; ++i)
        confirmations[i].store(0, std::memory_order_relaxed);
}

ShadowAtlas::~ShadowAtlas()
{
    ReleaseDevice();
}

void ShadowAtlas::SetDevice(ID3D11Device* newDevice, ID3D11DeviceContext* newContext)
{
    ReleaseDevice();
    device = newDevice;
    context = newContext;
    failed = false;
}

void ShadowAtlas::ReleaseDevice()
{
    tiles.clear();
    pages.clear();
    context.Reset();
    device.Reset();
}

void ShadowAtlas::Clear()
{
    tiles.clear();
    for (Page& page : pages)
        page.packer.Reset(kShadowAtlasPageSize, kShadowAtlasPageSize);
}

bool ShadowAtlas::NearlyFull() const
{
    if (static_cast<int>(pages.size()) < kShadowAtlasMaxPages)
        return false;
    const ShelfPacker& last = pages.back().packer;
    return static_cast<float>(last.UsedHeight()) > static_cast<float>(last.Height()) * kRefillThreshold;
}

bool ShadowAtlas::AddPage()
{
    if (!device || failed || static_cast<int>(pages.size()) >= kShadowAtlasMaxPages)
        return false;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = kShadowAtlasPageSize;
    desc.Height = kShadowAtlasPageSize;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    Page page;
    // Start fully transparent, so unused texels next to a slot never show.
    std::vector<std::uint8_t> zeros(static_cast<size_t>(kShadowAtlasPageSize) * kShadowAtlasPageSize * 4, 0);
    D3D11_SUBRESOURCE_DATA initial = {};
    initial.pSysMem = zeros.data();
    initial.SysMemPitch = kShadowAtlasPageSize * 4;
    if (FAILED(device->CreateTexture2D(&desc, &initial, page.texture.GetAddressOf())) ||
        FAILED(device->CreateShaderResourceView(page.texture.Get(), nullptr, page.view.GetAddressOf())))
    {
        failed = true;
        return false;
    }
    page.packer.Reset(kShadowAtlasPageSize, kShadowAtlasPageSize);
    pages.push_back(std::move(page));
    return true;
}

void ShadowAtlas::BuildRecord(Entry& entry, const Page& page, int slotX, int slotY, const ShadowTileLayout& layout)
{
    // Slot = transparent texel, tile, copy of the tile's last row or column.
    const int size = layout.size;
    const int slot = size + 2;
    const size_t texels = static_cast<size_t>(slot) * static_cast<size_t>(slot);
    entry.record.assign(sizeof(ShadowUploadHeader) + texels * 4, 0);
    std::uint8_t* pixels = entry.record.data() + sizeof(ShadowUploadHeader);
    for (int y = 1; y < slot; ++y)
    {
        const int ty = std::min(y - 1, size - 1);
        for (int x = 1; x < slot; ++x)
        {
            const int tx = std::min(x - 1, size - 1);
            const float value = tileScratch[static_cast<size_t>(ty) * static_cast<size_t>(size) + static_cast<size_t>(tx)];
            std::uint8_t* texel = pixels + (static_cast<size_t>(y) * static_cast<size_t>(slot) + static_cast<size_t>(x)) * 4;
            texel[0] = texel[1] = texel[2] = 255;
            texel[3] = static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
        }
    }
    ShadowUploadHeader header;
    header.texture = page.texture.Get();
    header.confirmations = confirmations.get();
    header.id = nextUploadId++;
    header.confirmationCount = kConfirmationCount;
    header.left = static_cast<std::uint32_t>(slotX);
    header.top = static_cast<std::uint32_t>(slotY);
    header.size = static_cast<std::uint32_t>(slot);
    std::memcpy(entry.record.data(), &header, sizeof(header));
    entry.uploadId = header.id;
    entry.queuedList = nullptr;
    entry.queuedFrame = 0;
}

bool ShadowAtlas::UploadDone(const Entry& entry) const
{
    return entry.record.empty() ||
           confirmations[entry.uploadId % kConfirmationCount].load(std::memory_order_acquire) == entry.uploadId;
}

void ShadowAtlas::RunUpload(ID3D11DeviceContext* target, const std::uint8_t* record, size_t size)
{
    if (target == nullptr || record == nullptr || size < sizeof(ShadowUploadHeader))
        return;
    // The record may sit at any alignment inside a draw list's callback buffer.
    ShadowUploadHeader header;
    std::memcpy(&header, record, sizeof(header));
    const size_t texels = static_cast<size_t>(header.size) * static_cast<size_t>(header.size);
    if (header.texture == nullptr || header.size == 0 || size != sizeof(header) + texels * 4)
        return;
    D3D11_BOX box = {};
    box.left = header.left;
    box.top = header.top;
    box.right = header.left + header.size;
    box.bottom = header.top + header.size;
    box.front = 0;
    box.back = 1;
    target->UpdateSubresource(header.texture, 0, &box, record + sizeof(header), header.size * 4, 0);
    if (header.confirmations != nullptr && header.confirmationCount > 0)
        header.confirmations[header.id % header.confirmationCount].store(header.id, std::memory_order_release);
}

std::span<const std::uint8_t> ShadowAtlas::PendingUpload(const ShadowKey& key, const void* drawList, std::uint64_t frame)
{
    const auto found = tiles.find(key.Packed());
    if (found == tiles.end())
        return {};
    Entry& entry = found->second;
    if (UploadDone(entry))
    {
        if (!entry.record.empty())
            std::vector<std::uint8_t>().swap(entry.record);
        return {};
    }
    if (entry.queuedList == drawList && entry.queuedFrame == frame)
        return {};
    entry.queuedList = drawList;
    entry.queuedFrame = frame;
    return std::span<const std::uint8_t>(entry.record.data(), entry.record.size());
}

void ShadowAtlas::FlushUploads()
{
    for (auto& [packed, entry] : tiles)
    {
        if (UploadDone(entry))
            continue;
        RunUpload(context.Get(), entry.record.data(), entry.record.size());
        std::vector<std::uint8_t>().swap(entry.record);
    }
}

int ShadowAtlas::PendingCount() const
{
    int count = 0;
    for (const auto& [packed, entry] : tiles)
    {
        if (!UploadDone(entry))
            ++count;
    }
    return count;
}

const ShadowTile* ShadowAtlas::Acquire(const ShadowKey& key)
{
    if (!device || !context)
        return nullptr;
    const std::uint64_t packed = key.Packed();
    if (const auto found = tiles.find(packed); found != tiles.end())
        return &found->second.tile;

    const ShadowTileLayout layout = TileLayout(key);
    const int slot = layout.size + 2;
    if (slot > kShadowAtlasPageSize)
        return nullptr;

    int pageIndex = -1;
    int slotX = 0;
    int slotY = 0;
    for (int i = 0; i < static_cast<int>(pages.size()) && pageIndex < 0; ++i)
    {
        if (pages[static_cast<size_t>(i)].packer.Allocate(slot, slot, slotX, slotY))
            pageIndex = i;
    }
    if (pageIndex < 0)
    {
        if (!AddPage() || !pages.back().packer.Allocate(slot, slot, slotX, slotY))
            return nullptr;
        pageIndex = static_cast<int>(pages.size()) - 1;
    }

    BakeShadowTile(key, tileScratch);
    ++bakes;
    const Page& page = pages[static_cast<size_t>(pageIndex)];
    Entry entry;
    BuildRecord(entry, page, slotX, slotY, layout);

    ShadowTile& tile = entry.tile;
    tile.texture = static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(page.view.Get()));
    tile.texelUv = ImVec2(1.0f / static_cast<float>(kShadowAtlasPageSize), 1.0f / static_cast<float>(kShadowAtlasPageSize));
    tile.uv = ImVec2(static_cast<float>(slotX + 1) * tile.texelUv.x, static_cast<float>(slotY + 1) * tile.texelUv.y);
    tile.layout = layout;
    return &tiles.emplace(packed, std::move(entry)).first->second.tile;
}

namespace
{

struct ShadowState
{
    ShadowAtlas atlas;
    bool deviceReady = false;
    bool clearPending = false;
};

ShadowState& State()
{
    static ShadowState state;
    return state;
}

bool CreateShadowDeviceObjects()
{
    ShadowState& state = State();
    state.atlas.SetDevice(Device(), DeviceContext());
    state.deviceReady = Device() != nullptr && DeviceContext() != nullptr;
    // Shadows are optional: without a device they draw nothing, which is not fatal.
    return true;
}

void InvalidateShadowDeviceObjects()
{
    ShadowState& state = State();
    state.atlas.ReleaseDevice();
    state.deviceReady = false;
}

void ShadowNewFrame()
{
    ShadowState& state = State();
    if (state.clearPending || state.atlas.NearlyFull())
        state.atlas.Clear();
    state.clearPending = false;
}

// Tiles are baked in pixels, so a new scale needs new tiles. The atlas is emptied at the
// next frame start because draws recorded in this frame may still read the current tiles.
void ShadowScaleChanged(float)
{
    State().clearPending = true;
}

// Uploads a tile while the frame renders. UpdateSubresource binds nothing, so the renderer's
// state needs no reset afterwards.
void ShadowUploadCallback(const ImDrawList*, const ImDrawCmd* command)
{
    if (command == nullptr || command->UserCallbackData == nullptr || command->UserCallbackDataSize <= 0)
        return;
    ShadowAtlas::RunUpload(DeviceContext(), static_cast<const std::uint8_t*>(command->UserCallbackData),
                           static_cast<size_t>(command->UserCallbackDataSize));
}

void DrawNineSlice(ImDrawList* drawList, const NineSlice& grid, ImTextureID texture, ImU32 color, bool skipCenter)
{
    drawList->PushTexture(ImTextureRef(texture));
    const int quads = skipCenter ? 8 : 9;
    drawList->PrimReserve(quads * 6, 16);
    const ImDrawIdx base = static_cast<ImDrawIdx>(drawList->_VtxCurrentIdx);
    for (int row = 0; row < 4; ++row)
    {
        for (int column = 0; column < 4; ++column)
            drawList->PrimWriteVtx(ImVec2(grid.x[column], grid.y[row]), ImVec2(grid.u[column], grid.v[row]), color);
    }
    for (int row = 0; row < 3; ++row)
    {
        for (int column = 0; column < 3; ++column)
        {
            if (skipCenter && row == 1 && column == 1)
                continue;
            const ImDrawIdx a = static_cast<ImDrawIdx>(base + row * 4 + column);
            drawList->PrimWriteIdx(a);
            drawList->PrimWriteIdx(static_cast<ImDrawIdx>(a + 1));
            drawList->PrimWriteIdx(static_cast<ImDrawIdx>(a + 5));
            drawList->PrimWriteIdx(a);
            drawList->PrimWriteIdx(static_cast<ImDrawIdx>(a + 5));
            drawList->PrimWriteIdx(static_cast<ImDrawIdx>(a + 4));
        }
    }
    drawList->PopTexture();
}

// Shared by shadow and glow: blurs the shape pMin..pMax (already offset and spread).
void DrawBlurredShape(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, float sigma, ImU32 color,
                      bool outsideOnly)
{
    if (drawList == nullptr || (color & IM_COL32_A_MASK) == 0)
        return;
    const float width = pMax.x - pMin.x;
    const float height = pMax.y - pMin.y;
    if (!(width > 0.0f) || !(height > 0.0f))
        return;
    ShadowState& state = State();
    if (!state.deviceReady)
        return;
    const float clampedRadius = std::clamp(radius, 0.0f, std::min(width, height) * 0.5f);
    const ShadowKey key = MakeShadowKey(clampedRadius, sigma, outsideOnly);
    const ShadowTile* tile = state.atlas.Acquire(key);
    if (tile == nullptr)
        return;
    // A tile that is not on its page yet is uploaded by a callback in front of its draw.
    const std::span<const std::uint8_t> upload = state.atlas.PendingUpload(key, drawList, FrameIndex());
    if (!upload.empty())
        drawList->AddCallback(&ShadowUploadCallback, const_cast<std::uint8_t*>(upload.data()), upload.size());
    const NineSlice grid = ShadowNineSlice(pMin, pMax, tile->layout, tile->uv, tile->texelUv);
    DrawNineSlice(drawList, grid, tile->texture, color, outsideOnly);
}

} // namespace
} // namespace internal

ShadowSpec ElevationShadow(int level)
{
    const Theme& theme = T();
    const ShadowLevel& shadow = theme.elevation.Level(level);
    ShadowSpec spec;
    spec.offset = Px(shadow.offset);
    spec.blur = Px(shadow.blur);
    spec.spread = Px(shadow.spread);
    spec.color = ToU32(WithAlpha(theme.colors.shadow, shadow.opacity));
    return spec;
}

void DrawShadow(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, const ShadowSpec& spec)
{
    const ImVec2 shapeMin(pMin.x + spec.offset.x - spec.spread, pMin.y + spec.offset.y - spec.spread);
    const ImVec2 shapeMax(pMax.x + spec.offset.x + spec.spread, pMax.y + spec.offset.y + spec.spread);
    // Blur is a radius: the visible falloff ends about `blur` pixels outside the shape,
    // which is two standard deviations.
    internal::DrawBlurredShape(drawList, shapeMin, shapeMax, std::max(0.0f, radius + spec.spread),
                               std::max(0.0f, spec.blur) * 0.5f, spec.color, false);
}

void DrawElevation(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, int level)
{
    if (level <= 0)
        return;
    DrawShadow(drawList, pMin, pMax, radius, ElevationShadow(level));
}

void DrawGlow(ImDrawList* drawList, ImVec2 pMin, ImVec2 pMax, float radius, ImU32 color, float size)
{
    if (!(size > 0.0f))
        return;
    internal::DrawBlurredShape(drawList, pMin, pMax, radius, size * 0.5f, color, true);
}

namespace internal
{

Subsystem ShadowSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "shadow";
    subsystem.shutdown = &InvalidateShadowDeviceObjects;
    subsystem.newFrame = &ShadowNewFrame;
    subsystem.scaleChanged = &ShadowScaleChanged;
    subsystem.invalidateDeviceObjects = &InvalidateShadowDeviceObjects;
    subsystem.createDeviceObjects = &CreateShadowDeviceObjects;
    return subsystem;
}

} // namespace internal

} // namespace facet
