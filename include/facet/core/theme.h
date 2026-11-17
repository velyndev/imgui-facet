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

    ImVec4 separator;        // translucent hairlines over any surface
    ImVec4 separatorOpaque;  // hairlines where translucency would double up

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

    ImVec4 focusRing;

    ImVec4 shadow;     // shadow color; its strength comes from the Elevation tokens
    ImVec4 glassTint;  // tint of blurred panels; its strength comes from Glass::tintOpacity
    ImVec4 scrim;      // dims the content behind modal layers

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

// Corner radii by layer. A pill shape (radius = half the height) is used only by toggles
// and chips and is not a token.
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

    float focusRingWidth = 3.0f;
    float hairline = 1.0f;       // rendered as at least one physical pixel
    float minHitTarget = 24.0f;  // smallest clickable area, even for smaller visuals
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

    static Theme Light();
    static Theme Dark();

    // Sets colors.accent and derives accentHover, accentPressed, accentSubtle, onAccent,
    // accentText and focusRing from it in OKLCH. onAccent is white or black, whichever is
    // more readable, and hover and pressed move the lightness away from it, so text on
    // every accent state reaches at least 4.5:1. accentText depends on the surface colors,
    // so call this after changing them. The accent alpha is ignored.
    void ApplyAccent(const ImVec4& accent);
};

// The current theme. Valid at any time, also before Init; Theme::Light() until SetTheme.
// During an animated switch it is the blend of the previous and the new theme.
const Theme& T();

// Makes `theme` current. With `animate`, while Facet is initialized and motion is not
// reduced, the switch is spread over motion.durationStandard (times motion.scale) of the
// new theme: colors blend in OKLab and lengths linearly, one blend step per facet::NewFrame;
// switches (dark, weights, flags) change at the halfway point. Without `animate` the theme
// changes at once. Either way the library modules are notified of every change.
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

// The color for the outline of a stock control (a checkbox, a text field): separatorOpaque
// mixed toward label until it reaches the 3:1 non-text contrast on bg, bgSecondary and
// bgTertiary. Translucent fills alone stay far below that.
ImVec4 ControlBoundaryColor(const Colors& colors);

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
