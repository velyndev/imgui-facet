# Facet

Facet is a design framework for Dear ImGui applications on Windows with Direct3D 11. Its goal is
a consistent set of custom widgets, a light and a dark theme, icon rendering and proper scaling
for desktop tools and in-game overlay menus, without modifying Dear ImGui.

**Status: early development.** The building blocks of Phase 1 exist inside the library and
work. Two places still show Dear ImGui's own scrollbar instead of the overlay scrollbar of the
scroll block: a popup level whose content is taller than the room (the menu, popover or dialog
window itself; lists inside a level scroll with the overlay bar), and the multiline text of
`TextArea`, which sizes and colors the bar from the scroll tokens. The overlay bar for both is
planned. The fourteen widget family headers in `include/facet/widgets/`
declare every widget with documentation comments, and every declared widget of the fourteen
families is implemented; none is a placeholder:

- Buttons, Selection and Lists.
- Sliders, including the two-dimensional pads (`XYPad`, `BoxRangePad`, `JoystickPad`),
  `ArcRangeSlider` and `AllocationBar`.
- Inputs: the text inputs and the value inputs.
- Pickers: the color pickers (`ColorWell`, `ColorPicker`, `ColorInputs`, `SwatchPalette`,
  `SatValSquare`, `ColorChannelSlider`, `ColorModeEditor`) and `DatePicker`, `DateRangePicker`,
  `TimePicker`, `AnglePicker`, `IconPicker`, `ThumbnailPicker` and `FileBrowser`.
- Navigation: `Sidebar`, `TabBar`, `MenuBar`, menus and menu items, `Breadcrumbs`, `Pagination`,
  `StepIndicator`, `Wizard` and `ShortcutSheet`.
- Containers.
- Layout: `SettingsRow`, `Stack`, `Grid`, the separators, `DependentGroup`, `SplitView`, `Flow`,
  `ListDetail`, `Panels` and `ItemGrid`.
- Overlays: `Modal`, `ConfirmDialog`, `Popover`, `ContextMenu`, tooltips (plain and rich), toasts,
  `Drawer`, `LoadingOverlay`, `CommandPalette`, `UnsavedChangesBar` and `WatermarkOverlay`.
- Display, including `DataTable` and its cell editing.
- Charts: `Sparkline`, `Legend`, `LineChart`, `BarChart`, `DonutChart`, `Gauge`, `LevelMeter`,
  `FrameTimeGraph` and `ChartRangeBrush`.
- Media: `ImageView`, `Avatar`, `AvatarGroup`, `ThumbnailStrip`, `Carousel`, `ImageViewer`,
  `ImageCompare`, `DropZone` and `TimelineScrubber`, next to the core image and icon items.
- Composites: the inventory and card composites (`InventoryGrid`, `InventoryTile`,
  `InventorySlot`, `InventoryDetailCard`, `MediaCard`, `UserChip`, `ProfileSwitcher`) and the
  public drag and drop calls (`BeginDragSource`, `BeginDropTarget`, `AcceptDrop`); the search and
  app controls (`SearchFilterBar`, `SettingsSearch` with its hit counts, `PresetList`,
  `KeybindList`, `ThemeModeControl`, `AccentPicker`, `UiScaleControl`, `Onboarding`,
  `ConsoleView`); and the editors (`PropertyGrid`, `GradientEditor`, `CurveEditor`,
  `BezierEditor`, `LayerList`, `PreviewCanvas`, `CanvasElement`).

Besides the widgets, `ApplyReorderMove`, `PassesFilter`, the built-in strings and the edit
session queries in `common.h` work. The status of each widget is the last line of its entry on its
family page under `docs/widgets/` (see `docs/widgets.md`); widgets that are planned but not
declared yet are listed at the end of their family page. What exists today:

- The CMake build with pinned dependencies and presets for MSVC x64 and x86. The version is
  written once, in `include/facet/core/version.h`, which CMake reads.
- The library lifetime: `facet::Init`, `NewFrame`, `EndFrame`, `Shutdown`,
  `InvalidateDeviceObjects`, `CreateDeviceObjects`, `Version`, and the embedding of
  `assets/icons/*.svg` into the library at build time.
- The public core headers in `include/facet/core/`. Every declared function links; the state
  of each area:
  - Core, theme and scale: light and dark presets, a runtime accent with derived readable
    states (text at 4.5:1, the focus ring and an eight-color chart palette at 3:1 on every
    surface), an accent for one region (`PushAccent` / `PopAccent`), animated theme switches, a
    mode that follows the Windows app theme and `ApplyImGuiStyle` for stock Dear ImGui widgets
    (`theme.h`); the UI scale with three modes and pixel snapping helpers (`scale.h`). The theme
    also holds the tokens of popups, menus, scrollbars, lists, drag and drop and text fields,
    which the blocks below use, and of the widgets: status fill states and washes, visited
    links, code tints, checkerboards, dots, ticks, steps, progress tracks, copied feedback,
    containers, dividers and panels, tabs, tables, pickers, charts, media, flicks and inertia.
    See `docs/theming.md` and `docs/scaling.md`.
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
  session queries (`IsItemEditCommitted`, `IsItemEditCanceled`) work; `NextVisibleRows`
  forwards to the list core and hands out rows inside the row ranges the list core opens (a
  `VirtualList` and a `DataTable` open one), and returns false outside them. `facet/facet.h`
  includes every core and widget header. The rules for widget declarations are in `docs/api-conventions.md`.
- The building blocks of the widgets, internal to the library (`src/internal/`, each header
  starts with a usage sketch): item behaviour (hover, press, focus, disabled and hit targets
  of at least `minHitTarget`), keyboard modality and the focus ring, labelled rows and row
  groups, overflow fitting, the value drag core (linear, logarithmic, stepped, wrapping and
  delta mappings with one edit session per drag or key press, and a drag on two axes for pads),
  the multi-handle model, the selection pill, fuzzy matching with ranked match ranges, chord
  capture, the chart core (scales, ticks and labels, hover snapping, decimation, gaps and series
  colors), the control anatomy
  every widget shares (`control.h`: sizes and paddings, the label in front of or above a
  control, text cut with an ellipsis and its tooltip, fill feedback, the field frame, the loading
  spinner and the help and error line, whose errors are collected for `ValidationSummary`), and:
  - the popup host (`popup_host.h`, `popup_place.h`): placement beside an anchor with flip and
    clamp, an end alignment that turns toward the window holding the anchor, nested levels on Dear
    ImGui's popup stack, Escape and the context menu keys through
    key ownership, focus return, glass or opaque surfaces, an open motion and a close motion
    that replays the last frame, tooltips with hover intent, panel levels that leave the page
    around them usable, and the modal scrim; and the menu rows on it (`popup_menu.h`): submenus
    that open after a rest or by key and stay open while the pointer aims at them, a shortcut
    column, icons, check marks or radio dots, and typeahead;
  - the scroll core (`scroll.h`): scroll areas with overlay bars, smooth wheel scrolling that
    hands over between nested areas, edge fades, revealing a rectangle and edge scrolling, and
    the same bars for a window another call begins (a Dear ImGui table);
  - the list and selection core (`list_core.h`, `list_view.h`): virtualized rows, Dear
    ImGui's multi-select with box selection, keyboard and typeahead, trees, the loading,
    error and empty states, an active row driven from a field that keeps the keyboard, and the
    row look and keys on their own for containers that place their rows themselves;
  - the drag and drop core (`drag_drop.h`): typed drags that work with stock Dear ImGui
    sources and targets, ordered and wrapping drop zones with the gap or an insertion line,
    the drag preview, edge scrolling of scroll areas, and a keyboard path that a source opts
    into;
  - the text field core (`text_field.h`, `text_field_number.h`): fields on Dear ImGui's own
    text editing with the Facet frame, adornments, edit sessions and number editing with a
    logarithmic drag and a hook for a field's own text grammar.

  The blocks call each other through these headers: a list view scrolls in a scroll area and
  keeps the wheel inside a popup level, a drag edge scrolls the area under it, a drag within a
  list view reports the move, a text field hands its list keys to an open completion level and
  drives the active row of a list view there, and a table runs its rows through the list row
  block (tested in `tests/test_block_combinations.cpp` and `tests/test_list_core.cpp`).
- `facet::EndFrame` reports a `PushAccent`, `PushControlSize` or Facet scope left open
  through Dear ImGui's recoverable error path and clears it.
- `facet_demo`, the test app: a gallery with a page for every foundation, building block and
  widget family, in a light and a dark theme at UI scales from 75 to 200 %. `docs/demo.md`
  describes its window and its pages.
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

`docs/demo.md` describes the window, the pages, and the capture and selftest modes with their
options.

## Design tokens

`DESIGN.md` lists the starting value of every theme token and the reason for each visual
choice; the reasons for the test app's own chrome are in `docs/demo.md`.

## Layout

```
include/facet/   public headers: core/ (one per module), widgets/ (one per widget family)
src/             library sources; src/internal holds headers shared inside the library
assets/icons/    SVG files embedded into the library
cmake/           dependency, warning and asset embedding scripts
docs/            topic guides: theming, scaling, images and icons, layout, input;
                 api-conventions.md (the rules every widget declaration follows);
                 writing-a-widget.md (how a widget is built, tested, shown and documented);
                 widgets.md (the widget families, with one page each under docs/widgets/);
                 demo.md (the test app: its pages, capture and selftest modes)
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
