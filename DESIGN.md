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
`src/internal/selection_pill.h` draws it for the demo navigation, the current item of the
Overflow page and the prototypes on the Selection pill page.

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
| Focus ring | 3 wide, in the focusRing color (see Colors) | Visible on every surface without hiding the control |
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
| Larger controls | fader 140, pad 192, picker 256, number field beside a slider 72, histogram 40, chart 160, panel 192, card 256, thumbnail 128, inventory slot 64; knob arc 270 degrees | Multiples of the 4 px grid, most of them whole steps of the largest spacing (32), so large controls line up with the content around them; the knob arc leaves its gap at the bottom where the value reads |
| Popup placement | 4 from the anchor, 8 from the display edges | Close enough to read as attached, clear of the anchor's outline; never touching the display edge |
| Menus | at least 180 wide, submenus open after 0.2 s of rest | Short menu labels still get a target that is easy to hit; a pointer crossing a row on its way does not open its submenu |
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
| focusRing | the accent hue, lightness moved to 3:1 on every surface | the same | The ring sits on whatever surface holds the control, so it needs the non-text contrast on all of them; an accent that already has it is kept as it is |
| Chart palette (chart[0] to chart[7]) | eight hues from the accent hue, chroma 0.14, lightness from 0.56, moved to 3:1 | the same from lightness 0.72 | Neighbouring series differ by at least 90 degrees of hue, and every series stays visible on every surface for any accent |
| scrollThumb, scrollThumbHover | black at 45 / 60 % | white at 45 / 62 % | The scroll position reads at 3:1 on every surface, and the thumb gets stronger where the pointer is |
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
  the lightness until it reaches 4.5:1 on every surface, for links and accent text; focusRing
  and the chart palette do the same for 3:1, measured as drawn (8 bits per channel). The tests
  check this for 24 hues at five lightness levels in both themes.
- Outlines of controls reach 3:1 (non-text contrast) on the content surfaces; the scrollbar
  thumbs reach 3:1 on every surface.

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
  height of the layer sets its shadow.
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
- **Scrim.** The host draws the modal scrim from colors.scrim and fades it with the dialog;
  Dear ImGui's own dim is not used, so the dim and the dialog arrive together.
- **Rows in menus** draw the focus ring inside the row, because the menu padding is narrower
  than the ring.

Scroll core:

- **Overlay bar.** A thumb in scrollThumb and scrollThumbHover with radii.element (at most half
  its thickness); the fill[3] track shows only while the bar is wide. The position stays
  visible without taking content width, and the track appears only where the pointer aims.
- **Edge fades.** A gradient of the surface color over scroll.edgeFade whose strength grows with
  the content left on that side, so the fade eases in instead of popping.
- **Track paging.** A press on the track pages by the visible length less one edge fade, so the
  line at the old edge stays in view.
- **Wheel gestures** stay with the window they scroll while the pointer rests, so content
  sliding under the pointer, a focused slider included, does not take the wheel over.

List core:

- **Rows.** Hover fill[2]; the selection accentSubtle while the list has keyboard focus and
  fill[1] otherwise, so it stays visible but quieter when focus is elsewhere; selected labels
  semibold; rows inset one hairline top and bottom with radii.control, so adjacent selected
  rows stay separate.
- **Row focus ring** is drawn after all rows, so neighbours never cover it.
- **Active row of a driven list** (the open list of a field that keeps the keyboard) takes the
  hover fill and the focus ring, because it is where Enter goes, the same promise the ring makes
  for a focused row.
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
- **Drop target.** An accentSubtle fill with a focusRing outline at focusRingWidth and no halo,
  so it is not mistaken for the focus ring.
- **Refusal.** No highlight and the not-allowed cursor, so refusal does not rest on color.
- **Gap.** Rows move apart along the standard spring, keyed by item, to show where the item
  lands; the landing place gets a fill[2] wash; under reduced motion the rows jump.
- **Keyboard drag.** The ghost sits spacing.xs above the landing place with the focus ring and
  springs to each new place, so the keyboard user sees the same lift as the pointer user.

Text field core:

- **Fill.** fill[2] at rest and fill[1] on hover, like the other control fills, with the 3:1
  boundary hairline against the surface.
- **Read-only.** No fill, boundary only, because it reads as text that can be selected but not
  typed.
- **Disabled.** fill[3], a separator hairline and labelTertiary text.
- **Active field.** A focusRing-colored hairline even after a mouse click, because typing goes
  where that outline is; the keyboard focus ring is added only after keyboard input.
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
- **Planned widgets.** A declared widget without its implementation draws a dashed separator
  hairline with square corners at the size the widget will take, with "Name: planned" in
  footnote labelSecondary; it is not focusable, so a page of placeholders never looks operable.
  Every widget group draws this one look: real controls are solid and rounded, so no
  placeholder can be taken for one, and a container placeholder reads as a frame around the
  caller's content.
- **Placeholder captions.** Every placeholder group shares one caption rule: a box too narrow
  for "Name: planned (label)" shows the label alone, shortened with an ellipsis when needed,
  and the whole text on hover; a box without a label shows "Name: planned" or the name. A
  labelled box never falls back to the name, because boxes side by side in a row differ by
  their labels, while the box look and the row already say that the widget is planned. The
  caption sits on the first line of a tall box, level with the row label beside it.
- **Field messages.** The FieldValidation placeholder is spacing.xs taller than its text, so its
  hairline never crosses the text.
- **Placeholder sizes.** Pads, cell rows and the inline calendar shrink to the available width
  by default, so a placeholder never runs under the scrollbar; content placeholders are at
  least a small control tall, so a box around one line of text does not cut its descenders.
