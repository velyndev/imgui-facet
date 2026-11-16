#pragma once

#include <filesystem>
#include <string>

#include "run_options.h"

namespace facet::demo
{

class AppShell;

struct CaptureOptions
{
    std::filesystem::path directory;
    int width = 1280;
    int height = 800;
    float scale = 1.0f;
    std::string page;        // empty: every registered page
    int warmupFrames = 60;   // frames rendered before each readback
    ThemeSelection themes = ThemeSelection::Both;
};

// Renders every selected page once per capture state and theme into PNG files named
// <page>__<state>__<theme>__<W>x<H>@<scale>.png and writes index.json next to them.
// Prints one line per written file. Returns 0 when every file was written and no D3D
// call failed, 1 otherwise.
int RunCapture(AppShell& shell, const CaptureOptions& options);

} // namespace facet::demo
