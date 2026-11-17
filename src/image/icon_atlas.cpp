// Icon registry and atlas.
//
// Registry: every embedded assets/icons/<name>.svg is registered under <name> on first use;
// RegisterIcon adds more. Documents are parsed when an icon is first rasterized.
//
// Atlas: pages of kPageSize square RGBA textures, filled shelf by shelf. A glyph is one
// icon at one whole pixel size, rasterized as a white mask on first use and uploaded with
// UpdateSubresource. A scale change empties the atlas at the next frame start, so it only
// holds the sizes the current scale needs.

#include "facet/core/icons.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <lunasvg.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/theme.h"
#include "image/image_internal.h"
#include "internal/alloc_guard.h"
#include "internal/device.h"
#include "internal/embedded_assets.h"
#include "internal/reserve.h"
#include "internal/subsystem.h"

using Microsoft::WRL::ComPtr;

namespace facet
{
namespace
{

constexpr int kPageSize = 1024;
// Larger icons are rasterized at this size and scaled when drawn.
constexpr int kMaxGlyphSide = 256;
// Empty texels around every glyph, so filtering never picks up a neighbour.
constexpr int kGlyphPadding = 1;
// Most pages the atlas holds (16 MiB of textures). A glyph that needs one more empties the
// atlas at the next frame start instead, so icons whose size animates, or apps that use many
// sizes, cannot grow it without bound.
constexpr int kMaxPages = 4;

constexpr std::string_view kIconPrefix = "icons/";
constexpr std::string_view kIconSuffix = ".svg";

struct IconEntry
{
    std::string name;
    std::string_view source;  // embedded bytes or `owned`
    std::string owned;
    std::unique_ptr<lunasvg::Document> document;
    bool parseFailed = false;
};

struct Registry
{
    std::unordered_map<std::uint32_t, IconEntry> entries;
    std::vector<std::uint32_t> order;  // ids sorted by name
};

void SortOrder(Registry& registry)
{
    registry.order.clear();
    registry.order.reserve(registry.entries.size());
    for (const auto& [id, entry] : registry.entries)
        registry.order.push_back(id);
    std::sort(registry.order.begin(), registry.order.end(), [&](std::uint32_t a, std::uint32_t b) {
        return registry.entries.at(a).name < registry.entries.at(b).name;
    });
}

std::unique_ptr<Registry>& RegistryStorage()
{
    static std::unique_ptr<Registry> registry;
    return registry;
}

// The registry with the embedded icons, built on first use. nullptr while it cannot be
// built for lack of memory; the next call tries again.
Registry* Icons()
{
    std::unique_ptr<Registry>& registry = RegistryStorage();
    if (registry == nullptr)
    {
        try
        {
            internal::CheckAllocation();
            auto built = std::make_unique<Registry>();
            for (const internal::EmbeddedAsset& asset : internal::EmbeddedAssets())
            {
                const std::string_view path(asset.name);
                if (!path.starts_with(kIconPrefix) || !path.ends_with(kIconSuffix))
                    continue;
                const std::string_view name =
                    path.substr(kIconPrefix.size(), path.size() - kIconPrefix.size() - kIconSuffix.size());
                // Icons live directly in assets/icons; subfolders are not part of the set.
                if (name.empty() || name.find('/') != std::string_view::npos)
                    continue;
                IconEntry entry;
                entry.name = std::string(name);
                entry.source = std::string_view(reinterpret_cast<const char*>(asset.data), asset.size);
                built->entries.emplace(Icon::Hash(name), std::move(entry));
            }
            SortOrder(*built);
            registry = std::move(built);
        }
        catch (const std::bad_alloc&)
        {
            return nullptr;
        }
    }
    return registry.get();
}

const IconEntry* FindEntry(std::uint32_t id)
{
    const Registry* registry = Icons();
    if (registry == nullptr)
        return nullptr;
    const auto it = registry->entries.find(id);
    return it != registry->entries.end() ? &it->second : nullptr;
}

// Parses the icon on first use. A source that does not parse is remembered as broken; a
// failed allocation throws std::bad_alloc and leaves the entry as it was, so the next use
// tries again.
const lunasvg::Document* Document(std::uint32_t id)
{
    Registry* registry = Icons();
    if (registry == nullptr)
        return nullptr;
    const auto it = registry->entries.find(id);
    if (it == registry->entries.end())
        return nullptr;
    IconEntry& entry = it->second;
    if (entry.document == nullptr && !entry.parseFailed)
    {
        entry.document = internal::ParseSvgDocument(entry.source.data(), entry.source.size());
        entry.parseFailed = entry.document == nullptr;
    }
    return entry.document.get();
}

// --- Atlas --------------------------------------------------------------------------

struct Glyph
{
    int page = -1;  // -1: the icon could not be rasterized; drawn as the placeholder
    ImVec2 uv0;
    ImVec2 uv1;
};

struct Page
{
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
    int cursorX = 0;
    int shelfY = 0;
    int shelfHeight = 0;
};

struct Atlas
{
    std::vector<Page> pages;
    std::unordered_map<std::uint64_t, Glyph> glyphs;  // key: icon id << 32 | pixel size
    internal::RgbaImage scratch;
    bool initialized = false;
    bool deviceValid = false;
    bool resetPending = false;
    // Frame of the last failed allocation; glyphs are not tried again in that frame.
    std::uint64_t failedFrame = 0;
};

Atlas& State()
{
    static Atlas atlas;
    return atlas;
}

void Clear(Atlas& atlas)
{
    atlas.pages.clear();
    atlas.glyphs.clear();
    // Frame numbers restart at Init, so a failure remembered from before would block glyphs
    // in an unrelated frame later.
    atlas.failedFrame = 0;
}

bool AddPage(Atlas& atlas)
{
    ID3D11Device* device = internal::Device();
    if (device == nullptr)
        return false;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = kPageSize;
    desc.Height = kPageSize;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // Transparent from the start, so padding texels sample as empty. A failed allocation
    // throws std::bad_alloc to GetGlyph.
    internal::CheckAllocation();
    const std::vector<std::uint8_t> zeros(static_cast<std::size_t>(kPageSize) * kPageSize * 4u, 0);
    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = zeros.data();
    data.SysMemPitch = kPageSize * 4;

    Page page;
    if (FAILED(device->CreateTexture2D(&desc, &data, &page.texture)) ||
        FAILED(device->CreateShaderResourceView(page.texture.Get(), nullptr, &page.view)))
        return false;
    atlas.pages.push_back(std::move(page));
    return true;
}

// Finds room for a side x side glyph; returns false when no page can be created. Past the
// page budget it asks for the atlas to be emptied at the next frame start (recorded draws
// may still read the pages this frame), where every glyph in use is drawn again.
bool Allocate(Atlas& atlas, int side, int& pageIndex, int& x, int& y)
{
    const int cell = side + kGlyphPadding;
    if (!atlas.pages.empty())
    {
        Page& page = atlas.pages.back();
        if (page.cursorX + cell > kPageSize)
        {
            page.shelfY += page.shelfHeight;
            page.cursorX = 0;
            page.shelfHeight = 0;
        }
        if (page.shelfY + cell <= kPageSize)
        {
            pageIndex = static_cast<int>(atlas.pages.size()) - 1;
            x = page.cursorX + kGlyphPadding;
            y = page.shelfY + kGlyphPadding;
            page.cursorX += cell;
            page.shelfHeight = std::max(page.shelfHeight, cell);
            return true;
        }
    }
    if (static_cast<int>(atlas.pages.size()) >= kMaxPages)
    {
        atlas.resetPending = true;
        return false;
    }
    if (!AddPage(atlas))
        return false;
    Page& page = atlas.pages.back();
    pageIndex = static_cast<int>(atlas.pages.size()) - 1;
    x = kGlyphPadding;
    y = kGlyphPadding;
    page.cursorX = cell;
    page.shelfHeight = cell;
    return true;
}

const Glyph* GetGlyph(Atlas& atlas, std::uint32_t id, int side)
{
    const std::uint64_t key = (static_cast<std::uint64_t>(id) << 32) | static_cast<std::uint32_t>(side);
    const auto found = atlas.glyphs.find(key);
    if (found != atlas.glyphs.end())
        return &found->second;

    // Only a source that cannot be rasterized is remembered as missing. A missing context or
    // a full or failed page is transient: the glyph is tried again in a later frame.
    ID3D11DeviceContext* context = internal::DeviceContext();
    const std::uint64_t frame = internal::FrameIndex();
    if (context == nullptr || (atlas.failedFrame != 0 && atlas.failedFrame == frame))
        return nullptr;
    // Running out of memory is transient as well: the glyph is not remembered. That
    // includes a registry that could not be built yet, which says nothing about the icon.
    if (Icons() == nullptr)
    {
        atlas.failedFrame = frame;
        return nullptr;
    }
    // The map entry is created before a page gives out room, and nothing after that can
    // fail, so a failed attempt takes no room in the atlas.
    try
    {
        if (!internal::RasterizeIcon(id, side, atlas.scratch))
            return &atlas.glyphs.emplace(key, Glyph{}).first->second;
        internal::CheckAllocation();
        Glyph& glyph = atlas.glyphs.emplace(key, Glyph{}).first->second;
        int pageIndex = 0;
        int x = 0;
        int y = 0;
        if (!Allocate(atlas, side, pageIndex, x, y))
        {
            atlas.glyphs.erase(key);
            atlas.failedFrame = frame;
            return nullptr;
        }
        const D3D11_BOX box = {static_cast<UINT>(x), static_cast<UINT>(y), 0, static_cast<UINT>(x + side),
                               static_cast<UINT>(y + side), 1};
        context->UpdateSubresource(atlas.pages[static_cast<std::size_t>(pageIndex)].texture.Get(), 0, &box,
                                   atlas.scratch.pixels.data(), static_cast<UINT>(side * 4), 0);
        constexpr float kInv = 1.0f / static_cast<float>(kPageSize);
        glyph.page = pageIndex;
        glyph.uv0 = ImVec2(static_cast<float>(x) * kInv, static_cast<float>(y) * kInv);
        glyph.uv1 = ImVec2(static_cast<float>(x + side) * kInv, static_cast<float>(y + side) * kInv);
        return &glyph;
    }
    catch (const std::bad_alloc&)
    {
        // A page that could not be created throws from Allocate: drop the unfinished entry.
        atlas.glyphs.erase(key);
        atlas.failedFrame = frame;
        return nullptr;
    }
}

// The placeholder for an unknown or unreadable icon: a crossed box in the tint color.
void DrawMissing(ImDrawList* drawList, ImVec2 pos, float side, ImU32 color)
{
    const float thickness = HairlineThickness();
    const float inset = std::floor(side * 0.125f);
    const ImVec2 a(pos.x + inset, pos.y + inset);
    const ImVec2 b(pos.x + side - inset, pos.y + side - inset);
    StrokeRoundRect(drawList, a, b, color, 0.0f, thickness);
    drawList->AddLine(a, b, color, thickness);
    drawList->AddLine(ImVec2(a.x, b.y), ImVec2(b.x, a.y), color, thickness);
}

// --- Hooks ---------------------------------------------------------------------------

bool InitHook()
{
    Atlas& atlas = State();
    atlas.initialized = true;
    atlas.deviceValid = false;
    atlas.resetPending = false;
    return true;
}

void ShutdownHook()
{
    Atlas& atlas = State();
    Clear(atlas);
    atlas.scratch = internal::RgbaImage{};
    atlas.initialized = false;
    atlas.deviceValid = false;
    atlas.resetPending = false;
}

void InvalidateHook()
{
    Atlas& atlas = State();
    Clear(atlas);
    atlas.deviceValid = false;
}

bool CreateHook()
{
    // Pages are created on demand.
    State().deviceValid = true;
    return true;
}

void NewFrameHook()
{
    Atlas& atlas = State();
    if (!atlas.resetPending)
        return;
    // The previous frame was rendered, so the old pages are no longer referenced.
    atlas.resetPending = false;
    Clear(atlas);
}

void ScaleChangedHook(float)
{
    State().resetPending = true;
}

} // namespace

void DrawIcon(ImDrawList* drawList, Icon icon, ImVec2 pos, float size, ImU32 color)
{
    Atlas& atlas = State();
    if (drawList == nullptr || !(size > 0.0f) || (color & IM_COL32_A_MASK) == 0 || !atlas.initialized ||
        !atlas.deviceValid)
        return;
    const float side = std::max(1.0f, std::round(size));
    const ImVec2 snapped(std::floor(pos.x + 0.5f), std::floor(pos.y + 0.5f));
    const int glyphSide = static_cast<int>(std::min(side, static_cast<float>(kMaxGlyphSide)));
    const Glyph* glyph = icon.IsValid() ? GetGlyph(atlas, icon.id, glyphSide) : nullptr;
    if (glyph == nullptr || glyph->page < 0)
    {
        DrawMissing(drawList, snapped, side, color);
        return;
    }
    const ImTextureRef texture(static_cast<ImTextureID>(
        reinterpret_cast<std::uintptr_t>(atlas.pages[static_cast<std::size_t>(glyph->page)].view.Get())));
    drawList->AddImage(texture, snapped, ImVec2(snapped.x + side, snapped.y + side), glyph->uv0, glyph->uv1,
                       color);
}

void IconGlyph(Icon icon, float size, ImU32 color)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;
    const float side = size > 0.0f ? size : ImGui::GetFontSize();
    const float height = std::max(side, ImGui::GetFontSize());
    const ImVec2 pos(window->DC.CursorPos.x, window->DC.CursorPos.y + window->DC.CurrLineTextBaseOffset);
    const ImRect bb(pos, ImVec2(pos.x + side, pos.y + height));
    ImGui::ItemSize(bb.GetSize(), 0.0f);
    if (!ImGui::ItemAdd(bb, 0))
        return;
    if (color == 0)
    {
        const ImVec4 label = T().colors.label;
        color = ToU32(WithAlpha(label, label.w * ImGui::GetStyle().Alpha));
    }
    DrawIcon(window->DrawList, icon, ImVec2(pos.x, pos.y + 0.5f * (height - side)), side, color);
}

Icon RegisterIcon(const char* name, const char* svgSource)
{
    if (name == nullptr || name[0] == '\0' || svgSource == nullptr)
        return {};
    const std::string_view nameView(name);
    const std::uint32_t id = Icon::Hash(nameView);
    Registry* registry = Icons();
    if (registry == nullptr)
        return {};
    const auto existing = registry->entries.find(id);
    if (existing != registry->entries.end() && existing->second.name != nameView)
        return {};

    // Everything that allocates happens before the registry changes, so running out of
    // memory leaves it as it was.
    IconEntry entry;
    try
    {
        internal::CheckAllocation();
        entry.name = std::string(nameView);
        entry.owned = svgSource;
        entry.document = internal::ParseSvgDocument(entry.owned.data(), entry.owned.size());
        if (entry.document == nullptr)
            return {};
        if (existing == registry->entries.end())
        {
            // Room for the new id in the sorted order, so SortOrder below cannot allocate.
            internal::CheckAllocation();
            internal::ReserveAtLeast(registry->order, registry->entries.size() + 1);
        }
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
    entry.source = entry.owned;

    if (existing != registry->entries.end())
    {
        existing->second = std::move(entry);
        // std::string moves may keep the small-string buffer inside the object, so point
        // the view at the moved string again.
        existing->second.source = existing->second.owned;
        // Drop the glyphs of the old drawing; their atlas space stays unused until the
        // atlas is rebuilt.
        Atlas& atlas = State();
        std::erase_if(atlas.glyphs, [id](const auto& item) { return static_cast<std::uint32_t>(item.first >> 32) == id; });
    }
    else
    {
        try
        {
            internal::CheckAllocation();
            // A single insertion that throws has no effect.
            auto inserted = registry->entries.emplace(id, std::move(entry)).first;
            inserted->second.source = inserted->second.owned;
        }
        catch (const std::bad_alloc&)
        {
            return {};
        }
        SortOrder(*registry);
    }
    return Icon(nameView);
}

Icon FindIcon(const char* name)
{
    if (name == nullptr)
        return {};
    const std::string_view nameView(name);
    const IconEntry* entry = FindEntry(Icon::Hash(nameView));
    return entry != nullptr && entry->name == nameView ? Icon(nameView) : Icon();
}

int IconCount()
{
    const Registry* registry = Icons();
    return registry != nullptr ? static_cast<int>(registry->order.size()) : 0;
}

const char* IconNameAt(int index)
{
    const Registry* registry = Icons();
    if (registry == nullptr || index < 0 || static_cast<std::size_t>(index) >= registry->order.size())
        return nullptr;
    return registry->entries.at(registry->order[static_cast<std::size_t>(index)]).name.c_str();
}

const char* IconName(Icon icon)
{
    const IconEntry* entry = FindEntry(icon.id);
    return entry != nullptr ? entry->name.c_str() : nullptr;
}

namespace internal
{

int IconAtlasPageCount()
{
    return static_cast<int>(State().pages.size());
}

void ResetIconRegistry()
{
    RegistryStorage().reset();
}

bool RasterizeIcon(std::uint32_t iconId, int pixels, RgbaImage& out)
{
    const lunasvg::Document* document = Document(iconId);
    if (document == nullptr)
        return false;
    return RasterizeSvgDocument(*document, pixels, pixels, true, out);
}

Subsystem IconSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "icon";
    subsystem.init = &InitHook;
    subsystem.shutdown = &ShutdownHook;
    subsystem.newFrame = &NewFrameHook;
    subsystem.scaleChanged = &ScaleChangedHook;
    subsystem.invalidateDeviceObjects = &InvalidateHook;
    subsystem.createDeviceObjects = &CreateHook;
    return subsystem;
}

} // namespace internal

} // namespace facet
