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
    bool overlay = false;    // the gallery floats as a shell over the backdrop (overlay mode)
    bool matrix = false;     // every size of kMatrixSizes at every scale of kMatrixScales, in app
                             // and overlay mode; width, height, scale and overlay are not used
};

// Renders every selected page once per capture state and theme into PNG files named
// <page>__<state>__<theme>__<W>x<H>@<scale>.png (overlay mode adds "__overlay"; a matrix run
// adds "__app" or "__overlay" to every name) and writes index.json next to them. Prints one line
// per written file. Returns 0 when every file was written and no D3D call failed, 1 otherwise.
int RunCapture(AppShell& shell, const CaptureOptions& options);

} // namespace facet::demo
