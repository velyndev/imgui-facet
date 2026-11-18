# Design

Every visual choice in Facet with a one-line reason. Values are the starting tokens in
`include/facet/core/theme.h` and `src/core/theme.cpp`, in design units (pixels at scale 1).
Widgets read them from `facet::T()` and pass lengths through `facet::Px()`.

This document sets the rules for the planned widgets as well as for the parts that exist;
README.md lists what exists today.

## Direction

Facet is a component framework and gallery for C++ developers who build desktop tools and
in-game overlay menus. The look is restrained and quiet: it should feel at home next to the
system's own windows and step back behind the content.

Three dials set the character, each from 1 (calm) to 3 (bold):

| Dial | Setting | Meaning |
|---|---|---|
| ENERGY | 1 | Low contrast between surfaces, one accent, no decoration that does not carry information |
| RHYTHM | 2 | An even 4 px grid with clear grouping; dense enough for tools, never cramped |
| MOTION | 2 | Short, soft motion that only marks a change of state; nothing loops for attention |

**Identity motif: the sliding selection pill.** One soft accent-tinted highlight sits behind
the selected entry and springs to the next one when the selection changes (standard spring).
Tabs, the sidebar, segmented controls and lists are to share this one gesture once they exist,
so it is recognisably Facet without any ornament; today the selection pill block in
`src/internal/selection_pill.h` draws it for the demo navigation and the prototypes on the
Selection pill page.

**Dose caps.** Effects are allowed only where they carry meaning:

- Backdrop blur: only on the floating menu shell and on popovers.
- Shadows: only on floating layers (popovers, menus, dialogs, the shell).
- Glow: only as the focus ring, plus one opt-in accent glow per screen.
- Accent: only on the selection, the primary action and keyboard focus.

Reduced motion follows the Windows "Animation effects" setting. Interactive items show the hand
cursor, disabled ones the not-allowed cursor.

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
| Motion springs | quick (0.25 s, damping 1), standard (0.35 s, 0.85), gentle (0.5 s, 1) | The standard spring overshoots slightly, which reads as a physical selection highlight |
| Motion durations | 120 / 200 / 350 ms | Motion only signals a state change, so it is over before the eye leaves the control |
| Loading periods | shimmer sweep 1.4 s, spinner turn 1 s | Slow enough to read as waiting, fast enough to show that something runs |
| Elevation | three shadow levels: offset 1 / 4 / 12, blur 3 / 16 / 40 | Shadows separate floating layers by height |
| Glass | blur radius 20, tint opacity 0.72, saturation 1.5 | Enough tint to keep text on the panel readable over any backdrop |
| Width classes | Compact below 400, Regular from 400, Expanded from 720 (available width) | Below 400 a label and a usable control no longer fit side by side; from 720 a side column fits next to the content |
| Labelled rows | label column 112 (gap included), narrowest control beside it 128 | Labels line up across rows; a row stacks before its control gets too narrow to operate |

## Colors

| Token | Light | Dark | Reason |
|---|---|---|---|
| Surfaces bg, bgSecondary, bgTertiary, bgElevated | #FFFFFF, #F2F2F7, #E5E5EA, #FFFFFF | #000000, #1C1C1E, #2C2C2E, #3A3A3C | White and black bases with grey steps for grouping and depth |
| bgElevatedLevels 1 / 2 / 3 | #FFFFFF for all three | #2C2C2E, #3A3A3C, #444446 | A black shadow cannot darken black, so in dark a higher layer is a lighter surface; level 2 is bgElevated |
| label, labelSecondary, labelTertiary, labelQuaternary | black at 100 / 60 / 30 / 16 % | white at 100 / 60 / 30 / 16 % | One text color in four strengths keeps every surface consistent |
| fill[0] to fill[3] | #787880 at 20 / 16 / 12 / 8 % | #787880 at 36 / 32 / 24 / 18 % | Translucent control fills work on every surface |
| separator | #3C3C43 at 29 % | #545458 at 60 % | A translucent hairline reads the same on every surface |
| separatorOpaque | #C6C6C8 | #38383A | Where a translucent line would double up (crossing table borders) |
| Accent | #0066CC | #1F6FEB | One blue, dark enough in each theme for white text on it |
| accentSubtle | the accent at 14 % | the accent at 24 % | A selection background that tints without competing with the text on it |
| Accent presets | blue, purple, pink, red, orange, yellow, green, graphite | the same | A short list of distinct hues; any of them derives readable states |
| Status: destructive, success, warning | #C4000F, #146C2E, #8A5300 | #FF6B5F, #32D74B, #FFD60A | Readable as text on the content surfaces |
| shadow | black | black | A neutral shadow; its strength comes from the elevation opacity |
| Elevation opacity | 0.10 / 0.12 / 0.18 | 0.36 / 0.44 / 0.56 | Dark surfaces need more strength for a shadow to show over bgSecondary and bgTertiary |
| glassTint | #FFFFFF | #1C1C1E | The panel tint matches the theme's base surfaces, so glass reads as a surface |
| Scrim | black at 20 % | black at 48 % | Dims the content behind a modal layer without hiding it |

Contrast rules, checked by the unit tests:

- label and labelSecondary reach 4.5:1 on every surface, the elevation surfaces included, and
  on every fill over a surface; they are the only label tokens for text that must be read,
  placeholders included.
- labelTertiary is below 4.5:1 and is used only for disabled text and decorative glyphs.
- The accent is set at runtime. `Theme::ApplyAccent` derives the other accent colors in OKLCH:
  onAccent is white or black, whichever is more readable; hover and pressed move the lightness
  away from onAccent so text on them stays at 4.5:1 or more; accentText keeps the hue and moves
  the lightness until it reaches 4.5:1 on every surface, for links and accent text. The tests
  check this for 24 hues at five lightness levels in both themes.
- Outlines of controls reach 3:1 (non-text contrast) on the content surfaces.

## Core

- **Continuous corners.** Each corner is a superellipse quadrant with exponent 3. Its curvature
  is zero where it meets the straight edge, so the edge flows into the corner without the kink
  a circular arc shows on large surfaces. It reaches 1.42 times the radius along each edge,
  which puts its middle point exactly where a circular arc of the same radius has it, so both
  look the same size. Corner points come from a unit table cached per segment count.
- **Hairlines** are whole pixels and start on the pixel grid at every scale, so separators never
  smear into two half-bright rows.
- **Theme switches** blend over the standard duration (200 ms) with an ease-in-out curve:
  colors in OKLab so the midpoint keeps an even lightness, lengths linearly; switches such as
  `dark` flip at the halfway point. A switch the user did not start (the Windows app mode
  changing) uses the same transition.
- **Motion** is solved in closed form for the elapsed time: exponential smoothing, damped
  springs and eased tweens. The same motion looks the same at 30 and at 240 frames per second,
  and a long frame never makes a spring explode. Values snap onto their target once the rest
  is too small to see, so screens come to rest.
- **Reduced motion**: every animated value jumps to its target, skeletons stop sweeping, and the
  spinner fades in and out in place instead of turning.
- **Fonts.** Segoe UI Variable is loaded once per weight from its named instances (Text
  Regular, Semibold, Bold; Display Semibold and Bold for the two largest styles, whose optical
  size is drawn for large text). Where it is missing or its instances cannot be selected, the
  static Segoe UI files are used. Nothing is bundled.
- **Stock Dear ImGui widgets** follow the theme through `facet::ApplyImGuiStyle`: frame padding
  makes a stock control exactly one control height, frames get a hairline outline of 3:1, and
  the scrollbar is a pill.
- **Loading states.** A skeleton is a fill[2] block with a fill[3] band sweeping left to right;
  every skeleton on screen sweeps in step from one shared clock, so one screen has one loading
  language.

## Blocks

The building blocks in `src/internal/` that widgets are made of. The choices behind each:

- **Hit target.** The hit rectangle grows to minHitTarget around small visuals and yields to
  any neighbour whose visible rectangle is under the pointer; Dear ImGui still reports the
  visible rectangle.
- **Focus ring.** One hairline of surface between control and ring, concentric corners in the
  control's own corner shape (circular arcs around knobs), focusRing width, a soft halo,
  faded over durationFast; shown only after keyboard or gamepad input, because a mouse user
  already knows where they clicked.
- **Ring color.** focusRing when it reaches 3:1 on every surface, otherwise accentText, so the
  ring is visible with every accent preset.
- **Selection pill motion.** The leading edge uses the standard spring at 0.7 of its response,
  so the pill stretches slightly and settles; it animates relative to the group's content so
  scrolling never animates it.
- **Selection pill label.** The selected entry's label is semibold, because the pill tint is
  soft by design and selection must not rest on color alone.
- **Row labels.** Labels in the control's text style on the control text baseline, so label
  and value read as one line.
- **Row groups.** A group shares its widest label as the column, capped at 40 % of the row, so
  one long label cannot squeeze every control.
- **Reset slot.** A group reserves the reset slot in every row when one row is resettable, so
  the controls end in one line; the slot stays reserved while nothing is modified, so the
  control does not jump when the reset button appears.
- **Modified marker.** A labelSecondary dot after the label marks a changed setting, a shape
  rather than a color change, so the meaning does not rest on color alone.
- **Cut row labels.** A cut label shows its full text as a tooltip on hover and, under the
  label, on keyboard focus of the row's control, so keyboard users can read it too.
- **Row texts.** Description under the label, help text under the control, and an error
  replaces the help text with the error icon in destructive.
- **Row descriptions.** A description wraps across the control column when nothing sits under
  the control, so a short label does not squeeze it into a narrow column beside empty space.
- **Row icons.** A row's leading icon is labelSecondary, because the label carries the meaning
  and the icon only helps find the row; a group keeps the icon room in every row so the label
  texts start in one line.
- **Disabled rows.** A disabled row draws its label, marker, description and help text in
  labelTertiary; an error keeps destructive, because it still needs to be read.
- **Series colors.** Hues step around the OKLCH circle from the accent hue, neighbours far
  apart, and each color moves in lightness until it reaches 3:1 on every surface, so lines
  and areas stay distinguishable from the background with any accent.
- **Plot edges** sit on whole pixels, so clip edges and grid lines stay crisp at every scale.

### Prototype controls

The Blocks pages operate the blocks through prototype controls in `demo/`. They are not the
Phase 2 widgets; their look is the starting point for them:

- **Fills** step fill[2], fill[1], fill[0] for rest, hover and press with a 3:1 boundary
  hairline, so the states differ in more than one subtle shade and the edge stays visible.
- **Tracks.** The unfilled slider track is in the 3:1 boundary color so the range ends stay
  visible.
- **Knobs** are bg with a 3:1 outline and grow 15 % while held, which confirms the grab.
- **Span.** A range's filled span thickens by spacing.xs while a press would grab it or it is
  dragged, so the user sees that the whole span moves.
- **Marks.** Step ticks (including an off-grid max) and snap stops are labelSecondary dots.
- **Read-only** fills in labelSecondary, so a value that cannot change does not look like an
  accent selection.

## Effects

The dose caps in Direction apply. The choices behind each effect:

| Choice | Reason |
|---|---|
| A shadow's blur is two standard deviations of the Gaussian | The token then says how far beyond the shape the shadow visibly ends |
| Shadows are baked once per corner radius (whole pixels) and blur (half pixels) and drawn as 9-slices | One small tile serves every size of a shape; the quads batch with the rest of the drawing |
| Tiles are white with the shadow in alpha | The vertex color sets the shadow color and strength without a new tile |
| Tiles are baked in physical pixels and rebuilt after a scale change | Soft edges stay as smooth at 200 % as at 100 % |
| Glow is cut out inside its shape | It can sit on top of translucent fills without tinting them |
| In dark, height is carried by the elevation surface as well as the shadow | Over the black background only the lighter surface can show which layer is higher |
| Glass copies the render target instead of reading it | Host swap chains rarely allow reading, and multisampled targets need a resolve anyway |
| The blur halves and doubles the image a few times (dual filter) | A wide, smooth blur at a small, fixed cost per pixel |
| The blur works in 16-bit float | Wide gradients show no banding, and sRGB targets are blurred in linear light |
| Saturation is raised in the last blur pass | The tint washes colors out; a little extra saturation keeps the backdrop recognisable |
| One blur per frame and render target | Several panels cost one blur; draw every backdrop before the first panel |
| The fallback is the tint at full opacity | Text on the panel stays readable when nothing can be blurred, and a failure mid-frame looks the same |

## Images

- **Icon grid.** Icons are drawn on a 16 unit grid with 1.5 unit strokes, round caps and joins,
  and a 1 unit margin. One stroke weight and one corner style make glyphs from different
  places read as one set; the margin keeps them from touching the edges of buttons and fields.
- **Icon pixels.** Icons are rasterized at the whole pixel size they are drawn at and placed on
  whole pixels, so a 16 unit icon is never a scaled copy of a 24 unit one and stays sharp at
  every UI scale.
- **Icon color.** Icons default to the label color; a tint replaces it. Accent-colored icons use
  accentText, which reaches 4.5:1 on every surface.
- **Loading image.** An image that is still loading is a skeleton (the same sweep and shared
  clock as every other skeleton) with a faint picture glyph in labelTertiary, which tells an
  image slot apart from a text placeholder.
- **Failed image.** It shows the image-off glyph and a short label in labelSecondary on fill[3]
  with a separator hairline, so the empty slot is explained instead of looking like a layout
  gap.
- **Image corners** follow FillRect, so images and cards with the same radius token have the
  same corner shape.

## Demo

- **Stock controls.** The gallery maps the theme tokens onto the stock Dear ImGui style with
  `facet::ApplyImGuiStyle`, so the stock controls used for page settings follow the theme and
  the scale.
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
- **Pages.** The window background with 24 / 20 padding; every page starts with a Title1
  heading and a one-paragraph summary in labelSecondary, sections with a Title3 heading.
- **Settings rows.** A page labels each setting in one fixed label column in footnote
  labelSecondary, with the control after it, so option rows line up; in narrow windows the
  label moves above its control. The Blocks pages use the row block instead (see Blocks), the
  other pages keep this simpler row, so the Foundations and Media pages stay on stock controls.
- **Samples.** Shapes that only show a token or an effect carry no text and no knob, and their
  caption names them, so nothing that cannot be operated looks like a control.
- **Planned widgets.** A declared widget without its implementation draws a box at the size the
  widget will take with "Name: planned" in footnote labelSecondary; it is not focusable, so a
  page of placeholders never looks operable. Controls and content widgets draw a fill[3] box
  with a separator hairline in the control radius; structure widgets (lists, navigation,
  containers, layout, overlays) draw a dashed separator hairline with square corners, so a
  container placeholder reads as a frame around the caller's content.
- **Narrow placeholders.** A narrow controls placeholder drops the label part first, then the
  widget name, before the word "planned" is shortened, so the box still says that it is a
  placeholder.
- **Field messages.** The FieldValidation placeholder is spacing.xs taller than its text, so its
  hairline never crosses the text.
- **Placeholder sizes.** Pads, cell rows and the inline calendar shrink to the available width
  by default, so a placeholder never runs under the scrollbar; content placeholders are at
  least a small control tall, so a box around one line of text does not cut its descenders.
