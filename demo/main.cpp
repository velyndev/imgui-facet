// facet_demo: gallery application for Facet.
//
//   facet_demo [--page id] [--theme light|dark] [--warp]      interactive window
//   facet_demo --capture <dir> [--size WxH] [--scale S] [--page id] [--frames N]
//              [--theme light|dark|both] [--warp]
//   facet_demo --selftest [--size WxH] [--scale S] [--page id] [--theme light|dark|both] [--warp]
//   facet_demo --list-pages
//
// An option that the chosen mode does not use is an error, so a typo in a script never
// silently produces a run with other settings than intended.
//
// The program is built for the console subsystem. Started from a terminal or a script,
// it behaves like any command line tool: the shell waits for it, stdout and stderr can be
// redirected and the exit code is reliable, which capture and selftest runs depend on.
// Started without arguments from Explorer, Windows opens a console for it; the program
// releases that console right away, so only the demo window stays (the console may show
// for a moment). A GUI subsystem program attaching to its parent console was not used
// because shells do not wait for GUI programs and redirected output is unreliable there.

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
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
#include "selftest.h"

namespace
{

using namespace facet::demo;

enum class Mode
{
    Interactive,
    Capture,
    Selftest,
};

struct Options
{
    Mode mode = Mode::Interactive;
    std::filesystem::path captureDirectory;
    int width = 1280;
    int height = 800;
    float scale = 1.0f;
    int warmupFrames = 60;
    std::string page;
    ThemeSelection theme = ThemeSelection::Both;
    bool warp = false;
    bool help = false;
    bool listPages = false;

    // Which options were given, to reject the ones the mode does not use.
    bool sawSize = false;
    bool sawScale = false;
    bool sawFrames = false;
    bool sawPage = false;
    bool sawTheme = false;
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
        "  facet_demo [--page id] [--theme light|dark] [--warp]\n"
        "  facet_demo --capture <dir> [--size WxH] [--scale S] [--page id] [--frames N]\n"
        "             [--theme light|dark|both] [--warp]\n"
        "  facet_demo --selftest [--size WxH] [--scale S] [--page id] [--theme light|dark|both] [--warp]\n"
        "  facet_demo --list-pages\n"
        "\n"
        "  --capture <dir>  render every page state offscreen and write PNG files and index.json\n"
        "  --selftest       run the scripted input tests of the pages\n"
        "  --size WxH       offscreen size in pixels (default 1280x800)\n"
        "  --scale S        UI scale from 0.5 to 4 (default 1)\n"
        "  --page id        only this page (interactive: the page shown first)\n"
        "  --frames N       capture: warm-up frames per page state before readback (default 60)\n"
        "  --theme T        light, dark or both (default: both for capture, dark otherwise)\n"
        "  --warp           render with the WARP software rasterizer\n"
        "  --list-pages     print the registered page ids and capture states\n"
        "\n"
        "An option that the chosen mode does not use is rejected.\n");
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

bool ParseTheme(const std::wstring& text, ThemeSelection& theme)
{
    if (text == L"light")
        theme = ThemeSelection::Light;
    else if (text == L"dark")
        theme = ThemeSelection::Dark;
    else if (text == L"both")
        theme = ThemeSelection::Both;
    else
        return false;
    return true;
}

bool Reject(bool given, const char* option, const char* modeName)
{
    if (given)
        std::fprintf(stderr, "error: %s is not used by %s\n", option, modeName);
    return given;
}

// Checks that every given option is used by the chosen mode and fills in the per-mode
// defaults. Returns false and prints the reason otherwise.
bool ValidateForMode(Options& options)
{
    if (options.listPages)
    {
        const bool extra = options.mode != Mode::Interactive || options.sawSize || options.sawScale ||
                           options.sawFrames || options.sawPage || options.sawTheme || options.warp;
        if (extra)
        {
            std::fprintf(stderr, "error: --list-pages takes no other options\n");
            return false;
        }
        return true;
    }

    switch (options.mode)
    {
    case Mode::Interactive:
    {
        const char* name = "the interactive window";
        if (Reject(options.sawSize, "--size", name) || Reject(options.sawScale, "--scale", name) ||
            Reject(options.sawFrames, "--frames", name))
            return false;
        if (!options.sawTheme)
            options.theme = ThemeSelection::Dark;
        if (options.theme == ThemeSelection::Both)
        {
            std::fprintf(stderr, "error: the interactive window shows one theme: use --theme light or dark\n");
            return false;
        }
        return true;
    }
    case Mode::Capture:
        return true;
    case Mode::Selftest:
        if (Reject(options.sawFrames, "--frames", "--selftest"))
            return false;
        if (!options.sawTheme)
            options.theme = ThemeSelection::Dark;
        return true;
    }
    return true;
}

// Returns false and prints the reason when the command line is invalid.
bool ParseOptions(int argc, wchar_t** argv, Options& options)
{
    bool sawCapture = false;
    bool sawSelftest = false;
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
            options.captureDirectory = value;
            sawCapture = true;
        }
        else if (arg == L"--selftest")
        {
            sawSelftest = true;
        }
        else if (arg == L"--size")
        {
            const wchar_t* value = next("--size");
            if (value == nullptr)
                return false;
            if (!ParseSize(value, options.width, options.height))
            {
                std::fprintf(stderr, "error: --size expects WxH from 16x16 to 16384x16384, for example 1280x800\n");
                return false;
            }
            options.sawSize = true;
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
            options.scale = scale;
            options.sawScale = true;
        }
        else if (arg == L"--page")
        {
            const wchar_t* value = next("--page");
            if (value == nullptr)
                return false;
            options.page = ToUtf8(value);
            options.sawPage = true;
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
            options.warmupFrames = static_cast<int>(frames);
            options.sawFrames = true;
        }
        else if (arg == L"--theme")
        {
            const wchar_t* value = next("--theme");
            if (value == nullptr)
                return false;
            if (!ParseTheme(value, options.theme))
            {
                std::fprintf(stderr, "error: --theme expects light, dark or both\n");
                return false;
            }
            options.sawTheme = true;
        }
        else if (arg == L"--warp")
        {
            options.warp = true;
        }
        else if (arg == L"--list-pages")
        {
            options.listPages = true;
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

    if (sawCapture && sawSelftest)
    {
        std::fprintf(stderr, "error: --capture and --selftest cannot be combined\n");
        return false;
    }
    if (sawCapture)
        options.mode = Mode::Capture;
    else if (sawSelftest)
        options.mode = Mode::Selftest;
    if (options.help)
        return true;
    return ValidateForMode(options);
}

void ReleaseConsoleWhenLaunchedFromExplorer()
{
    DWORD processes[2] = {};
    // Only this process attached means Windows created the console just for us.
    if (GetConsoleProcessList(processes, 2) == 1)
        FreeConsole();
}

int RunInteractive(const Options& options)
{
    GalleryState gallery;
    gallery.dark = options.theme == ThemeSelection::Dark;
    if (!options.page.empty())
    {
        gallery.selectedPage = FindPage(options.page);
        if (gallery.selectedPage < 0)
        {
            std::fprintf(stderr, "error: no page with id '%s'\n", options.page.c_str());
            return 1;
        }
    }

    AppShell shell;
    ShellDesc desc;
    desc.warp = options.warp;
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

    auto frame = [&] {
        float clear[4];
        ClearColor(clear);
        if (!shell.BeginFrame(clear))
            return false;
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        facet::NewFrame();

        // DrawGallery re-applies the style when the monitor DPI or the UI scale changed.
        gallery.baseScale = shell.DpiScale();
        PageCtx ctx;
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

    int result = 1;
    if (options.mode == Mode::Capture)
    {
        CaptureOptions capture;
        capture.directory = options.captureDirectory;
        capture.width = options.width;
        capture.height = options.height;
        capture.scale = options.scale;
        capture.page = options.page;
        capture.warmupFrames = options.warmupFrames;
        capture.themes = options.theme;
        result = RunCapture(shell, capture);
    }
    else
    {
        SelftestOptions selftest;
        selftest.page = options.page;
        selftest.width = options.width;
        selftest.height = options.height;
        selftest.scale = options.scale;
        selftest.themes = options.theme;
        result = RunSelftest(shell, selftest);
    }
    shell.Destroy();
    return result;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc <= 1)
        ReleaseConsoleWhenLaunchedFromExplorer();

    Options options;
    if (!ParseOptions(argc, argv, options))
    {
        PrintUsage();
        return 2;
    }
    if (options.help)
    {
        PrintUsage();
        return 0;
    }
    // Clashing page ids would silently overwrite capture files and selftest scripts.
    if (!ValidateRegistry())
        return 1;
    if (options.listPages)
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
        return RunInteractive(options);
    return RunOffscreen(options);
}
