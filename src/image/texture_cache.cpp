// Image handles and the texture cache.
//
// Slots live in one vector; a handle names a slot by index and must carry the generation
// the slot holds. Generations come from one counter that never restarts, so a handle from
// before Shutdown or ReleaseImage can never match a later image.
//
// Asynchronous loads: a small pool of worker threads (started on the first asynchronous
// request) reads and decodes; the main thread uploads finished results in newFrame or in
// WaitForImage. Results whose slot was released or reused in the meantime are dropped.
//
// Running out of memory: every allocation a public function makes sits in a narrow try
// block that turns std::bad_alloc into the function's failure value (alloc_guard.h). The
// bookkeeping that undoes a half-finished load never allocates: Allocate reserves room in
// freeSlots and releaseQueue for every slot it hands out, and an asynchronous load is one
// Task the frame thread allocates up front, which a worker hands back by linking it into the
// result list.

#include "facet/core/image.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <d3d11.h>
#include <windows.h>
#include <wrl/client.h>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "image/image_internal.h"
#include "internal/alloc_guard.h"
#include "internal/device.h"
#include "internal/image_io.h"
#include "internal/reserve.h"
#include "internal/subsystem.h"

using Microsoft::WRL::ComPtr;

namespace facet
{
namespace
{

// A worker keeps its file buffer between jobs up to this capacity (4 MiB).
constexpr std::size_t kKeptReadBufferBytes = std::size_t{4} << 20;
// WaitForImage treats a timeout of this length (one day) or longer, infinity included, as
// "no time limit".
constexpr float kUnlimitedWaitSeconds = 86400.0f;

enum class SourceKind : std::uint8_t
{
    None,
    File,    // `path` is kept, so the image can be loaded again after device loss
    Memory,  // encoded bytes, not kept
    Pixels,  // CreateImageRGBA, not kept
    Svg,     // `svg` is kept and rasterized per scale
};

struct Slot
{
    std::uint32_t generation = 0;  // 0 while the slot is free
    ImageStatus status = ImageStatus::Invalid;
    SourceKind kind = SourceKind::None;
    ComPtr<ID3D11ShaderResourceView> view;
    int width = 0;
    int height = 0;
    std::string path;
    internal::SvgSourcePtr svg;
};

// One asynchronous load: queued as a job, decoded by a worker, then linked into the result
// list. The frame thread allocates it, so a worker never allocates to report a result.
struct Task
{
    std::uint32_t index = 0;
    std::uint32_t generation = 0;
    std::string path;                  // read from this file when not empty
    std::vector<std::uint8_t> data;    // otherwise decode these bytes
    internal::DecodedImage image;      // empty when reading or decoding failed
    std::unique_ptr<Task> next;        // the next finished task in the result list

    Task() = default;
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task() { internal::FreeImage(image); }
};

// Frees a result list one task at a time, so a long list cannot nest destructor calls.
void FreeTasks(std::unique_ptr<Task> head)
{
    while (head != nullptr)
        head = std::move(head->next);
}

struct Cache
{
    std::vector<Slot> slots;
    std::vector<std::uint32_t> freeSlots;  // capacity covers every slot (see Allocate)
    std::uint32_t nextGeneration = 1;
    bool initialized = false;
    bool deviceValid = false;
    bool svgScaleDirty = false;

    // Views dropped during the frame; released in the next newFrame. Its spare capacity
    // covers every slot that can still drop a view (see Allocate).
    std::vector<ComPtr<ID3D11ShaderResourceView>> releaseQueue;

    // Worker pool. `mutex` guards jobs, results and stop.
    std::mutex mutex;
    std::condition_variable jobReady;
    std::condition_variable resultReady;
    std::deque<std::unique_ptr<Task>> jobs;
    std::unique_ptr<Task> results;  // finished tasks, newest first
    std::vector<std::thread> workers;
    bool stop = false;
};

Cache& State()
{
    static Cache cache;
    return cache;
}

std::uint32_t NextGeneration(Cache& cache)
{
    const std::uint32_t generation = cache.nextGeneration++;
    if (cache.nextGeneration == 0)
        cache.nextGeneration = 1;
    return generation;
}

Slot* Resolve(ImageHandle handle)
{
    Cache& cache = State();
    if (!handle.IsValid() || handle.index >= cache.slots.size())
        return nullptr;
    Slot& slot = cache.slots[handle.index];
    return slot.generation == handle.generation ? &slot : nullptr;
}

// Takes a free slot and gives it a new generation, or returns an invalid handle when the
// bookkeeping cannot grow. Before handing out a slot it reserves what Free needs, so Free
// never allocates: freeSlots holds room for every slot, and releaseQueue has room for one
// more view per slot (no more slots than that can hold a view at once).
ImageHandle Allocate(Cache& cache, SourceKind kind)
{
    const bool grow = cache.freeSlots.empty();
    const std::size_t slotCount = cache.slots.size() + (grow ? 1u : 0u);
    try
    {
        internal::CheckAllocation();
        internal::ReserveAtLeast(cache.freeSlots, slotCount);
        internal::ReserveAtLeast(cache.releaseQueue, cache.releaseQueue.size() + slotCount);
        if (grow)
            cache.slots.emplace_back();
    }
    catch (const std::bad_alloc&)
    {
        return {};
    }
    std::uint32_t index = 0;
    if (!grow)
    {
        index = cache.freeSlots.back();
        cache.freeSlots.pop_back();
    }
    else
    {
        index = static_cast<std::uint32_t>(cache.slots.size() - 1);
    }
    Slot& slot = cache.slots[index];
    slot = Slot{};
    slot.generation = NextGeneration(cache);
    slot.kind = kind;
    return ImageHandle{index, slot.generation};
}

// Never allocates: the room it pushes into was reserved by Allocate.
void Free(Cache& cache, std::uint32_t index)
{
    Slot& slot = cache.slots[index];
    if (slot.status == ImageStatus::Loading)
    {
        // A job still in the queue would only be decoded and then dropped; remove it. A
        // decode already running finishes and its result is dropped in ProcessResults.
        const std::uint32_t generation = slot.generation;
        std::lock_guard lock(cache.mutex);
        std::erase_if(cache.jobs, [&](const std::unique_ptr<Task>& job) {
            return job->index == index && job->generation == generation;
        });
    }
    if (slot.view)
        cache.releaseQueue.push_back(std::move(slot.view));
    slot = Slot{};
    cache.freeSlots.push_back(index);
}

// The decoder's side limit is the upload limit, so no image is decoded only to be rejected.
static_assert(internal::kMaxImageSide == D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION);

ComPtr<ID3D11ShaderResourceView> CreateTexture(const void* pixels, int width, int height, int stride)
{
    ID3D11Device* device = internal::Device();
    if (device == nullptr || pixels == nullptr || width <= 0 || height <= 0 ||
        width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION || height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION)
        return nullptr;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = pixels;
    data.SysMemPitch = static_cast<UINT>(stride);

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(device->CreateTexture2D(&desc, &data, &texture)))
        return nullptr;
    ComPtr<ID3D11ShaderResourceView> view;
    if (FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, &view)))
        return nullptr;
    return view;
}

// Uploads pixels into the slot and sets its status to Ready or Failed.
void Upload(Slot& slot, const void* pixels, int width, int height, int stride)
{
    slot.view = CreateTexture(pixels, width, height, stride);
    if (slot.view)
    {
        slot.width = width;
        slot.height = height;
        slot.status = ImageStatus::Ready;
    }
    else
    {
        slot.width = 0;
        slot.height = 0;
        slot.status = ImageStatus::Failed;
    }
}

bool RasterizeSlot(Slot& slot)
{
    internal::RgbaImage raster;
    bool rasterized = false;
    try
    {
        rasterized = slot.svg != nullptr && internal::RasterizeSvgSource(*slot.svg, Scale(), raster);
    }
    catch (const std::bad_alloc&)
    {
        rasterized = false;
    }
    if (!rasterized)
    {
        slot.status = ImageStatus::Failed;
        return false;
    }
    Upload(slot, raster.pixels.data(), raster.width, raster.height, raster.width * 4);
    return slot.status == ImageStatus::Ready;
}

// --- File reading -------------------------------------------------------------

// Returns false (with `out` empty) instead of throwing when a buffer cannot be allocated.
bool ReadFile(std::string_view path, std::vector<std::uint8_t>& out)
{
    out.clear();
    if (path.empty() || path.size() > static_cast<std::size_t>(INT_MAX))
        return false;
    const int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(),
                                               static_cast<int>(path.size()), nullptr, 0);
    if (wideLength <= 0)
        return false;
    std::wstring widePath;
    try
    {
        internal::CheckAllocation();
        widePath.resize(static_cast<std::size_t>(wideLength));
    }
    catch (const std::bad_alloc&)
    {
        return false;
    }
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
                        widePath.data(), wideLength);

    std::FILE* file = nullptr;
    if (_wfopen_s(&file, widePath.c_str(), L"rb") != 0 || file == nullptr)
        return false;
    bool ok = false;
    if (_fseeki64(file, 0, SEEK_END) == 0)
    {
        // Larger files are rejected before anything is allocated (internal/image_io.h).
        const long long length = _ftelli64(file);
        if (length > 0 && static_cast<unsigned long long>(length) <= internal::kMaxEncodedImageBytes &&
            _fseeki64(file, 0, SEEK_SET) == 0)
        {
            bool sized = false;
            try
            {
                internal::CheckAllocation();
                out.resize(static_cast<std::size_t>(length));
                sized = true;
            }
            catch (const std::bad_alloc&)
            {
                sized = false;
            }
            ok = sized && std::fread(out.data(), 1, out.size(), file) == out.size();
        }
    }
    std::fclose(file);
    if (!ok)
        out.clear();
    return ok;
}

// --- Worker pool --------------------------------------------------------------

void WorkerMain()
{
    Cache& cache = State();
    std::vector<std::uint8_t> fileBytes;
    for (;;)
    {
        std::unique_ptr<Task> task;
        {
            std::unique_lock lock(cache.mutex);
            cache.jobReady.wait(lock, [&] { return cache.stop || !cache.jobs.empty(); });
            if (cache.stop)
                return;
            task = std::move(cache.jobs.front());
            cache.jobs.pop_front();
        }

        // An exception leaving a worker would end the host process. A failed allocation (or
        // any other throw) turns into an empty image, so the slot still resolves to Failed.
        try
        {
            if (!task->path.empty())
            {
                if (ReadFile(task->path, fileBytes))
                    internal::DecodeImage(fileBytes.data(), fileBytes.size(), task->image);
            }
            else
            {
                internal::DecodeImage(task->data.data(), task->data.size(), task->image);
            }
        }
        catch (...)
        {
            internal::FreeImage(task->image);
        }
        // The encoded copy is not needed any more.
        std::vector<std::uint8_t>().swap(task->data);
        // Do not keep the buffer of an unusually large file for the rest of the run.
        if (fileBytes.capacity() > kKeptReadBufferBytes)
            std::vector<std::uint8_t>().swap(fileBytes);

        {
            std::lock_guard lock(cache.mutex);
            if (cache.stop)
                return;  // the task and its pixels are freed with `task`
            // Linking the task in allocates nothing, so a worker cannot fail here.
            task->next = std::move(cache.results);
            cache.results = std::move(task);
        }
        cache.resultReady.notify_all();
    }
}

// Two workers keep a decode from blocking the next one without taking many cores from
// the host; one on single-core machines.
void StartWorkers(Cache& cache)
{
    if (!cache.workers.empty())
        return;
    const unsigned cores = std::thread::hardware_concurrency();
    const unsigned count = cores > 1 ? 2u : 1u;
    {
        std::lock_guard lock(cache.mutex);
        cache.stop = false;
    }
    for (unsigned i = 0; i < count; ++i)
        cache.workers.emplace_back(WorkerMain);
}

// Stops the pool: queued jobs are dropped, decodes in progress finish and are discarded.
void StopWorkers(Cache& cache)
{
    {
        std::lock_guard lock(cache.mutex);
        cache.stop = true;
        cache.jobs.clear();
    }
    cache.jobReady.notify_all();
    for (std::thread& worker : cache.workers)
        worker.join();
    cache.workers.clear();

    std::unique_ptr<Task> finished;
    {
        std::lock_guard lock(cache.mutex);
        finished = std::move(cache.results);
        cache.stop = false;
    }
    FreeTasks(std::move(finished));
}

// Queues `task`, starting the workers first if needed. Returns false and leaves `task` with
// the caller when the queue cannot grow or no worker can be started.
bool Enqueue(Cache& cache, std::unique_ptr<Task>& task)
{
    try
    {
        internal::CheckAllocation();
        StartWorkers(cache);
        std::lock_guard lock(cache.mutex);
        // Adding at the end of a deque has no effect when it throws, so the task stays ours.
        cache.jobs.push_back(std::move(task));
    }
    catch (const std::exception&)
    {
        // std::bad_alloc from the queue, std::system_error when a thread cannot start.
        return false;
    }
    cache.jobReady.notify_one();
    return true;
}

// Uploads every finished decode whose slot still waits for it.
void ProcessResults(Cache& cache)
{
    if (!cache.deviceValid)
        return;
    std::unique_ptr<Task> finished;
    {
        std::lock_guard lock(cache.mutex);
        if (cache.results == nullptr)
            return;
        finished = std::move(cache.results);
    }
    for (Task* task = finished.get(); task != nullptr; task = task->next.get())
    {
        Slot* slot = Resolve(ImageHandle{task->index, task->generation});
        if (slot != nullptr && slot->status == ImageStatus::Loading)
        {
            if (task->image.pixels != nullptr)
                Upload(*slot, task->image.pixels, task->image.width, task->image.height, task->image.width * 4);
            else
                slot->status = ImageStatus::Failed;
        }
        internal::FreeImage(task->image);
    }
    FreeTasks(std::move(finished));
}

// Gives `task` a slot and queues it. On failure nothing is left behind: no slot, no job.
ImageHandle StartAsync(std::unique_ptr<Task> task, SourceKind kind)
{
    Cache& cache = State();
    if (!cache.initialized)
        return {};
    const ImageHandle handle = Allocate(cache, kind);
    if (!handle.IsValid())
        return {};
    Slot& slot = cache.slots[handle.index];
    slot.status = ImageStatus::Loading;
    if (kind == SourceKind::File)
    {
        try
        {
            internal::CheckAllocation();
            slot.path = task->path;
        }
        catch (const std::bad_alloc&)
        {
            Free(cache, handle.index);
            return {};
        }
    }
    task->index = handle.index;
    task->generation = handle.generation;
    if (!Enqueue(cache, task))
    {
        Free(cache, handle.index);
        return {};
    }
    return handle;
}

// A task for an asynchronous load; nullptr when it cannot be allocated.
std::unique_ptr<Task> NewTask(std::string_view path, const void* data, std::size_t size)
{
    try
    {
        internal::CheckAllocation();
        auto task = std::make_unique<Task>();
        task->path = path;
        if (data != nullptr)
        {
            const auto* bytes = static_cast<const std::uint8_t*>(data);
            task->data.assign(bytes, bytes + size);
        }
        return task;
    }
    catch (const std::bad_alloc&)
    {
        return nullptr;
    }
}

// --- Subsystem hooks ------------------------------------------------------------

bool InitHook()
{
    Cache& cache = State();
    cache.initialized = true;
    cache.deviceValid = false;
    cache.svgScaleDirty = false;
    return true;
}

void ShutdownHook()
{
    Cache& cache = State();
    StopWorkers(cache);
    cache.slots.clear();
    cache.freeSlots.clear();
    cache.releaseQueue.clear();
    cache.initialized = false;
    cache.deviceValid = false;
    cache.svgScaleDirty = false;
}

void InvalidateHook()
{
    Cache& cache = State();
    cache.releaseQueue.clear();
    for (Slot& slot : cache.slots)
    {
        slot.view.Reset();
        if (slot.status == ImageStatus::Ready)
            slot.status = ImageStatus::Lost;
        slot.width = 0;
        slot.height = 0;
    }
    cache.deviceValid = false;
}

bool CreateHook()
{
    Cache& cache = State();
    cache.deviceValid = true;
    for (std::uint32_t index = 0; index < cache.slots.size(); ++index)
    {
        Slot& slot = cache.slots[index];
        if (slot.generation == 0 || slot.status != ImageStatus::Lost)
            continue;
        if (slot.kind == SourceKind::Svg)
        {
            RasterizeSlot(slot);
        }
        else if (slot.kind == SourceKind::File)
        {
            std::unique_ptr<Task> task = NewTask(slot.path, nullptr, 0);
            if (task != nullptr)
            {
                task->index = index;
                task->generation = slot.generation;
            }
            // Without memory for the reload the image fails like an unreadable file.
            slot.status = task != nullptr && Enqueue(cache, task) ? ImageStatus::Loading : ImageStatus::Failed;
        }
    }
    cache.svgScaleDirty = false;
    return true;
}

void NewFrameHook()
{
    Cache& cache = State();
    // The previous frame was rendered, so nothing references these views any more.
    cache.releaseQueue.clear();

    if (cache.svgScaleDirty && cache.deviceValid)
    {
        // Every replaced raster drops a view, and each slot may drop one more later in the
        // frame (see Allocate). Without room for both the rasters wait for a later frame.
        try
        {
            cache.releaseQueue.reserve(cache.slots.size() * 2);
        }
        catch (const std::bad_alloc&)
        {
            ProcessResults(cache);
            return;
        }
        cache.svgScaleDirty = false;
        for (Slot& slot : cache.slots)
        {
            if (slot.generation == 0 || slot.kind != SourceKind::Svg || slot.status != ImageStatus::Ready)
                continue;
            // The old raster stays alive until the next frame in case it is still drawn.
            if (slot.view)
                cache.releaseQueue.push_back(std::move(slot.view));
            RasterizeSlot(slot);
        }
    }
    ProcessResults(cache);
}

void ScaleChangedHook(float)
{
    // Rasterized at the start of the next frame: this frame may already have drawn the
    // current textures.
    State().svgScaleDirty = true;
}

// --- Drawing ----------------------------------------------------------------------

float StyleAlpha()
{
    return ImGui::GetCurrentContext() != nullptr ? ImGui::GetStyle().Alpha : 1.0f;
}

// Fills the destination rectangle with the texture: the shape comes from FillRect, so the
// corners follow the theme, and its vertices get UVs mapped linearly over the rectangle.
void DrawTextured(ImDrawList* drawList, ImTextureRef texture, ImVec2 textureSize, ImVec2 pMin, ImVec2 pMax,
                  const ImageOptions& options)
{
    const internal::FitResult fit = internal::FitImage(pMin, pMax, textureSize, options.fit);
    if (fit.max.x <= fit.min.x || fit.max.y <= fit.min.y)
        return;
    const ImU32 tint = ScaleAlpha(options.tint, StyleAlpha());
    if ((tint & IM_COL32_A_MASK) == 0)
        return;

    drawList->PushTexture(texture);
    // Rounded corners must be real geometry here: the baked corner shapes live in the font
    // atlas, which is not the texture bound for this shape.
    drawList->PushDrawFlag(ImDrawFlags_UseTexForRoundCorners, false);
    const int vertexStart = drawList->VtxBuffer.Size;
    FillRect(drawList, fit.min, fit.max, tint, options.radius);
    ImGui::ShadeVertsLinearUV(drawList, vertexStart, drawList->VtxBuffer.Size, fit.min, fit.max, fit.uvMin,
                              fit.uvMax, true);
    drawList->PopDrawFlag();
    drawList->PopTexture();
}

ImVec2 ViewSize(ID3D11ShaderResourceView* view)
{
    if (view == nullptr)
        return ImVec2(0.0f, 0.0f);
    ComPtr<ID3D11Resource> resource;
    view->GetResource(&resource);
    ComPtr<ID3D11Texture2D> texture;
    if (!resource || FAILED(resource.As(&texture)))
        return ImVec2(0.0f, 0.0f);
    D3D11_TEXTURE2D_DESC desc = {};
    texture->GetDesc(&desc);
    return ImVec2(static_cast<float>(desc.Width), static_cast<float>(desc.Height));
}

ImTextureRef ToTextureRef(ID3D11ShaderResourceView* view)
{
    return ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(view)));
}

// Item size: explicit components win, 0 takes the image size and keeps its aspect ratio.
ImVec2 ResolveItemSize(ImVec2 size, ImVec2 imageSize)
{
    if (size.x > 0.0f && size.y > 0.0f)
        return size;
    if (imageSize.x <= 0.0f || imageSize.y <= 0.0f)
    {
        const float side = Px(T().metrics.controlHeightLarge);
        return ImVec2(size.x > 0.0f ? size.x : side, size.y > 0.0f ? size.y : side);
    }
    if (size.x > 0.0f)
        return ImVec2(size.x, size.x * imageSize.y / imageSize.x);
    if (size.y > 0.0f)
        return ImVec2(size.y * imageSize.x / imageSize.y, size.y);
    return imageSize;
}

// Reserves the item; returns false when it is clipped.
bool AddItem(ImVec2 size, ImRect& bb)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
    bb = ImRect(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + size.x, window->DC.CursorPos.y + size.y));
    ImGui::ItemSize(bb);
    return ImGui::ItemAdd(bb, 0);
}

} // namespace

// --- Loading ----------------------------------------------------------------------

ImageHandle LoadImageMemory(const void* data, std::size_t size)
{
    Cache& cache = State();
    if (!cache.initialized || !cache.deviceValid)
        return {};
    // The decoder allocates with malloc and reports a failed allocation as undecodable data.
    internal::DecodedImage decoded;
    if (!internal::DecodeImage(data, size, decoded))
        return {};
    const ImageHandle handle = Allocate(cache, SourceKind::Memory);
    if (!handle.IsValid())
    {
        internal::FreeImage(decoded);
        return {};
    }
    Slot& slot = cache.slots[handle.index];
    Upload(slot, decoded.pixels, decoded.width, decoded.height, decoded.width * 4);
    internal::FreeImage(decoded);
    if (slot.status != ImageStatus::Ready)
    {
        Free(cache, handle.index);
        return {};
    }
    return handle;
}

ImageHandle LoadImageFile(const char* path)
{
    Cache& cache = State();
    if (path == nullptr || !cache.initialized || !cache.deviceValid)
        return {};
    std::vector<std::uint8_t> bytes;
    if (!ReadFile(path, bytes))
        return {};
    const ImageHandle handle = LoadImageMemory(bytes.data(), bytes.size());
    if (!handle.IsValid())
        return {};
    Slot& slot = cache.slots[handle.index];
    try
    {
        internal::CheckAllocation();
        slot.path = path;
    }
    catch (const std::bad_alloc&)
    {
        Free(cache, handle.index);
        return {};
    }
    slot.kind = SourceKind::File;
    return handle;
}

ImageHandle LoadImageFileAsync(const char* path)
{
    if (path == nullptr || path[0] == '\0' || !State().initialized)
        return {};
    std::unique_ptr<Task> task = NewTask(path, nullptr, 0);
    if (task == nullptr)
        return {};
    return StartAsync(std::move(task), SourceKind::File);
}

ImageHandle LoadImageMemoryAsync(const void* data, std::size_t size)
{
    // Larger data would be rejected by the decoder; refuse it before copying it.
    if (data == nullptr || size == 0 || size > internal::kMaxEncodedImageBytes || !State().initialized)
        return {};
    std::unique_ptr<Task> task = NewTask({}, data, size);
    if (task == nullptr)
        return {};
    return StartAsync(std::move(task), SourceKind::Memory);
}

ImageHandle CreateImageRGBA(const void* pixels, int width, int height, int stride)
{
    Cache& cache = State();
    if (!cache.initialized || !cache.deviceValid || pixels == nullptr || width <= 0 || height <= 0 ||
        width > INT_MAX / 4)
        return {};
    if (stride == 0)
        stride = width * 4;
    if (stride < width * 4)
        return {};
    const ImageHandle handle = Allocate(cache, SourceKind::Pixels);
    if (!handle.IsValid())
        return {};
    Slot& slot = cache.slots[handle.index];
    Upload(slot, pixels, width, height, stride);
    if (slot.status != ImageStatus::Ready)
    {
        Free(cache, handle.index);
        return {};
    }
    return handle;
}

void ReleaseImage(ImageHandle image)
{
    if (Resolve(image) != nullptr)
        Free(State(), image.index);
}

ImageStatus GetImageStatus(ImageHandle image)
{
    const Slot* slot = Resolve(image);
    return slot != nullptr ? slot->status : ImageStatus::Invalid;
}

bool IsImageReady(ImageHandle image)
{
    return GetImageStatus(image) == ImageStatus::Ready;
}

bool ImageFailed(ImageHandle image)
{
    const ImageStatus status = GetImageStatus(image);
    return status != ImageStatus::Ready && status != ImageStatus::Loading;
}

ImVec2 ImageSize(ImageHandle image)
{
    const Slot* slot = Resolve(image);
    if (slot == nullptr || slot->status != ImageStatus::Ready)
        return ImVec2(0.0f, 0.0f);
    return ImVec2(static_cast<float>(slot->width), static_cast<float>(slot->height));
}

ImTextureRef ImageTexture(ImageHandle image)
{
    const Slot* slot = Resolve(image);
    if (slot == nullptr || slot->status != ImageStatus::Ready)
        return ImTextureRef();
    return ToTextureRef(slot->view.Get());
}

bool WaitForImage(ImageHandle image, float timeoutSeconds)
{
    Cache& cache = State();
    // Converting a huge float to clock ticks would overflow into a deadline in the past, so
    // timeouts from one day up (and infinity) wait without a limit. NaN counts as 0.
    const bool unlimited = timeoutSeconds >= kUnlimitedWaitSeconds;
    const float seconds = timeoutSeconds > 0.0f && !unlimited ? timeoutSeconds : 0.0f;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                              std::chrono::duration<float>(seconds));
    const auto hasResult = [&] { return cache.results != nullptr; };
    for (;;)
    {
        ProcessResults(cache);
        const ImageStatus status = GetImageStatus(image);
        if (status != ImageStatus::Loading || !cache.deviceValid)
            return status == ImageStatus::Ready;
        std::unique_lock lock(cache.mutex);
        if (cache.results != nullptr)
            continue;
        if (unlimited)
        {
            cache.resultReady.wait(lock, hasResult);
            continue;
        }
        if (!cache.resultReady.wait_until(lock, deadline, hasResult))
        {
            lock.unlock();
            ProcessResults(cache);
            return IsImageReady(image);
        }
    }
}

// --- Drawing ----------------------------------------------------------------------

void DrawImage(ImDrawList* drawList, ImageHandle image, ImVec2 pMin, ImVec2 pMax, const ImageOptions& options)
{
    if (drawList == nullptr)
        return;
    const Slot* slot = Resolve(image);
    const ImageStatus status = slot != nullptr ? slot->status : ImageStatus::Invalid;
    if (status == ImageStatus::Ready)
    {
        DrawTextured(drawList, ToTextureRef(slot->view.Get()),
                     ImVec2(static_cast<float>(slot->width), static_cast<float>(slot->height)), pMin, pMax,
                     options);
        return;
    }
    if (!options.showStates)
        return;
    if (status == ImageStatus::Loading)
        internal::DrawImageLoading(drawList, pMin, pMax, options.radius);
    else
        internal::DrawImageFailed(drawList, pMin, pMax, options.radius,
                                  status == ImageStatus::Lost ? "Unavailable" : "Failed to load");
}

void DrawImage(ImDrawList* drawList, ID3D11ShaderResourceView* texture, ImVec2 pMin, ImVec2 pMax,
               const ImageOptions& options)
{
    if (drawList == nullptr)
        return;
    const ImVec2 size = ViewSize(texture);
    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        if (options.showStates)
            internal::DrawImageFailed(drawList, pMin, pMax, options.radius, "Failed to load");
        return;
    }
    DrawTextured(drawList, ToTextureRef(texture), size, pMin, pMax, options);
}

void Image(ImageHandle image, ImVec2 size, const ImageOptions& options)
{
    ImRect bb;
    if (!AddItem(ResolveItemSize(size, ImageSize(image)), bb))
        return;
    DrawImage(ImGui::GetWindowDrawList(), image, bb.Min, bb.Max, options);
}

void Image(ID3D11ShaderResourceView* texture, ImVec2 size, const ImageOptions& options)
{
    ImRect bb;
    if (!AddItem(ResolveItemSize(size, ViewSize(texture)), bb))
        return;
    DrawImage(ImGui::GetWindowDrawList(), texture, bb.Min, bb.Max, options);
}

namespace internal
{

bool ReadFileBytes(const char* path, std::vector<std::uint8_t>& out)
{
    out.clear();
    return path != nullptr && ReadFile(path, out);
}

ImageCacheCounts GetImageCacheCounts()
{
    Cache& cache = State();
    ImageCacheCounts counts;
    counts.slots = static_cast<int>(cache.slots.size());
    for (const Slot& slot : cache.slots)
    {
        if (slot.generation != 0)
            ++counts.inUse;
        if (slot.generation != 0 && slot.status == ImageStatus::Loading)
            ++counts.loading;
    }
    std::lock_guard lock(cache.mutex);
    counts.queuedJobs = static_cast<int>(cache.jobs.size());
    return counts;
}

ImageHandle CreateSvgImage(SvgSourcePtr source)
{
    Cache& cache = State();
    if (source == nullptr || !cache.initialized || !cache.deviceValid)
        return {};
    const ImageHandle handle = Allocate(cache, SourceKind::Svg);
    if (!handle.IsValid())
        return {};
    Slot& slot = cache.slots[handle.index];
    slot.svg = std::move(source);
    if (!RasterizeSlot(slot))
    {
        Free(cache, handle.index);
        return {};
    }
    return handle;
}

Subsystem TextureSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "texture";
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
