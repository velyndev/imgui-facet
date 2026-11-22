// facet_consumer: a small Direct3D 11 application that uses Facet as another project would.
//
//   facet_consumer                 renders frames offscreen on a WARP device, operates a widget
//                                  with queued mouse input, checks the result and exits with 0
//                                  when every check passed (the facet_consumer test)
//   facet_consumer --window        opens a window with the same content and runs until closed
//   facet_consumer --window --frames N
//                                  closes the window after N frames
//
// It does not use anything from the Facet demo or tests: only the public headers, its own Dear
// ImGui target (see CMakeLists.txt) and the Dear ImGui backends compiled into that target.

#include <windows.h>

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <facet/facet.h>

#ifndef CONSUMER_IMCONFIG
#error "consumer_imconfig.h is not in effect: IMGUI_USER_CONFIG did not reach this file"
#endif
static_assert(sizeof(ImDrawIdx) == 4, "consumer_imconfig.h selects 32-bit draw indices");

// Declared by imgui_impl_win32.h only behind #if 0, so that it does not need windows.h.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

using Microsoft::WRL::ComPtr;

namespace
{

// --- Application content ---------------------------------------------------------------------

// A small glyph drawn like the built-in icons (16 x 16 view box, 1.5 stroke in currentColor),
// registered as an icon and loaded as an SVG image. As an icon its shapes become a mask that
// is tinted at draw time; as an image the colors stay as written (currentColor is black).
constexpr const char* kGlyphSvg =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 16 16\" fill=\"none\" stroke=\"currentColor\""
    " stroke-width=\"1.5\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
    "<rect x=\"2.5\" y=\"2.5\" width=\"11\" height=\"11\" rx=\"2.5\"/>"
    "<circle cx=\"8\" cy=\"8\" r=\"2\" fill=\"currentColor\" stroke=\"none\"/>"
    "</svg>";

constexpr int kPatternSide = 32;

struct Assets
{
    facet::Icon glyph;
    facet::ImageHandle svg;
    facet::ImageHandle file;
    facet::ImageHandle pixels;
};

struct AppState
{
    bool dark = false;
    float uiScale = 1.0f;
    int presses = 0;
    ImVec2 toggleMin = ImVec2(0.0f, 0.0f);  // the Toggle's item rectangle in the last frame
    ImVec2 toggleMax = ImVec2(0.0f, 0.0f);
};

// A two-tone pattern from the theme's accent and background, as straight-alpha RGBA rows.
std::vector<unsigned char> PatternPixels()
{
    const ImU32 a = facet::ToU32(facet::T().colors.accent);
    const ImU32 b = facet::ToU32(facet::T().colors.bgSecondary);
    std::vector<unsigned char> rgba(static_cast<size_t>(kPatternSide) * kPatternSide * 4);
    for (int y = 0; y < kPatternSide; ++y)
    {
        for (int x = 0; x < kPatternSide; ++x)
        {
            const ImU32 color = ((x / 8 + y / 8) % 2 == 0) ? a : b;
            unsigned char* p = &rgba[(static_cast<size_t>(y) * kPatternSide + x) * 4];
            p[0] = static_cast<unsigned char>(color >> IM_COL32_R_SHIFT);
            p[1] = static_cast<unsigned char>(color >> IM_COL32_G_SHIFT);
            p[2] = static_cast<unsigned char>(color >> IM_COL32_B_SHIFT);
            p[3] = 255;
        }
    }
    return rgba;
}

// Writes the pattern as a 24-bit BMP file, so the example can load an image file without
// shipping one.
bool WritePatternBmp(const std::filesystem::path& path)
{
    const std::vector<unsigned char> rgba = PatternPixels();
    const int rowBytes = (kPatternSide * 3 + 3) & ~3;
    const std::uint32_t pixelBytes = static_cast<std::uint32_t>(rowBytes * kPatternSide);
    unsigned char header[54] = {};
    auto put32 = [&](int offset, std::uint32_t value) {
        for (int i = 0; i < 4; ++i)
            header[offset + i] = static_cast<unsigned char>(value >> (8 * i));
    };
    header[0] = 'B';
    header[1] = 'M';
    put32(2, 54 + pixelBytes);
    put32(10, 54);
    put32(14, 40);
    put32(18, kPatternSide);
    put32(22, kPatternSide);  // positive height: rows from the bottom up
    header[26] = 1;
    header[28] = 24;
    put32(34, pixelBytes);

    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;
    out.write(reinterpret_cast<const char*>(header), sizeof(header));
    std::vector<char> row(static_cast<size_t>(rowBytes), 0);
    for (int y = kPatternSide - 1; y >= 0; --y)
    {
        for (int x = 0; x < kPatternSide; ++x)
        {
            const unsigned char* p = &rgba[(static_cast<size_t>(y) * kPatternSide + x) * 4];
            row[static_cast<size_t>(x) * 3 + 0] = static_cast<char>(p[2]);
            row[static_cast<size_t>(x) * 3 + 1] = static_cast<char>(p[1]);
            row[static_cast<size_t>(x) * 3 + 2] = static_cast<char>(p[0]);
        }
        out.write(row.data(), rowBytes);
    }
    return static_cast<bool>(out);
}

std::filesystem::path PatternPath()
{
    std::error_code error;
    std::filesystem::path dir = std::filesystem::temp_directory_path(error);
    if (error)
        dir = std::filesystem::current_path();
    return dir / "facet_consumer_pattern.bmp";
}

// Loads every asset. Needs Facet to be initialized; handles do not survive facet::Shutdown.
void LoadAssets(Assets& assets)
{
    assets.glyph = facet::RegisterIcon("consumer-glyph", kGlyphSvg);
    assets.svg = facet::LoadSvgMemory(kGlyphSvg, 0, ImVec2(24.0f, 24.0f));
    const std::filesystem::path path = PatternPath();
    if (WritePatternBmp(path))
        assets.file = facet::LoadImageFileAsync(reinterpret_cast<const char*>(path.u8string().c_str()));
    const std::vector<unsigned char> pixels = PatternPixels();
    assets.pixels = facet::CreateImageRGBA(pixels.data(), kPatternSide, kPatternSide);
}

void ReleaseAssets(Assets& assets)
{
    facet::ReleaseImage(assets.svg);
    facet::ReleaseImage(assets.file);
    facet::ReleaseImage(assets.pixels);
    assets.svg = assets.file = assets.pixels = facet::ImageHandle{};
}

// The window content: a few themed widgets on the app frame.
void DrawContent(AppState& state, const Assets& assets)
{
    if (!facet::BeginAppFrame("consumer"))
        return;

    facet::Text(facet::TextStyle::Title2, "Facet consumer example");
    facet::IconGlyph(assets.glyph);
    ImGui::SameLine();
    ImGui::TextUnformatted("An icon registered from SVG text");

    if (facet::Toggle("Dark theme", &state.dark))
        facet::SetThemeMode(state.dark ? facet::ThemeMode::Dark : facet::ThemeMode::Light);
    state.toggleMin = ImGui::GetItemRectMin();
    state.toggleMax = ImGui::GetItemRectMax();

    if (facet::Slider("UI scale", &state.uiScale, 0.75f, 2.0f, {.format = "%.2f", .step = 0.25, .width = 320.0f}))
        facet::SetUiScale(state.uiScale);

    if (facet::Button("Count a press", {.variant = facet::ButtonVariant::Primary}))
    {
        ++state.presses;
        facet::ShowToast("Button pressed", {.tone = facet::Tone::Success});
    }
    ImGui::SameLine();
    ImGui::Text("Presses: %d", state.presses);

    facet::ImageView("SVG image", assets.svg, {.width = 48.0f, .height = 48.0f});
    ImGui::SameLine();
    facet::ImageView("Image file", assets.file, {.corners = facet::Radius::Element, .width = 48.0f, .height = 48.0f});
    ImGui::SameLine();
    facet::ImageView("Image from pixels", assets.pixels, {.width = 48.0f, .height = 48.0f});

    facet::EndAppFrame();
}

// The Dear ImGui settings this example uses: no ini file and keyboard navigation on.
void ConfigureIo()
{
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
}

// The look: Facet keeps the stock Dear ImGui style in step with its theme and scale.
void ConfigureFacetLook(float dpiScale)
{
    facet::SetScaleMode(facet::ScaleMode::FollowDpi);
    facet::SetDpiScale(dpiScale);
    facet::SyncImGuiStyle(true);
    facet::ApplyImGuiStyle();
}

bool CreateDevice(D3D_DRIVER_TYPE type, ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context)
{
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    const HRESULT hr = D3D11CreateDevice(nullptr, type, nullptr, 0, levels, 1, D3D11_SDK_VERSION,
                                         device.GetAddressOf(), nullptr, context.GetAddressOf());
    if (FAILED(hr))
        std::fprintf(stderr, "error: D3D11CreateDevice failed (0x%08lX)\n", static_cast<unsigned long>(hr));
    return SUCCEEDED(hr);
}

// --- Offscreen run (the test) ---------------------------------------------------------------

constexpr int kWidth = 800;
constexpr int kHeight = 600;
constexpr float kFrameTime = 1.0f / 60.0f;

struct Offscreen
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> view;
    ComPtr<ID3D11Texture2D> staging;

    bool Create()
    {
        if (!CreateDevice(D3D_DRIVER_TYPE_WARP, device, context))
            return false;
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = kWidth;
        desc.Height = kHeight;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf())) ||
            FAILED(device->CreateRenderTargetView(texture.Get(), nullptr, view.GetAddressOf())))
            return false;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        return SUCCEEDED(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf()));
    }

    void Release()
    {
        if (context)
        {
            context->ClearState();
            context->Flush();
        }
        staging.Reset();
        view.Reset();
        texture.Reset();
        context.Reset();
        device.Reset();
    }

    // Pixels within `tolerance` per channel of `color`.
    int CountPixels(const ImVec4& color, int tolerance)
    {
        context->CopyResource(staging.Get(), texture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
            return -1;
        const int want[3] = {static_cast<int>(std::lround(color.x * 255.0f)), static_cast<int>(std::lround(color.y * 255.0f)),
                             static_cast<int>(std::lround(color.z * 255.0f))};
        int count = 0;
        for (int y = 0; y < kHeight; ++y)
        {
            const auto* row = static_cast<const unsigned char*>(mapped.pData) + static_cast<size_t>(mapped.RowPitch) * y;
            for (int x = 0; x < kWidth; ++x)
            {
                const unsigned char* p = row + static_cast<size_t>(x) * 4;
                if (std::abs(p[0] - want[0]) <= tolerance && std::abs(p[1] - want[1]) <= tolerance &&
                    std::abs(p[2] - want[2]) <= tolerance)
                    ++count;
            }
        }
        context->Unmap(staging.Get(), 0);
        return count;
    }
};

// One frame in the order Facet requires: backend and Dear ImGui frame start, facet::NewFrame,
// the UI, facet::EndFrame, ImGui::Render, then rendering the draw data on Init's context.
void OffscreenFrame(Offscreen& target, AppState& state, const Assets& assets)
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(kWidth), static_cast<float>(kHeight));
    io.DeltaTime = kFrameTime;
    ImGui_ImplDX11_NewFrame();
    ImGui::NewFrame();
    facet::NewFrame();
    DrawContent(state, assets);
    facet::EndFrame();
    ImGui::Render();

    const ImVec4& bg = facet::T().colors.bg;
    const float clear[4] = {bg.x, bg.y, bg.z, 1.0f};
    ID3D11RenderTargetView* views[] = {target.view.Get()};
    target.context->OMSetRenderTargets(1, views, nullptr);
    target.context->ClearRenderTargetView(target.view.Get(), clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

int g_failures = 0;

void Check(bool condition, const char* what)
{
    std::printf("%s: %s\n", condition ? "pass" : "FAIL", what);
    if (!condition)
        ++g_failures;
}

bool StartFacet(Offscreen& target)
{
    if (!ImGui_ImplDX11_Init(target.device.Get(), target.context.Get()))
        return false;
    facet::InitDesc desc;
    desc.device = target.device.Get();
    desc.context = target.context.Get();
    return facet::Init(desc);
}

int RunOffscreen()
{
    Offscreen target;
    if (!target.Create())
    {
        std::fprintf(stderr, "error: no WARP device or offscreen target\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ConfigureIo();
    ImGui::GetIO().AddFocusEvent(true);
    ConfigureFacetLook(1.0f);
    Check(StartFacet(target), "the DX11 backend and facet::Init start on a WARP device");
    std::printf("Facet %s, fonts: %s\n", facet::Version(), facet::FontOriginName(facet::ActiveFontOrigin()));

    AppState state;
    Assets assets;
    LoadAssets(assets);
    Check(assets.glyph.IsValid(), "RegisterIcon accepts the SVG glyph");
    Check(facet::IsImageReady(assets.svg), "LoadSvgMemory rasterizes the SVG image");
    Check(facet::IsImageReady(assets.pixels), "CreateImageRGBA uploads the pixels");
    Check(facet::WaitForImage(assets.file, 10.0f), "LoadImageFileAsync decodes the BMP file");

    for (int i = 0; i < 5; ++i)
        OffscreenFrame(target, state, assets);
    Check(facet::FrameIndex() == 5, "facet::NewFrame counted five frames");
    Check(target.CountPixels(facet::T().colors.accent, 2) > 200, "the primary button shows the light accent fill");

    // A click on the Toggle's switch, queued like a platform backend would.
    const ImVec2 click((state.toggleMax.x - (state.toggleMax.y - state.toggleMin.y) * 0.5f),
                       (state.toggleMin.y + state.toggleMax.y) * 0.5f);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent(click.x, click.y);
    OffscreenFrame(target, state, assets);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
    OffscreenFrame(target, state, assets);
    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
    OffscreenFrame(target, state, assets);
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    for (int i = 0; i < 60; ++i)
        OffscreenFrame(target, state, assets);
    Check(state.dark && facet::GetThemeMode() == facet::ThemeMode::Dark, "a click on the Toggle switches to the dark theme");
    Check(facet::T().dark && !facet::ThemeTransitionActive(), "the theme transition finished");
    Check(target.CountPixels(facet::T().colors.accent, 2) > 200, "the primary button shows the dark accent fill");

    // Device objects recreated on the same device, between frames.
    facet::InvalidateDeviceObjects();
    ImGui_ImplDX11_InvalidateDeviceObjects();
    Check(ImGui_ImplDX11_CreateDeviceObjects(), "the DX11 backend recreates its objects");
    Check(facet::CreateDeviceObjects(), "facet::CreateDeviceObjects recreates Facet's objects");
    Check(facet::GetImageStatus(assets.pixels) == facet::ImageStatus::Lost, "an image from pixels reports Lost");
    facet::ReleaseImage(assets.pixels);
    const std::vector<unsigned char> pixels = PatternPixels();
    assets.pixels = facet::CreateImageRGBA(pixels.data(), kPatternSide, kPatternSide);
    Check(facet::WaitForImage(assets.file, 10.0f), "the image file is loaded again");
    Check(facet::IsImageReady(assets.svg), "the SVG image is rasterized again");
    for (int i = 0; i < 3; ++i)
        OffscreenFrame(target, state, assets);
    Check(target.CountPixels(facet::T().colors.accent, 2) > 200, "the frame after recreation draws the button");

    // A new device: Facet and the backend stop and start again, and the assets are loaded again.
    facet::Shutdown();
    ImGui_ImplDX11_Shutdown();
    target.Release();
    Check(target.Create(), "a second WARP device and target");
    Check(StartFacet(target), "facet::Init on the second device");
    LoadAssets(assets);
    Check(facet::WaitForImage(assets.file, 10.0f), "the assets load on the second device");
    for (int i = 0; i < 3; ++i)
        OffscreenFrame(target, state, assets);
    Check(target.CountPixels(facet::T().colors.accent, 2) > 200, "the second device draws the button");

    ReleaseAssets(assets);
    facet::Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    target.Release();
    std::error_code error;
    std::filesystem::remove(PatternPath(), error);

    std::printf("%d check(s) failed\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}

// --- Window run -------------------------------------------------------------------------------

struct Window
{
    HWND hwnd = nullptr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swapChain;
    ComPtr<ID3D11RenderTargetView> backBuffer;
    UINT resizeWidth = 0;
    UINT resizeHeight = 0;
    float dpiScale = 1.0f;
};

Window* g_window = nullptr;

void CreateBackBufferView(Window& window)
{
    ComPtr<ID3D11Texture2D> buffer;
    if (SUCCEEDED(window.swapChain->GetBuffer(0, IID_PPV_ARGS(buffer.GetAddressOf()))))
        window.device->CreateRenderTargetView(buffer.Get(), nullptr, window.backBuffer.GetAddressOf());
}

bool CreateDeviceAndSwapChain(Window& window)
{
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.OutputWindow = window.hwnd;
    desc.SampleDesc.Count = 1;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    HRESULT hr = E_FAIL;
    for (const D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP})
    {
        hr = D3D11CreateDeviceAndSwapChain(nullptr, type, nullptr, 0, levels, 1, D3D11_SDK_VERSION, &desc,
                                           window.swapChain.GetAddressOf(), window.device.GetAddressOf(), nullptr,
                                           window.context.GetAddressOf());
        if (SUCCEEDED(hr))
            break;
    }
    if (FAILED(hr))
        return false;
    CreateBackBufferView(window);
    return true;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return 1;
    switch (msg)
    {
    case WM_SIZE:
        if (g_window != nullptr && wParam != SIZE_MINIMIZED)
        {
            g_window->resizeWidth = LOWORD(lParam);
            g_window->resizeHeight = HIWORD(lParam);
        }
        return 0;
    case WM_DPICHANGED:
        if (g_window != nullptr)
        {
            g_window->dpiScale = static_cast<float>(HIWORD(wParam)) / 96.0f;
            const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                         suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

int RunWindow(int maxFrames)
{
    // The application decides the DPI awareness; Facet never changes it.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    WNDCLASSEXW windowClass = {sizeof(windowClass)};
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    // IDC_ARROW is a resource id spelled as a narrow string pointer unless UNICODE is defined.
    windowClass.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    windowClass.lpszClassName = L"FacetConsumer";
    RegisterClassExW(&windowClass);

    Window window;
    g_window = &window;
    window.hwnd = CreateWindowExW(0, windowClass.lpszClassName, L"Facet consumer example", WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT, CW_USEDEFAULT, 960, 640, nullptr, nullptr, windowClass.hInstance,
                                  nullptr);
    if (window.hwnd == nullptr || !CreateDeviceAndSwapChain(window))
    {
        std::fprintf(stderr, "error: cannot create the window or the device\n");
        return 1;
    }
    window.dpiScale = static_cast<float>(GetDpiForWindow(window.hwnd)) / 96.0f;
    ShowWindow(window.hwnd, SW_SHOWDEFAULT);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ConfigureIo();
    ConfigureFacetLook(window.dpiScale);
    ImGui_ImplWin32_Init(window.hwnd);
    ImGui_ImplDX11_Init(window.device.Get(), window.context.Get());
    facet::InitDesc init;
    init.device = window.device.Get();
    init.context = window.context.Get();
    if (!facet::Init(init))
        std::fprintf(stderr, "error: facet::Init failed\n");

    AppState state;
    Assets assets;
    LoadAssets(assets);

    bool running = true;
    for (int frame = 0; running && (maxFrames <= 0 || frame < maxFrames); ++frame)
    {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT)
                running = false;
        }
        if (!running)
            break;
        if (window.resizeWidth != 0 && window.resizeHeight != 0)
        {
            window.backBuffer.Reset();
            window.swapChain->ResizeBuffers(0, window.resizeWidth, window.resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            window.resizeWidth = window.resizeHeight = 0;
            CreateBackBufferView(window);
        }
        if (facet::DpiScale() != window.dpiScale)
            facet::SetDpiScale(window.dpiScale);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        facet::NewFrame();
        DrawContent(state, assets);
        facet::EndFrame();
        ImGui::Render();

        const ImVec4& bg = facet::T().colors.bg;
        const float clear[4] = {bg.x, bg.y, bg.z, 1.0f};
        ID3D11RenderTargetView* views[] = {window.backBuffer.Get()};
        window.context->OMSetRenderTargets(1, views, nullptr);
        window.context->ClearRenderTargetView(window.backBuffer.Get(), clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        const HRESULT presented = window.swapChain->Present(1, 0);
        if (presented == DXGI_ERROR_DEVICE_REMOVED || presented == DXGI_ERROR_DEVICE_RESET)
        {
            // A new device: stop Facet and the backend, create the device, start both again and
            // load the assets again (handles do not survive facet::Shutdown).
            facet::Shutdown();
            ImGui_ImplDX11_Shutdown();
            window.backBuffer.Reset();
            window.swapChain.Reset();
            window.context.Reset();
            window.device.Reset();
            if (!CreateDeviceAndSwapChain(window))
                break;
            ImGui_ImplDX11_Init(window.device.Get(), window.context.Get());
            init.device = window.device.Get();
            init.context = window.context.Get();
            facet::Init(init);
            LoadAssets(assets);
        }
    }

    ReleaseAssets(assets);
    facet::Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    window.backBuffer.Reset();
    window.swapChain.Reset();
    window.context.Reset();
    window.device.Reset();
    DestroyWindow(window.hwnd);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    g_window = nullptr;
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    bool windowed = false;
    int maxFrames = 0;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--window") == 0)
            windowed = true;
        else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc)
            maxFrames = std::atoi(argv[++i]);
        else
        {
            std::fprintf(stderr, "usage: facet_consumer [--window [--frames N]]\n");
            return 2;
        }
    }
    return windowed ? RunWindow(maxFrames) : RunOffscreen();
}
