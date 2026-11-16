#pragma once

#include <functional>
#include <vector>

#include <d3d11.h>
#include <wrl/client.h>

namespace facet::demo
{

class AppShell;

// R8G8B8A8_UNORM render target plus a staging copy for CPU readback.
class OffscreenTarget
{
public:
    bool Create(ID3D11Device* device, int width, int height);
    void Release();
    void BindAndClear(ID3D11DeviceContext* context, const float clearColor[4]);
    // Copies the target to the CPU as tightly packed RGBA rows, top row first.
    bool Readback(ID3D11DeviceContext* context, std::vector<unsigned char>& rgba);

    int Width() const { return width; }
    int Height() const { return height; }

private:
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> view;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    int width = 0;
    int height = 0;
};

struct HeadlessOptions
{
    int width = 1280;
    int height = 800;
    float scale = 1.0f;
};

// One deterministic offscreen run: a fresh ImGui context with the DX11 backend and
// Facet initialized, a fixed 1/60 s time step, no OS input and the mouse parked outside
// the display. A fresh context per run keeps every capture independent of the runs
// before it.
class HeadlessSession
{
public:
    HeadlessSession() = default;
    ~HeadlessSession();
    HeadlessSession(const HeadlessSession&) = delete;
    HeadlessSession& operator=(const HeadlessSession&) = delete;

    bool Begin(AppShell& shell, const HeadlessOptions& options);
    void End();

    // Renders one frame: ui() runs between ImGui::NewFrame and ImGui::Render.
    void Frame(const std::function<void()>& ui);

    bool Readback(std::vector<unsigned char>& rgba);

    // False when any D3D call of this session failed.
    bool Healthy() const { return healthy; }

    static constexpr float kTimeStep = 1.0f / 60.0f;

private:
    AppShell* shell = nullptr;
    OffscreenTarget target;
    bool active = false;
    bool backendReady = false;
    bool healthy = true;
};

} // namespace facet::demo
