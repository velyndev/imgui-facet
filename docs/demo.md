# The test app

`facet_demo` is the test app of this repository. It shows every part of the library on a page of
its own, renders pages into PNG files in its capture mode, and operates them with scripted input
in its selftest mode. It is built when `FACET_BUILD_DEMO` is on (see the build section of
`README.md`).

## Window and pages

The window is borderless, resizable and per-monitor DPI aware, with a light and a dark theme or
one that follows the Windows app mode, and a UI scale setting. The pages, listed by group in the
navigation:

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
  - Popup host: a dropdown list and a long list that shrinks to the room it has, a menu with
    submenus, shortcuts that work and a checked choice, a context menu, a glass popover,
    tooltips, a modal with a confirm level, and levels placed near the display edges; a
    capture state shows a level in its close motion.
  - Scroll: a long area with edge fades, nested areas that hand the wheel over, an area that
    scrolls on both axes, the edge fades on every surface, content that fits, and a reduce
    motion switch.
  - List core: a virtual list of one million generated rows, a multiple selection, a sorted
    and filtered list that keeps its selection by id, a single choice on the selection pill,
    a tree of this gallery's pages, and the loading, error and empty states.
  - Drag and drop: a reorderable list, two lists that exchange rows (Ctrl copies) and a zone
    that takes list rows only, a grid of slots that moves, stacks, swaps and splits, a tree
    with drops before, after and into a group, and a long list that edge scrolls.
  - Text field core: plain, narrow, search, password, validated, floating label, limited and
    loading fields, a number field with steppers and drag, a multiline area, a read-only
    field, and the three control sizes.
- Widgets: one page per widget family, and two or three for the families whose widget groups
  are built apart (Buttons, Selection, Sliders, Text inputs and Value inputs, Color pickers and
  Other pickers, Lists, Navigation, Containers, Layout, Overlays, Display and Data table,
  Charts, Media, and for the composites Inventory and cards, Search and app controls, and
  Editors and canvas). Every page shows every widget of its family or group in its variants,
  sizes and states (read-only, disabled, error, mixed, loading and empty where the widget has
  them, and narrow widths), with capture states that force hover, press, keyboard focus, open
  popups and drags per section, and a selftest that operates each of them by mouse and keyboard
  and checks the values.

The page list scrolls when it is taller than the window and keeps the heading of the group
at the top pinned. Its rows use the item behaviour, the keyboard focus ring and the selection
pill block; the selected title is semibold, and a cut title shows in full on hover and on
keyboard focus. `facet_demo --list-pages` prints the page ids with their capture states.

A capture mode renders pages offscreen into PNG files, and a selftest mode operates pages with
scripted mouse and keyboard input; both are described below.

## Run

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

## Capture mode

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

## Selftest mode

```
facet_demo --selftest [--size WxH] [--scale S] [--page id] [--theme light|dark|both] [--warp]
```

Runs the input scripts that pages register, followed by checks, offscreen at the given size and
scale (defaults `1280x800`, scale 1) in the given theme (default dark; `both` runs every script
once per theme). Scripts click with any mouse button, double click, press and release the mouse
apart, drag along a path (also out of the window) and hold a drag still at its end, turn the
wheel, press and hold keys with modifiers (the Menu key included), rest the mouse on an item,
type text, copy and paste through a clipboard of the run's own (the system clipboard is never
touched), and check values with a tolerance, text, the clipboard, focus, and whether an item is
or is not in the visible part of its window. Durations are counted
in frames of the fixed 1/60 s time step, so a run does not depend on the speed of the machine.
A scripted click on a control outside the visible part of its window scrolls it into view
first. It prints a `PASS` or `FAIL` line per check, naming the page and theme, and exits with 1
when any check failed.

## Design reasons of the app shell

`DESIGN.md` gives the reasons for the tokens, the widgets and the building blocks the pages use.
The reasons for the app's own chrome are kept here, next to its description.

- **Title strip.** The window background with a hairline below it; the title in the headline
  style, labels in labelSecondary in front of their controls.
- **Navigation.** bgSecondary, group names in footnote labelSecondary, rows one control height
  tall, and the selection pill in accentSubtle that springs between rows. The groups come in a
  fixed order (Foundations, Media, Blocks, Widgets). The list scrolls when it is taller than the
  window; the heading of the group at the top stays pinned and the next heading pushes it out,
  so a scrolled list still says which group its rows belong to, and a newly selected page is
  scrolled into view. The rows run on the item behaviour, the focus ring and the selection pill
  block; the selected title is semibold, and a cut title shows in full beside the row on
  keyboard focus and at the pointer on hover.
