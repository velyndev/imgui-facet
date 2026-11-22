# Theming

Everything Facet draws takes its colors, lengths, text sizes and timings from one value, the
current theme, returned by `facet::T()` (`include/facet/core/theme.h`). The token values and
the reason for each are listed in `DESIGN.md`.

## The theme value

`facet::Theme` is a plain struct of token groups:

| Group | Holds |
|---|---|
| `colors` | Surfaces (with one per elevation level, read with `ElevatedSurface(level)`), translucent fills, four label strengths, separators, the control outline at rest and hovered (`controlBorder`, `controlBorderHover`), the knob face (`knob`), the accent and its derived states (visited links included), status colors with their solid fill states and translucent washes, focus ring, shadow, glass tint, scrim, the chart palette (`chart[kChartPaletteSize]`), the scrollbar thumbs, the code view tints and the two checkerboard cells |
| `spacing` | The 4 px grid: `xs` 4 to `xxxl` 32 |
| `radii` | `element`, `control`, `card`, `popover`, `window`, and where continuous corners start |
| `metrics` | Control heights, toggle, checkbox and slider sizes, focus ring width (2), hairline (the width of every outline and divider), minimum hit target, the default sizes of larger controls (faders, pads, pickers, charts, panels, cards, thumbnails, slots), status and page dots, slider ticks, the segment inset, progress tracks per control size, the icon of an empty state, step markers and their connectors, and the checkerboard cell |
| `type` | Size and weight of every `TextStyle` |
| `motion` | Springs, durations, loading periods, the popup open and close motion, how long copied feedback stays, the live dot pulse, a global `scale` and `reduceMotion` |
| `elevation` | Three shadow levels (their surfaces are in `colors`) |
| `glass` | Backdrop blur settings |
| `layout` | Width class breakpoints, the labelled row columns, the divider band of split views and panels, the panel header |
| `overlay` | Popup placement, menus and their shortcut gap, tooltip timing, toasts, dialog, drawer and command palette sizes, the popover arrow, the command palette's distance from the top |
| `containers` | Shell size and minimum, title bar, resize band, sidebar, icon rail and inspector widths, the status bar height |
| `scroll` | Overlay scrollbar widths, inset, the gap to the content, minimum thumb, the delay before an idle bar dims and how far, wheel distance, edge fade |
| `list` | Row heights per control size, tree indent, default visible rows, typeahead timeout |
| `drag` | Drag start threshold, preview opacity, edge scrolling, the delay before a resting drag opens a target, the flick speed and the inertia of a released pan |
| `field` | Text field padding and caret width |
| `tabs` | Narrowest and widest tab of a tab bar, the underline of the selected tab |
| `table` | Data table header height, narrowest column, column resize band |
| `picker` | Day cell of the date pickers, the color bars and hue ring of the color pickers, the time picker columns |
| `charts` | Series line width, point markers, the gap between bars, legend swatches, the donut hole, the gauge arc, level meter segments and peak hold |
| `media` | The overlap of grouped avatars, the timeline playhead, the dashes of a drop zone outline |
| `composites` | Curve and easing editor points, gradient stops and their remove distance, the easing preview time, the narrowest property label column, the search field beside filter chips, key bind chord columns, the inventory category bar, media card covers and their horizontal layout, canvas snapping and resize handles |

Lengths are design units. Widgets convert them with `facet::Px()` (see `docs/scaling.md`).

`facet::Theme::Light()` and `facet::Theme::Dark()` return the presets. A custom theme is a
preset with some fields changed:

```cpp
facet::Theme theme = facet::Theme::Dark();
theme.radii.card = 10.0f;
theme.motion.scale = 1.5f;      // every duration and spring 1.5 times longer
facet::SetTheme(theme);
```

### The token behind an option's default

Widget options whose comment says "0 for the token default" (or names a default size) read these
tokens. Each widget in the table reads the token listed for its option.

| Options | Token |
|---|---|
| `ShellOptions::initialSize`, `minSize` | `containers.shellSize`, `containers.shellMinSize` |
| `AppFrameOptions::resizeBorder`, `TitleBarOptions::height` | `containers.resizeBorder`, `containers.titleBarHeight` |
| `SidebarOptions::width` (and its icon rail) | `containers.sidebarWidth` (`containers.railWidth`) |
| `InspectorPaneOptions::paneWidth`, `minPaneWidth` | `containers.inspectorWidth`, `containers.inspectorMinWidth` |
| `ModalOptions::width`, `DrawerOptions::extent` | `overlay.modalWidth`, `overlay.drawerWidth` (Start, End) or `overlay.drawerHeight` (Top, Bottom) |
| `TooltipOptions::maxWidth`, `ToastOptions::duration` | `overlay.tooltipMaxWidth`, `overlay.toastDuration` |
| `maxVisibleRows` (dropdowns, Autocomplete, CommandPalette) | `list.popupRows` |
| `visibleRows` (ListBox, SelectableList, TransferList, SearchableChecklist) | `list.inlineRows` |
| `height` of vertical sliders and channel faders | `metrics.faderLength` |
| `fieldWidth` (SliderWithInput, RangeInput), `histogramHeight` | `metrics.numberFieldWidth`, `metrics.histogramHeight` |
| `sweep` (ArcRangeSlider, Knob) | `metrics.dialSweep` |
| `OptionCardGroupOptions::cardWidth`, `ThumbnailPickerOptions::thumbnailSize`, `FileBrowserOptions::height` | `metrics.cardWidth`, `metrics.thumbnailSize`, `metrics.panelHeight` |
| `ProgressBarOptions::size` (track thickness), `SecureFieldOptions::strength` (the meter) | `metrics.progressHeightSmall`, `progressHeight`, `progressHeightLarge`; the meter takes the small one |
| `EmptyStateOptions::icon` (not compact), `StatusDot`, `CycleSelectorOptions::pageDots` | `metrics.emptyIconSize`, `metrics.statusDotSize`, `metrics.pageDotSize` |
| `CopyButton`, `CopyField`, `InlineCode` (how long Strings::copied shows), `StatusDotOptions::pulse` | `motion.feedbackHold`, `motion.pulsePeriod` |
| `SplitViewOptions`, `PanelsOptions` (the divider), `PanelOptions::collapsible` (header and collapsed strip) | `layout.dividerBand`, `layout.panelHeaderHeight` |
| `PopoverOptions::arrow`, `CommandPalette` (its place) | `overlay.arrowSize`, `overlay.commandPaletteTop` |
| `DrawerOptions::dismissible` and toasts (a flick closes them), `Carousel` (a flick pages), `ImageViewer` (a released pan) | `drag.flickSpeed`, `drag.inertiaTime` |
| `TabStyle::Underline`, `StepIndicator` | `tabs.underlineHeight`, `metrics.stepMarkerSize` and `metrics.stepConnectorWidth` |
| `ColorWell` and alpha tracks, `ImageViewerOptions::checkerboard` | `colors.checker`, `metrics.checkerCell` |
| `ColorChannelSlider` and the bars of `ColorPicker`, `ColorPickerMode::Wheel`, `TimePicker` columns | `picker.colorBarHeight`, `picker.hueRingWidth`, `picker.timeColumnWidth` |
| Series lines and `markers`, `BarChart` bars, `Legend` entries, `DonutChart` (not `pie`), `Gauge`, `LevelMeterOptions::segments` and `peakHold` | `charts.lineWidth`, `charts.markerSize`, `charts.barGap`, `charts.legendSwatch`, `charts.donutHole`, `charts.gaugeWidth`, `charts.meterSegmentGap`, `charts.peakHold` |
| `AvatarGroup`, `TimelineScrubber`, `DropZone` | `media.avatarOverlap`, `media.playheadWidth`, `media.dashLength` and `media.dashGap` |
| `CurveEditor` and `BezierEditor` points, `GradientEditor` stops, `BezierEditorOptions::preview` | `composites.editorHandleSize`, `composites.gradientStopSize` and `composites.stopRemoveDistance`, `composites.easingPreviewTime` |
| `PropertyGridOptions::labelWidth` (the column drag), `SearchFilterBar` (chips under the field), `KeybindList` | `composites.propertyLabelMinWidth`, `composites.searchFieldMinWidth`, `composites.keybindColumnWidth` |
| `InventoryItem::tint`, `MediaCardOptions::width` (cover size and the horizontal layout) | `composites.inventoryTintHeight`, `composites.mediaCoverAspect` and `composites.mediaCardHorizontalBelow` |
| `CanvasElementOptions::snap` and `resizable` | `composites.canvasSnapDistance`, `composites.canvasHandleSize` |

## Switching

```cpp
facet::SetTheme(theme);          // animated while Facet runs
facet::SetTheme(theme, false);   // at once
```

An animated switch lasts `motion.durationStandard * motion.scale` of the new theme. Facet
blends one step per `facet::NewFrame`: colors in OKLab, lengths and timings linearly, and
switches (`dark`, font weights, flags) and whole-number counts (rows, toasts) change at the
halfway point. `T()` returns the blend
while it runs; `facet::ThemeTransitionActive()` tells whether one runs. Before `facet::Init`,
while motion is reduced, or with a duration of 0 the switch is immediate.

`facet::MixThemes(a, b, t)` computes such a blend directly; `t = 0` and `t = 1` return exact
copies of `a` and `b`.

## Light, dark or the system setting

```cpp
facet::SetThemeMode(facet::ThemeMode::System);
```

`ThemeMode::Light` and `ThemeMode::Dark` show the presets. `ThemeMode::System` shows the preset
that matches the Windows app mode (the registry value `AppsUseLightTheme`), and while Facet runs
it watches that value: when the user changes the Windows setting, the theme follows with an
animated switch at the next `facet::NewFrame`. `facet::SystemPrefersDarkTheme()` reads the
setting.

A mode switch replaces only `dark`, `colors` and `elevation` with those of the preset. The other
groups of the current theme (spacing, radii, metrics, type, motion, glass and the groups after
it) stay as the app set them with `SetTheme`, so a reduced-motion or motion-scale setting survives a switch, including
one that follows the Windows setting.

## Accent

```cpp
facet::SetAccent(IM_COL32(0x24, 0x8A, 0x3D, 0xFF));
```

`SetAccent` derives from one color, in OKLCH:

- `onAccent`: white or black, whichever has the higher contrast on the accent.
- `accentHover`, `accentPressed`: the lightness moved away from `onAccent`, so text stays at
  4.5:1 or more on every state.
- `accentSubtle`: the accent at low opacity, for selection backgrounds.
- `accentText`: the accent hue with its lightness moved until it reaches 4.5:1 on every surface,
  for links and accent-colored text.
- `focusRing`: the accent hue with its lightness moved until it reaches 3:1 (the non-text
  contrast) on every surface and against `fill[0]` over each, opaque; an accent that already
  reaches it is used as it is. The marks that carry a state use it too: the filled part of a
  track, an on switch, the accent outline of a selected tile or a focused field, drop indicators.
- `chart[0]` to `chart[7]`: the chart palette, eight hues stepping around the OKLCH circle from
  the accent hue, each moved in lightness until it reaches 3:1 on every surface. Charts, legends
  and other indexed data take color `i % kChartPaletteSize` for entry `i`.
- `accentTextVisited`: the `accentText` hue turned by 60 degrees, moved until it reaches 4.5:1 on
  every surface, for visited links.

`ApplyAccent` also derives the states of the status colors, which do not depend on the accent:
`onDestructive`, `destructiveHover` and `destructivePressed` like the accent states, `onSuccess`
and `onWarning` (white or black), and `destructiveSubtle`, `successSubtle` and `warningSubtle`, the
status colors at 14 % opacity. A theme that changes `destructive`, `success` or `warning` calls
`theme.ApplyAccent(theme.colors.accent)` afterwards.

`SetThemeMode` keeps the accent of the last `SetAccent` for its presets until
`facet::ClearAccent()`; `SetTheme` shows the accent of the theme passed to it.
`facet::AccentPresets()` lists named accent choices for pickers.

### An accent for one region

```cpp
facet::PushAccent(facet::Rgb(0xC93400));
// ... Facet drawing in this region reads the orange accent from facet::T() ...
facet::PopAccent();
```

Between the two calls `facet::T()` returns the current theme with the pushed accent applied,
derived exactly as above. The current theme, the accent of `SetAccent` and the stock Dear ImGui
style stay as they are, and nothing animates. Like `SetAccent`, `PushAccent` takes an `ImVec4`
or a packed `ImU32`. Pushes nest up to `facet::kAccentStackDepth`
deep, and each depth has its own theme: a `const Theme&` taken from `facet::T()` inside a
scope keeps its accent while nested scopes come and go, and stays valid until the `PopAccent`
of its own scope or the next theme change. A `PopAccent` without a push, a push beyond that depth and an accent still pushed at
`facet::EndFrame` are reported through Dear ImGui's recoverable error path and the debugger
output; `EndFrame` then clears the stack, so the next frame starts without a scoped accent.

## Outlines and increased contrast

Every outline is one hairline (`metrics.hairline`, at least one physical pixel) in a border token:
`controlBorder` for the edge of a control or a tile, `separator` for dividing lines and the edge of
a surface. A state changes the color, not the width: `controlBorderHover` under the pointer, the
accent outline (`focusRing`) for a focused field or a selected tile, `destructive` for an error.
In the presets the outline is quiet: the fill shapes a control, the hairline only completes it.

```cpp
facet::SetIncreasedContrast(true);       // every outline and divider at 3:1
facet::SetFollowSystemContrast(true);    // or follow the Windows high contrast setting
```

While the variant is on (`facet::IncreasedContrast()`), `T()` resolves `separator`,
`controlBorder` and `controlBorderHover` to colors that reach 3:1 on every surface of the theme:
`separatorOpaque` mixed toward `label` until it does, the hover color a few steps further. Nothing
else changes. It applies to the presets, to every theme given to `SetTheme`, to each step of an
animated switch and inside `PushAccent` scopes, and `ApplyImGuiStyle` writes it into the stock
style. `IncreasedContrastEnabled()` is the application's switch, `SystemIncreasedContrast()` the
Windows setting (read about once per second), which counts only after
`SetFollowSystemContrast(true)`; following it is off by default.

`SetTheme` takes a theme with the standard border colors. A border color that comes back from
`T()` unchanged while the variant is on counts as the standard color it replaced, so
`facet::Theme t = facet::T(); t.radii.card = 10.0f; facet::SetTheme(t);` keeps the variant a switch
that can be turned off again.

## Contrast guarantees

The unit tests check, for both presets:

- `label` and `labelSecondary` reach 4.5:1 on every surface and on every fill over every surface.
  Text that must be read uses only these two (placeholders use `labelSecondary`).
  `labelTertiary` and `labelQuaternary` stay below 4.5:1 on purpose, for disabled text and
  decoration.
- In the dark preset the elevation surfaces get lighter with the level, because a black
  shadow cannot darken the black background; in the light preset they are all white.
- The status colors reach 4.5:1 as text on `bg`, `bgSecondary` and `bgTertiary`; `onDestructive`
  reaches 4.5:1 on every destructive state, `onSuccess` and `onWarning` on their fills; `label`
  and `labelSecondary` reach 4.5:1 on every wash (`accentSubtle` and the status washes) over the
  content surfaces, and the tone color itself 3:1 there.
- `accentTextVisited` and the code tints `codeKeyword`, `codeString` and `codeNumber` reach 4.5:1
  on every surface.
- For accents across 24 hues at five lightness levels, every accent state carries readable
  `onAccent` text, `accentText` is readable on every surface, and `focusRing` and every chart
  palette color reach 3:1 on every surface as drawn (8 bits per channel); `focusRing` also against
  `fill[0]` over every surface and against `accentSubtle` over every surface, so a filled track,
  an on switch and a selected tile's outline read at 3:1.
- The control outline is quiet on purpose: `controlBorder` stays between 1.25:1 and 1.8:1 over
  every surface, `controlBorderHover` above it and below 3:1, and `separator` below
  `controlBorder`. `facet::ControlBoundaryColor()` returns `controlBorder`. In the increased
  contrast variant (below) all three reach 3:1 on every surface, in both presets and for every
  accent preset.
- `scrollThumb` and `scrollThumbHover` reach 3:1 on every surface.
- The two `checker` cells differ by 1.2:1 to 2:1, so alpha shows without competing with the color
  over them, and both stay light in the light preset and dark in the dark one.

A custom theme with other surface colors should call `theme.ApplyAccent(theme.colors.accent)`
after changing them, because `accentText`, `focusRing` and the chart palette depend on the
surfaces.

## Stock Dear ImGui widgets

`facet::ApplyImGuiStyle()` writes the current theme and scale into the Dear ImGui style: every
color, the paddings, spacings, roundings and border sizes from the tokens (scaled), the font
scale factors and the Body text size as the base font size. A stock control is then exactly
one control height tall and has a hairline outline. Sizes without a token take the Dear ImGui
default times the scale. Settings that are not sizes and have no token keep their values:
anti-aliasing, tessellation, global and disabled alpha, hover delays and tooltip flags, text
and title alignment, the window menu and color button positions, tree line flags, angled
table header settings and the mouse cursor scale.

`facet::SyncImGuiStyle(true)` makes `facet::NewFrame` call it whenever the theme or the scale
changed, including every step of a theme transition. It is off by default, so a host with a
style of its own keeps it.

## Motion settings

`motion.scale` multiplies every duration, spring response and loading period. While
`facet::ReduceMotion()` is true, every animated value jumps to its target. It is true when the
theme's `motion.reduceMotion` is set or when the Windows setting "Animation effects" is off;
`facet::SetFollowSystemMotion(false)` ignores the Windows setting (the demo does this in its
capture and selftest runs so they render the same everywhere).
