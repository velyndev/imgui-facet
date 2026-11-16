# Scaling

Facet lays out and draws in design units: a length in a theme token is the number of pixels
at scale 1. `facet::Px(units)` multiplies by the current scale (`include/facet/core/scale.h`):

```cpp
const float padding = facet::Px(facet::T().spacing.md);
const ImVec2 size = facet::Px(ImVec2(120.0f, facet::T().metrics.controlHeight));
```

## Where the scale comes from

`facet::Scale()` depends on the mode:

| Mode | Scale | For |
|---|---|---|
| `ScaleMode::FollowDpi` (default) | `DpiScale() * UiScale()` | Desktop apps: follows the monitor DPI |
| `ScaleMode::Manual` | `UiScale()` | Overlays where the host decides alone |
| `ScaleMode::FitDisplay` | `fit * UiScale()` | Menus that grow and shrink with the host's render size |

- `facet::SetDpiScale(dpi / 96.0f)` is called by the host. Facet never queries or changes the
  process DPI awareness, because it may run inside another application. Clamped to 0.5 to 8.
- `facet::SetUiScale(s)` is the user's preference, clamped to 0.5 to 4.
- In `FitDisplay`, `fit = min(width / 1920, height / 1080)` of `ImGui::GetIO().DisplaySize`,
  updated in every `facet::NewFrame` (at least 0.25).

A change of `Scale()` notifies the library modules (shadow and icon atlases are rasterized
per scale).

## Text

Text styles push a font at its design size (`facet::PushTextStyle`), and Dear ImGui scales it
through the style's font scale factors. While a Dear ImGui context exists, Facet keeps them
equal to the scale: `style.FontScaleDpi = Scale() / UiScale()` and
`style.FontScaleMain = UiScale()`. They are written whenever the scale changes and again in
every `facet::NewFrame`, so text and every `Px()` length always grow by the same factor.
`facet::ApplyImGuiStyle()` also scales the stock widget sizes (see `docs/theming.md`).

A host that sets the font scale itself, such as an overlay inside another application, calls
`facet::SetManageFontScale(false)`. Facet then leaves both factors alone, also in
`ApplyImGuiStyle`, and Facet text grows with whatever font scale the host sets. For text to
line up with `Px()` lengths, that font scale should equal `facet::Scale()`.

## Pixel snapping

At fractional scales such as 1.25 or 1.5, a `Px()` length rarely lands on a whole pixel. Thin
lines placed between pixels are drawn half bright over two rows and look blurred. The helpers:

| Helper | Use |
|---|---|
| `facet::Snap(p)` | Round a coordinate or a length (float or ImVec2) to whole pixels |
| `facet::SnapRect(min, max)` | Round both corners of a rectangle |
| `facet::PxSnapped(units)` | `Px()` rounded, never below 1 for a positive length: borders, gaps, insets |
| `facet::SnapStroke(p, thickness)` | The center line for a stroke of whole-pixel thickness, so it covers whole pixels |
| `facet::HairlineThickness()` | `Px(T().metrics.hairline)` rounded to whole pixels, at least 1 |
| `facet::HairlineH`, `facet::HairlineV` | Hairlines that start on the pixel grid |

Dear ImGui puts the pixel grid at whole coordinates: a 1 px line along x = 10.5 covers exactly
the pixel column 10.

## Testing at other scales

The demo renders any scale offscreen:

```
facet_demo --capture out --size 900x600 --scale 1.5 --warp
facet_demo --selftest --scale 2 --warp
```

The Draw page shows unsnapped and snapped hairlines side by side at the current scale.
