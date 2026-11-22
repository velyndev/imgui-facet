# Design

Every visual choice in Facet with a one-line reason. Values are the starting tokens in
`include/facet/core/theme.h` and `src/core/theme.cpp`, in design units (pixels at scale 1).
Widgets read them from `facet::T()` and pass lengths through `facet::Px()`.

This document sets the rules for every part of Facet; README.md lists what exists today.

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
Tabs, the sidebar, segmented controls and lists share this one gesture, so it is recognisably
Facet without any ornament; today the selection pill block in `src/internal/selection_pill.h`
draws it for segmented controls, toggle button groups, choice chips, the key bind mode
selector, the lists, tab bars, the sidebar, pagination, the time picker columns, the demo
navigation and the block prototypes.

**Dose caps.** Effects are allowed only where they carry meaning:

- Backdrop blur: only on the floating menu shell and on popovers.
- Shadows: only on floating layers (popovers, menus, dialogs, the shell) and the level 1 shadow
  under slider and toggle knobs, which lifts the one part of a control the pointer grabs.
- Glow: only one opt-in accent glow per screen; the focus ring is a plain stroke.
- Accent: only on the selection, the primary action and keyboard focus.

## Quiet layers

A control is shaped by its fill and one faint hairline, never by a light outline. Depth in the
dark theme comes from near-black greys stepping up from black. Color and weight are spent on state
(the selection, the primary action, keyboard focus), not on edges. The rules, each with its reason:

- **One border rule.** Every outline is one hairline (`metrics.hairline`, at least one physical
  pixel) in a border token: `controlBorder` for the edge of a control or a tile, `separator` for
  dividing lines and the edge of a surface. A state changes the color or the fill, never the
  width: hover takes `controlBorderHover`, a focused field and a selected tile the accent outline
  (`focusRing`, the accent at 3:1), an error `destructive`. Reason: one stroke weight everywhere
  reads as one product, and a weight that changes with state reads as a layout jump.
  `src/internal/control.h` holds the helpers (`ControlOutlineColor`, `DrawOutline`,
  `DrawSurfaceOutline`) and `tests/test_border_rule.cpp` scans the sources for any other width.
- **Strokes that are not outlines** (chart lines, glyphs, check marks, slider tracks, the arcs of
  knobs and gauges, the focus ring) keep their own weights, each from a named token. Reason: they
  carry data or state, not an edge.
- **The focus ring** is two units wide, one hairline outside the control, without a halo, and it
  shows only while the keyboard modality is on. Inside lists, menus, tables and grids of tiles it
  sits inside the row, cell or tile. Reason: a keyboard user needs one clear mark, a pointer user
  needs none, and a ring inside a row is never cut by the list edge nor pushes the layout.
- **Rows, cells and tiles.** The row under the pointer and the current row show `fill[2]`; a
  selected row shows `accentSubtle` (plus its check mark); a selected tile or card adds the accent
  outline at the one border width. No state draws a thicker stroke. Reason: rows are scanned in
  quantity, so their state must read from a fill, not from a frame around each of them.
- **Capsules.** A radius of half the shorter side or more gives a capsule with circular ends, in
  the shared fill and stroke helpers, whatever the corner settings say. Reason: the continuous
  corner flattens a pill's ends into a rounded rectangle, which reads as a mistake on a switch.
- **Knobs and tracks.** Slider and toggle knobs are solid discs in `knob` (white in both themes)
  with the `controlBorder` hairline and the level 1 shadow; the unfilled track is `fill[0]` and
  the filled part the accent mark (`focusRing`), 3:1 against both. Reason: a solid knob reads as
  the thing to grab; a hollow ring with a light outline reads as a frame.
- **Hover only on controls.** A part the pointer can drag that is not a control with a look of its
  own (the span between range handles) shows no hover change; only the handles do. Reason: motion
  signals a state change of a control, and a span that swells under the pointer signals nothing.
- **The dark ramp.** bg `#000000`, bgSecondary `#111113`, bgTertiary `#19191C`, elevated
  `#18181B` / `#1E1E21` / `#26262A`; fills and edges are white at low opacity. Reason: steps of
  near-black separate layers without any light grey edge, and a floating surface shows its height
  through its lighter grey and its separator hairline, because a shadow cannot darken black.
- **Quiet outlines, increased contrast on request.** In the standard presets a control's outline
  stays below 3:1 on purpose; text keeps 4.5:1 and the focus ring and the marks that carry a state
  keep 3:1. `SetIncreasedContrast(true)` (or the Windows high contrast setting, while the
  application follows it with `SetFollowSystemContrast`) resolves `controlBorder`,
  `controlBorderHover` and `separator` to colors that reach 3:1 on every surface. Reason: a 3:1
  outline on every control is the light grey frame this look removes, and the users who need it
  get it from one switch.
- **The chosen value of a popup list** (a dropdown, a combo) shows the `accentSubtle` row fill with
  its check mark, like a selected row of a list box. Reason: the same state reads the same in every
  list, open or inline.
- **One grab face.** Slider knobs and color handles share one face: a white disc with the
  `controlBorder` hairline and the level 1 shadow; a color handle shows its color inside a white
  rim, and on a light color under it the hairline turns black. Reason: one grab shape on every
  track and plane; the handle position is the value, so its edge keeps 3:1 against any color under it.

Reduced motion follows the Windows "Animation effects" setting. Interactive items show the hand
cursor, disabled ones the not-allowed cursor.

## Tokens

| Token group | Values | Reason |
|---|---|---|
| Spacing | 4 px grid: 4, 8, 12, 16, 20, 24, 32 | One rhythm for every gap and padding |
| Radii | element 6, control 8, card 12, popover 14, window 16; a capsule (half the shorter side) for toggle tracks, chips, badges, status pills, scroll thumbs and thin tracks | Radius signals the layer; not everything is a capsule |
| Corners | Continuous path from radius 10 up, circular arcs below and for every capsule | Large surfaces get a smooth corner join; small ones cannot show the difference; a capsule needs true half circles |
| Control heights | small 22, regular 28, large 36 | Pointer-density sizes for desktop tools and menus |
| Toggle, checkbox, slider | toggle 38 x 22 with a 2 inset, checkbox 16, slider track 4 and knob 18 | Sized against the 28 control height so rows line up |
| Focus ring | 2 wide, one hairline outside the control (inside rows, cells and tiles), in the focusRing color, no halo | Visible on every surface without hiding the control or competing with the content |
| Hairline | 1, at least one physical pixel; the width of every outline and divider | One stroke weight; outlines and separators stay crisp at every scale |
| Minimum hit target | 24 | Small visuals stay easy to click |
| Type | caption 11, footnote 12, callout 12, body 13, headline 13 semibold, title3 15 semibold, title2 17 semibold, title1 22 bold, large title 26 bold | Compact desktop scale with weight carrying the hierarchy |
| Motion springs | quick (0.25 s, damping 1), standard (0.35 s, 0.85), gentle (0.5 s, 1) | The standard spring overshoots slightly, which reads as a physical selection highlight |
| Motion durations | 120 / 200 / 350 ms | Motion only signals a state change, so it is over before the eye leaves the control |
| Loading periods | shimmer sweep 1.4 s, spinner turn 1 s | Slow enough to read as waiting, fast enough to show that something runs |
| Elevation | three shadow levels: offset 1 / 4 / 12, blur 3 / 16 / 40 | Shadows separate floating layers by height |
| Glass | blur radius 20, tint opacity 0.72, saturation 1.5 | Enough tint to keep text on the panel readable over any backdrop |
| Width classes | Compact below 400, Regular from 400, Expanded from 720 (available width) | Below 400 a label and a usable control no longer fit side by side; from 720 a side column fits next to the content |
| Labelled rows | label column 112 (gap included), narrowest control beside it 128 | Labels line up across rows; a row stacks before its control gets too narrow to operate |
| Larger controls | fader 140, pad 192, picker 256, number field beside a slider 72, histogram 40, chart 160, panel 192, card 256, thumbnail 128, inventory slot 64; knob arc 270 degrees | Multiples of the 4 px grid, most of them whole steps of the largest spacing (32), so large controls line up with the content around them; the knob arc leaves its gap at the bottom where the value reads |
| Popup placement | 4 from the anchor, 8 from the display edges | Close enough to read as attached, clear of the anchor's outline; never touching the display edge |
| Menus | at least 180 wide, submenus open after 0.2 s of rest, 24 between a label and its shortcut | Short menu labels still get a target that is easy to hit; a pointer crossing a row on its way does not open its submenu; the shortcut column reads as separate from the longest label |
| Tabs | 64 to 240 wide | A one-word tab stays an easy target; one long title cannot push the other tabs into the overflow menu |
| Tables | header 32, columns at least 48, resize band 8 | The header sits on the regular list row rhythm; a column keeps room for a short value and its sort mark; the resize band is two grid steps, easy to grab without covering the cell |
| Date pickers | day cell 32 | Seven days fit the 256 picker width with padding, and each day is above the minimum hit target |
| Tooltips | 0.5 s delay, 0.4 s grace, text wraps at 280 | The first tooltip waits until the pointer rests; moving on to a neighbour shows its tooltip at once; long text stays a readable column |
| Toasts | 5 s, 320 wide, 8 apart, 16 from the corner, 3 at once | Long enough to read two lines, few enough that the stack never covers the content it reports on |
| Dialogs and drawers | modal 440, drawer 320 wide or 280 high, command palette 560 | Wide enough for a title and two buttons in one row, narrow enough to read as a layer over the content |
| Shell | first size 420 x 520, smallest 280 x 160, title bar 40, resize band 6 | A settings menu fits without scrolling at the first size; at the smallest the title bar and one control row remain |
| Side columns | sidebar 220, icon rail 52, inspector 280 (at least 200) | Room for an icon and a short label; the rail keeps the icon on the minimum hit target with a margin |
| Scrollbars | 6 wide at rest, 10 while hovered or dragged, 2 inset, thumb at least 24, hidden after 1 s idle, edge fade 16, 64 per wheel notch | A thin bar leaves the content its width; it grows where the pointer needs a target, and the thumb never gets shorter than the minimum hit target |
| List rows | small 24, regular 32, large 40; indent 16 per tree level; dropdowns show 8 rows, inline lists 6; typeahead resets after 1 s | A list is scanned row by row and gets one grid step more air than a form row; 8 rows show a choice without hiding the field above |
| Drag and drop | starts after 4, preview at 70 %, edge scroll in a 32 band at up to 800 per second, a resting drag opens a target after 0.6 s | A small slip stays a click; the preview shows what moves without hiding where it lands; scrolling speeds up toward the edge so the user controls it |
| Text fields | 8 inner padding, caret 1.5 rounded to whole pixels | Text lines up with the labels of other controls; the caret stays one or two crisp pixels at every scale |
| Popup motion | open 150 ms from 96 % size, close 100 ms | A popup grows out of its anchor, which shows where it came from; it leaves faster than it came so the next action is never kept waiting |
| Status dots | 8, page dots 6 | A status dot reads at a glance next to Body text; page dots are a step smaller because they only count, they do not report a state |
| Slider marks | ticks 4, the track height, in labelTertiary | A tick as thick as the track reads as part of it, not as a separate control; labelTertiary keeps the ticks below the value mark |
| Segment inset | 2, the toggle knob inset | The selection pill sits inside the track the same way a toggle knob sits in its track |
| Progress tracks | small 4, regular 6, large 8 | Small equals the slider track so a strength meter under a field matches the sliders; each size adds one half grid step |
| State icons | 32 above an empty or error state | One grid step of the largest spacing: big enough to read as the subject of the empty area, small enough not to become an illustration |
| Status bar | 24 high | The small list row height, so status segments line up with small controls |
| Feedback hold | 1.5 s | A copied check stays long enough to be seen after the pointer moves away, short enough that it does not linger as a stale state |
| Live pulse | 2 s per pulse, still with reduced motion | Slower than the spinner, so a live dot reads as calm presence and not as work in progress |
| Dividers and panels | divider band 8, panel header 32 | The divider band is the table resize band, so every drag edge is grabbed the same way; a panel header sits on the regular list row rhythm |
| Popover arrow and palette | arrow 8 deep, command palette 96 from the top | One spacing step points at the anchor without reaching it; the palette sits where the eye starts reading, clear of a title bar |
| Flicks and inertia | a release faster than 600 per second is a flick, a released pan keeps its speed for 0.3 s | Fast enough that a slow drag is never read as a flick; the pan comes to rest within a standard duration and a half, so it never drifts on |
| Tab underline and steps | underline 2, step marker 24, connector 2 | Twice the hairline reads as a mark, not as a separator; a step marker holds one digit at the minimum hit target |
| Checkerboard | 6 per cell | Small enough that a swatch one control height wide shows two rows, large enough to read as a pattern at scale 1 |
| Color pickers | color bars 12, hue ring 20, time columns 48 | A bar thinner than the slider knob so the knob overlaps it as on a slider track; the ring wide enough to aim at a hue; a time column holds two digits with padding |
| Charts | lines 2, markers 6 (the page dot), bars 2 apart, legend swatch 10, donut hole 60 % of the diameter, gauge arc 12, meter segments 2 apart, peak hold 1 s | A 2 line reads at 3:1 contrast where a hairline would vanish; markers and swatches stay smaller than the text beside them; the gauge arc matches the color bars; a peak is held long enough to read its level |
| Media | grouped avatars overlap by a quarter, playhead 2, drop zone dashes 6 with gaps of 4 | A quarter keeps each face recognisable while the group stays compact; the playhead matches the chart lines; the dashes read as a drop target, not as the dashed outline of a planned placeholder |
| Editor handles | curve and easing points 10, gradient stops 14, a stop dragged 32 off the bar is removed, the easing preview runs 1 s | A point larger than a chart marker reads as something to grab while neighbours stay apart; a stop is wide enough to show its own color; the remove distance is one large step, so a slip never deletes a stop; the preview is slow enough to follow the curve with the eye |
| Property grid, search and key binds | label column at least 64, search field at least 160 beside the chips, chord columns 160 | The label column keeps a short name readable after the user drags it narrow; a search field narrower than a row control would hide the query, so the chips move under it; a chord column holds a two-modifier chord in key caps |
| Inventory and media cards | category bar 4, cover 16:9 (its height 0.5625 of the width), a card below 192 wide puts the cover beside the text | The category bar has the slider track height, so it reads as a color band and not as a hairline; the common video aspect keeps covers from being cropped; below the pad width a cover above the text leaves the title no room |
| Preview canvas | snap within 6, resize handles 8 | The snap band is wider than the drag start threshold, so an element snaps only when the user aims at a guide; handles twice the track height are visible on the element's outline and grow to the minimum hit target |

## Colors

| Token | Light | Dark | Reason |
|---|---|---|---|
| Surfaces bg, bgSecondary, bgTertiary, bgElevated | #FFFFFF, #F2F2F7, #E5E5EA, #FFFFFF | #000000, #111113, #19191C, #1E1E21 | White and black bases; in dark, near-black greys step up from black, so layers separate without a light edge |
| bgElevatedLevels 1 / 2 / 3 | #FFFFFF for all three | #18181B, #1E1E21, #26262A | A black shadow cannot darken black, so in dark a higher layer is a lighter surface; level 2 is bgElevated |
| label, labelSecondary, labelTertiary, labelQuaternary | black at 100 / 60 / 30 / 16 % | white at 100 / 60 / 32 / 18 % | One text color in four strengths keeps every surface consistent |
| fill[0] to fill[3] | #787880 at 20 / 16 / 12 / 8 % | white at 14 / 10 / 7 / 4 % | Translucent control fills work on every surface; in dark, white at low opacity keeps the greys neutral on the near-black ramp |
| separator | black at 10 % | white at 8 % | A translucent hairline reads the same on every surface, quieter than a control's edge |
| separatorOpaque | #E5E5E7 | #242426 | Where a translucent line would double up (crossing table borders) |
| controlBorder, controlBorderHover | black at 14 / 22 % | white at 14 / 22 % | The one faint hairline that completes a control's fill (`ControlBoundaryColor` returns it, and "boundary" below means it); hover strengthens the color, never the width; 3:1 only in the increased contrast variant |
| knob | #FFFFFF | #FFFFFF | A solid knob reads as the part to grab in both themes; its hairline and shadow set it off a light track |
| Accent | #0066CC | #1F6FEB | One blue, dark enough in each theme for white text on it |
| accentSubtle | the accent at 14 % | the accent at 24 % | A selection background that tints without competing with the text on it |
| focusRing | the accent hue, lightness moved to 3:1 on every surface and against fill[0] over each | the same | The ring sits on whatever surface holds the control, so it needs the non-text contrast on all of them; the same color marks a filled track, an on switch and a selected tile next to fill[0]; an accent that already has it is kept as it is |
| Chart palette (chart[0] to chart[7]) | eight hues from the accent hue, chroma 0.14, lightness from 0.56, moved to 3:1 | the same from lightness 0.72 | Neighbouring series differ by at least 90 degrees of hue, and every series stays visible on every surface for any accent |
| scrollThumb, scrollThumbHover | black at 45 / 60 % | white at 45 / 62 % | The scroll position reads at 3:1 on every surface, and the thumb gets stronger where the pointer is |
| Accent presets | blue, purple, pink, red, orange, yellow, green, graphite | the same | A short list of distinct hues; any of them derives readable states |
| Status: destructive, success, warning | #C4000F, #146C2E, #8A5300 | #FF6B5F, #32D74B, #FFD60A | Readable as text on the content surfaces |
| onDestructive, destructiveHover, destructivePressed | white on destructive; hover and pressed 0.04 and 0.08 darker | black on destructive; hover and pressed lighter | The same derivation as the accent states, so a destructive button behaves like a primary one and its text stays at 4.5:1 |
| onSuccess, onWarning | white, white | black, black | Whichever of white or black reads better on the solid fill |
| destructiveSubtle, successSubtle, warningSubtle | the status color at 14 % | the status color at 14 % | Enough to tint the surface; the dark accentSubtle strength (24 %) would take labelSecondary below 4.5:1 on the bright dark warning, so the washes keep 14 % in both themes; the tone glyph keeps 3:1 on them |
| accentTextVisited | accentText with its hue turned 60 degrees, lightness moved to 4.5:1 | the same | A visited link stays a link (same chroma and strength) but differs in hue from an unvisited one |
| Code tints keyword, string, number | #7A1FA2, #1A6B2B, #9A4A00 | #D7A8FF, #7EE08F, #FFB86B | Three hues far apart that reach 4.5:1 on every surface; comments use labelSecondary, so code never needs a color that is hard to read |
| checker[0], checker[1] | #FFFFFF, #D1D1D6 | #2A2A2D, #19191C | Two greys of the theme's own surfaces, 1.2:1 to 1.5:1 apart: alpha shows on them, and they never compete with the color drawn over them |
| shadow | black | black | A neutral shadow; its strength comes from the elevation opacity |
| Elevation opacity | 0.10 / 0.12 / 0.18 | 0.36 / 0.44 / 0.56 | Dark surfaces need more strength for a shadow to show over bgSecondary and bgTertiary |
| glassTint | #FFFFFF | #111113 | The panel tint matches the theme's base surfaces, so glass reads as a surface |
| Scrim | black at 20 % | black at 55 % | Dims the content behind a modal layer without hiding it; dark needs more because the content under it is already dark |

Contrast rules, checked by the unit tests:

- label and labelSecondary reach 4.5:1 on every surface, the elevation surfaces included, and
  on every fill over every surface; they are the only label tokens for text that must be read,
  placeholders included.
- labelTertiary is below 4.5:1 and is used only for disabled text and decorative glyphs.
- The accent is set at runtime. `Theme::ApplyAccent` derives the other accent colors in OKLCH:
  onAccent is white or black, whichever is more readable; hover and pressed move the lightness
  away from onAccent so text on them stays at 4.5:1 or more; accentText keeps the hue and moves
  the lightness until it reaches 4.5:1 on every surface, for links and accent text; focusRing
  and the chart palette do the same for 3:1, measured as drawn (8 bits per channel). The tests
  check this for 24 hues at five lightness levels in both themes.
- The focus ring and the marks that carry a state (a check mark on its fill, the filled part of a
  track, an on switch against an off one, the scroll thumb, the accent outline of a selected tile)
  reach 3:1.
- The outline of a control at rest is quiet on purpose: in the standard presets controlBorder
  stays between 1.25:1 and 1.8:1 over every surface (the tests lock both ends), and a divider
  stays below it. In the increased contrast variant controlBorder, controlBorderHover and
  separator reach 3:1 on every surface, in both themes and for every accent preset.

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
  makes a stock control exactly one control height, frames get the controlBorder hairline (3:1 in
  the increased contrast variant), and the scrollbar is a capsule.
- **Loading states.** A skeleton is a fill[2] block with a fill[3] band sweeping left to right;
  every skeleton on screen sweeps in step from one shared clock, so one screen has one loading
  language.

## Blocks

The building blocks in `src/internal/` that widgets are made of. The choices behind each:

- **Hit target.** The hit rectangle grows to minHitTarget around small visuals and yields to
  any neighbour whose visible rectangle is under the pointer; Dear ImGui still reports the
  visible rectangle.
- **Focus ring.** One hairline of surface between control and ring, concentric corners in the
  control's own corner shape (circular arcs around knobs), focusRing width, no halo, faded over
  durationFast; shown only after keyboard or gamepad input, because a mouse user already knows
  where they clicked. Rows, cells and tiles draw it inside their bounds (`DrawFocusRingInside`).
- **No ring from the pointer.** The current row of an open dropdown, a completion list or the
  command palette follows the pointer, and such a row shows its fill only; it shows the ring when
  the keys put it there while the keyboard modality is on (a list opened with Enter, an arrow
  press), because a ring that follows the mouse tells the pointer user what they already see.
- **Ring color.** focusRing, which reaches 3:1 on every surface for any accent (see Colors), so
  the ring is visible with every accent preset and every custom accent.
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
- **Series colors.** The first eight are the chart palette of the theme (see Colors); later
  series repeat its hues further from the surfaces in lightness, and every color is checked
  again for 3:1, so lines and areas stay distinguishable from the background with any accent.
- **Plot edges** sit on whole pixels, so clip edges and grid lines stay crisp at every scale.

Popup host:

- **Surfaces by kind.** Menus, lists and popovers take the popover radius and elevation 2,
  dialogs the window radius and elevation 3, tooltips the control radius and elevation 1; the
  height of the layer sets its shadow and its surface, and every floating layer has the separator
  hairline, which with the lighter grey shows height over black, where a shadow cannot.
- **Padding by kind.** spacing.xs for menus and lists, so rows run almost edge to edge;
  spacing.xl for dialogs; spacing.sm by spacing.xs for tooltips.
- **Open and close motion.** A level enters with ease-out over popupOpen and leaves with ease-in
  over popupClose, growing from the edge that faces its anchor and sliding anchorGap toward it,
  so the layer reads as coming from its control.
- **Tooltip at the pointer.** It sits after and below the cursor by spacing.lg and spacing.xl,
  so the cursor never covers the text.
- **Little room.** A level with less than two control heights of room beside its anchor covers
  the anchor instead of shrinking further, so it always shows at least two rows.
- **Sticky side.** A level keeps its side while only its content changes, so a filtered list
  does not jump between above and below.
- **Toward its window.** A level opened from the page aligned at one end of its anchor turns to
  the other end when only that keeps it over the window that holds the anchor, so a "more" button
  at the start of a narrow toolbar opens its menu over the toolbar and not over the navigation
  beside it.
- **Scrim.** The host draws the modal scrim from colors.scrim and fades it with the dialog;
  Dear ImGui's own dim is not used, so the dim and the dialog arrive together.
- **Rows in menus** draw the focus ring inside the row, because the menu padding is narrower
  than the ring, and take the shared row look (`DrawListRowFill`), so a menu row and a list row
  under the pointer look the same.
- **Menu row pitch.** Menu rows take the list row height and meet without a gap, the same as the
  rows of an open list, so the pointer moving down a menu is always on a row and the highlight
  never blinks off between two rows.
- **Menu shortcuts** sit right-aligned at the row's end in labelSecondary, so the labels stay
  the first thing read and every shortcut lines up in one column.
- **Menu leading slot.** An icon (labelSecondary) or a check mark sits in a slot that every row of
  the level keeps once one row uses it, so the labels start in one line. The check is accentText,
  the same as in list rows, because it marks the current choice and the accent is for selection.

Scroll core:

- **Overlay bar.** A thumb in scrollThumb and scrollThumbHover with radii.element (at most half
  its thickness); the fill[3] track shows only while the bar is wide. The position stays
  visible without taking content width, and the track appears only where the pointer aims.
- **Edge fades.** A gradient of the surface color over scroll.edgeFade whose strength grows with
  the content left on that side, so the fade eases in instead of popping. A surface pushed by the
  page stays with the page: an area inside a popup fades into the popup surface, never into a
  dark band of the page behind it.
- **Track paging.** A press on the track pages by the visible length less one edge fade, so the
  line at the old edge stays in view.
- **Wheel gestures** stay with the window they scroll while the pointer rests, so content
  sliding under the pointer, a focused slider included, does not take the wheel over.

List core:

- **Rows.** The row look of `list_view.h` (`ListRowLook`, `DrawListRowFill`): hover fill[2], the
  selection accentSubtle whether or not the list has the focus, so a selection never changes its
  look when the focus moves on; selected labels semibold; rows inset one hairline top and bottom
  (`ListRowFillRect`) with radii.control, so adjacent selected rows stay separate.
- **Row focus ring** is drawn after all rows, inside the row's fill, so neighbours never cover it
  and the list edge never cuts it.
- **Active row of a driven list** (the open list of a field that keeps the keyboard) takes the
  hover fill, because it is where Enter goes; it takes the ring only when the keys moved it there.
- **Trees.** chevron-right and chevron-down in labelSecondary, one list.indent per level, and
  separator hairlines as indent guides.
- **Data states.** The spinner and Strings::loading in labelSecondary; the error glyph in
  destructive with the text in label; the empty text in labelSecondary, centered; all keep the
  list's size, so the layout does not jump when the data arrives.

Drag and drop core:

- **Ghost.** The source item's size on the level 3 elevated surface with its shadow and
  hairline, faded to drag.ghostOpacity, so it reads as lifted above the page without hiding
  where it goes.
- **Badges** on the ghost (count, copy) are an accent pill with onAccent text at full opacity,
  because they are information, not part of the faded item.
- **Insertion line.** The focusRing color at focusRingWidth with round ends, the one mark that
  reaches 3:1 on every surface.
- **Drop target.** An accentSubtle fill with the accent outline at the one border width, the look
  of a selected tile, so it is not mistaken for the focus ring.
- **Refusal.** No highlight and the not-allowed cursor, so refusal does not rest on color.
- **Gap.** Rows move apart along the standard spring, keyed by item, to show where the item
  lands; the landing place gets a fill[2] wash; under reduced motion the rows jump.
- **Keyboard drag.** The ghost sits spacing.xs above the landing place with the focus ring and
  springs to each new place, so the keyboard user sees the same lift as the pointer user.

Text field core:

- **Fill.** fill[2] at rest and fill[1] on hover, like the other control fills, with the
  controlBorder hairline, controlBorderHover under the pointer.
- **Read-only.** No fill, the controlBorder hairline only, because it reads as text that can be
  selected but not typed.
- **Disabled.** fill[3], a separator hairline and labelTertiary text.
- **Active field.** The accent outline (focusRing color) at the hairline width even after a mouse
  click, because typing goes where that outline is; the keyboard focus ring is added only after
  keyboard input.
- **Error.** A destructive hairline in place of the boundary, which keeps its color when
  disabled.
- **Caret.** The label color, the strongest contrast on every surface, field.caretWidth wide and
  rounded to whole pixels so it stays crisp.
- **Selection.** accentSubtle behind label text.
- **Placeholder.** labelSecondary, the lightest label that still reaches 4.5:1.
- **Edge fade.** Text that continues past the text area fades out over scroll.edgeFade on that
  side only; only glyphs fade, the selection and caret keep full alpha.
- **Password dots.** The font's own bullet, centered in Dear ImGui's password advance, so caret
  and selection line up with the dots.
- **Floating label.** It moves from the placeholder position to Caption size at the top with
  motion.quick; the frame grows to spacing.xs, Caption, the text and spacing.sm, which equals
  controlHeightLarge at Regular.
- **Adornment buttons.** The control height less spacing.xs per side, a plain fill on hover and
  press, radii.element, and the icon at the field's text size; spacing.sm between the icon or
  suffix and the text, spacing.xs between buttons.
- **Clear button** is not a Tab stop, because clearing is also Ctrl+A then Delete and a stop on
  every search field slows Tab.
- **Message line.** Footnote, spacing.xs under the frame; errors start with the error icon.
- **Spinner** is drawn with a stroke of an eighth of its size.
- **Drag fields** show the left-right resize cursor over the field and its label, which says
  that dragging changes the value.

Control anatomy (`src/internal/control.h`), the parts every widget family shares:

- **Padding by size.** Push-like controls keep spacing.sm, spacing.md and spacing.lg between their
  edge and their content at Small, Regular and Large, so the space around the text grows with the
  height and every size looks equally balanced.
- **Icons beside text** are as large as the font size, whole pixels, spacing.sm from the text, so
  an icon reads as part of its label at every size.
- **Labels in front** take the label column of a settings row (layout.labelColumn, the gap
  included) whatever the label says; a longer label ends in an ellipsis and shows in full on hover
  and keyboard focus. So a column of sliders, toggles, dropdowns and fields starts every control at
  one x, the same x as the controls of settings rows. A widget narrower than the label column plus
  layout.minRowControl, where a settings row stacks, gives its label only its text width and
  spacing.sm, at most half the widget, so the control keeps room to be operated. A compact control
  that fits its content (a segmented control, a color well) keeps its label at its text width too,
  because it sits inline among other controls where a column would only open a gap.
- **Trailing columns** (a slider's value, the number field beside a slider) are shared by the
  controls of one window that end at one x and show the same kind of value, at the widest of them,
  so the tracks of a column end at one x too and keep one length. A shared column that would leave
  a track shorter than layout.minRowControl is dropped by every slider of the column alike, and in
  a narrow column the sliders share the widest label text width as well, because a column whose
  tracks start and end at different x reads as broken.
- **Labels above** a group or a tall control use the control text style and stand spacing.xs above
  it, the same gap as the message line below, so label, control and message read as one block.
- **One message line** for every control: Footnote, spacing.xs under it, help text in
  labelSecondary, an error in destructive with the error icon; every error is also collected for
  ValidationSummary.
- **One disabled look** for every variant of a control: fill[3], a separator hairline where the
  control has a surface, labelTertiary content, no hover or press feedback, so a disabled primary
  action no longer claims the accent.
- **Disabled pictures and colors** (images in tiles, thumbnails and option cards, swatches and
  gradients) keep 35 % of their opacity in every family: the content stays visible as information
  while it reads as unavailable, and the theme has no disabled image token to differ by.
- **Fill feedback.** Hover blends the rest fill toward the hover fill and a press toward the
  pressed fill over durationFast, so the feedback is the same in every widget.

### One state, one look

The same state looks the same in every family because one helper draws it; a widget never draws
these parts itself:

| State or part | Helper | Rule |
|---|---|---|
| Hover and press | `ItemBehavior` (item.h) and `StateFill` | The fill blends toward the hover and the pressed fill over durationFast |
| Keyboard focus | `DrawItemFocusRing` and `DrawFocusRing`; `DrawFocusRingInside` for rows, cells and tiles (focus.h) | focusRing color, two units wide, one hairline of surface between control and ring (inside a row, at its edge), shown after keyboard input only |
| Outlines | `ControlOutlineColor`, `DrawOutline`, `DrawSurfaceOutline` (control.h) | One hairline: controlBorder (hover controlBorderHover, focused or selected the accent, error destructive), separator for surfaces |
| Rows, cells and tiles | `ListRowLook`, `ListRowFillRect`, `DrawListRowFill` (list_view.h) | Hover and current row fill[2], selected accentSubtle, a selected tile also the accent outline |
| Disabled | `ControlLabelColor`, `NeutralFill`, `kDisabledPictureOpacity` (control.h) | fill[3], a separator hairline, labelTertiary content, pictures and colors at 35 % |
| Read-only | `FieldFrameLook::readOnly` and each family's read-only marks | No fill, the controlBorder hairline; selections in fill[0] (a slider's filled part in labelSecondary) instead of the accent |
| Help and error line | `DrawControlMessage` (control.h) | Footnote spacing.xs under the control; an error in destructive with the error icon |
| Labels in front and above | `MeasureFrontLabel`, `MeasureAboveLabel` (control.h) | The label column of a settings row in front, spacing.xs above |
| Popup surfaces | The popup host by level kind (popup_host.h) | Radius, elevation and padding by kind, open and close motion from the motion tokens |
| Selection | The selection pill (selection_pill.h) | accentSubtle pill on the standard spring, semibold label |
| Pointer | `ItemBehavior` | The hand cursor on interactive items, the not-allowed cursor on disabled ones |

Intended differences keep their reason line in the family sections below (for example the
destructive close button of the title bar, or the drag cursors of fields that drag).

### Prototype controls

The Blocks pages operate the blocks through prototype controls in `demo/`. They are not the
finished widgets; their look was the starting point for the widget families:

- **Fills** step fill[2], fill[1], fill[0] for rest, hover and press with the controlBorder
  hairline, so the states differ in more than one subtle shade.
- **Tracks.** The unfilled slider track is fill[0] and the filled part the accent mark, which
  reaches 3:1 against it, so the value reads without an outline.
- **Knobs** follow the knob rule of Quiet layers and grow 15 % while held, which confirms the grab.
- **Span.** A range's filled span keeps its thickness and color under the pointer and while it is
  dragged; the handles show that the range is held.
- **Marks.** Step ticks (including an off-grid max) and snap stops are labelSecondary dots.
- **Read-only** fills in labelSecondary, so a value that cannot change does not look like an
  accent selection.

## Widgets

Button:

- **Secondary** is the neutral fill with the controlBorder hairline, like a text field, so a row of
  neutral controls reads as one set.
- **Secondary hover** turns the hairline controlBorderHover; the outline width never changes.
- **Primary** is shaped by its accent fill alone, with onAccent text; only an accent below 3:1 on
  some surface gets the focusRing edge. Reason: the lighter accent rim read as a second outline
  around the main action in the dark theme.
- **Destructive** is the solid destructive fill with onDestructive text and no hairline, because
  the destructive color reaches 3:1 itself; its weight matches the primary action, so a dangerous
  action is never the quieter one.
- **Plain** is text only, with a fill only while hovered or pressed, for actions next to a primary
  one that should not compete with it.
- **Loading** keeps the width and shows the spinner where the icon is (where the label is without
  an icon) in the content color, so the row does not shift while work runs.
- **Icon only.** An empty visible label gives a square of one control height, so icon buttons line
  up with the control height grid.

ProgressBar:

- **Track and fill.** The track is fill[0]; the value is the tone color at 3:1 (focusRing for the
  accent tone), so the value reads on every surface while the track stays a quiet frame; a buffer
  fill in labelTertiary sits between them in strength.
- **Thickness** follows the control size (4, 6, 8) with radii.element capped at half the
  thickness, which rounds the ends of these thin tracks fully.
- **Placement.** Without a label line the track is centered in one control height, so it lines up
  with the labels of a settings row; with one it follows the line directly.
- **Motion.** A new value eases in over durationStandard; an indeterminate bar sweeps a segment of
  30 % of the track on the shared loading clock, so it moves in step with skeletons, and fades in
  place with reduced motion.

Buttons and selection:

- **Toggle.** A capsule track: off is fill[0] with the controlBorder hairline, on is the accent
  mark (focusRing, 3:1 against an off track) without an outline; the knob is a solid disc in the
  knob token with the controlBorder hairline and the level 1 shadow in every size and state, so on
  and off differ in fill, edge and knob position, not only in hue. The knob moves with the quick
  spring, because it is a small state change.
- **Disabled and read-only toggles.** A disabled on toggle keeps its accent track at the disabled
  picture opacity; a read-only on toggle uses labelSecondary. Reason: on and off must differ by more
  than the knob position (labelSecondary keeps 3:1 against an off track on every surface), and a
  read-only value must not claim the accent.
- **Checkbox and radio marks.** Unchecked is a quiet box: the neutral fill with the controlBorder
  hairline, controlBorderHover under the pointer; checked is the accent. Mixed is a minus instead of a check, so the three states differ by shape.
  Box corners are at most a quarter of the side, so a box never reads as a radio circle. The radio
  dot is 3/8 of the mark and grows in with the fill, so the change reads as one motion.
- **Read-only selections** (segments, chips, cards) use fill[0] instead of the accent, so a value
  that cannot change does not look like a live selection; a read-only checked mark keeps an unfilled
  quiet box with the glyph in label. Reason: a labelSecondary fill was a light grey surface in the
  dark theme.
- **Segmented track and toggle groups.** The track is fill[2] with the controlBorder hairline, like
  a Secondary button. The pill is accentSubtle inset by segmentInset with concentric corners, and
  hairline dividers stand only between unselected neighbours, so the pill reads as one piece.
- **Latches** (a latched IconButton, selected chips and cards) take accentSubtle plus a focusRing
  hairline, so a latch shows by its edge as well as its tint.
- **Chips** are capsules (the radii rule allows capsules for toggles, chips and badges). The remove button sits
  inside the pill's end and is not a Tab stop, so a row of chips keeps one stop per chip; Delete
  and Backspace remove.
- **CopyButton.** The confirmation check is in the success color on the neutral variants, because
  it reports success; the width holds both texts, so nothing moves while it confirms.
- **Copy confirmations** have two looks on one timing (feedbackHold times motion.scale): a control
  that copies (CopyButton, CopyField) swaps its own icon for the check in place, because it already
  has room for it; copied text without such room (InlineCode, KeyValueList, CodeView on Ctrl+C)
  shows the check and Strings::copied in a tooltip level above it, so the text never moves.
- **Link.** A hairline underline only under the pointer or the keyboard focus marks the link
  without underlining every link at rest; a visited link uses accentTextVisited.
- **CycleSelector** takes the text field frame, because it shows a value. Its chevrons are
  labelSecondary, and labelTertiary at an end that cannot step; page dots use pageDotSize, the
  chosen one in label.
- **Option cards** are bgSecondary with radii.card, because they are grouped content on a page. A
  radio mark stands for one choice and a checkbox mark for several, so the two forms read
  differently.

Sliders:

- **Knob size.** sliderKnobSize scaled by the control height of the size and kept even, so Small,
  Regular and Large differ and the knob centre sits on a whole pixel.
- **Value column.** As wide as the widest value shown so far by any slider of its column (the
  trailing column rule under Control anatomy), giving way below layout.minRowControl, so the value
  never jitters, the tracks of a column keep one length, and each track keeps a usable length.
- **Range values** read low, a short drawn bar, high, so the pair reads as a range without a word
  to translate.
- **Value bubble.** A small elevated surface (level 1, separator hairline, label text) above the
  knob while it is held or focused. Reason: in the dark theme an inverted bubble was the brightest
  object on screen.
- **Crowded ticks** are thinned to keep three tick sizes apart, and the last tick always stays, so
  both ends stay marked.
- **Stepped stops** are evenly spaced whatever their values, so a list of choices reads as equal
  steps; stop labels are Caption under the ticks.
- **Knob.** A track arc in fill[0], the value as an accent arc, a dot or a wiper, and a face in
  the knob token with the controlBorder hairline, like a slider knob.
- **Histogram bars.** The selected stretch in the accent, the rest in fill[0], top corners rounded
  with half of radii.element, so the bars read as one chart under the range.
- **Multi slider segments** take the series colors (the chart palette) unless the caller gives a
  color; a handle dragged off the track
  takes the disabled look, which shows that releasing it there removes it.
- **Pads** (XYPad, BoxRangePad, the square JoystickPad) use the field frame: a pad is an input
  surface, so it shares a field's fill, boundary, hover, read-only, disabled and error looks.
- **One handle shape.** Every pad point, box handle, arc handle, allocation divider and stick is the
  slider knob, so the whole family has one handle.
- **XYPad guides** are controlBorder hairlines and its readouts labelSecondary Footnote: the guides
  only decorate, the values must be read.
- **Pad grid.** Separator hairlines at every step up to 32 per axis, otherwise at the quarters,
  because more lines would read as a texture.
- **BoxRangePad box.** An accentSubtle wash with the accent outline at the one border width: it is
  the selected range, like a range slider's filled span, and like it keeps its look under the
  pointer. Edge knobs show
  only on edges at least three knobs long, so a small box does not pile up knobs.
- **AllocationBar** segments sit on the slider track in chart palette colors with Caption
  captions; the share is kept over the label when crowded, because the number is the data.
- **ArcRangeSlider** defaults to four control heights so two handles and both values fit inside;
  its band is the accent mark on the fill[0] track arc and keeps its look under the pointer.
- **JoystickPad.** An accent line from the centre shows the vector, a separator cross marks zero
  and the deadzone edge is a controlBorder hairline; the spring back uses motion.standard because the return
  signals the release.

Text inputs:

- **FieldValidation.** A hairline outline in the tone color around the validated item, matching
  the error outline of Facet fields.
- **ValidationSummary.** A block on destructiveSubtle with label-colored, always underlined links
  and the error glyph, so the list reads as errors without relying on red text.
- **TagInput** chips are one control size smaller than the field, so a row of chips keeps the
  field's height. The text part stays beside the last chip while `spacing.xxxl` or the typed text
  still fits, so a short tag never leaves an empty row under it.
- **FilterField** toggles are the glyphs "Aa" and an underlined "ab", which users already read as
  match case and whole word; the on state is a fill plus the label color.
- **CopyField.** The copy icon turns into a check for motion.feedbackHold as the confirmation.
- **PinCodeInput.** Each cell is a field frame one control height square, and the next cell has
  the active outline. An error shakes the cells three swings over motion.durationSlow,
  spacing.xs wide, skipped with reduced motion.
- **MaskedInput.** The unfilled part of the mask shows after the text in labelSecondary as a format
  hint.
- **PathField.** Idle paths are shortened in the middle, so the file name stays visible.
- **InlineRename.** Idle text gets a subtle fill on hover, which shows that it can be edited.
- **SecureField** strength meter is destructive, warning and success by thirds of the strength,
  and the Caps Lock hint uses the message line, so the warning sits where help text always is.
- **TextArea** resize bar is centered on the bottom edge, where a resize is expected.
- **Autocomplete** shows the inline completion in labelSecondary after the caret, so it reads as a
  suggestion, not as typed text.

Value inputs:

- **Key caps.** fill[2] with the controlBorder hairline and radii.element, Footnote (Caption at
  Small), spacing.xs apart, so a cap reads as a key on a page and on a field fill; a held cap
  takes fill[0] and a focusRing outline. KeyCap, ShortcutHint and KeybindCapture share this look
  (`src/internal/key_caps.h`) and differ only in placement: on the text baseline inline, centered
  inside a control in a field.
- **KeybindCapture.** A listening slot has the focusRing hairline of an active field and shows the
  held keys as held caps, so the user sees the chord form before it is taken.
- **Rating.** Filled icons in focusRing, which reaches 3:1 on every surface, empty ones in
  labelSecondary; a caller's own icon has no filled form, so a filled one also gets an
  accentSubtle backdrop and the value is never shown by color alone.
- **DurationInput.** Digits in label and separators in labelSecondary, with half of spacing.xs on
  both sides of each separator; the segment the keys edit has accentSubtle behind it while
  focused, so the user sees which unit Up and Down change. It drags vertically with the up-down
  resize cursor, because Left and Right already pick the segment.
- **Unit menu and slot buttons.** The unit label and a chevron in the trailing slot, with the plain
  hover fill of slot buttons, so the unit reads as part of the value and as a menu.
- **SizeInput.** The lock (lock and unlock icons) sits at the end of the width field, between the
  two values; swap uses the refresh icon, presets the chevron.
- **HexInput** shows "0x" and a gap after every two digits only while not typing, so the typed text
  is the plain digits that overtype edits.
- **Rejected text.** A committed text a number-like field cannot read shows Strings::invalidValue as
  its error until the field is used again, so the user sees why nothing changed.

Lists:

- **Popup list rows** (Dropdown, MultiDropdown, BeginCombo) keep a leading slot: an accentText
  check marks the chosen item, a checkbox in multiple-choice lists, so the choice reads without
  color alone.
- **A combo list shows at its real height.** The first time a combo opens, before its rows are
  counted, its list stays hidden one frame longer, so it appears where it stays instead of
  shrinking toward the field with the rows moving under the pointer.
- **Group headers** are Footnote labelSecondary rows that take no hover or focus, so they read as
  structure, not as choices.
- **Filter matches** are label text on a fill[1] wash with the rest of the label in
  labelSecondary, the contrast rule of the fuzzy block.
- **Inline framed lists** (ListBox, SelectableList, checklists, transfer lists) have no fill and the
  boundary hairline, so row hover, selection and pill fills read on any surface.
- **Search row** of a popup list is a frameless field with a search glyph over a separator
  hairline, so it reads as part of the list, not as a second control.
- **Reorderable rows** lead with a grip glyph; the landing place is a fill[2] slot while the rows
  spring aside.
- **TransferList** move buttons are Secondary buttons with arrow glyphs (right and left, down and up
  when stacked), disabled while nothing is selected.
- **Checkboxes in lists** are the selection family's checkbox mark, so a checked row and a
  checked Checkbox look the same.

Containers:

- **Section headers** (accordion, disclosure row, collapsible group box) are full-width rows with a
  chevron and a fill[2] hover, so they read as toggles without a frame; the link disclosure is
  underlined on hover, so its hover cue does not depend on color.
- **Card.** A bgSecondary surface with radii.card and no shadow, because it sits on the page and
  does not float.
- **Group box.** A separator hairline with radii.card, so it groups without adding a surface.
- **Accordion sections** are divided by hairlines, so the headers scan as one list.
- **Toolbar.** A selected tool button gets the accentSubtle fill with accentText, so the on state
  shows by fill and color; a separator is a hairline inset by spacing.xs, so it divides groups
  without reaching the row's edges.
- **Shell.** Glass with an elevation 2 shadow and radii.window, the one floating menu the blur dose
  cap allows. Its grip is two short diagonal strokes (a glyph, 1.5 hairlines), labelTertiary at rest and labelSecondary while a
  band is hovered, so resizing is discoverable without a heavy mark.
- **Title bar close button** takes the destructive fill on hover and press, because it ends the
  window.
- **Status bar.** A bgSecondary strip with a top hairline separates chrome from content; status
  shows as a tone dot or icon.
- **Mini window.** An opaque elevation 1 surface with radii.card: it floats, but below the shell.
- **Inspector pane.** bgSecondary when docked and an elevation 2 drawer when Compact; the drawer
  leaves a strip of spacing.xxxl of the main content in view, and the resize band shows a
  focusRing-colored hairline on hover.

Display:

- **Marks** (badges, pills, key caps, inline code) are their text line plus two xs steps and put
  their text on the baseline of the line around them, so they read as part of a sentence at
  every scale.
- **Tone washes.** Text on a tone wash is always label; the tone shows in its icon or dot (3:1),
  so no state depends on color alone.
- **Banners and inline alerts** share one surface: the tone wash, the tone icon, a Headline title,
  small Secondary and Plain action buttons, and a close button; an inline alert adds a hairline
  because it sits among fields.
- **Code views, logs and stat tiles** sit on bgSecondary with a hairline, so their edges show on bg
  and on bgSecondary alike.
- **Count badge.** A solid capsule (a circle for one digit) with the number centered; a badge is a
  capsule on a tone wash, so the solid fill tells a count from a status.
- **Live dot.** A pulsing dot grows a fading ring once per pulsePeriod and stands still with
  reduced motion.

Data table:

- **Surface.** DataTable sits on the grouped surface (bgSecondary, card radius, hairline): a
  bounded data region whose scrolled content needs a visible edge, like the log view.
- **Header labels** are Footnote labelSecondary, label while sorted, with an arrow for the
  direction: column captions rank below the data, and the arrow carries the sort without color.
- **Rows** have no lines and use the list fills, selection and focus ring, so table rows read and
  behave like every Facet list row.
- **Row band.** The row fill and focus ring span the visible table width, also while scrolled
  sideways: one band per row, and the ring is never cut.
- **Cell focus.** During cell navigation the cell cursor is the focus ring inside the focused cell,
  shown after keyboard input only, so it marks the cell F2 or Enter will edit without reaching into
  a neighbouring cell or row.
- **Header dividers** are separator hairlines inset by spacing.sm; while a resize band is hovered
  or dragged the divider runs the header height in focusRing at the same hairline width, so the
  resize affordance shows where and when it acts without a heavier stroke.
- **Frozen columns.** A separator hairline marks their edge while the table is scrolled sideways, so it shows
  where pinned content ends as other columns pass under it.
- **Cell padding** is spacing.md, so cell text sits spacing.lg from the surface edge, the same
  inset as list row text.

Layout:

- **Divider grip.** A split or panels divider shows a short grip in the scroll thumb color in the
  middle of its hairline: the part that reaches 3:1 and tells that it can be dragged.
- **Dependent group** draws a separator hairline down the middle of its indent, so the indented
  rows read as belonging to the setting above.
- **Panel header** has a hairline under it and folds to a strip of the header height, so a
  collapsed panel keeps a way back without taking room.
- **List and detail** sit side by side with a hairline band and no grip, because that divider does
  not move.
- **Flow count chip** is a neutral Badge, so hidden items are counted without looking like a
  control.

Overlays:

- **Popover arrow** (overlay.arrowSize, the surface color, a hairline on its two slanted sides)
  ties the card to its anchor at a glance.
- **Popovers do not block.** A card next to a control should not freeze the page; a click outside
  both dismisses it and acts.
- **Toast.** An elevation 2 surface with the popover radius, newest nearest the corner, sliding in
  from the edge and moving on a spring: status news that never covers or jumps the page content.
- **Drawer.** Flush with its edge, window radius on the inner corners only, sliding from its edge,
  so it reads as part of the window frame, not a floating card.
- **Dialog title** in Title3 with a Plain Small close button submitted last: the title leads, and
  keyboard focus lands on the content first.
- **Hold to confirm.** A fill[0] fill grows across the held button, so the waiting is visible, not
  only timed.
- **Loading overlay.** The scrim plus a level 1 card under the spinner and message, so the message
  keeps AA contrast over any content.
- **Command palette.** A frameless Large search field with a hairline under it, recent picks first:
  the field is the focus of the layer, and repeat actions are one key away.
- **Unsaved changes bar.** A level 1 card floating above the window bottom: present without hiding
  the page edge, it slides only when the change count crosses zero.
- **Watermark.** A level 1 strip with a hairline and Footnote text, readable over any scene and
  quieter than the UI.
- **Toasts keep clear of the watermark.** Where a watermark strip crosses the toast column in the
  same half of the display, the stack starts one stack gap past it, so neither hides the other.

Navigation:

- **Tab underline** in the focus ring color: a non-text selection mark must reach 3:1 on every
  surface, and focusRing is the accent derived for exactly that.
- **Count badges** use the grey wash for Neutral and the tone's solid fill otherwise, so a count
  that asks for attention stands out without relying on color alone.
- **Sidebar** on bgSecondary with the card radius and flush list-height rows, so the column reads as
  one navigation surface beside the page content.
- **Rail badges** sit on the icon's top-right corner, so the count stays visible without widening
  the rail.
- **Menu bar** has a hairline under it, which separates the titles from the content without a
  filled band.
- **Radio menu rows** show a dot at 40 % of the icon slot, so a choice of several reads differently
  from a check mark.
- **Step markers.** A done step is an accent disc with a check, the current step accentSubtle with
  the accent hairline, an open step a controlBorder hairline; connectors take the focus ring color when done and
  the boundary color otherwise. The check keeps progress readable without color.
- **Breadcrumbs** show ancestors in labelSecondary and the current place in label: hierarchy
  without extra weight.
- **Pagination.** The current page sits on the selection pill, the identity motif that marks the
  selection in every bar.
- **Wizard** content slides spacing.xxxl over motion.durationStandard: the motion signals the
  direction of the step change, and it stands still with reduced motion.
- **Shortcut sheet.** The list stops growing at half the work area, so the title and the search
  field stay on screen.

Color pickers:

- **Handles** are the one grab face of Quiet layers with the picked color inside the white rim;
  hover changes only the outline color and the handle grows on press only. Where the white rim
  would vanish on a light color, the hairline is black (ColorHandleEdge).
- **Gradients** are drawn in gamma sRGB as exact linear stretches (the hue in six, the square as
  white to hue under a black alpha ramp), so the color under a handle's centre is the value it
  stands for.
- **Color bars** are picker.colorBarHeight, thinner than the knob, with rounded caps in the end
  colors: the slider anatomy, and the knob centre always sits on its color.
- **Disabled pickers** dim gradients, swatches and handles to 35 % and give handles a separator
  hairline without shadow: the color stays visible as information while reading as unavailable.
- **Chosen swatch.** The accent hairline one gap outside it and a check mark. Reason: the swatch
  color can match the accent, so the outline reads against the surface and the check carries the
  state.
- **Swatch names** show as the swatch's tooltip on hover and on keyboard focus, so a keyboard user
  learns what a swatch stands for as a pointer user does.
- **Mixed color well** shows diagonal labelTertiary stripes over fill[2]: a value that is not one
  color.
- **Wheel mode** puts hue 0 at the top and keeps the triangle fixed (hue right, white lower left,
  black upper left), so the arrow keys keep their direction.
- **Compare strip** shows the current color left and the original right, and only the right half
  can be pressed: one target restores.
- **ColorModeEditor preview** animates with SampleAnimatedColor and stands still under reduced
  motion: motion only shows what the setting does.

Date, time and other pickers:

- **Calendar choice.** The picked day and the range ends sit on the accent fill with onAccent
  semibold numbers, because a calendar has one strong choice that must read at a glance.
- **Today** is the accent hairline; the picked day is the accent fill. Reason: a mark and a fill
  keep two states apart at one outline width.
- **Range band.** accentSubtle across the days between the ends and half cells under the ends, so
  the band reads as one span joining them.
- **Date and time fields** use the ISO segment order with the focused segment on accentSubtle, the
  look of DurationInput, because one segmented look serves every segmented field.
- **Time columns** use the selection pill in scrolling columns, the identity motif applied to a
  picker.
- **Angle dial.** The Knob's face with four quarter ticks in labelTertiary (decoration) and an
  accent needle with a tip dot, so the direction reads without the number.
- **Icon and thumbnail tiles.** The chosen tile has the selected tile look of the row rule
  (accentSubtle and the accent outline at the one border width), so the choice does not depend on
  the fill alone.
- **Thumbnail badges.** A lock on a bgElevated disc and a check on an accent disc, so locked and
  checked tiles differ by shape.
- **File browser.** The list is framed like the inline lists, and the path is cut in the middle so
  the folder name stays visible.
- **Popover surfaces.** Calendar popovers use the glass surface (the popover dose cap); the time and
  icon popovers stay solid so their edge fades blend into bgElevated.

Charts:

- **Readout.** A box on the level 1 elevated surface with a hairline, inside the plot, so reading
  values needs no hover delay and never leaves the chart.
- **Shape cues.** Markers cycle circle, square, diamond and triangle per series, hidden legend
  entries are hollow, and stacked segments and donut slices keep a bar gap of surface between them,
  so series differ without relying on color.
- **Axis labels** are measured, then thinned until none overlap; automatic ranges end on whole
  ticks so the grid frames the data.
- **Area fills** take the alpha of accentSubtle over the series color, the strength of every other
  tinted wash, so an area never hides the grid or the series above it.
- **Range brush.** Edges in the focus ring color with grips on the level 1 surface and a control
  boundary hairline; the window has the accent wash and the outside the fill wash. The edges reach
  3:1 on every surface. The window body keeps its look under the pointer and while dragged; only
  the grips show hover (rule of Quiet layers).
- **Gauge and meter tones.** Zones and fills use the progress tone colors (focusRing for the
  accent), which reach 3:1; level meter fills have square ends so zone pieces and segments meet
  edge to edge.
- **Frame time graph.** Frames over the target use the warning color and also rise past the target
  line, so they stand out without color too.

Media:

- **Sharp pictures.** Image boxes land on whole pixels and an image at its texture size is drawn
  one texel per pixel, so pictures stay sharp at scale 1.
- **Rounded image corners** are geometry with the texture mapped over them, so no picture shows
  past a corner.
- **Avatar fallback.** Without a ready image an avatar shows initials in label on fill[1], or the
  user icon: a neutral fallback that invents no picture.
- **Selected avatars** get the accent outline at the one border width inside their edge, with a
  hairline gap; a chosen thumbnail takes the selected tile look (accentSubtle and the accent
  hairline) with its picture two hairlines in. The choice shows without the item growing or
  relying on color alone.
- **Surface rings.** Overlapping avatars and an avatar's status dot are ringed in the surface color,
  so the edge between them stays visible.
- **Image feedback.** Hover and press on images are fill[2] and fill[1] washes over the picture,
  the same feedback as other controls.
- **Carousel buttons and image captions** sit on bgElevated with a hairline, so their glyphs and
  text keep their contrast over any picture.
- **Image compare divider.** A surface-colored line between boundary hairlines, visible over light
  and dark pictures, with the slider knob as its handle.
- **Timeline layers.** The accent mark (focusRing) for the played part on a fill[0] track,
  labelTertiary for the buffered band, accentSubtle for the loop region and loop markers as solid
  knob-token faces with the level 1 shadow and the controlBorder hairline, so each layer reads apart.
- **Carousel dots.** The current page in focusRing, the others in controlBorder, so the position
  reads at 3:1 while the other dots stay quiet.
- **Drop zone** outline is dashed in controlBorder at the hairline width; an accepted drag takes the
  drag and drop target look and a refused type gets no highlight.
- **Glide.** Strips and carousels hand the drag's speed to a spring (standard motion) on release,
  so they glide and snap without a jump; with reduced motion they jump.

Composites, inventory and cards:

- **Inventory slot surface.** fill[2] with the controlBorder hairline, so an empty slot reads by
  its fill like any control (3:1 in the increased contrast variant).
- **Count and lock chips** sit on bgElevated with a separator hairline, the count in a capsule and
  the lock in a circle, so they read over any picture in both themes and share the shape of every
  badge.
- **Cooldown.** A pie in scrim over the picture, clockwise from the top, so it reads as a clock
  running down.
- **A lifted item's place** is drawn at 1 minus drag.ghostOpacity, so the user sees where the item
  came from while the ghost carries it.
- **Selected tile and selected media card.** accentSubtle and the accent outline at the one border
  width (the row rule), so the selection does not rest on the fill color alone.
- **A disabled tile's picture** takes the disabled picture opacity of Control anatomy next to
  labelTertiary text.
- **Detail card stat differences.** success when better, destructive when worse, with the sign
  giving the direction, so the comparison does not rest on color alone.
- **ProfileSwitcher current profile.** A check in accentText plus a semibold name, not color alone.
- **Narrow media card cover.** A square of metrics.slotSize, at most half the inner width, so a
  narrow card stays one compact row.
- **Extra drag preview content** sits in a floating layer on the level 2 elevated surface under
  the ghost, so it reads as part of the lifted item and never lands in the caller's window.

Composites, search and app controls:

- **AccentPicker custom well.** A custom accent gets the same accent outline as a chosen swatch, so
  the choice reads as one mark across swatches and well.
- **SettingsSearch reveal.** The revealed hit gets an accentSubtle fill behind its row for
  motion.feedbackHold, then fades over durationSlow, so the eye finds where the search landed;
  reduced motion ends it without a fade.
- **PresetList marks.** Built-in and Modified are words in Footnote labelSecondary at the row end,
  so a preset's state is never told by color alone.
- **KeybindList conflict.** A shared chord shows the warning icon in the warning token plus "Also
  bound to" and the other action's name on both rows; below propertyLabelMinWidth the label
  stacks above the fields instead of vanishing.
- **KeybindList and LayerList with a height** end their rows before the overlay scroll bar at its
  hovered width, so the bar never lies over a reset button or a layer toggle and both stay easy to
  hit.
- **ConsoleView candidates.** Completion candidates and "No results" show as one Footnote
  labelSecondary line above the field, so they do not cover the log.
- **Onboarding footer.** The current step is an accent dot plus "Step x of n", so progress is told
  in words as well as color.
- **SearchFilterBar count.** The result count sits in Footnote labelSecondary between field and
  chips, close to the query it counts.
- **UiScaleControl** applies a dragged scale on release, because applying it during the drag moves
  the track under the pointer.

Composites, editors and canvas:

- **Property grid column edge.** Its divider shows only while hovered, dragged or focused: a
  permanent line would cut every row in two.
- **Gradient stop with its popover open** keeps a focusRing outline, so it is clear which stop the
  picker edits.
- **Gradient stops dragged off the editor** fade to 35 % before they go, so the removal is visible
  before the release.
- **Easing number fields** carry the control point's name in front (Strings::controlPoint1 and
  controlPoint2), so the four numbers say which point they move, not only by their order.
- **Curve and easing editors** draw their curve in the first chart series color and their points
  in the slider knob look: the curve is data, the points are controls.
- **Layer toggles** are glyphs alone in labelSecondary (a set lock in label), named by their
  tooltip: a row of buttons per layer would outweigh the layer names.
- **Canvas snap guides and the selected element's outline** use focusRing, which reaches 3:1 on
  every surface; a selected element also takes accentSubtle, and its handles fill with the accent
  under the pointer instead of a thicker stroke. The center lines are labelTertiary because they
  are reference lines shown only while dragging; the dashed safe area frame is controlBorder, as
  quiet as the screen edge.
- **Canvas screen** keeps the window background with the controlBorder hairline, so the area
  elements are dragged in has an edge like a control; its grid stays in separator so it does not
  compete with the elements.
- **Canvas anchor control** sits beside the selected element only where it hides no other
  element, else beside the canvas: a picker that covers the layout it edits hides what it places.

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
  the scale. The pages of ready widget families set their own demos with Facet controls (a
  Toggle for a layer that shows or hides, a SegmentedControl for a data state, a Slider for a
  value, a TextField), so a page reads as one product; the Foundations pages and the Blocks pages
  set their options with Facet buttons, checkboxes, sliders and fields too. A stock control stays
  only where the page shows Facet working with stock Dear ImGui items, and its row label says so.
- **Mixed sizes on one line.** Controls of different sizes in one row are centered on the
  tallest one, so their labels sit on one line instead of stepping down from a shared top. The
  row label is centered on that same line, so it lines up with the controls' text instead of
  sitting a few pixels above it.
- **Split family pages.** A family with two distinct groups of widgets shows each group on a page
  of its own (Text inputs and Value inputs, Color pickers and Other pickers, Display and Data
  table, and the composites on three pages: Inventory and cards, Search and app controls, Editors
  and canvas), so a page stays short enough to scan and each page's note speaks for what it shows.
- **Demo widths.** A set width on a demo control is clamped to the room left in its row, so a
  page holds at a large scale in a small window.
- **Tokens page borders.** The Tokens page lists the border tokens (`separator`,
  `separatorOpaque`, `controlBorder`, `controlBorderHover`) and `knob` with their contrast on bg,
  and its switch turns the increased contrast variant on and off. Reason: the quiet outlines are a
  deliberate choice, and the page shows both the choice and the variant that reaches 3:1.
- **Value bubble room.** A section whose first slider shows a value bubble leaves spacing.lg
  above that row, so the bubble of a held or focused knob never covers the paragraph above it.
- **App shell.** The reasons for the test app's own chrome (the title bar, the navigation, the
  theme editor, the overlay backdrop and the showcase page) are in `docs/demo.md` under "Design
  reasons of the app shell", next to its description.
- **Pages.** The window background with 24 / 20 padding; every page starts with a Title1
  heading and a one-paragraph summary in labelSecondary, sections with a Title3 heading.
- **Settings rows.** A page labels each setting in one fixed label column in footnote
  labelSecondary, with the control after it, so option rows line up; in narrow windows the
  label moves above its control. The Blocks pages use the row block instead (see Blocks), the
  other pages keep this simpler row.
- **Samples.** Shapes that only show a token or an effect carry no text and no knob, and their
  caption names them, so nothing that cannot be operated looks like a control.
