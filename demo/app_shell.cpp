#include "app_shell.h"

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string_view>
#include <utility>
#include <vector>

#include <dwmapi.h>
#include <windowsx.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>

// Declared here because imgui_impl_win32.h keeps the declaration behind #if 0 to avoid
// pulling windows.h into every user of the header.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

using Microsoft::WRL::ComPtr;

namespace facet::demo
{
namespace
{

constexpr wchar_t kWindowClass[] = L"FacetDemoWindow";

// Smallest client size in 96 DPI units. Keeps the navigation and content readable.
constexpr int kMinWidth = 640;
constexpr int kMinHeight = 400;

int ScaleForDpi(int value, UINT dpi)
{
    return MulDiv(value, static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI);
}

// Thickness of the invisible resize frame Windows would use for this DPI.
int ResizeBorder(UINT dpi)
{
    return GetSystemMetricsForDpi(SM_CXFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

} // namespace

AppShell::~AppShell()
{
    Destroy();
}

bool AppShell::Create(const ShellDesc& desc)
{
    instance = GetModuleHandleW(nullptr);
    hidden = desc.hidden;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &AppShell::WndProcThunk;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    if (RegisterClassExW(&wc) == 0)
    {
        std::fprintf(stderr, "error: RegisterClassExW failed (%lu)\n", GetLastError());
        return false;
    }
    classRegistered = true;

    // Hidden windows use WS_POPUP plus WS_EX_TOOLWINDOW: never shown and never listed in
    // the taskbar, they only give offscreen runs a message queue.
    const DWORD style = hidden ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    const DWORD exStyle = hidden ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
    hwnd = CreateWindowExW(exStyle, kWindowClass, desc.title, style, CW_USEDEFAULT, CW_USEDEFAULT,
                           desc.width, desc.height, nullptr, nullptr, instance, this);
    if (hwnd == nullptr)
    {
        std::fprintf(stderr, "error: CreateWindowExW failed (%lu)\n", GetLastError());
        return false;
    }

    if (!CreateDevice(desc.warp))
        return false;

    if (hidden)
        return true;

    // Size the window for the DPI of the monitor it landed on and center it in the work area.
    // The size is clamped to the work area so the title strip (the only drag region and the
    // home of the window buttons) is always on screen. When the requested size does not fit,
    // the window starts maximized; restoring it gives the clamped size.
    dpi = GetDpiForWindow(hwnd);
    MONITORINFO monitor = {};
    monitor.cbSize = sizeof(monitor);
    GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitor);
    const int workLeft = static_cast<int>(monitor.rcWork.left);
    const int workTop = static_cast<int>(monitor.rcWork.top);
    const int workWidth = static_cast<int>(monitor.rcWork.right) - workLeft;
    const int workHeight = static_cast<int>(monitor.rcWork.bottom) - workTop;
    const int requestedWidth = ScaleForDpi(desc.width, dpi);
    const int requestedHeight = ScaleForDpi(desc.height, dpi);
    const int width = std::min(requestedWidth, workWidth);
    const int height = std::min(requestedHeight, workHeight);
    const bool startMaximized = width < requestedWidth || height < requestedHeight;
    const int x = std::max(workLeft, workLeft + (workWidth - width) / 2);
    const int y = std::max(workTop, workTop + (workHeight - height) / 2);
    // SWP_FRAMECHANGED makes Windows send WM_NCCALCSIZE, which removes the system frame.
    SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    // A one pixel frame extension keeps the DWM shadow and snap animations.
    const MARGINS margins = {0, 0, 1, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    if (!CreateSwapChain())
        return false;

    ShowWindow(hwnd, startMaximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    return true;
}

bool AppShell::CreateDevice(bool warp)
{
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                        D3D_FEATURE_LEVEL_10_0};

    auto tryCreate = [&](D3D_DRIVER_TYPE type, UINT createFlags) {
        device.Reset();
        context.Reset();
        return D3D11CreateDevice(nullptr, type, nullptr, createFlags, levels,
                                 static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
                                 device.GetAddressOf(), nullptr, context.GetAddressOf());
    };

    D3D_DRIVER_TYPE type = warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
    HRESULT hr = tryCreate(type, flags);
    if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG) != 0)
    {
        // The debug layer is an optional Windows feature ("Graphics Tools"). Continue without it.
        flags &= ~static_cast<UINT>(D3D11_CREATE_DEVICE_DEBUG);
        hr = tryCreate(type, flags);
    }
    if (FAILED(hr) && !warp)
    {
        std::fprintf(stderr, "warning: no hardware D3D11 device (0x%08lX), using WARP\n",
                     static_cast<unsigned long>(hr));
        type = D3D_DRIVER_TYPE_WARP;
        hr = tryCreate(type, flags);
    }
    if (FAILED(hr))
    {
        std::fprintf(stderr, "error: D3D11CreateDevice failed (0x%08lX)\n",
                     static_cast<unsigned long>(hr));
        deviceFailed = true;
        return false;
    }
    usingWarp = type == D3D_DRIVER_TYPE_WARP;
    debugLayer = (flags & D3D11_CREATE_DEVICE_DEBUG) != 0;
    return true;
}

bool AppShell::CreateSwapChain()
{
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    ComPtr<IDXGIFactory2> factory;
    if (FAILED(device.As(&dxgiDevice)) || FAILED(dxgiDevice->GetAdapter(adapter.GetAddressOf())) ||
        FAILED(adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()))))
    {
        std::fprintf(stderr, "error: cannot reach the DXGI factory\n");
        deviceFailed = true;
        return false;
    }

    RECT client = {};
    GetClientRect(hwnd, &client);
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = static_cast<UINT>(client.right - client.left);
    desc.Height = static_cast<UINT>(client.bottom - client.top);
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    const HRESULT hr = factory->CreateSwapChainForHwnd(device.Get(), hwnd, &desc, nullptr, nullptr,
                                                       swapChain.GetAddressOf());
    if (FAILED(hr))
    {
        std::fprintf(stderr, "error: CreateSwapChainForHwnd failed (0x%08lX)\n",
                     static_cast<unsigned long>(hr));
        deviceFailed = true;
        return false;
    }
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    DXGI_SWAP_CHAIN_DESC1 actual = {};
    swapChain->GetDesc1(&actual);
    bufferWidth = actual.Width;
    bufferHeight = actual.Height;
    return CreateBackBufferView();
}

bool AppShell::CreateBackBufferView()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    if (SUCCEEDED(hr))
        hr = device->CreateRenderTargetView(backBuffer.Get(), nullptr, backBufferView.GetAddressOf());
    if (FAILED(hr))
    {
        std::fprintf(stderr, "error: cannot create the back buffer view (0x%08lX)\n",
                     static_cast<unsigned long>(hr));
        deviceFailed = true;
        return false;
    }
    return true;
}

void AppShell::ResizeSwapChainIfNeeded()
{
    if (pendingWidth == 0 || pendingHeight == 0)
        return;
    const UINT width = pendingWidth;
    const UINT height = pendingHeight;
    pendingWidth = pendingHeight = 0;
    if (width == bufferWidth && height == bufferHeight)
        return;

    // Every reference to the back buffers must be gone before ResizeBuffers.
    backBufferView.Reset();
    context->OMSetRenderTargets(0, nullptr, nullptr);
    const HRESULT hr = swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr))
    {
        std::fprintf(stderr, "error: ResizeBuffers failed (0x%08lX)\n", static_cast<unsigned long>(hr));
        deviceFailed = true;
        return;
    }
    bufferWidth = width;
    bufferHeight = height;
    CreateBackBufferView();
}

void AppShell::Destroy()
{
    if (context)
    {
        context->ClearState();
        context->Flush();
    }
    backBufferView.Reset();
    swapChain.Reset();
    ReportLiveObjects();
    context.Reset();
    device.Reset();

    if (hwnd != nullptr)
    {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
    if (classRegistered)
    {
        UnregisterClassW(kWindowClass, instance);
        classRegistered = false;
    }
}

void AppShell::ReportLiveObjects()
{
    if (!debugLayer || !device)
        return;
    ComPtr<ID3D11Debug> debug;
    ComPtr<ID3D11InfoQueue> queue;
    if (FAILED(device.As(&debug)) || FAILED(device.As(&queue)))
        return;

    // Only the device itself (held by this report and by the shell) may still be alive.
    // Anything else is a leak: print it so it shows up in the terminal, not only in a debugger.
    queue->ClearStoredMessages();
    context->ClearState();
    context->Flush();
    debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
    const UINT64 count = queue->GetNumStoredMessages();
    int leaks = 0;
    for (UINT64 i = 0; i < count; ++i)
    {
        SIZE_T size = 0;
        queue->GetMessage(i, nullptr, &size);
        std::vector<char> storage(size);
        auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
        if (FAILED(queue->GetMessage(i, message, &size)))
            continue;
        if (message->ID == D3D11_MESSAGE_ID_LIVE_DEVICE ||
            message->ID == D3D11_MESSAGE_ID_LIVE_OBJECT_SUMMARY)
            continue;
        const std::string_view text(message->pDescription, message->DescriptionByteLength);
        if (text.rfind("Live ", 0) == 0)
        {
            std::fprintf(stderr, "d3d11 debug: %.*s\n", static_cast<int>(text.size()), text.data());
            ++leaks;
        }
    }
    if (leaks > 0)
        std::fprintf(stderr, "d3d11 debug: %d live object(s) at shutdown\n", leaks);
}

bool AppShell::PumpMessages()
{
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        if (msg.message == WM_QUIT)
            closeRequested = true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return !closeRequested;
}

bool AppShell::BeginFrame(const float clearColor[4])
{
    if (!swapChain || deviceFailed || minimized)
        return false;
    if (occluded)
    {
        if (swapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
            return false;
        occluded = false;
    }
    ResizeSwapChainIfNeeded();
    if (!backBufferView)
        return false;

    // The flip model unbinds the back buffer on every Present, so bind it every frame.
    ID3D11RenderTargetView* views[] = {backBufferView.Get()};
    context->OMSetRenderTargets(1, views, nullptr);
    context->ClearRenderTargetView(backBufferView.Get(), clearColor);
    inFrame = true;
    return true;
}

void AppShell::EndFrame()
{
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    const HRESULT hr = swapChain->Present(1, 0);
    inFrame = false;
    if (hr == DXGI_STATUS_OCCLUDED)
        occluded = true;
    else if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        std::fprintf(stderr, "error: device lost (0x%08lX)\n",
                     static_cast<unsigned long>(device->GetDeviceRemovedReason()));
        deviceFailed = true;
        closeRequested = true;
    }
}

void AppShell::SetLiveResizeCallback(std::function<void()> callback)
{
    liveResize = std::move(callback);
}

void AppShell::SetCaptionRect(float x0, float y0, float x1, float y1)
{
    captionRect.left = static_cast<LONG>(x0);
    captionRect.top = static_cast<LONG>(y0);
    captionRect.right = static_cast<LONG>(x1);
    captionRect.bottom = static_cast<LONG>(y1);
}

// Window commands are posted rather than executed directly: they run after the current
// frame, so a resize or minimize never starts while ImGui is in the middle of a frame.
void AppShell::Minimize()
{
    PostMessageW(hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
}

void AppShell::ToggleMaximize()
{
    PostMessageW(hwnd, WM_SYSCOMMAND, IsWindowMaximized() ? SC_RESTORE : SC_MAXIMIZE, 0);
}

void AppShell::Close()
{
    PostMessageW(hwnd, WM_CLOSE, 0, 0);
}

bool AppShell::IsWindowMaximized() const
{
    return hwnd != nullptr && IsZoomed(hwnd) != FALSE;
}

float AppShell::DpiScale() const
{
    return static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
}

bool AppShell::ConsumeDpiChange()
{
    const bool changed = dpiChanged;
    dpiChanged = false;
    return changed;
}

bool AppShell::DeviceHealthy() const
{
    if (deviceFailed || !device)
        return false;
    return device->GetDeviceRemovedReason() == S_OK;
}

LRESULT CALLBACK AppShell::WndProcThunk(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* shell = static_cast<AppShell*>(create->lpCreateParams);
        shell->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(shell));
    }
    auto* shell = reinterpret_cast<AppShell*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCDESTROY)
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    if (shell == nullptr || msg == WM_NCDESTROY)
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    return shell->WndProc(msg, wParam, lParam);
}

LRESULT AppShell::HitTest(LPARAM lParam) const
{
    POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ScreenToClient(hwnd, &point);
    RECT client = {};
    GetClientRect(hwnd, &client);

    if (!IsWindowMaximized())
    {
        const int border = ResizeBorder(dpi);
        const bool left = point.x < border;
        const bool right = point.x >= client.right - border;
        const bool top = point.y < border;
        const bool bottom = point.y >= client.bottom - border;
        if (top && left)
            return HTTOPLEFT;
        if (top && right)
            return HTTOPRIGHT;
        if (bottom && left)
            return HTBOTTOMLEFT;
        if (bottom && right)
            return HTBOTTOMRIGHT;
        if (left)
            return HTLEFT;
        if (right)
            return HTRIGHT;
        if (top)
            return HTTOP;
        if (bottom)
            return HTBOTTOM;
    }
    if (PtInRect(&captionRect, point))
        return HTCAPTION;
    return HTCLIENT;
}

LRESULT AppShell::WndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (imguiInput && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return 1;

    switch (msg)
    {
    case WM_NCCALCSIZE:
        if (wParam == TRUE && !hidden)
        {
            // Returning 0 makes the whole window client area: no system title bar or frame.
            // A maximized window extends past the monitor by the frame size, so inset it.
            if (IsWindowMaximized())
            {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                const UINT windowDpi = GetDpiForWindow(hwnd);
                const int frameX = ResizeBorder(windowDpi);
                const int frameY = GetSystemMetricsForDpi(SM_CYFRAME, windowDpi) +
                                   GetSystemMetricsForDpi(SM_CXPADDEDBORDER, windowDpi);
                params->rgrc[0].left += frameX;
                params->rgrc[0].right -= frameX;
                params->rgrc[0].top += frameY;
                params->rgrc[0].bottom -= frameY;
            }
            return 0;
        }
        break;

    case WM_NCHITTEST:
        if (!hidden)
            return HitTest(lParam);
        break;

    case WM_GETMINMAXINFO:
    {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        const UINT windowDpi = hwnd != nullptr ? GetDpiForWindow(hwnd) : USER_DEFAULT_SCREEN_DPI;
        info->ptMinTrackSize.x = ScaleForDpi(kMinWidth, windowDpi);
        info->ptMinTrackSize.y = ScaleForDpi(kMinHeight, windowDpi);
        return 0;
    }

    case WM_DPICHANGED:
    {
        dpi = HIWORD(wParam);
        dpiChanged = true;
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            minimized = true;
            return 0;
        }
        minimized = false;
        pendingWidth = LOWORD(lParam);
        pendingHeight = HIWORD(lParam);
        // Inside the modal size loop the main loop does not run; render from here instead.
        if (inSizeMove && liveResize && !inFrame && !inLiveResize)
        {
            inLiveResize = true;
            liveResize();
            inLiveResize = false;
        }
        return 0;

    case WM_ENTERSIZEMOVE:
        inSizeMove = true;
        return 0;

    case WM_EXITSIZEMOVE:
        inSizeMove = false;
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_SYSCOMMAND:
        // Alt alone would open the hidden window menu and freeze input until the next key.
        if ((wParam & 0xFFF0) == SC_KEYMENU)
            return 0;
        break;

    case WM_CLOSE:
        // The main loop tears down D3D and ImGui first, then Destroy() removes the window.
        closeRequested = true;
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace facet::demo
