#pragma once

#include <functional>

#include <windows.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace facet::demo
{

struct ShellDesc
{
    const wchar_t* title = L"Facet demo";
    int width = 1280;     // client size in 96 DPI units; scaled to the monitor DPI
    int height = 800;
    bool hidden = false;  // offscreen runs: the window is never shown and has no swap chain
    bool warp = false;    // use the WARP software rasterizer instead of the GPU
};

// Win32 window plus D3D11 device for the demo.
//
// The visible window has no system title bar or frame but keeps the WS_OVERLAPPEDWINDOW
// style bits, so Windows still provides resizing from the edges, dragging, snapping,
// minimize and maximize animations. The UI marks its drag strip with SetCaptionRect.
class AppShell
{
public:
    AppShell() = default;
    ~AppShell();
    AppShell(const AppShell&) = delete;
    AppShell& operator=(const AppShell&) = delete;

    bool Create(const ShellDesc& desc);
    void Destroy();

    // Dispatches pending window messages. Returns false once the user asked to close.
    bool PumpMessages();

    // Resizes the swap chain when needed and binds and clears the back buffer.
    // Returns false when the frame should be skipped (minimized or fully occluded).
    bool BeginFrame(const float clearColor[4]);
    // Renders the current ImGui draw data and presents.
    void EndFrame();

    // Called from inside the modal move or size loop of Windows, so the content keeps
    // rendering while the user drags an edge. Never called while a frame is in progress.
    void SetLiveResizeCallback(std::function<void()> callback);

    // Drag region in client pixels for the current frame. Mouse presses inside it move the
    // window; double-clicks maximize. Set an empty rect to disable.
    void SetCaptionRect(float x0, float y0, float x1, float y1);

    // Forwards window messages to the Dear ImGui Win32 backend. Enable only after
    // ImGui_ImplWin32_Init and disable before ImGui_ImplWin32_Shutdown.
    void SetImGuiInputEnabled(bool enabled) { imguiInput = enabled; }

    void Minimize();
    void ToggleMaximize();
    void Close();
    bool IsWindowMaximized() const;

    // Monitor DPI / 96 for the visible window, 1 for hidden windows.
    float DpiScale() const;
    // True once after the window moved to a monitor with another DPI.
    bool ConsumeDpiChange();

    // False after any D3D call failed or the device was removed.
    bool DeviceHealthy() const;

    HWND Hwnd() const { return hwnd; }
    ID3D11Device* Device() const { return device.Get(); }
    ID3D11DeviceContext* Context() const { return context.Get(); }
    bool UsesWarp() const { return usingWarp; }
    bool DebugLayerActive() const { return debugLayer; }

private:
    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HitTest(LPARAM lParam) const;

    bool CreateDevice(bool warp);
    bool CreateSwapChain();
    bool CreateBackBufferView();
    void ResizeSwapChainIfNeeded();
    void ReportLiveObjects();

    HWND hwnd = nullptr;
    HINSTANCE instance = nullptr;
    bool hidden = false;
    bool usingWarp = false;
    bool classRegistered = false;

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferView;
    bool debugLayer = false;

    UINT dpi = 96;
    bool dpiChanged = false;
    bool closeRequested = false;
    bool minimized = false;
    bool occluded = false;
    bool inSizeMove = false;
    bool inFrame = false;
    bool inLiveResize = false;
    bool deviceFailed = false;
    bool imguiInput = false;
    UINT bufferWidth = 0;
    UINT bufferHeight = 0;
    UINT pendingWidth = 0;
    UINT pendingHeight = 0;
    RECT captionRect = {};
    std::function<void()> liveResize;
};

} // namespace facet::demo
