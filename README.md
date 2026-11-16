# Facet

Facet is a design framework for Dear ImGui applications on Windows with Direct3D 11. Its goal is
a consistent set of custom widgets, a light and a dark theme, icon rendering and proper scaling
for desktop tools and in-game overlay menus, without modifying Dear ImGui.

**Status: early development.** No Facet widgets exist yet. What exists today:

- The CMake build with pinned dependencies and presets for MSVC x64 and x86.
- The library skeleton: `facet::Init`, `NewFrame`, `EndFrame`, `Shutdown`, `Version` and the
  embedding of `assets/icons/*.svg` into the library at build time.
- `facet_demo`, a gallery application with a borderless, resizable, per-monitor DPI aware
  window, a light and a dark theme and a UI scale setting. It currently has one page
  (Foundation) that shows text sizes, a few stock Dear ImGui widgets, the theme surfaces with
  the contrast ratios of their text colors, the embedded assets, the UI scale and the frame time.
- A capture mode that renders pages offscreen into PNG files, and a selftest mode that operates
  pages with scripted mouse and keyboard input.
- Unit tests with doctest.

## Requirements

- Windows 10 or 11.
- Visual Studio 2026 (or its Build Tools) with the C++ workload (MSVC v145) and a Windows SDK.
- CMake 4.2 or newer for the "Visual Studio 18 2026" generator used by the presets.
  The project itself requires CMake 3.24.
- Git and network access on the first configure: CMake downloads Dear ImGui, FreeType,
  lunasvg and doctest.

## Build

```
cmake --preset msvc-x64
cmake --build --preset msvc-x64-release
ctest --preset msvc-x64-release
```

Other build presets: `msvc-x64-debug`, `msvc-x86-release`, `msvc-x86-debug` (configure
preset `msvc-x86`). Build output goes to `build/<preset>/`, programs to
`build/<preset>/bin/<Debug|Release>/`.

The configure preset `msvc-ninja-x64` (build and test presets `msvc-ninja-x64-debug` and
`msvc-ninja-x64-release`) uses Ninja Multi-Config with `cl`. It has to be run from an x64
developer command prompt, because Ninja does not locate the compiler by itself.

CMake options:

| Option | Default | Meaning |
|---|---|---|
| `FACET_BUILD_DEMO` | ON when top level | Build `facet_demo` |
| `FACET_BUILD_TESTS` | ON when top level | Build `facet_tests` and register them with CTest |
| `FACET_USE_FREETYPE` | ON | Rasterize fonts with FreeType (fetched Dear ImGui only) |
| `FACET_IMGUI_TARGET` | empty | Name of an existing Dear ImGui target to link instead of fetching Dear ImGui |
| `FACET_WARNINGS_AS_ERRORS` | OFF | Compile Facet's own code with `/WX` |

The library target is `facet::facet`. It never compiles the Dear ImGui platform or renderer
backends; when Facet fetches Dear ImGui, the Win32 and DX11 backends are available separately
as `facet::imgui_backends`. Using `FACET_IMGUI_TARGET` is not covered by a build test yet.

## Run the demo

```
build\msvc-x64\bin\Release\facet_demo.exe [--page id] [--theme light|dark] [--warp]
```

The title strip at the top moves the window (double-click maximizes) and holds a "Dark theme"
switch, a "UI scale" list (75 to 200 %, multiplied with the monitor DPI scale) and working
Minimize, Maximize and Close buttons; all of them work with the mouse and the keyboard. When
the window is too narrow for one row, the theme and scale controls move to a second row. The
window edges resize it. `--page` selects the page shown first, `--theme` the starting theme
(default dark). In Debug builds the D3D11 debug layer is used when the Windows "Graphics
Tools" optional feature is installed.

Every mode rejects options it does not use (exit code 2), so a mistyped run never silently
uses other settings.

### Capture mode

```
facet_demo --capture <dir> [--size WxH] [--scale S] [--page id] [--frames N]
           [--theme light|dark|both] [--warp]
```

Renders every page once per capture state and theme into an offscreen 8-bit RGBA target, using
a fixed time step of 1/60 s and no OS input, and writes
`<page>__<state>__<theme>__<W>x<H>@<scale>.png` plus `index.json` (size, scale, themes, and the
page, state, theme and file name of every image) into `<dir>`. The window is never shown.
Defaults: `1280x800`, scale 1, both themes, 60 warm-up frames per image. `--warp` renders with
the WARP software rasterizer, so the output does not depend on the GPU; two WARP runs on the
same machine produce identical files. The exit code is 0 only when every file was written and
no Direct3D call failed.

### Selftest mode

```
facet_demo --selftest [--size WxH] [--scale S] [--page id] [--theme light|dark|both] [--warp]
```

Runs the input scripts that pages register: synthetic clicks and key presses, followed by
checks, offscreen at the given size and scale (defaults `1280x800`, scale 1) in the given
theme (default dark; `both` runs every script once per theme). It prints a `PASS` or `FAIL`
line per check, naming the page and theme, and exits with 1 when any check failed.

`facet_demo --list-pages` prints the page ids with their capture states.

## Layout

```
include/facet/   public headers
src/             library sources; src/internal holds headers shared inside the library
assets/icons/    SVG files embedded into the library
cmake/           dependency, warning and asset embedding scripts
demo/            facet_demo; demo/pages holds one file per page
tests/           doctest unit tests, one file per topic
third_party/stb/ vendored stb headers
```

Source files are collected with globs, so a new file under `src/`, `demo/`, `tests/` or
`assets/icons/` is picked up without editing a CMake file. Demo pages and their selftest
scripts register themselves (`FACET_DEMO_PAGE`, `FACET_DEMO_SELFTEST` in
`demo/pages/pages.h` and `demo/selftest.h`). Page ids must be unique: `facet_demo` exits
with an error naming the id when two pages, or two selftest scripts, use the same one.

## License

MIT, see `LICENSE`. Third-party licenses are listed in `THIRD_PARTY_NOTICES.md`.
