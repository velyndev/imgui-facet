# Design

Every visual choice in Facet with a one-line reason. Values are the starting tokens in
`include/facet/core/theme.h` and `src/core/theme.cpp`, in design units (pixels at scale 1).
Widgets read them from `facet::T()` and pass lengths through `facet::Px()`.

The direction is a restrained desktop look for tools and in-game overlay menus: low visual
energy, an even rhythm, and motion only where a state changes.

## Tokens

| Token group | Values | Reason |
|---|---|---|
| Spacing | 4 px grid: 4, 8, 12, 16, 20, 24, 32 | One rhythm for every gap and padding |
| Radii | element 6, control 8, card 12, popover 14, window 16; pill only for toggles and chips | Radius signals the layer; not everything is a pill |
| Corners | Continuous path from radius 10 up, circular arcs below | Large surfaces get a smooth corner join; small ones cannot show the difference |
| Control heights | small 22, regular 28, large 36 | Pointer-density sizes for desktop tools and menus |
| Toggle, checkbox, slider | toggle 38 x 22 with a 2 inset, checkbox 16, slider track 4 and knob 18 | Sized against the 28 control height so rows line up |
| Focus ring | 3 wide, accent at 70 % (light) or 80 % (dark) | Visible on every surface without hiding the control |
| Hairline | 1, at least one physical pixel | Separators stay crisp at every scale |
| Minimum hit target | 24 | Small visuals stay easy to click |
| Type | caption 11, footnote 12, callout 12, body 13, headline 13 semibold, title3 15 semibold, title2 17 semibold, title1 22 bold, large title 26 bold | Compact desktop scale with weight carrying the hierarchy |
| Motion | springs quick (0.25 s, damping 1), standard (0.35 s, 0.85), gentle (0.5 s, 1); durations 120 / 200 / 350 ms | Motion only signals a state change; the standard spring overshoots slightly for the selection highlight |
| Elevation | three shadow levels: offset 1 / 4 / 12, blur 3 / 16 / 40 | Shadows separate floating layers by height |
| Glass | blur radius 20, tint opacity 0.72, saturation 1.5 | Enough tint to keep text on the panel readable over any backdrop |

## Colors

| Token | Light | Dark | Reason |
|---|---|---|---|
| Surfaces bg, bgSecondary, bgTertiary, bgElevated | #FFFFFF, #F2F2F7, #E5E5EA, #FFFFFF | #000000, #1C1C1E, #2C2C2E, #3A3A3C | White and black bases with grey steps for grouping and depth |
| label, labelSecondary, labelTertiary, labelQuaternary | black at 100 / 60 / 30 / 16 % | white at 100 / 60 / 30 / 16 % | One text color in four strengths keeps every surface consistent |
| fill[0] to fill[3] | #787880 at 20 / 16 / 12 / 8 % | #787880 at 36 / 32 / 24 / 18 % | Translucent control fills work on every surface |
| Accent | #0066CC | #1F6FEB | One blue, dark enough in each theme for white text on it |
| Status: destructive, success, warning | #C4000F, #146C2E, #8A5300 | #FF6B5F, #32D74B, #FFD60A | Readable as text on the content surfaces |
| Elevation opacity | 0.10 / 0.12 / 0.18 | 0.36 / 0.44 / 0.56 | Shadows need more strength to show on dark surfaces |
| Scrim | black at 20 % | black at 48 % | Dims the content behind a modal layer without hiding it |

Contrast rules, checked by the unit tests:

- label and labelSecondary reach 4.5:1 on every surface; they are the only label tokens for
  text that must be read, placeholders included.
- labelTertiary is below 4.5:1 and is used only for disabled text and decorative glyphs.
- The accent is set at runtime. `Theme::ApplyAccent` derives the other accent colors in OKLCH:
  onAccent is white or black, whichever is more readable; hover and pressed move the lightness
  away from onAccent so text on them stays at 4.5:1 or more; accentText keeps the hue and moves
  the lightness until it reaches 4.5:1 on all four surfaces, for links and accent text.
- The accent is used only on selection, the primary action and focus.

## Core

No further choices recorded yet.

## Effects

Rules for where effects may appear: backdrop blur only on the menu shell and popovers;
shadows only on floating layers; glow only as the focus ring plus one opt-in accent glow.

No further choices recorded yet.

## Images

No choices recorded yet.

## Demo

- Until Facet widgets exist, the gallery maps the theme tokens onto the stock Dear ImGui
  style colors. Stock frames get a hairline border whose color is derived to reach 3:1 on the
  content surfaces, because translucent fills alone leave an unchecked checkbox nearly
  invisible on light surfaces.
