#include "capture.h"

#include <cstdio>
#include <fstream>
#include <system_error>
#include <utility>
#include <vector>

// The demo compiles its own PNG encoder with internal linkage; the library does not
// provide one. STBIW_WINDOWS_UTF8 lets stbi_write_png take UTF-8 paths.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#if defined(_MSC_VER)
#pragma warning(push, 0)
#pragma warning(disable : 4996)
#endif
#include <stb_image_write.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include "app_shell.h"
#include "gallery.h"
#include "offscreen.h"
#include "pages/pages.h"

namespace facet::demo
{
namespace
{

// One size, scale and mode of a capture run.
struct CaptureConfig
{
    int width = 1280;
    int height = 800;
    float scale = 1.0f;
    bool overlay = false;
};

// The size matrix of --matrix.
constexpr int kMatrixSizes[][2] = {{900, 600}, {1280, 800}, {1920, 1200}};
constexpr float kMatrixScales[] = {1.0f, 1.5f, 2.0f};

struct CapturedFile
{
    std::string page;
    std::string state;
    std::string theme;
    std::string file;
    CaptureConfig config;
};

std::string FormatScale(float scale)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%g", static_cast<double>(scale));
    return buffer;
}

std::string JsonString(const std::string& text)
{
    std::string out = "\"";
    for (const char c : text)
    {
        switch (c)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char escaped[8];
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(c));
                out += escaped;
            }
            else
            {
                out += c;
            }
            break;
        }
    }
    out += '"';
    return out;
}

bool WriteIndex(const std::filesystem::path& path, const CaptureOptions& options,
                const std::vector<CapturedFile>& files)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out << "{\n";
    out << "  \"width\": " << options.width << ",\n";
    out << "  \"height\": " << options.height << ",\n";
    out << "  \"scale\": " << FormatScale(options.scale) << ",\n";
    out << "  \"matrix\": " << (options.matrix ? "true" : "false") << ",\n";
    out << "  \"themes\": [";
    const std::vector<bool> themes = SelectedThemes(options.themes);
    for (size_t i = 0; i < themes.size(); ++i)
        out << (i == 0 ? "" : ", ") << JsonString(ThemeName(themes[i]));
    out << "],\n";
    out << "  \"files\": [";
    for (size_t i = 0; i < files.size(); ++i)
    {
        out << (i == 0 ? "\n" : ",\n");
        out << "    {\"page\": " << JsonString(files[i].page) << ", \"state\": "
            << JsonString(files[i].state) << ", \"theme\": " << JsonString(files[i].theme)
            << ", \"width\": " << files[i].config.width << ", \"height\": " << files[i].config.height
            << ", \"scale\": " << FormatScale(files[i].config.scale) << ", \"mode\": "
            << JsonString(files[i].config.overlay ? "overlay" : "app") << ", \"file\": " << JsonString(files[i].file)
            << "}";
    }
    out << (files.empty() ? "]\n" : "\n  ]\n");
    out << "}\n";
    return static_cast<bool>(out);
}

// Renders one page state in one theme in a fresh offscreen session and writes the PNG.
// Prints the reason and returns false on failure.
bool CaptureOne(AppShell& shell, const CaptureOptions& options, const CaptureConfig& config, int pageIndex,
                int stateIndex, bool dark, const std::string& fileName, std::vector<unsigned char>& pixels)
{
    HeadlessSession session;
    HeadlessOptions headless;
    headless.width = config.width;
    headless.height = config.height;
    headless.scale = config.scale;
    if (!session.Begin(shell, headless))
        return false;

    GalleryState gallery;
    gallery.selectedPage = pageIndex;
    gallery.dark = dark;
    gallery.baseScale = config.scale;
    gallery.overlayMode = config.overlay;
    PageCtx ctx;
    ctx.capture = true;
    ctx.captureState = stateIndex;
    for (int frame = 0; frame < options.warmupFrames; ++frame)
    {
        ctx.captureFramesLeft = options.warmupFrames - 1 - frame;
        session.Frame([&] { DrawGallery(gallery, ctx, nullptr); });
    }
    if (ctx.captureError != nullptr)
    {
        std::fprintf(stderr, "error: %s: %s\n", fileName.c_str(), ctx.captureError);
        return false;
    }

    const std::filesystem::path filePath = options.directory / fileName;
    const std::string pathText = reinterpret_cast<const char*>(filePath.u8string().c_str());
    if (!session.Readback(pixels) || !session.Healthy())
    {
        std::fprintf(stderr, "error: readback failed for %s\n", fileName.c_str());
        return false;
    }
    const int stride = config.width * 4;
    if (stbi_write_png(pathText.c_str(), config.width, config.height, 4, pixels.data(), stride) == 0)
    {
        std::fprintf(stderr, "error: cannot write %s\n", pathText.c_str());
        return false;
    }
    std::printf("wrote %s\n", pathText.c_str());
    std::fflush(stdout);
    return true;
}

} // namespace

int RunCapture(AppShell& shell, const CaptureOptions& options)
{
    const std::span<const PageInfo> pages = Pages();
    std::vector<int> selected;
    for (int i = 0; i < static_cast<int>(pages.size()); ++i)
    {
        if (options.page.empty() || pages[static_cast<size_t>(i)].id == options.page)
            selected.push_back(i);
    }
    if (selected.empty())
    {
        std::fprintf(stderr, "error: no page with id '%s'\n", options.page.c_str());
        return 1;
    }

    std::error_code error;
    std::filesystem::create_directories(options.directory, error);
    if (error)
    {
        std::fprintf(stderr, "error: cannot create %s: %s\n",
                     reinterpret_cast<const char*>(options.directory.u8string().c_str()),
                     error.message().c_str());
        return 1;
    }

    std::vector<CaptureConfig> configs;
    if (options.matrix)
    {
        for (const bool overlay : {false, true})
            for (const auto& size : kMatrixSizes)
                for (const float scale : kMatrixScales)
                    configs.push_back({size[0], size[1], scale, overlay});
    }
    else
    {
        configs.push_back({options.width, options.height, options.scale, options.overlay});
    }

    std::vector<CapturedFile> written;
    bool ok = true;
    std::vector<unsigned char> pixels;

    for (const CaptureConfig& config : configs)
    {
        std::string suffix = "__" + std::to_string(config.width) + "x" + std::to_string(config.height) + "@" +
                             FormatScale(config.scale);
        if (options.matrix || config.overlay)
            suffix += config.overlay ? "__overlay" : "__app";
        for (const int pageIndex : selected)
        {
            const PageInfo& page = pages[static_cast<size_t>(pageIndex)];
            for (int stateIndex = 0; stateIndex < static_cast<int>(page.states.size()); ++stateIndex)
            {
                for (const bool dark : SelectedThemes(options.themes))
                {
                    CapturedFile file;
                    file.page = page.id;
                    file.state = page.states[static_cast<size_t>(stateIndex)];
                    file.theme = ThemeName(dark);
                    file.config = config;
                    file.file = file.page + "__" + file.state + "__" + file.theme + suffix + ".png";
                    if (CaptureOne(shell, options, config, pageIndex, stateIndex, dark, file.file, pixels))
                        written.push_back(std::move(file));
                    else
                        ok = false;
                }
            }
        }
    }

    const std::filesystem::path indexPath = options.directory / "index.json";
    if (!WriteIndex(indexPath, options, written))
    {
        std::fprintf(stderr, "error: cannot write index.json\n");
        ok = false;
    }
    else
    {
        std::printf("wrote %s\n", reinterpret_cast<const char*>(indexPath.u8string().c_str()));
    }

    if (!shell.DeviceHealthy())
        ok = false;
    std::printf("capture: %zu file(s) written, %s\n", written.size(), ok ? "ok" : "FAILED");
    return ok ? 0 : 1;
}

} // namespace facet::demo
