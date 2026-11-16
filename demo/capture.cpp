#include "capture.h"

#include <cstdio>
#include <fstream>
#include <system_error>
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

struct CapturedFile
{
    std::string page;
    std::string state;
    std::string file;
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
    out << "  \"files\": [";
    for (size_t i = 0; i < files.size(); ++i)
    {
        out << (i == 0 ? "\n" : ",\n");
        out << "    {\"page\": " << JsonString(files[i].page) << ", \"state\": "
            << JsonString(files[i].state) << ", \"file\": " << JsonString(files[i].file) << "}";
    }
    out << (files.empty() ? "]\n" : "\n  ]\n");
    out << "}\n";
    return static_cast<bool>(out);
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

    const std::string suffix = "__" + std::to_string(options.width) + "x" +
                               std::to_string(options.height) + "@" + FormatScale(options.scale);
    std::vector<CapturedFile> written;
    bool ok = true;
    std::vector<unsigned char> pixels;

    for (const int pageIndex : selected)
    {
        const PageInfo& page = pages[static_cast<size_t>(pageIndex)];
        for (int stateIndex = 0; stateIndex < static_cast<int>(page.states.size()); ++stateIndex)
        {
            const std::string& state = page.states[static_cast<size_t>(stateIndex)];
            const std::string fileName = page.id + "__" + state + suffix + ".png";
            const std::filesystem::path filePath = options.directory / fileName;

            HeadlessSession session;
            HeadlessOptions headless;
            headless.width = options.width;
            headless.height = options.height;
            headless.scale = options.scale;
            if (!session.Begin(shell, headless))
            {
                ok = false;
                continue;
            }

            GalleryState gallery;
            gallery.selectedPage = pageIndex;
            PageCtx ctx;
            ctx.capture = true;
            ctx.captureState = stateIndex;
            ctx.scale = options.scale;
            for (int frame = 0; frame < options.warmupFrames; ++frame)
                session.Frame([&] { DrawGallery(gallery, ctx, nullptr); });

            const bool readOk = session.Readback(pixels);
            const std::string pathText = reinterpret_cast<const char*>(filePath.u8string().c_str());
            if (!readOk || !session.Healthy())
            {
                std::fprintf(stderr, "error: readback failed for %s\n", fileName.c_str());
                ok = false;
                continue;
            }
            const int stride = options.width * 4;
            if (stbi_write_png(pathText.c_str(), options.width, options.height, 4, pixels.data(), stride) == 0)
            {
                std::fprintf(stderr, "error: cannot write %s\n", pathText.c_str());
                ok = false;
                continue;
            }
            std::printf("wrote %s\n", pathText.c_str());
            std::fflush(stdout);
            written.push_back({page.id, state, fileName});
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
