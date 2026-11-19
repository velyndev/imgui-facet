# Facet

Facet is a design framework for Dear ImGui applications on Windows with Direct3D 11. Its goal is
a consistent set of custom widgets, a light and a dark theme, icon rendering and proper scaling
for desktop tools and in-game overlay menus, without modifying Dear ImGui.

**Status: early development.** No Facet widget is implemented yet. The fourteen widget family
headers in `include/facet/widgets/` declare the planned widgets with documentation comments,
and every declared function links. Each widget function draws a placeholder that says
"planned", returns the neutral result and writes nothing; End functions, queries and helpers
such as `PassesFilter` draw nothing and return neutral values. The exceptions: `ApplyReorderMove`
works, the built-in strings and the edit session queries in `common.h` work, and the
`BeginDataTable` placeholder opens a plain Dear ImGui table so the caller's cells land in it (see
`docs/widgets.md` and the family pages under `docs/widgets/`).
The building blocks the widgets will be made of exist inside the library. What exists today:

- The CMake build with pinned dependencies and presets for MSVC x64 and x86. The version is
  written once, in `include/facet/core/version.h`, which CMake reads.
- The library lifetime: `facet::Init`, `NewFrame`, `EndFrame`, `Shutdown`,
  `InvalidateDeviceObjects`, `CreateDeviceObjects`, `Version`, and the embedding of
  `assets/icons/*.svg` into the library at build time.
- The public core headers in `include/facet/core/`. Every declared function links; the state
  of each area:
  - Core, theme and scale: light and dark presets, a runtime accent with derived readable
    states, an accent for one region (`PushAccent` / `PopAccent`), animated theme switches, a
    mode that follows the Windows app theme and `ApplyImGuiStyle` for stock Dear ImGui widgets
    (`theme.h`); the UI scale with three modes and pixel snapping helpers (`scale.h`). See
    `docs/theming.md` and `docs/scaling.md`.
  - Core, layout: width classes of the available width and the column layout of labelled rows
    (`layout.h`) work. See `docs/layout.md`.
  - Core, input: key chords as text and back, key binds in the modes Hold, Toggle, Always and
    Off, and the shortcut registry with global, window and focused item scopes and conflict
    lookup (`input.h`) work. Key binds accept extra modifiers unless `exactModifiers` is set;
    chords and shortcuts need exactly their modifiers. See `docs/input.md`.
  - Core, color: conversions with OKLab, OKLCH and WCAG contrast (`color.h`) work.
  - Core, drawing: rectangles with circular or continuous corners, hairlines, rounded
    gradients, checkerboards, arcs with round caps, `DrawGroup` and pointer cursor helpers
    (`draw.h`) work.
  - Core, text: Segoe UI Variable weights loaded from the system (static Segoe UI as
    fallback), text styles and text measuring (`text.h`) work.
  - Core, motion: smoothing, springs and tweens keyed by ImGui id, and reduced motion that
    follows the Windows setting (`anim.h`) work.
  - Effects: soft shadows and glow drawn from a baked atlas, and glass panels that blur what
    was drawn behind them, with an opaque tinted fallback where the render target cannot be
    read (`effects.h`). The demo page Effects shows each of them.
  - Images: PNG, JPEG, BMP, TGA and GIF loading (synchronous and on worker threads), RGBA
    uploads, SVG rasterized per UI scale, and drawing with contain, cover and fill, rounded
    corners, tint and loading and failed placeholders work (`image.h`, see
    `docs/images-and-icons.md`).
  - Icons: the built-in SVG icon set, icons registered at runtime, and drawing from a
    per-scale atlas work (`icons.h`).
- `include/facet/widgets/common.h`: the types the widget families share, the control size
  stack (`PushControlSize`), control heights, the built-in strings (`SetStrings`) and the edit
  session queries (`IsItemEditCommitted`, `IsItemEditCanceled`) work; `NextVisibleRows` is a
  stub that returns false until the list core exists (planned). `facet/facet.h` includes every
  core and widget header. The rules for widget declarations are in `docs/api-conventions.md`.
- The building blocks of the widgets, internal to the library (`src/internal/`, each header
  starts with a usage sketch): item behaviour (hover, press, focus, disabled and hit targets
  of at least `minHitTarget`), keyboard modality and the focus ring, labelled rows and row
  groups, overflow fitting, the value drag core (linear, logarithmic, stepped, wrapping and
  delta mappings with one edit session per drag or key press), the multi-handle model, the
  selection pill, fuzzy matching with ranked match ranges, chord capture, and the chart core
  (scales, ticks and labels, hover snapping, decimation and series colors). The popup host,
  scroll core, list and selection core, drag and drop core and text field core are planned.
- `facet::EndFrame` reports a `PushAccent`, `PushControlSize` or Facet scope left open
  through Dear ImGui's recoverable error path and clears it.
- `facet_demo`, a gallery application with a borderless, resizable, per-monitor DPI aware
  window, a light and a dark theme or one that follows the Windows app mode, and a UI scale
  setting. Its pages, listed by group in the navigation:
  - Foundations
    - Tokens: every color token with its live contrast ratios, the accent presets, spacing,
      radii, control heights and the elevation levels.
    - Type: the text styles and weights, truncation, wrapping and match highlighting.
    - Motion: smoothing, a spring and a tween side by side with replay, reduce motion and
      slow-down switches, and their curves sampled at 240 and 30 frames per second.
    - Draw: corners, hairlines, gradients and alpha, arcs and loading indicators, and
      `DrawGroup`.
    - Effects: the elevation shadows, a glass panel over a busy backdrop next to its opaque
      fallback, and the focus ring halo and accent glow.
  - Media
    - Icons: the built-in icon set in a searchable grid at three sizes, with an accent tint
      and an empty state.
    - Images: fit modes, corner radii, tint, asynchronous loading with its loading and failed
      states, and an SVG at three sizes.
  - Blocks: one page per building block, each operated through prototype controls that are
    built only from the blocks (they are not the Phase 2 widgets) and covered by selftests.
    - Item behaviour: hover, press, focus and disabled states with their animated amounts,
      and grown hit targets.
    - Focus: the focus ring on every surface it must stay visible on, and cut by the edge of
      a scrolling region; shown only after keyboard input.
    - Rows and width classes: labelled rows with a shared label column, descriptions, help
      and error texts, modified markers and reset buttons, stacking in narrow widths.
    - Overflow: a row of buttons that moves what does not fit behind a "More" button; the
      current item stays in the row on the selection pill.
    - Value drag: linear, whole-number, stepped, logarithmic, snapping, vertical, read-only
      and disabled sliders with their edit sessions, fine and coarse movement, and Escape to
      restore the value.
    - Multi-handle: range sliders whose handles stop at or push each other, logarithmic,
      stepped, mirrored and vertical ones, a draggable span, and three handles that can be
      added and removed.
    - Selection pill: the sliding highlight behind the selected entry, with a capture state
      that shows it on the way to a new entry.
    - Fuzzy matching: the built-in icon names ranked as you type, with the matched letters
      highlighted, an empty state, and the time one key press takes over 10000 generated
      names.
    - Keys and shortcuts: chord capture, three registered shortcuts (two global, one bound to
      the page's window, one of them a plain letter that a text field keeps), a conflict
      message, and one key bind per mode with a live indicator.
    - Chart core: generated functions with nice ticks, a crosshair that follows the mouse or
      the arrow keys after Tab, a logarithmic axis, a 100000 sample series drawn through
      decimation, the series colors with their contrast, and an empty state.
  - Widgets: one page per widget family (Buttons, Selection, Sliders, Inputs, Pickers, Lists,
    Navigation, Containers, Layout, Overlays, Display, Charts, Media, Composites). Every widget
    on them is a placeholder of its declared size that cannot be operated.

  The page list scrolls when it is taller than the window and keeps the heading of the group
  at the top pinned. Its rows use the item behaviour, the keyboard focus ring and the selection
  pill block; the selected title is semibold, and a cut title shows in full on hover and on
  keyboard focus. `facet_demo --list-pages` prints the page ids with their capture states.
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

The title strip at the top moves the window (double-click maximizes) and holds a "Theme" list
(Light, Dark, and System, which follows the Windows app mode), a "UI scale" list (75 to 200 %,
multiplied with the monitor DPI scale) and working Minimize, Maximize and Close buttons; all of
them work with the mouse and the keyboard. When the window is too narrow for one row, the theme
and scale controls move to a second row. The window edges resize it. `--page` selects the page
shown first, `--theme` the starting theme (default dark); it takes only light or dark, so
System is chosen in the list. In Debug builds the D3D11 debug layer is used when the Windows "Graphics
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

Runs the input scripts that pages register, followed by checks, offscreen at the given size and
scale (defaults `1280x800`, scale 1) in the given theme (default dark; `both` runs every script
once per theme). Scripts click, press and release the mouse apart, drag along a path (also out
of the window), turn the wheel, press and hold keys with modifiers, rest the mouse on an item,
type text, and check values with a tolerance, text, focus and visibility. Durations are counted
in frames of the fixed 1/60 s time step, so a run does not depend on the speed of the machine.
A scripted click on a control outside the visible part of its window scrolls it into view
first. It prints a `PASS` or `FAIL` line per check, naming the page and theme, and exits with 1
when any check failed.

## Design tokens

`DESIGN.md` lists the starting value of every theme token and the reason for each visual
choice.

## Layout

```
include/facet/   public headers: core/ (one per module), widgets/ (one per widget family)
src/             library sources; src/internal holds headers shared inside the library
assets/icons/    SVG files embedded into the library
cmake/           dependency, warning and asset embedding scripts
docs/            topic guides: theming, scaling, images and icons, layout, input;
                 api-conventions.md (the rules every widget declaration follows); widgets.md
                 (the widget families, with one page each under docs/widgets/)
demo/            facet_demo; demo/pages holds one file per page
tests/           doctest unit tests, one file per topic; tests/support holds the shared
                 Dear ImGui and WARP session for tests that run frames and scripted input;
                 tests/headers compiles every public header alone
third_party/stb/ vendored stb headers
```

Source files are collected with globs, so a new file under `src/`, `demo/`, `tests/` or
`assets/icons/` is picked up without editing a CMake file. Demo pages and their selftest
scripts register themselves (`FACET_DEMO_PAGE`, `FACET_DEMO_SELFTEST` in
`demo/pages/pages.h` and `demo/selftest.h`). Page ids must be unique: `facet_demo` exits
with an error naming the id when two pages, or two selftest scripts, use the same one.

## License

MIT, see `LICENSE`. Third-party licenses are listed in `THIRD_PARTY_NOTICES.md`.
