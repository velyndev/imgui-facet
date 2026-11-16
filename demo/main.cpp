// facet_demo: gallery application for Facet.
//
//   facet_demo                         interactive window
//   facet_demo --capture <dir> [--size WxH] [--scale S] [--page id] [--frames N] [--warp]
//
// The program is built for the console subsystem. Started from a terminal or a script,
// it behaves like any command line tool: the shell waits for it, stdout and stderr can be
// redirected and the exit code is reliable, which capture runs depend on.
// Started without arguments from Explorer, Windows opens a console for it; the program
// releases that console right away, so only the demo window stays (the console may show
// for a moment). A GUI subsystem program attaching to its parent console was not used
// because shells do not wait for GUI programs and redirected output is unreliable there.

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>

#include <windows.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <facet/facet.h>

#include "app_shell.h"
#include "capture.h"
#include "gallery.h"
#include "pages/pages.h"

namespace
{

using namespace facet::demo;

enum class Mode
{
    Interactive,
    Capture,
};

struct Options
{
    Mode mode = Mode::Interactive;
    CaptureOptions capture;
    std::string page;
    bool warp = false;
    bool help = false;
};

std::string ToUtf8(const wchar_t* text)
{
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
        return {};
    std::string out(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), length, nullptr, nullptr);
    return out;
}

void PrintUsage()
{
    std::printf(
        "usage:\n"
        "  facet_demo\n"
        "  facet_demo --capture <dir> [--size WxH] [--scale S] [--page id] [--frames N] [--warp]\n"
        "\n"
        "  --capture <dir>  render every page state offscreen and write PNG files and index.json\n"
        "  --size WxH       offscreen size in pixels (default 1280x800)\n"
        "  --scale S        UI scale (default 1)\n"
        "  --page id        only this page\n"
        "  --frames N       warm-up frames per page state before readback (default 60)\n"
        "  --warp           render with the WARP software rasterizer\n"
        "  --list-pages     print the registered page ids and capture states\n");
}

bool ParseSize(const std::wstring& text, int& width, int& height)
{
    const wchar_t* begin = text.c_str();
    wchar_t* end = nullptr;
    const long w = std::wcstol(begin, &end, 10);
    if (end == begin || (*end != L'x' && *end != L'X'))
        return false;
    begin = end + 1;
    const long h = std::wcstol(begin, &end, 10);
    if (end == begin || *end != 0)
        return false;
    if (w < 16 || h < 16 || w > 16384 || h > 16384)
        return false;
    width = static_cast<int>(w);
    height = static_cast<int>(h);
    return true;
}

// Returns false and prints the reason when the command line is invalid.
bool ParseOptions(int argc, wchar_t** argv, Options& options, bool& listPages)
{
    bool sawCapture = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i];
        auto next = [&](const char* name) -> const wchar_t* {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "error: %s needs a value\n", name);
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == L"--capture")
        {
            const wchar_t* value = next("--capture");
            if (value == nullptr)
                return false;
            options.capture.directory = value;
            sawCapture = true;
        }
        else if (arg == L"--size")
        {
            const wchar_t* value = next("--size");
            if (value == nullptr)
                return false;
            if (!ParseSize(value, options.capture.width, options.capture.height))
            {
                std::fprintf(stderr, "error: --size expects WxH, for example 1280x800\n");
                return false;
            }
        }
        else if (arg == L"--scale")
        {
            const wchar_t* value = next("--scale");
            if (value == nullptr)
                return false;
            wchar_t* end = nullptr;
            const float scale = std::wcstof(value, &end);
            if (end == value || *end != 0 || !(scale >= 0.5f && scale <= 4.0f))
            {
                std::fprintf(stderr, "error: --scale expects a number from 0.5 to 4\n");
                return false;
            }
            options.capture.scale = scale;
        }
        else if (arg == L"--page")
        {
            const wchar_t* value = next("--page");
            if (value == nullptr)
                return false;
            options.page = ToUtf8(value);
        }
        else if (arg == L"--frames")
        {
            const wchar_t* value = next("--frames");
            if (value == nullptr)
                return false;
            wchar_t* end = nullptr;
            const long frames = std::wcstol(value, &end, 10);
            if (end == value || *end != 0 || frames < 1 || frames > 10000)
            {
                std::fprintf(stderr, "error: --frames expects a number from 1 to 10000\n");
                return false;
            }
            options.capture.warmupFrames = static_cast<int>(frames);
        }
        else if (arg == L"--warp")
        {
            options.warp = true;
        }
        else if (arg == L"--list-pages")
        {
            listPages = true;
        }
        else if (arg == L"--help" || arg == L"-h" || arg == L"/?")
        {
            options.help = true;
        }
        else
        {
            std::fprintf(stderr, "error: unknown argument '%s'\n", ToUtf8(arg.c_str()).c_str());
            return false;
        }
    }

    if (sawCapture)
        options.mode = Mode::Capture;
    options.capture.page = options.page;
    return true;
}

void ReleaseConsoleWhenLaunchedFromExplorer()
{
    DWORD processes[2] = {};
    // Only this process attached means Windows created the console just for us.
    if (GetConsoleProcessList(processes, 2) == 1)
        FreeConsole();
}

int RunInteractive(bool warp)
{
    AppShell shell;
    ShellDesc desc;
    desc.warp = warp;
    if (!shell.Create(desc))
        return 1;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ConfigureImGuiIo(false);
    ApplyStyle(shell.DpiScale());
    ImGui_ImplWin32_Init(shell.Hwnd());
    ImGui_ImplDX11_Init(shell.Device(), shell.Context());
    shell.SetImGuiInputEnabled(true);

    facet::InitDesc init;
    init.device = shell.Device();
    init.context = shell.Context();
    if (!facet::Init(init))
        std::fprintf(stderr, "error: facet::Init failed\n");

    GalleryState gallery;
    auto frame = [&] {
        if (shell.ConsumeDpiChange())
            ApplyStyle(shell.DpiScale());
        float clear[4];
        ClearColor(clear);
        if (!shell.BeginFrame(clear))
            return false;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        facet::NewFrame();

        PageCtx ctx;
        ctx.scale = shell.DpiScale();
        DrawGallery(gallery, ctx, &shell);

        facet::EndFrame();
        ImGui::Render();
        shell.EndFrame();
        return true;
    };
    shell.SetLiveResizeCallback([&] { frame(); });

    while (shell.PumpMessages())
    {
        if (!frame())
            Sleep(10);  // minimized or occluded: nothing to draw
    }

    shell.SetLiveResizeCallback(nullptr);
    shell.SetImGuiInputEnabled(false);
    facet::Shutdown();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    const bool healthy = shell.DeviceHealthy();
    shell.Destroy();
    return healthy ? 0 : 1;
}

int RunOffscreen(const Options& options)
{
    AppShell shell;
    ShellDesc desc;
    desc.hidden = true;
    desc.warp = options.warp;
    if (!shell.Create(desc))
        return 1;
    std::printf("device: %s%s\n", shell.UsesWarp() ? "WARP" : "hardware",
                shell.DebugLayerActive() ? ", D3D11 debug layer" : "");

    const int result = RunCapture(shell, options.capture);
    shell.Destroy();
    return result;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc <= 1)
        ReleaseConsoleWhenLaunchedFromExplorer();

    Options options;
    bool listPages = false;
    if (!ParseOptions(argc, argv, options, listPages))
    {
        PrintUsage();
        return 2;
    }
    if (options.help)
    {
        PrintUsage();
        return 0;
    }
    if (listPages)
    {
        for (const PageInfo& page : Pages())
        {
            std::printf("%s", page.id.c_str());
            for (const std::string& state : page.states)
                std::printf(" %s", state.c_str());
            std::printf("\n");
        }
        return 0;
    }

    if (options.mode == Mode::Interactive)
        return RunInteractive(options.warp);
    return RunOffscreen(options);
}
