#pragma once

// Design tokens and the current theme.
//
// A Theme is a plain struct of token groups. Widgets read every color, spacing, radius,
// size and duration from facet::T() and pass lengths through facet::Px() (scale.h); they
// never use literals. Lengths here are in design units: pixels at scale 1.
// Colors are ImVec4 in sRGB with straight alpha (see color.h). Like every Facet function,
// these are for the thread that runs the Dear ImGui frame (see context.h).

#include <span>

#include <imgui.h>

#include "facet/core/anim.h"

namespace facet
{

// Text styles from largest to smallest. Sizes and weights live in Theme::type.
enum class TextStyle
{
    LargeTitle,
    Title1,
    Title2,
    Title3,
    Headline,
    Body,
    Callout,
    Footnote,
    Caption,
};
inline constexpr int kTextStyleCount = 9;

enum class FontWeight
{
    Regular,
    Semibold,
    Bold,
};
inline constexpr int kFontWeightCount = 3;

// Colors in one cycle of the chart palette (Colors::chart).
inline constexpr int kChartPaletteSize = 8;

// Semantic colors. Each name says where the color is used, not what it looks like.
struct Colors
{
    // Surfaces, from the window background up to floating layers.
    ImVec4 bg;           // window and app background
    ImVec4 bgSecondary;  // grouped content, sidebars, cards on bg
    ImVec4 bgTertiary;   // content nested inside bgSecondary
    ImVec4 bgElevated;   // popovers, menus, dialogs and other floating layers
    // The surface of a floating layer at elevation level 1, 2 and 3 (see Elevation), lighter
    // the higher it floats in the dark theme, where a black shadow cannot darken the black
    // background; all white in the light theme, where the shadow shows the height. Level 2
    // equals bgElevated. Read it with ElevatedSurface(level).
    ImVec4 bgElevatedLevels[3];

    // Translucent grey fills for control backgrounds, strongest first: fill[0] for pressed
    // or large thumbs, fill[1] default control background, fill[2] hovered rows, fill[3]
    // the faintest wash.
    ImVec4 fill[4];

    // Text and glyphs, most to least prominent. label and labelSecondary reach 4.5:1 on
    // every surface and are the only ones for text that must be read, placeholders
    // included (placeholders use labelSecondary). labelTertiary is below 4.5:1 and is for
    // disabled text and decorative glyphs; labelQuaternary for decoration only.
    ImVec4 label;
    ImVec4 labelSecondary;
    ImVec4 labelTertiary;
    ImVec4 labelQuaternary;

    // Dividing lines and the edge of a surface (card, panel, popup, table): one hairline of
    // `separator`, translucent so it reads the same over every surface; separatorOpaque where
    // translucency would double up (crossing table borders). The edge of a control or a tile uses
    // controlBorder (below) instead.
    ImVec4 separator;
    ImVec4 separatorOpaque;

    // The accent marks selection, the primary action and focus. Theme::ApplyAccent derives
    // every color below from `accent`.
    ImVec4 accent;
    ImVec4 accentHover;
    ImVec4 accentPressed;
    ImVec4 accentSubtle;  // translucent selection background
    ImVec4 onAccent;      // text and glyphs drawn on accent fills
    // Accent-colored text and links on the surfaces: the accent hue with its lightness
    // moved until it reaches 4.5:1 on bg, bgSecondary, bgTertiary, bgElevated and every
    // bgElevatedLevels surface.
    ImVec4 accentText;

    // Status colors, readable as text on bg, bgSecondary and bgTertiary.
    ImVec4 destructive;
    ImVec4 success;
    ImVec4 warning;

    // The keyboard focus ring: the accent hue with its lightness moved until it reaches 3:1
    // (WCAG non-text contrast) on bg, bgSecondary, bgTertiary, bgElevated and every
    // bgElevatedLevels surface, and against fill[0] drawn over each of them. Accent marks that
    // carry a state and must stay visible on any surface without being text use it too: the filled
    // part of a track, an on switch, the accent outline of a selected tile, drop indicators.
    ImVec4 focusRing;

    ImVec4 shadow;     // shadow color; its strength comes from the Elevation tokens
    ImVec4 glassTint;  // tint of blurred panels; its strength comes from Glass::tintOpacity
    ImVec4 scrim;      // dims the content behind modal layers

    // Series colors of charts, legends and other indexed data (docs/api-conventions.md rule 10):
    // the hues step around the OKLCH circle from the accent hue, neighbours far apart, and each
    // color reaches 3:1 on every surface listed for focusRing. Theme::ApplyAccent derives them.
    // Data index i takes chart[i % kChartPaletteSize]; later cycles repeat the hues at another
    // lightness.
    ImVec4 chart[kChartPaletteSize];

    // Thumbs of the overlay scrollbars, at rest and while hovered or dragged. Both reach 3:1 on
    // every surface, so the scroll position can be seen on any of them.
    ImVec4 scrollThumb;
    ImVec4 scrollThumbHover;

    // States of a solid destructive fill (the Destructive button variant): onDestructive is white
    // or black, whichever is more readable on `destructive`, and hover and pressed move the
    // lightness away from it, so its text reaches 4.5:1 on every state. Theme::ApplyAccent derives
    // them from `destructive`. (since 0.1)
    ImVec4 destructiveHover;
    ImVec4 destructivePressed;
    ImVec4 onDestructive;
    // Text and glyphs on solid success and warning fills (solid badges, status pills), white or
    // black like onDestructive. Derived by Theme::ApplyAccent. (since 0.1)
    ImVec4 onSuccess;
    ImVec4 onWarning;
    // Translucent tone washes behind banners, inline alerts, subtle badges and delta chips, as
    // accentSubtle is for the accent: the status color at 14 % in both themes. label and
    // labelSecondary stay readable on each over every content surface. Derived by
    // Theme::ApplyAccent from the status colors. (since 0.1)
    ImVec4 destructiveSubtle;
    ImVec4 successSubtle;
    ImVec4 warningSubtle;
    // Visited links: the accentText hue turned by a sixth of the circle, its lightness moved until
    // it reaches 4.5:1 on every surface listed for accentText. Derived by Theme::ApplyAccent.
    // (since 0.1)
    ImVec4 accentTextVisited;
    // Tints of the code view (keywords, string literals, numbers; comments use labelSecondary),
    // each reaching 4.5:1 on every surface. (since 0.1)
    ImVec4 codeKeyword;
    ImVec4 codeString;
    ImVec4 codeNumber;
    // The two cells of the checkerboard behind translucent colors and transparent pixels (color
    // wells, alpha tracks, the image viewer), opaque, a light and a darker grey of the theme. They
    // only show alpha, so they need no contrast with anything. (since 0.1)
    ImVec4 checker[2];
    // The edge of a control or a tile (a field, a secondary button, an off switch, an empty slot):
    // one hairline, quiet on purpose, so a control is shaped by its fill and the edge only
    // completes it. controlBorderHover is the same edge under the pointer. In the standard presets
    // they stay below the 3:1 non-text contrast; the increased contrast variant
    // (SetIncreasedContrast) resolves them, and separator, to colors that reach 3:1 on every
    // surface. (since 0.1)
    ImVec4 controlBorder;
    ImVec4 controlBorderHover;
    // The face of slider and toggle knobs, solid in both themes; the knob takes the controlBorder
    // hairline and the level 1 shadow. (since 0.1)
    ImVec4 knob;

    // The surface of elevation level 1 to 3; the level is clamped into that range.
    const ImVec4& ElevatedSurface(int level) const
    {
        const int index = level < 1 ? 0 : (level > 3 ? 2 : level - 1);
        return bgElevatedLevels[index];
    }
};

// The 4 px spacing grid.
struct Spacing
{
    float xs = 4.0f;
    float sm = 8.0f;
    float md = 12.0f;
    float lg = 16.0f;
    float xl = 20.0f;
    float xxl = 24.0f;
    float xxxl = 32.0f;
};

// Corner radii by layer. A capsule (a radius of half the shorter side or more) is not a token:
// toggle tracks, chips, badges, scroll thumbs and thin tracks ask for it, and the drawing helpers
// (draw.h) give it circular ends whatever the corner settings below say.
struct Radii
{
    float element = 6.0f;  // small parts inside controls: checkboxes, tags, swatches
    float control = 8.0f;
    float card = 12.0f;
    float popover = 14.0f;
    float window = 16.0f;
    // From this radius up, corners use the continuous (curvature-matched) path instead of
    // a circular arc, when continuousCorners is on.
    float continuousFrom = 10.0f;
    bool continuousCorners = true;
};

// Sizes of controls.
struct Metrics
{
    float controlHeightSmall = 22.0f;
    float controlHeight = 28.0f;
    float controlHeightLarge = 36.0f;

    float toggleWidth = 38.0f;
    float toggleHeight = 22.0f;
    float toggleKnobInset = 2.0f;  // gap between the track edge and the knob

    float checkboxSize = 16.0f;

    float sliderTrackHeight = 4.0f;
    float sliderKnobSize = 18.0f;

    float focusRingWidth = 2.0f;  // the keyboard focus ring, the one stroke wider than a hairline around a control
    float hairline = 1.0f;        // every outline and divider; rendered as at least one physical pixel
    float minHitTarget = 24.0f;  // smallest clickable area, even for smaller visuals

    // Default sizes of larger controls, used when a widget's options leave the size at 0.
    float faderLength = 140.0f;       // vertical sliders and faders
    float dialSweep = 270.0f;         // degrees of a knob's or arc slider's arc, open at the bottom
    float padSize = 192.0f;           // side of two-dimensional pads (XYPad, BoxRangePad, JoystickPad)
    float pickerWidth = 256.0f;       // inline color picker and saturation square
    float numberFieldWidth = 72.0f;   // the number field beside a slider (SliderWithInput, RangeInput)
    float histogramHeight = 40.0f;    // bar area of RangeSliderHistogram
    float chartHeight = 160.0f;       // charts and curve, easing and envelope editors
    float panelHeight = 192.0f;       // code, log and table views and the file browser
    float cardWidth = 256.0f;         // option cards and other cards of a fixed width
    float thumbnailSize = 128.0f;     // thumbnails and media cards
    float slotSize = 64.0f;           // inventory slots

    // Marks and thin tracks (since 0.1).
    float statusDotSize = 8.0f;       // StatusDot, the live dot of a status pill
    float pageDotSize = 6.0f;         // page dots under a cycle selector or carousel
    float sliderTickSize = 4.0f;      // step and stop marks on a slider track
    float segmentInset = 2.0f;        // gap between a segmented track and its selection pill
    float progressHeightSmall = 4.0f; // progress track of ControlSize::Small (also strength meters)
    float progressHeight = 6.0f;      // progress track of ControlSize::Regular
    float progressHeightLarge = 8.0f; // progress track of ControlSize::Large
    float emptyIconSize = 32.0f;      // the icon above an empty or error state (not compact)

    // Steps and checkerboards (since 0.1).
    float stepMarkerSize = 24.0f;     // the numbered circle of a step indicator step
    float stepConnectorWidth = 2.0f;  // the line between two steps of a step indicator
    float checkerCell = 6.0f;         // side of one checkerboard cell (colors.checker)
};

struct TextStyleSpec
{
    float size = 13.0f;
    FontWeight weight = FontWeight::Regular;
};

// Size and weight for every TextStyle.
struct Type
{
    TextStyleSpec styles[kTextStyleCount] = {
        {26.0f, FontWeight::Bold},      // LargeTitle
        {22.0f, FontWeight::Bold},      // Title1
        {17.0f, FontWeight::Semibold},  // Title2
        {15.0f, FontWeight::Semibold},  // Title3
        {13.0f, FontWeight::Semibold},  // Headline
        {13.0f, FontWeight::Regular},   // Body
        {12.0f, FontWeight::Regular},   // Callout
        {12.0f, FontWeight::Regular},   // Footnote
        {11.0f, FontWeight::Regular},   // Caption
    };

    const TextStyleSpec& operator[](TextStyle style) const { return styles[static_cast<int>(style)]; }
    TextStyleSpec& operator[](TextStyle style) { return styles[static_cast<int>(style)]; }
};

struct Motion
{
    SpringSpec quick = {0.25f, 1.0f};      // small state changes: toggles, hover
    SpringSpec standard = {0.35f, 0.85f};  // the selection pill, panels
    SpringSpec gentle = {0.5f, 1.0f};      // large surfaces

    float durationFast = 0.12f;
    float durationStandard = 0.20f;
    float durationSlow = 0.35f;

    float scale = 1.0f;         // multiplies every duration and spring response
    bool reduceMotion = false;  // skip animations; values jump to their targets

    // Periods of the looping loading indicators (also multiplied by `scale`).
    float shimmerPeriod = 1.4f;  // one sweep of the skeleton highlight
    float spinnerPeriod = 1.0f;  // one turn of the spinner arc

    // Popups, menus and dialogs (also multiplied by `scale`): they fade in while growing from
    // popupScaleFrom of their size toward the anchor side, and fade out faster than they came.
    float popupOpen = 0.15f;
    float popupClose = 0.10f;
    float popupScaleFrom = 0.96f;

    // Feedback that stays a moment (since 0.1; also multiplied by `scale`).
    float feedbackHold = 1.5f;   // a copy button or field shows its check and Strings::copied this long
    float pulsePeriod = 2.0f;    // one pulse of a live status dot; it stays still while motion is reduced
};

// One soft shadow; lengths in design units.
struct ShadowLevel
{
    ImVec2 offset = {0.0f, 0.0f};
    float blur = 0.0f;
    float spread = 0.0f;
    float opacity = 0.0f;  // alpha applied to Colors::shadow
};

// Shadows of floating layers: 1 small (raised controls), 2 medium (popovers, menus),
// 3 large (dialogs, the floating menu shell).
struct Elevation
{
    ShadowLevel levels[3];

    // The shadow of level 1 to 3; the level is clamped into that range.
    const ShadowLevel& Level(int level) const
    {
        const int index = level < 1 ? 0 : (level > 3 ? 2 : level - 1);
        return levels[index];
    }
};

// Blurred translucent panels (backdrop blur).
struct Glass
{
    float blurRadius = 20.0f;   // design units
    float tintOpacity = 0.72f;  // alpha applied to Colors::glassTint
    float saturation = 1.5f;    // 1 keeps the backdrop saturation
};

// Responsive layout (layout.h): the width classes of an available width and the columns of
// labelled rows. Design units.
struct LayoutTokens
{
    float regularFrom = 400.0f;    // widths from here up are WidthClass::Regular, below Compact
    float expandedFrom = 720.0f;   // widths from here up are WidthClass::Expanded
    float labelColumn = 112.0f;    // label column of labelled rows, the gap to the control included
    float minRowControl = 128.0f;  // narrowest control kept beside its label; narrower rows stack
    float dividerBand = 8.0f;      // band around a split view or panels divider that drags it (since 0.1)
    float panelHeaderHeight = 32.0f;  // header row of a panel, and the strip of a collapsed one (since 0.1)
};

// Floating layers: popups, menus, tooltips, toasts, dialogs and drawers. Lengths in design units,
// times in seconds (times are multiplied by motion.scale like every duration).
struct OverlayTokens
{
    float anchorGap = 4.0f;          // between a popup and the rectangle it is anchored to
    float viewportMargin = 8.0f;     // a popup is moved or flipped to keep this far from the display edges
    float menuMinWidth = 180.0f;     // narrowest menu and context menu
    float submenuDelay = 0.2f;       // pointer rest on a menu row before its submenu opens
    float tooltipDelay = 0.5f;       // pointer rest on an item before its first tooltip shows
    float tooltipGrace = 0.4f;       // after a tooltip hides, the next one shows at once within this time
    float tooltipMaxWidth = 280.0f;  // tooltip text wraps here
    float toastDuration = 5.0f;      // a toast stays this long unless hovered
    float toastWidth = 320.0f;
    float toastStackGap = 8.0f;      // between stacked toasts
    float toastMargin = 16.0f;       // between the toast stack and the display corner
    int toastMaxCount = 3;           // toasts shown at once; later ones wait in the queue
    float modalWidth = 440.0f;       // modal and confirm dialogs
    float drawerWidth = 320.0f;      // drawers that slide in from the start or end edge
    float drawerHeight = 280.0f;     // drawers that slide in from the top or bottom edge
    float commandPaletteWidth = 560.0f;
    float menuShortcutGap = 24.0f;   // between a menu row's label and its shortcut text (since 0.1)
    float arrowSize = 8.0f;          // how far a popover's arrow reaches toward its anchor; its base is twice that (since 0.1)
    float commandPaletteTop = 96.0f; // between the top of the display and the command palette (since 0.1)
};

// Desktop surfaces: the floating shell, the app frame, sidebars and side panes. Design units.
struct ContainerTokens
{
    ImVec2 shellSize = {420.0f, 520.0f};     // first size of a shell, until a saved size exists
    ImVec2 shellMinSize = {280.0f, 160.0f};  // smallest shell: the title bar and one control row fit
    float titleBarHeight = 40.0f;            // shell and app frame title bars
    float resizeBorder = 6.0f;               // band along the edges that resizes a shell or app frame
    float sidebarWidth = 220.0f;             // an expanded sidebar
    float railWidth = 52.0f;                 // a sidebar collapsed to its icon rail
    float inspectorWidth = 280.0f;           // first width of an inspector pane
    float inspectorMinWidth = 200.0f;        // narrowest inspector pane
    float statusBarHeight = 24.0f;           // a status bar strip (since 0.1)
};

// Overlay scrollbars, smooth scrolling and edge fades (the scroll core). Lengths in design units,
// times in seconds.
struct ScrollTokens
{
    float barThickness = 6.0f;         // thumb width while the pointer is elsewhere
    float barThicknessHover = 10.0f;   // thumb width while the pointer is over the bar or drags it
    float barInset = 2.0f;             // between the thumb and the edge of the area
    float barGap = 4.0f;               // between the content and the bar at its hovered width, while the area scrolls
    float minThumbLength = 24.0f;      // a long content still gets a thumb that can be grabbed
    float barHideDelay = 1.0f;         // an idle bar fades out after this time
    float wheelDistance = 64.0f;       // scrolled per wheel notch, eased over motion.durationStandard
    float edgeFade = 16.0f;            // length of the fade where content continues past an edge
};

// Rows of lists, trees and tables (the list and selection core). Lengths in design units.
struct ListTokens
{
    float rowHeightSmall = 24.0f;      // rows of ControlSize::Small lists
    float rowHeight = 32.0f;           // rows of ControlSize::Regular lists
    float rowHeightLarge = 40.0f;      // rows of ControlSize::Large lists
    float indent = 16.0f;              // per tree level
    int popupRows = 8;                 // visible rows of a dropdown or completion list at most (maxVisibleRows 0)
    int inlineRows = 6;                // visible rows of an inline list (visibleRows 0)
    float typeaheadTimeout = 1.0f;     // seconds; a pause this long starts a new typeahead search
};

// Drag and drop (the drag and drop core). Lengths in design units, times in seconds.
struct DragTokens
{
    float startThreshold = 4.0f;       // pointer travel before a press becomes a drag
    float ghostOpacity = 0.7f;         // opacity of the drag preview
    float autoScrollZone = 32.0f;      // band inside a scroll area's edge where a drag scrolls it
    float autoScrollSpeed = 800.0f;    // units per second at the very edge, less further in
    float hoverOpenDelay = 0.6f;       // a drag resting this long on a closed node, tab or group opens it
    // Flicks and inertia (since 0.1): a release faster than flickSpeed (units per second) toward an
    // edge dismisses a drawer or toast and pages a carousel; a released pan keeps its speed and
    // loses it exponentially with this time constant (seconds; jumps to rest with reduced motion).
    float flickSpeed = 600.0f;
    float inertiaTime = 0.3f;
};

// Text fields (the text field core). Design units.
struct FieldTokens
{
    float padding = 8.0f;              // between the field frame and its text, icon or buttons
    float caretWidth = 1.5f;           // rounded to whole pixels, at least one
};

// Tabs of a TabBar. Design units.
struct TabTokens
{
    float minWidth = 64.0f;            // narrowest tab, also what the overflow measurement keeps per tab
    float maxWidth = 240.0f;           // widest tab; a longer label is cut with an ellipsis
    float underlineHeight = 2.0f;      // the accent line under the selected tab of TabStyle::Underline (since 0.1)
};

// Data tables. Design units.
struct TableTokens
{
    float headerHeight = 32.0f;        // the header row
    float minColumnWidth = 48.0f;      // a column is never resized narrower
    float resizeBand = 8.0f;           // width of the band on a column edge that resizes it
};

// Color, date and time pickers. Design units.
struct PickerTokens
{
    float dayCell = 32.0f;             // one day of the calendar grid, square
    float colorBarHeight = 12.0f;      // the hue and alpha bars and the track of a color channel slider (since 0.1)
    float hueRingWidth = 20.0f;        // the hue ring of ColorPickerMode::Wheel (since 0.1)
    float timeColumnWidth = 48.0f;     // one scrolling column (hours, minutes, seconds) of a time picker (since 0.1)
};

// Charts, gauges and meters (since 0.1). Lengths in design units, times in seconds.
struct ChartTokens
{
    float lineWidth = 2.0f;            // series lines and the needle of a gauge
    float markerSize = 6.0f;           // a dot on a point (markers, min and max, the crosshair readout)
    float barGap = 2.0f;               // between the bars of one category; categories keep spacing.sm
    float legendSwatch = 10.0f;        // the color square of a legend entry
    float donutHole = 0.6f;            // the hole of a donut chart as a share of its diameter
    float gaugeWidth = 12.0f;          // the arc of a gauge
    float meterSegmentGap = 2.0f;      // between the segments of a segmented level meter
    float peakHold = 1.0f;             // a level meter's peak mark stays this long, then falls (times motion.scale)
};

// Avatars, timelines and drop zones (since 0.1). Lengths in design units.
struct MediaTokens
{
    float avatarOverlap = 0.25f;       // how much of its diameter an avatar of a group hides under the next one
    float playheadWidth = 2.0f;        // the playhead line of a timeline scrubber
    float dashLength = 6.0f;           // the dashes of a drop zone outline
    float dashGap = 4.0f;              // the gaps between them
};

// Editors, canvases, search and settings controls, inventories and cards (the composites; since
// 0.1). Lengths in design units, times in seconds.
struct CompositeTokens
{
    // Curve, easing and gradient editors.
    float editorHandleSize = 10.0f;       // a point of a curve editor and a control point of an easing editor
    float gradientStopSize = 14.0f;       // the marker of a gradient stop, its color inside
    float stopRemoveDistance = 32.0f;     // a gradient stop dragged this far from the bar is removed
    float easingPreviewTime = 1.0f;       // one run of the easing editor's preview dot (times motion.scale)
    // Property grids, search and key bind lists.
    float propertyLabelMinWidth = 64.0f;  // narrowest label column the column drag of a property grid leaves
    float searchFieldMinWidth = 160.0f;   // narrowest search field beside the chips of a search filter bar;
                                          // a narrower bar puts the chips under the field
    float keybindColumnWidth = 160.0f;    // each chord column of a key bind list
    // Inventories and cards.
    float inventoryTintHeight = 4.0f;     // the category bar at the bottom of an inventory slot
    float mediaCoverAspect = 0.5625f;     // height of a media card's cover as a share of its width
    float mediaCardHorizontalBelow = 192.0f;  // a media card narrower than this puts its cover beside the text
    // Preview canvas.
    float canvasSnapDistance = 6.0f;      // a dragged canvas element this close to a guide snaps to it
    float canvasHandleSize = 8.0f;        // the edge and corner handles of a resizable canvas element
};

struct Theme
{
    bool dark = false;
    Colors colors;
    Spacing spacing;
    Radii radii;
    Metrics metrics;
    Type type;
    Motion motion;
    Elevation elevation;
    Glass glass;
    LayoutTokens layout;
    OverlayTokens overlay;
    ContainerTokens containers;
    ScrollTokens scroll;
    ListTokens list;
    DragTokens drag;
    FieldTokens field;
    TabTokens tabs;      // (since 0.1)
    TableTokens table;   // (since 0.1)
    PickerTokens picker; // (since 0.1)
    ChartTokens charts;  // (since 0.1)
    MediaTokens media;   // (since 0.1)
    CompositeTokens composites;  // (since 0.1)

    static Theme Light();
    static Theme Dark();

    // Sets colors.accent and derives accentHover, accentPressed, accentSubtle, onAccent,
    // accentText, accentTextVisited, focusRing and the chart palette from it in OKLCH. onAccent is
    // white or black, whichever is more readable, and hover and pressed move the lightness away
    // from it, so text on every accent state reaches at least 4.5:1. accentText (4.5:1), focusRing
    // and the chart colors (3:1) depend on the surface colors (focusRing also on fill[0]), so call
    // this after changing them.
    // It also derives the states of the status colors the same way (destructiveHover,
    // destructivePressed, onDestructive, onSuccess, onWarning and the three *Subtle washes), so
    // call it after changing destructive, success or warning too. The accent alpha is ignored.
    void ApplyAccent(const ImVec4& accent);
};

// The current theme. Valid at any time, also before Init; Theme::Light() until SetTheme.
// During an animated switch it is the blend of the previous and the new theme. While the
// increased contrast variant is on (IncreasedContrast), its separator, controlBorder and
// controlBorderHover are the 3:1 colors of that variant.
const Theme& T();

// Makes `theme` current. With `animate`, while Facet is initialized and motion is not
// reduced, the switch is spread over motion.durationStandard (times motion.scale) of the
// new theme: colors blend in OKLab and lengths linearly, one blend step per facet::NewFrame;
// switches (dark, weights, flags) change at the halfway point. Without `animate` the theme
// changes at once. Either way the library modules are notified of every change. `theme` holds
// the standard border colors; while the increased contrast variant is on, a border color passed
// back unchanged from T() counts as the standard one it replaced, so `Theme t = T(); ...;
// SetTheme(t);` never makes the variant's colors the standard ones.
void SetTheme(const Theme& theme, bool animate = true);

// Applies a new accent to the current theme (see Theme::ApplyAccent). A running transition
// finishes at once first. SetTheme shows the accent of the theme passed there; SetThemeMode
// applies the accent of the last SetAccent to its preset until ClearAccent.
void SetAccent(const ImVec4& accent);
void SetAccent(ImU32 accent);
// Forgets the accent of the last SetAccent, so SetThemeMode shows the presets' own accents
// again. The current theme keeps its colors until the next switch.
void ClearAccent();
bool HasCustomAccent();

// A scoped accent for one region of a frame. Until the matching PopAccent, T() returns the
// current theme with `accent` applied through Theme::ApplyAccent, so every derived accent token
// (hover, pressed, subtle, onAccent, accentText, focusRing) follows it. The current theme, the
// accent of SetAccent and the Dear ImGui style are not changed, and nothing animates: the
// region simply draws with the other accent. Pushes nest up to kAccentStackDepth deep; deeper
// pushes are reported and ignored (their pops still balance). A PopAccent without a push is
// reported and ignored, and facet::EndFrame reports accents still pushed and clears them, so
// the next frame starts unscoped. Reports use ImGui's recoverable error path (an assert only
// while io.ConfigErrorRecoveryEnableAssert is on) and the debugger output. No allocation.
// Each depth has its own theme: a reference taken from T() inside a scope keeps its accent
// while nested scopes are pushed and popped, and stays valid until the PopAccent of the scope
// it was taken in or the next theme change.
inline constexpr int kAccentStackDepth = 8;
void PushAccent(const ImVec4& accent);
void PushAccent(ImU32 accent);
void PopAccent();

// `a` blended toward `b`: colors in OKLab (alpha linearly), lengths, durations and springs
// linearly, switches and enums taken from `b` from t = 0.5 on. t is clamped to 0 to 1, and
// t = 0 and t = 1 return exact copies of `a` and `b`.
Theme MixThemes(const Theme& a, const Theme& b, float t);

// True while an animated SetTheme is still blending.
bool ThemeTransitionActive();

// Which preset the app shows. System follows the Windows "app mode" (light or dark); while
// Facet is initialized, a change of that setting is picked up in facet::NewFrame and switched
// to with an animation.
enum class ThemeMode
{
    Light,
    Dark,
    System,
};

// Switches to the colors (dark, colors, elevation) of Theme::Light() or Theme::Dark(), chosen
// by `mode`, with the accent last passed to SetAccent if there was one (see ClearAccent).
// Every other group of the current theme (spacing, radii, metrics, type, motion, glass) is
// kept, so settings made with SetTheme, such as motion.reduceMotion, survive a mode switch,
// including the automatic one in System mode. SetTheme leaves the mode at the value it has.
void SetThemeMode(ThemeMode mode, bool animate = true);
ThemeMode GetThemeMode();  // default Light

// The Windows app mode: true when apps should use a dark theme. Read from the registry; false
// when the value does not exist (older Windows versions have no dark app mode).
bool SystemPrefersDarkTheme();

// A named accent color for theme pickers.
struct AccentPreset
{
    const char* name;
    ImVec4 color;
};

// The accent choices offered by the demo and available to apps. The first one is the default
// accent of the light preset; the dark preset uses a lighter blue. A picker that offers the
// first entry as "theme default" calls ClearAccent (and SetThemeMode) for it instead of
// SetAccent, so each theme shows its own blue.
std::span<const AccentPreset> AccentPresets();

// The color for the outline of a control (a checkbox, a text field, the frames of stock Dear ImGui
// widgets): colors.controlBorder. It is quiet in the standard presets and reaches 3:1 on every
// surface in the increased contrast variant, where T() carries the variant's colors.
ImVec4 ControlBoundaryColor(const Colors& colors);

// The increased contrast variant. While it is on, T() resolves colors.separator,
// colors.controlBorder and colors.controlBorderHover to colors that reach the 3:1 non-text contrast
// on every surface of the theme (separatorOpaque mixed toward label until it does; the hover color a
// step further), so every outline and divider can be seen at 3:1. The standard presets keep them
// quiet. The variant applies to the presets, to every theme passed to SetTheme, to each step of an
// animated switch and inside PushAccent scopes, and ApplyImGuiStyle writes it into the stock style.
// Switching it changes the current theme at once and notifies the library modules.
void SetIncreasedContrast(bool enabled);
// The application's own switch (default off).
bool IncreasedContrastEnabled();
// True while the variant applies: the application's switch is on, or the Windows high contrast
// setting is on while Facet follows it.
bool IncreasedContrast();
// The Windows high contrast setting (SPI_GETHIGHCONTRAST) is on. Read at Init and again about once
// per second of frame time, so a change applies without a restart.
bool SystemIncreasedContrast();
// Whether IncreasedContrast() follows the Windows high contrast setting. Off by default: the
// application decides whether the system setting may change its look.
void SetFollowSystemContrast(bool follow);
bool FollowsSystemContrast();

// Writes the current theme and scale into a Dear ImGui style (the current context's style
// when null): every color, and every size, padding, rounding and border from the tokens,
// multiplied by Scale(); the font scale factors as in scale.h; the base font size from the
// Body text style. Sizes without a token take the Dear ImGui default times Scale().
// Settings that are not sizes and have no token keep their values: anti-aliasing,
// tessellation, global and disabled alpha, hover delays and tooltip flags, title, button and
// selectable text alignment, the window menu and color button positions, tree line flags,
// angled table header settings and the mouse cursor scale. Stock Dear ImGui widgets then
// follow the Facet theme.
void ApplyImGuiStyle(ImGuiStyle* style = nullptr);

// When on, facet::NewFrame calls ApplyImGuiStyle() whenever the theme or the scale changed
// since the last call, including every step of a theme transition. Off by default, so a
// host with its own Dear ImGui style keeps it.
void SyncImGuiStyle(bool enabled);
bool ImGuiStyleSynced();

} // namespace facet
