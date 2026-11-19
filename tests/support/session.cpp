#include "support/session.h"

#include <algorithm>
#include <cmath>

#include <facet/facet.h>

#pragma comment(lib, "d3d11.lib")

namespace facet::test
{
namespace
{

void QueueModifiers(ImGuiIO& io, ImGuiKeyChord chord, bool down)
{
    for (const ImGuiKey mod : {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiMod_Alt, ImGuiMod_Super})
    {
        if (chord & mod)
            io.AddKeyEvent(mod, down);
    }
}

ImGuiKey MainKey(ImGuiKeyChord chord)
{
    return static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
}

} // namespace

Session::Session(const SessionOptions& sessionOptions) : options(sessionOptions)
{
    const HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                         device.GetAddressOf(), nullptr, context.GetAddressOf());
    imgui = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DisplaySize = options.displaySize;
    io.DeltaTime = options.deltaTime;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Recoverable errors go to Dear ImGui's debug log only (it needs one output enabled).
    io.ConfigErrorRecoveryEnableAssert = false;
    io.ConfigErrorRecoveryEnableTooltip = false;
    io.ConfigErrorRecoveryEnableDebugLog = true;
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
    // What a frame start would set, so draw lists recorded outside a frame work too.
    ImGui::GetMainViewport()->Size = options.displaySize;
    ImGui::GetDrawListSharedData()->SetCircleTessellationMaxError(ImGui::GetStyle().CircleTessellationMaxError);
    // No platform backend: the only input is what a test queues.
    io.AddFocusEvent(true);
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    // The session's own clipboard instead of the system one.
    ImGuiPlatformIO& platform = ImGui::GetPlatformIO();
    platform.Platform_ClipboardUserData = this;
    platform.Platform_GetClipboardTextFn = &Session::GetClipboardText;
    platform.Platform_SetClipboardTextFn = &Session::SetClipboardText;

    ready = SUCCEEDED(hr) && device != nullptr;
    if (ready && options.initFacet)
        ready = InitFacet();
}

Session::~Session()
{
    if (facetStarted)
        facet::Shutdown();
    if (imgui != nullptr)
        ImGui::DestroyContext(imgui);
}

bool Session::InitFacet()
{
    if (facetStarted || device == nullptr)
        return facetStarted;
    facet::InitDesc desc;
    desc.device = device.Get();
    desc.context = context.Get();
    facetStarted = facet::Init(desc);
    ready = facetStarted;
    return facetStarted;
}

void Session::SetUi(std::function<void()> code)
{
    ui = std::move(code);
}

void Session::Frame()
{
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = options.deltaTime;
    io.DisplaySize = options.displaySize;
    tracked.clear();
    ImGui::NewFrame();
    facet::NewFrame();
    if (options.window)
    {
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(options.displaySize);
        ImGui::Begin("test", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    }
    if (ui)
        ui();
    if (options.window)
        ImGui::End();
    facet::EndFrame();
    ImGui::Render();
    ApplyTextureRequests();
    ++frames;
}

void Session::Frames(int count)
{
    for (int i = 0; i < count; ++i)
        Frame();
}

void Session::Seconds(float seconds)
{
    const int count = std::max(1, static_cast<int>(std::ceil(seconds / options.deltaTime - 1e-4f)));
    Frames(count);
}

// Plays the renderer's part: every texture request counts as done.
void Session::ApplyTextureRequests()
{
    for (ImTextureData* texture : ImGui::GetPlatformIO().Textures)
    {
        switch (texture->Status)
        {
        case ImTextureStatus_WantCreate:
            texture->SetTexID(nextTextureId++);
            texture->SetStatus(ImTextureStatus_OK);
            break;
        case ImTextureStatus_WantUpdates:
            texture->SetStatus(ImTextureStatus_OK);
            break;
        case ImTextureStatus_WantDestroy:
            texture->SetTexID(ImTextureID_Invalid);
            texture->SetStatus(ImTextureStatus_Destroyed);
            break;
        default:
            break;
        }
    }
}

const char* Session::GetClipboardText(ImGuiContext*)
{
    const auto* session = static_cast<const Session*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
    return session != nullptr ? session->clipboard.c_str() : "";
}

void Session::SetClipboardText(ImGuiContext*, const char* text)
{
    auto* session = static_cast<Session*>(ImGui::GetPlatformIO().Platform_ClipboardUserData);
    if (session != nullptr)
        session->clipboard = text != nullptr ? text : "";
}

void Session::Track(std::string_view name)
{
    TrackedItem item;
    item.name = name;
    item.rect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    item.id = ImGui::GetItemID();
    item.visible = ImGui::GetCurrentWindowRead()->ClipRect.Contains(item.rect.GetCenter());
    for (TrackedItem& entry : tracked)
    {
        if (entry.name == name)
        {
            entry = std::move(item);
            return;
        }
    }
    tracked.push_back(std::move(item));
}

const Session::TrackedItem* Session::FindTracked(std::string_view name) const
{
    for (const TrackedItem& entry : tracked)
    {
        if (entry.name == name)
            return &entry;
    }
    return nullptr;
}

ImRect Session::ItemRect(std::string_view name) const
{
    const TrackedItem* item = FindTracked(name);
    return item != nullptr ? item->rect : ImRect();
}

ImVec2 Session::ItemCenter(std::string_view name) const
{
    return ItemRect(name).GetCenter();
}

ImGuiID Session::ItemId(std::string_view name) const
{
    const TrackedItem* item = FindTracked(name);
    return item != nullptr ? item->id : 0;
}

bool Session::ItemVisible(std::string_view name) const
{
    const TrackedItem* item = FindTracked(name);
    return item != nullptr && item->visible;
}

void Session::MouseMove(ImVec2 pos)
{
    ImGui::GetIO().AddMousePosEvent(pos.x, pos.y);
    Frame();
}

void Session::MouseDown(ImGuiMouseButton button)
{
    ImGui::GetIO().AddMouseButtonEvent(button, true);
    Frame();
}

void Session::MouseUp(ImGuiMouseButton button)
{
    ImGui::GetIO().AddMouseButtonEvent(button, false);
    Frame();
}

void Session::Click(ImVec2 pos, ImGuiMouseButton button)
{
    MouseMove(pos);
    MouseDown(button);
    MouseUp(button);
    Frame();
}

void Session::DoubleClick(ImVec2 pos, ImGuiMouseButton button)
{
    MouseMove(pos);
    MouseDown(button);
    MouseUp(button);
    MouseDown(button);  // two frames after the first press
    MouseUp(button);
    Frame();
}

void Session::Drag(const std::vector<ImVec2>& path, int framesPerSegment, ImGuiMouseButton button)
{
    DragAndHold(path, framesPerSegment, 0, button);
}

void Session::DragAndHold(const std::vector<ImVec2>& path, int framesPerSegment, int holdFrames,
                          ImGuiMouseButton button)
{
    if (path.empty())
        return;
    framesPerSegment = std::max(1, framesPerSegment);
    MouseMove(path.front());
    MouseDown(button);
    for (size_t i = 1; i < path.size(); ++i)
    {
        for (int step = 1; step <= framesPerSegment; ++step)
        {
            const float t = static_cast<float>(step) / static_cast<float>(framesPerSegment);
            const ImVec2 a = path[i - 1];
            const ImVec2 b = path[i];
            MouseMove(ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t));
        }
    }
    Frames(std::max(0, holdFrames));  // the button stays down, the mouse rests at the last point
    MouseUp(button);
    Frame();
}

void Session::Wheel(ImVec2 pos, float wheelY, float wheelX)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(pos.x, pos.y);
    Frame();
    io.AddMouseWheelEvent(wheelX, wheelY);
    Frame();
}

void Session::KeyDown(ImGuiKeyChord chord)
{
    ImGuiIO& io = ImGui::GetIO();
    QueueModifiers(io, chord, true);
    const ImGuiKey key = MainKey(chord);
    if (key != ImGuiKey_None)
        io.AddKeyEvent(key, true);
    Frame();
}

void Session::KeyUp(ImGuiKeyChord chord)
{
    ImGuiIO& io = ImGui::GetIO();
    const ImGuiKey key = MainKey(chord);
    if (key != ImGuiKey_None)
        io.AddKeyEvent(key, false);
    QueueModifiers(io, chord, false);
    Frame();
}

void Session::KeyPress(ImGuiKeyChord chord)
{
    KeyDown(chord);
    KeyUp(chord);
    Frames(2);
}

void Session::HoldKey(ImGuiKeyChord chord, float seconds)
{
    KeyDown(chord);
    Seconds(seconds - options.deltaTime);
    KeyUp(chord);
}

void Session::Type(const char* text)
{
    ImGui::GetIO().AddInputCharactersUTF8(text);
    Frame();
}

} // namespace facet::test
