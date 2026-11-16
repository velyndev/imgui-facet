// facet_demo: gallery application for Facet.
//
//   facet_demo                         interactive window
//
// The program is built for the console subsystem. Started from a terminal or a script,
// it behaves like any command line tool: the shell waits for it, stdout and stderr can be
// redirected and the exit code is reliable, which scripted runs depend on.
// Started without arguments from Explorer, Windows opens a console for it; the program
// releases that console right away, so only the demo window stays (the console may show
// for a moment). A GUI subsystem program attaching to its parent console was not used
// because shells do not wait for GUI programs and redirected output is unreliable there.

#include <cstdio>
#include <string>

#include <windows.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <facet/facet.h>

#include "app_shell.h"
#include "gallery.h"
#include "pages/pages.h"

namespace
{

using namespace facet::demo;

struct Options
{
    bool warp = false;
    bool help = false;
    bool listPages = false;
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
        "  facet_demo [--warp]\n"
        "\n"
        "  --warp           render with the WARP software rasterizer\n"
        "  --list-pages     print the registered page ids and capture states\n");
}

// Returns false and prints the reason when the command line is invalid.
bool ParseOptions(int argc, wchar_t** argv, Options& options)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::wstring arg = argv[i];
        if (arg == L"--warp")
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
    return RunInteractive(options.warp);
}
