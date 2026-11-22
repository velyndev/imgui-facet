# The test app

`facet_demo` is the test app of this repository. It shows every part of the library on a page of
its own, renders pages into PNG files in its capture mode, and operates them with scripted input
in its selftest mode. It is built when `FACET_BUILD_DEMO` is on (see the build section of
`README.md`).

## Window and pages

The window is borderless, resizable and per-monitor DPI aware, with a light and a dark theme or
one that follows the Windows app mode, a UI scale setting and a theme editor. The pages, listed by
group in the navigation:

- Start
  - Showcase, the page the app opens on: a sample settings menu (a placeholder account, a search
    over the settings, tabs with hit counts while searching, rows of toggles, a dropdown, a
    slider, a two-handle range slider, a color well, two key binds and a preset list) next to an
    inventory grid with a detail card and its loading, error and empty states. Every setting,
    item and value is a labelled placeholder, and nothing changes outside the page. The two cards
    sit side by side in a wide window and one above the other in a narrow one.
- Foundations
  - Tokens: every color token with its live contrast ratios, the border tokens and the knob face
    marked quiet or increased contrast with an "Increased contrast" switch, the accent presets,
    spacing, radii, control heights and the elevation levels.
  - Type: the text styles and weights, truncation, wrapping and match highlighting.
  - Motion: smoothing, a spring and a tween side by side with replay, reduce motion and
    slow-down switches, and their curves sampled at 240 and 30 frames per second.
  - Draw: corners, hairlines, gradients and alpha, arcs and loading indicators, and
    `DrawGroup`.
  - Effects: the elevation shadows, a glass panel over a busy backdrop next to its opaque
    fallback, the focus ring and the accent glow.
- Media
  - Icons: the built-in icon set in a searchable grid at three sizes, with an accent tint
    and an empty state.
  - Images: fit modes, corner radii, tint, asynchronous loading with its loading and failed
    states, and an SVG at three sizes.
- Blocks: one page per building block, each operated through prototype controls that are
  built only from the blocks, not from the widgets, and covered by selftests.
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

The page list is the Sidebar of the navigation family, one section per group, with a filter field
above it that keeps the pages whose title, group or id contains the text (Escape clears it; no
match shows an empty state with a button that clears it). Below the Expanded width class the
sidebar is the icon rail: the titles become tooltips, and a button above the rail shows the
titles and the filter until its twin collapses them again. A newly selected page and a row the
arrow keys reach are scrolled into view. `facet_demo --list-pages` prints the page ids with their
capture states.

A capture mode renders pages offscreen into PNG files, and a selftest mode operates pages with
scripted mouse and keyboard input; both are described below.

## Run

```
build\msvc-x64\bin\Release\facet_demo.exe [--page id] [--theme light|dark] [--overlay] [--warp]
```

The title bar at the top is the containers family's title bar inside an app frame: its free area
moves the window (double-click maximizes), its minimize, maximize and close buttons work (the
maximize button shows the Windows snap layouts), and the frame's resize band and the window edges
resize it. Beside the title sit the theme editor button, the theme mode (Light, Dark, System) and
the UI scale (75 to 200 %, multiplied with the monitor DPI scale), as many as the width class has
room for: the scale needs the Expanded width, the theme mode the Regular one, and the editor holds
both. Everything works with the mouse and the keyboard. `--page` selects the page shown first
(default: the showcase), `--theme` the starting theme (default dark; System is chosen in the
app), `--overlay` starts in overlay mode. In Debug builds the D3D11 debug layer is used when the
Windows "Graphics Tools" optional feature is installed.

Every mode rejects options it does not use (exit code 2), so a mistyped run never silently
uses other settings.

### Theme editor

The editor opens from the title bar as a pane beside the page (it floats over the page in a
Compact width) and changes the live theme; every change animates as a theme switch does. It holds:

- the theme mode, the accent (the presets and a custom color), the UI scale and the increased
  contrast switch (`facet::SetIncreasedContrast`), which lifts every outline and divider to 3:1
  (the Tokens page has the same switch next to the border tokens);
- corner radii, spacing, type sizes and control heights, each as a factor over the preset's
  values, and under "Each token" every token of the group on its own (a token set on its own
  ignores the factor; its row's reset button returns it to the factor);
- the line widths (the border width `metrics.hairline` and the focus ring width), as a factor
  and a row per token;
- the motion scale and reduce motion, the blur strength and the shadow strength;
- overlay mode and whether its backdrop moves.

Each group has a reset button, and "Reset all" returns every token, the accent, the UI scale and
the contrast switch to the preset (the theme mode and the window mode stay). Sliders of spacing, type, control
heights and line widths apply when they are released, because they move the pane's own layout;
the arrow keys and the other sliders apply at once. "Copy as C++" puts a function on the clipboard that sets exactly
the edited tokens on a `facet::Theme` and compiles against the public headers:

```cpp
// Theme edits made in the facet_demo theme editor on facet::Theme::Dark().
// Use: facet::Theme theme = facet::Theme::Dark(); ApplyThemeEdits(theme); facet::SetTheme(theme);
#include <facet/facet.h>

inline void ApplyThemeEdits(facet::Theme& theme)
{
    theme.radii.card = 18.0f;
    theme.spacing.md = 14.0f;
}
```

The values are absolute and were made on the preset named in the first line. A changed UI scale
and the increased contrast switch are named in comments, because they are not theme tokens.

### Overlay mode

Overlay mode (`--overlay`, or the switch in the editor) floats the gallery as a Shell over a
generated backdrop that stands in for a game: slanted bands, circles and rows of text in the
chart palette, moving unless motion is reduced or the editor's switch stops it. The shell has the
backdrop blur, resizes at every edge and corner, keeps between its minimum and maximum size (and
the display), and moves by its title bar. The overlay controller of the containers family shows
and hides it with F9 (a registered shortcut, `demo.overlay`) or the shell's close button, fading
it, and reports its input regions: while the menu is hidden a pinned panel says which key shows
it and lets every click through; while it shows, the editor names the key and the number of input
regions. In overlay mode the window keeps its own resize band along the edges.

## Capture mode

```
facet_demo --capture <dir> [--size WxH] [--scale S] [--page id] [--frames N]
           [--theme light|dark|both] [--warp]
```

Renders every page once per capture state and theme into an offscreen 8-bit RGBA target, using
a fixed time step of 1/60 s and no OS input, and writes
`<page>__<state>__<theme>__<W>x<H>@<scale>.png` plus `index.json` (size, scale, themes, and the
page, state, theme, size, scale, mode and file name of every image) into `<dir>`. With `--overlay`
the gallery runs in overlay mode and the names end in `__overlay`. The window is never shown.
Defaults: `1280x800`, scale 1, both themes, 60 warm-up frames per image. `--warp` renders with
the WARP software rasterizer, so the output does not depend on the GPU; two WARP runs on the
same machine produce identical files. The exit code is 0 only when every file was written and
no Direct3D call failed.

```
facet_demo --capture <dir> --matrix --page id [--frames N] [--theme light|dark|both] [--warp]
```

The size matrix: the page at 900x600, 1280x800 and 1920x1200, each at scale 1, 1.5 and 2, in app
and overlay mode, into one folder, with names ending in `__<W>x<H>@<scale>__app` or `__overlay`.
It needs `--page` and takes neither `--size`, `--scale` nor `--overlay`.

## Selftest mode

```
facet_demo --selftest [--size WxH] [--scale S] [--page id] [--theme light|dark|both] [--overlay]
           [--warp]
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

Besides the scripts of the pages, scripts of the app itself (`FACET_DEMO_APP_SELFTEST` in
`demo/selftest.h`, in `demo/app_selftests.cpp`) start on a page and run after its script: the
title bar and its window buttons, the navigation with its filter and rail, the theme editor (every
group, a token row, the resets, the accent and the export) and overlay mode (show, hide, move,
resize at the edges and a corner, the size limits). Their result lines name `app:<script>`.
`--overlay` runs every script in overlay mode.

## Design reasons of the app shell

`DESIGN.md` gives the reasons for the tokens, the widgets and the building blocks the pages use.
The reasons for the app's own chrome are kept here, next to its description.

- **Title bar.** The containers family's title bar on the window background with a hairline
  below it, so the app shows the same window chrome it offers to apps; the controls beside the
  title drop out by width class, narrowest last the editor button, because the editor holds them all.
- **Navigation.** The navigation family's Sidebar on its bgSecondary card, inset by spacing.sm
  from the window edges, so the page list reads as one surface beside the page. The groups come
  in a fixed order (Start, Foundations, Media, Blocks, Widgets). Each page has an icon chosen for
  what it shows; in the rail the title is the tooltip, so pages may share an icon. The filter
  sits above the card, where a search sits in the other lists of the library.
- **Theme editor.** An inspector pane at the trailing edge, so the page stays in view while a
  value changes; groups are divided by hairline captions instead of cards, because the pane is
  narrow and every row is a control. Its switches take the pane width, so they start at the label
  column where the factor sliders start their tracks, and every factor slider steps on a grid of
  at most 20 steps through 1, so all of them show their stops as ticks and read as one kind of
  control.
- **Overlay backdrop.** Chart palette bands, circles and text, so the blur has colors, edges and
  type to soften, and motion that shows it is live; the text names the scene as generated.
- **Showcase.** Two cards in a reflowing grid: a settings menu and an inventory are the two kinds
  of screen the library is built for, and the grid puts them side by side only where both fit.
