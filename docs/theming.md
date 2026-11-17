# Theming

Everything Facet draws takes its colors, lengths, text sizes and timings from one value, the
current theme, returned by `facet::T()` (`include/facet/core/theme.h`). The token values and
the reason for each are listed in `DESIGN.md`.

## The theme value

`facet::Theme` is a plain struct of token groups:

| Group | Holds |
|---|---|
| `colors` | Surfaces (with one per elevation level, read with `ElevatedSurface(level)`), translucent fills, four label strengths, separators, the accent and its derived states, status colors, focus ring, shadow, glass tint, scrim |
| `spacing` | The 4 px grid: `xs` 4 to `xxxl` 32 |
| `radii` | `element`, `control`, `card`, `popover`, `window`, and where continuous corners start |
| `metrics` | Control heights, toggle, checkbox and slider sizes, focus ring width, hairline, minimum hit target |
| `type` | Size and weight of every `TextStyle` |
| `motion` | Springs, durations, loading periods, a global `scale` and `reduceMotion` |
| `elevation` | Three shadow levels (their surfaces are in `colors`) |
| `glass` | Backdrop blur settings |

Lengths are design units. Widgets convert them with `facet::Px()` (see `docs/scaling.md`).

`facet::Theme::Light()` and `facet::Theme::Dark()` return the presets. A custom theme is a
preset with some fields changed:

```cpp
facet::Theme theme = facet::Theme::Dark();
theme.radii.card = 10.0f;
theme.motion.scale = 1.5f;      // every duration and spring 1.5 times longer
facet::SetTheme(theme);
```

## Switching

```cpp
facet::SetTheme(theme);          // animated while Facet runs
facet::SetTheme(theme, false);   // at once
```

An animated switch lasts `motion.durationStandard * motion.scale` of the new theme. Facet
blends one step per `facet::NewFrame`: colors in OKLab, lengths and timings linearly, and
switches (`dark`, font weights, flags) change at the halfway point. `T()` returns the blend
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
groups of the current theme (spacing, radii, metrics, type, motion, glass) stay as the app set
them with `SetTheme`, so a reduced-motion or motion-scale setting survives a switch, including
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
- `focusRing`: the accent at 70 % (light) or 80 % (dark).

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
style stay as they are, and nothing animates. Pushes nest up to `facet::kAccentStackDepth`
deep, and each depth has its own theme: a `const Theme&` taken from `facet::T()` inside a
scope keeps its accent while nested scopes come and go, and stays valid until the `PopAccent`
of its own scope or the next theme change. A `PopAccent` without a push, a push beyond that depth and an accent still pushed at
`facet::EndFrame` are reported through Dear ImGui's recoverable error path and the debugger
output; `EndFrame` then clears the stack, so the next frame starts without a scoped accent.

## Contrast guarantees

The unit tests check, for both presets:

- `label` and `labelSecondary` reach 4.5:1 on every surface and on every fill over a surface.
  Text that must be read uses only these two (placeholders use `labelSecondary`).
  `labelTertiary` and `labelQuaternary` stay below 4.5:1 on purpose, for disabled text and
  decoration.
- In the dark preset the elevation surfaces get lighter with the level, because a black
  shadow cannot darken the black background; in the light preset they are all white.
- The status colors reach 4.5:1 as text on `bg`, `bgSecondary` and `bgTertiary`.
- For accents across 24 hues at five lightness levels, every accent state carries readable
  `onAccent` text and `accentText` is readable on every surface.
- `facet::ControlBoundaryColor()` reaches 3:1 on the content surfaces, for control outlines.

A custom theme with other surface colors should call `theme.ApplyAccent(theme.colors.accent)`
after changing them, because `accentText` depends on the surfaces.

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
