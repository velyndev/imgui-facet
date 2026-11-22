// Theme tokens: the light and dark presets, accent derivation, animated theme switches and
// the "follow the system" mode.

#include "facet/core/theme.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>

#include "core/core_internal.h"
#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/context.h"
#include "internal/device.h"
#include "internal/subsystem.h"

namespace facet
{
namespace
{

// Default accents: the same blue hue, each dark enough for white text on it in its theme.
constexpr ImVec4 kLightAccent = Rgb(0x0066CC);
constexpr ImVec4 kDarkAccent = Rgb(0x1F6FEB);

// Accent choices for pickers. Theme::ApplyAccent derives readable states from any of them.
constexpr std::array<AccentPreset, 8> kAccentPresets = {{
    {"Blue", Rgb(0x0066CC)},
    {"Purple", Rgb(0x8944AB)},
    {"Pink", Rgb(0xD30F77)},
    {"Red", Rgb(0xD70015)},
    {"Orange", Rgb(0xC93400)},
    {"Yellow", Rgb(0xFFCC00)},
    {"Green", Rgb(0x248A3D)},
    {"Graphite", Rgb(0x8E8E93)},
}};

// Lightness steps (OKLCH L) of the hover and pressed states of a solid fill (the accent and the
// destructive color).
constexpr float kHoverLightnessStep = 0.04f;
constexpr float kPressedLightnessStep = 0.08f;

// Opacity of accentSubtle in each theme, and of the status washes in both: the brightest status
// color (the dark warning) at the dark accentSubtle opacity would take labelSecondary on it
// below 4.5:1.
constexpr float kSubtleAlphaLight = 0.14f;
constexpr float kSubtleAlphaDark = 0.24f;
constexpr float kStatusSubtleAlpha = 0.14f;

// Hue turn (degrees) from accentText to accentTextVisited.
constexpr float kVisitedHueTurn = 60.0f;

// WCAG AA contrast for normal text, the target of accentText.
constexpr float kTextContrast = 4.5f;
// Lightness step (OKLCH L) of the accentText and focusRing searches.
constexpr float kTextLightnessStep = 0.01f;

// WCAG 2 non-text contrast for the focus ring, the state marks, the chart colors and the outlines
// of the increased contrast variant, and the mix step of the variant's outline search.
constexpr float kBoundaryContrast = 3.0f;
constexpr float kBoundaryMixStep = 0.05f;
// Mix steps the variant's hover outline goes past its rest outline toward label.
constexpr int kBoundaryHoverSteps = 3;

// How often the Windows high contrast setting is read, in seconds of frame time.
constexpr float kSystemContrastReadInterval = 1.0f;

// Chart palette: the chroma of every series, the lightness each theme starts from, and the hue
// offsets from the accent hue in degrees, ordered so that consecutive series differ by at least
// 90 degrees and the first four split the circle into quarters.
constexpr float kChartChroma = 0.14f;
constexpr float kChartLightnessLight = 0.56f;
constexpr float kChartLightnessDark = 0.72f;
constexpr float kChartHueOffsets[kChartPaletteSize] = {0.0f, 180.0f, 90.0f, 270.0f, 45.0f, 225.0f, 135.0f, 315.0f};
// Below this relative luminance of bg the surfaces count as dark: colors on them get lighter.
constexpr float kDarkSurfaceLuminance = 0.18f;

// Where Windows keeps the app mode (1 light, 0 dark).
constexpr wchar_t kPersonalizeKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr wchar_t kAppsUseLightTheme[] = L"AppsUseLightTheme";

// The lowest contrast of `text` on the four surfaces.
float MinSurfaceContrast(const Colors& c, const ImVec4& text)
{
    const ImVec4 surfaces[] = {c.bg,
                               c.bgSecondary,
                               c.bgTertiary,
                               c.bgElevated,
                               c.bgElevatedLevels[0],
                               c.bgElevatedLevels[1],
                               c.bgElevatedLevels[2]};
    float lowest = ContrastRatio(text, surfaces[0]);
    for (const ImVec4& surface : surfaces)
        lowest = std::min(lowest, ContrastRatio(text, surface));
    return lowest;
}

// The lowest contrast of a state mark: on the surfaces, and against fill[0] (the unfilled part of
// a track, an off switch) drawn over each of them.
float MinMarkContrast(const Colors& c, const ImVec4& mark)
{
    const ImVec4 surfaces[] = {c.bg,
                               c.bgSecondary,
                               c.bgTertiary,
                               c.bgElevated,
                               c.bgElevatedLevels[0],
                               c.bgElevatedLevels[1],
                               c.bgElevatedLevels[2]};
    float lowest = MinSurfaceContrast(c, mark);
    for (const ImVec4& surface : surfaces)
        lowest = std::min(lowest, ContrastRatio(mark, Over(c.fill[0], surface)));
    return lowest;
}

// The color rounded to 8 bits per channel, as Dear ImGui draws it.
ImVec4 AsDrawn(const ImVec4& color)
{
    return FromU32(ToU32(color));
}

using ContrastMeasure = float (*)(const Colors& c, const ImVec4& color);

// `color` (made opaque) with its lightness moved away from the surfaces (darker on light
// surfaces, lighter on dark ones) in small steps until `measure` reaches `contrast`. With `drawn`,
// each candidate is rounded to 8 bits per channel first, so the contrast holds for the color exactly
// as it is drawn. Black and white end the search, and one of them always reaches 4.5:1 on the
// preset surfaces.
ImVec4 MoveUntilContrast(const Colors& c, const ImVec4& color, bool darkSurfaces, float contrast, bool drawn,
                         ContrastMeasure measure = &MinSurfaceContrast)
{
    const Oklch lch = ToOklch(color);
    const float direction = darkSurfaces ? 1.0f : -1.0f;
    for (int step = 0;; ++step)
    {
        const float L = lch.L + direction * kTextLightnessStep * static_cast<float>(step);
        if (L < 0.0f || L > 1.0f)
            break;
        // Step 0 is the color itself, so one that already reaches the contrast stays exact.
        const ImVec4 exact = step == 0 ? WithAlpha(color, 1.0f) : FromOklch({L, lch.C, lch.h});
        const ImVec4 candidate = drawn ? AsDrawn(exact) : exact;
        if (measure(c, candidate) >= contrast)
            return candidate;
    }
    return darkSurfaces ? Rgb(0xFFFFFF) : Rgb(0x000000);
}

// The outline colors of the increased contrast variant: separatorOpaque mixed toward label until
// the color, as drawn, reaches 3:1 on every surface; the hover outline kBoundaryHoverSteps further.
// The divider takes the rest outline, so every line of the variant reads at 3:1.
void ApplyIncreasedContrastColors(Colors& c)
{
    int found = -1;
    for (int step = 0; static_cast<float>(step) * kBoundaryMixStep <= 1.0f; ++step)
    {
        const ImVec4 candidate = AsDrawn(Mix(c.separatorOpaque, c.label, static_cast<float>(step) * kBoundaryMixStep));
        if (MinSurfaceContrast(c, candidate) >= kBoundaryContrast)
        {
            found = step;
            break;
        }
    }
    if (found < 0)
    {
        c.controlBorder = c.label;
        c.controlBorderHover = c.label;
        c.separator = c.label;
        return;
    }
    const float rest = static_cast<float>(found) * kBoundaryMixStep;
    const float hover = std::min(1.0f, static_cast<float>(found + kBoundaryHoverSteps) * kBoundaryMixStep);
    c.controlBorder = AsDrawn(Mix(c.separatorOpaque, c.label, rest));
    c.controlBorderHover = AsDrawn(Mix(c.separatorOpaque, c.label, hover));
    c.separator = c.controlBorder;
}

// The chart palette: kChartPaletteSize hues around the accent hue at one chroma, each starting
// from the theme's series lightness and moved until it reaches 3:1 on every surface.
void DeriveChartPalette(Colors& c)
{
    const bool darkSurfaces = RelativeLuminance(c.bg) < kDarkSurfaceLuminance;
    const float accentHue = ToOklch(c.accent).h;
    const float lightness = darkSurfaces ? kChartLightnessDark : kChartLightnessLight;
    for (int i = 0; i < kChartPaletteSize; ++i)
    {
        const float hue = std::fmod(accentHue + kChartHueOffsets[i], 360.0f);
        c.chart[i] = MoveUntilContrast(c, FromOklch({lightness, kChartChroma, hue}), darkSurfaces, kBoundaryContrast,
                                       true);
    }
}

// The text color on a solid fill and the fill's hover and pressed states: the text is white or
// black, whichever is more readable, and the states move the lightness away from it, which only
// raises the contrast.
struct SolidFillStates
{
    ImVec4 on;
    ImVec4 hover;
    ImVec4 pressed;
};

SolidFillStates DeriveSolidFillStates(const ImVec4& fill)
{
    SolidFillStates states;
    states.on = ReadableOn(fill);
    const float direction = states.on.x > 0.5f ? -1.0f : 1.0f;
    const Oklch lch = ToOklch(fill);
    const auto shifted = [&](float step) {
        return FromOklch({std::clamp(lch.L + direction * step, 0.0f, 1.0f), lch.C, lch.h});
    };
    states.hover = shifted(kHoverLightnessStep);
    states.pressed = shifted(kPressedLightnessStep);
    return states;
}

// The states of the status colors: the destructive fill states, the text on solid success and
// warning fills, and the three translucent washes.
void DeriveStatusStates(Colors& c)
{
    const SolidFillStates destructive = DeriveSolidFillStates(c.destructive);
    c.onDestructive = destructive.on;
    c.destructiveHover = destructive.hover;
    c.destructivePressed = destructive.pressed;
    c.onSuccess = ReadableOn(c.success);
    c.onWarning = ReadableOn(c.warning);
    c.destructiveSubtle = WithAlpha(c.destructive, kStatusSubtleAlpha);
    c.successSubtle = WithAlpha(c.success, kStatusSubtleAlpha);
    c.warningSubtle = WithAlpha(c.warning, kStatusSubtleAlpha);
}

void SetElevation(Elevation& elevation, float opacity1, float opacity2, float opacity3)
{
    elevation.levels[0] = {{0.0f, 1.0f}, 3.0f, 0.0f, opacity1};
    elevation.levels[1] = {{0.0f, 4.0f}, 16.0f, 0.0f, opacity2};
    elevation.levels[2] = {{0.0f, 12.0f}, 40.0f, 0.0f, opacity3};
}

// --- Blending ----------------------------------------------------------------------------

float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

ImVec2 Lerp(ImVec2 a, ImVec2 b, float t)
{
    return ImVec2(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t));
}

SpringSpec Lerp(const SpringSpec& a, const SpringSpec& b, float t)
{
    return {Lerp(a.response, b.response, t), Lerp(a.dampingRatio, b.dampingRatio, t)};
}

template <class T>
const T& Pick(const T& a, const T& b, float t)
{
    return t < 0.5f ? a : b;
}

Colors MixColors(const Colors& a, const Colors& b, float t)
{
    Colors out;
    out.bg = Mix(a.bg, b.bg, t);
    out.bgSecondary = Mix(a.bgSecondary, b.bgSecondary, t);
    out.bgTertiary = Mix(a.bgTertiary, b.bgTertiary, t);
    out.bgElevated = Mix(a.bgElevated, b.bgElevated, t);
    for (int i = 0; i < 3; ++i)
        out.bgElevatedLevels[i] = Mix(a.bgElevatedLevels[i], b.bgElevatedLevels[i], t);
    for (int i = 0; i < 4; ++i)
        out.fill[i] = Mix(a.fill[i], b.fill[i], t);
    out.label = Mix(a.label, b.label, t);
    out.labelSecondary = Mix(a.labelSecondary, b.labelSecondary, t);
    out.labelTertiary = Mix(a.labelTertiary, b.labelTertiary, t);
    out.labelQuaternary = Mix(a.labelQuaternary, b.labelQuaternary, t);
    out.separator = Mix(a.separator, b.separator, t);
    out.separatorOpaque = Mix(a.separatorOpaque, b.separatorOpaque, t);
    out.accent = Mix(a.accent, b.accent, t);
    out.accentHover = Mix(a.accentHover, b.accentHover, t);
    out.accentPressed = Mix(a.accentPressed, b.accentPressed, t);
    out.accentSubtle = Mix(a.accentSubtle, b.accentSubtle, t);
    out.onAccent = Mix(a.onAccent, b.onAccent, t);
    out.accentText = Mix(a.accentText, b.accentText, t);
    out.destructive = Mix(a.destructive, b.destructive, t);
    out.success = Mix(a.success, b.success, t);
    out.warning = Mix(a.warning, b.warning, t);
    out.focusRing = Mix(a.focusRing, b.focusRing, t);
    out.shadow = Mix(a.shadow, b.shadow, t);
    out.glassTint = Mix(a.glassTint, b.glassTint, t);
    out.scrim = Mix(a.scrim, b.scrim, t);
    for (int i = 0; i < kChartPaletteSize; ++i)
        out.chart[i] = Mix(a.chart[i], b.chart[i], t);
    out.scrollThumb = Mix(a.scrollThumb, b.scrollThumb, t);
    out.scrollThumbHover = Mix(a.scrollThumbHover, b.scrollThumbHover, t);
    out.destructiveHover = Mix(a.destructiveHover, b.destructiveHover, t);
    out.destructivePressed = Mix(a.destructivePressed, b.destructivePressed, t);
    out.onDestructive = Mix(a.onDestructive, b.onDestructive, t);
    out.onSuccess = Mix(a.onSuccess, b.onSuccess, t);
    out.onWarning = Mix(a.onWarning, b.onWarning, t);
    out.destructiveSubtle = Mix(a.destructiveSubtle, b.destructiveSubtle, t);
    out.successSubtle = Mix(a.successSubtle, b.successSubtle, t);
    out.warningSubtle = Mix(a.warningSubtle, b.warningSubtle, t);
    out.accentTextVisited = Mix(a.accentTextVisited, b.accentTextVisited, t);
    out.codeKeyword = Mix(a.codeKeyword, b.codeKeyword, t);
    out.codeString = Mix(a.codeString, b.codeString, t);
    out.codeNumber = Mix(a.codeNumber, b.codeNumber, t);
    for (int i = 0; i < 2; ++i)
        out.checker[i] = Mix(a.checker[i], b.checker[i], t);
    out.controlBorder = Mix(a.controlBorder, b.controlBorder, t);
    out.controlBorderHover = Mix(a.controlBorderHover, b.controlBorderHover, t);
    out.knob = Mix(a.knob, b.knob, t);
    return out;
}

// Whole-number tokens (counts of rows and toasts) switch at the halfway point like enums.
int Lerp(int a, int b, float t)
{
    return t < 0.5f ? a : b;
}

OverlayTokens MixOverlay(const OverlayTokens& a, const OverlayTokens& b, float t)
{
    OverlayTokens out;
    out.anchorGap = Lerp(a.anchorGap, b.anchorGap, t);
    out.viewportMargin = Lerp(a.viewportMargin, b.viewportMargin, t);
    out.menuMinWidth = Lerp(a.menuMinWidth, b.menuMinWidth, t);
    out.submenuDelay = Lerp(a.submenuDelay, b.submenuDelay, t);
    out.tooltipDelay = Lerp(a.tooltipDelay, b.tooltipDelay, t);
    out.tooltipGrace = Lerp(a.tooltipGrace, b.tooltipGrace, t);
    out.tooltipMaxWidth = Lerp(a.tooltipMaxWidth, b.tooltipMaxWidth, t);
    out.toastDuration = Lerp(a.toastDuration, b.toastDuration, t);
    out.toastWidth = Lerp(a.toastWidth, b.toastWidth, t);
    out.toastStackGap = Lerp(a.toastStackGap, b.toastStackGap, t);
    out.toastMargin = Lerp(a.toastMargin, b.toastMargin, t);
    out.toastMaxCount = Lerp(a.toastMaxCount, b.toastMaxCount, t);
    out.modalWidth = Lerp(a.modalWidth, b.modalWidth, t);
    out.drawerWidth = Lerp(a.drawerWidth, b.drawerWidth, t);
    out.drawerHeight = Lerp(a.drawerHeight, b.drawerHeight, t);
    out.commandPaletteWidth = Lerp(a.commandPaletteWidth, b.commandPaletteWidth, t);
    out.menuShortcutGap = Lerp(a.menuShortcutGap, b.menuShortcutGap, t);
    out.arrowSize = Lerp(a.arrowSize, b.arrowSize, t);
    out.commandPaletteTop = Lerp(a.commandPaletteTop, b.commandPaletteTop, t);
    return out;
}

ContainerTokens MixContainers(const ContainerTokens& a, const ContainerTokens& b, float t)
{
    ContainerTokens out;
    out.shellSize = Lerp(a.shellSize, b.shellSize, t);
    out.shellMinSize = Lerp(a.shellMinSize, b.shellMinSize, t);
    out.titleBarHeight = Lerp(a.titleBarHeight, b.titleBarHeight, t);
    out.resizeBorder = Lerp(a.resizeBorder, b.resizeBorder, t);
    out.sidebarWidth = Lerp(a.sidebarWidth, b.sidebarWidth, t);
    out.railWidth = Lerp(a.railWidth, b.railWidth, t);
    out.inspectorWidth = Lerp(a.inspectorWidth, b.inspectorWidth, t);
    out.inspectorMinWidth = Lerp(a.inspectorMinWidth, b.inspectorMinWidth, t);
    out.statusBarHeight = Lerp(a.statusBarHeight, b.statusBarHeight, t);
    return out;
}

ScrollTokens MixScroll(const ScrollTokens& a, const ScrollTokens& b, float t)
{
    ScrollTokens out;
    out.barThickness = Lerp(a.barThickness, b.barThickness, t);
    out.barThicknessHover = Lerp(a.barThicknessHover, b.barThicknessHover, t);
    out.barInset = Lerp(a.barInset, b.barInset, t);
    out.minThumbLength = Lerp(a.minThumbLength, b.minThumbLength, t);
    out.barHideDelay = Lerp(a.barHideDelay, b.barHideDelay, t);
    out.wheelDistance = Lerp(a.wheelDistance, b.wheelDistance, t);
    out.edgeFade = Lerp(a.edgeFade, b.edgeFade, t);
    return out;
}

ListTokens MixList(const ListTokens& a, const ListTokens& b, float t)
{
    ListTokens out;
    out.rowHeightSmall = Lerp(a.rowHeightSmall, b.rowHeightSmall, t);
    out.rowHeight = Lerp(a.rowHeight, b.rowHeight, t);
    out.rowHeightLarge = Lerp(a.rowHeightLarge, b.rowHeightLarge, t);
    out.indent = Lerp(a.indent, b.indent, t);
    out.popupRows = Lerp(a.popupRows, b.popupRows, t);
    out.inlineRows = Lerp(a.inlineRows, b.inlineRows, t);
    out.typeaheadTimeout = Lerp(a.typeaheadTimeout, b.typeaheadTimeout, t);
    return out;
}

DragTokens MixDrag(const DragTokens& a, const DragTokens& b, float t)
{
    DragTokens out;
    out.startThreshold = Lerp(a.startThreshold, b.startThreshold, t);
    out.ghostOpacity = Lerp(a.ghostOpacity, b.ghostOpacity, t);
    out.autoScrollZone = Lerp(a.autoScrollZone, b.autoScrollZone, t);
    out.autoScrollSpeed = Lerp(a.autoScrollSpeed, b.autoScrollSpeed, t);
    out.hoverOpenDelay = Lerp(a.hoverOpenDelay, b.hoverOpenDelay, t);
    out.flickSpeed = Lerp(a.flickSpeed, b.flickSpeed, t);
    out.inertiaTime = Lerp(a.inertiaTime, b.inertiaTime, t);
    return out;
}

FieldTokens MixField(const FieldTokens& a, const FieldTokens& b, float t)
{
    FieldTokens out;
    out.padding = Lerp(a.padding, b.padding, t);
    out.caretWidth = Lerp(a.caretWidth, b.caretWidth, t);
    return out;
}

TabTokens MixTabs(const TabTokens& a, const TabTokens& b, float t)
{
    TabTokens out;
    out.minWidth = Lerp(a.minWidth, b.minWidth, t);
    out.maxWidth = Lerp(a.maxWidth, b.maxWidth, t);
    out.underlineHeight = Lerp(a.underlineHeight, b.underlineHeight, t);
    return out;
}

TableTokens MixTable(const TableTokens& a, const TableTokens& b, float t)
{
    TableTokens out;
    out.headerHeight = Lerp(a.headerHeight, b.headerHeight, t);
    out.minColumnWidth = Lerp(a.minColumnWidth, b.minColumnWidth, t);
    out.resizeBand = Lerp(a.resizeBand, b.resizeBand, t);
    return out;
}

PickerTokens MixPicker(const PickerTokens& a, const PickerTokens& b, float t)
{
    PickerTokens out;
    out.dayCell = Lerp(a.dayCell, b.dayCell, t);
    out.colorBarHeight = Lerp(a.colorBarHeight, b.colorBarHeight, t);
    out.hueRingWidth = Lerp(a.hueRingWidth, b.hueRingWidth, t);
    out.timeColumnWidth = Lerp(a.timeColumnWidth, b.timeColumnWidth, t);
    return out;
}

ChartTokens MixCharts(const ChartTokens& a, const ChartTokens& b, float t)
{
    ChartTokens out;
    out.lineWidth = Lerp(a.lineWidth, b.lineWidth, t);
    out.markerSize = Lerp(a.markerSize, b.markerSize, t);
    out.barGap = Lerp(a.barGap, b.barGap, t);
    out.legendSwatch = Lerp(a.legendSwatch, b.legendSwatch, t);
    out.donutHole = Lerp(a.donutHole, b.donutHole, t);
    out.gaugeWidth = Lerp(a.gaugeWidth, b.gaugeWidth, t);
    out.meterSegmentGap = Lerp(a.meterSegmentGap, b.meterSegmentGap, t);
    out.peakHold = Lerp(a.peakHold, b.peakHold, t);
    return out;
}

MediaTokens MixMedia(const MediaTokens& a, const MediaTokens& b, float t)
{
    MediaTokens out;
    out.avatarOverlap = Lerp(a.avatarOverlap, b.avatarOverlap, t);
    out.playheadWidth = Lerp(a.playheadWidth, b.playheadWidth, t);
    out.dashLength = Lerp(a.dashLength, b.dashLength, t);
    out.dashGap = Lerp(a.dashGap, b.dashGap, t);
    return out;
}

CompositeTokens MixComposites(const CompositeTokens& a, const CompositeTokens& b, float t)
{
    CompositeTokens out;
    out.editorHandleSize = Lerp(a.editorHandleSize, b.editorHandleSize, t);
    out.gradientStopSize = Lerp(a.gradientStopSize, b.gradientStopSize, t);
    out.stopRemoveDistance = Lerp(a.stopRemoveDistance, b.stopRemoveDistance, t);
    out.easingPreviewTime = Lerp(a.easingPreviewTime, b.easingPreviewTime, t);
    out.propertyLabelMinWidth = Lerp(a.propertyLabelMinWidth, b.propertyLabelMinWidth, t);
    out.searchFieldMinWidth = Lerp(a.searchFieldMinWidth, b.searchFieldMinWidth, t);
    out.keybindColumnWidth = Lerp(a.keybindColumnWidth, b.keybindColumnWidth, t);
    out.inventoryTintHeight = Lerp(a.inventoryTintHeight, b.inventoryTintHeight, t);
    out.mediaCoverAspect = Lerp(a.mediaCoverAspect, b.mediaCoverAspect, t);
    out.mediaCardHorizontalBelow = Lerp(a.mediaCardHorizontalBelow, b.mediaCardHorizontalBelow, t);
    out.canvasSnapDistance = Lerp(a.canvasSnapDistance, b.canvasSnapDistance, t);
    out.canvasHandleSize = Lerp(a.canvasHandleSize, b.canvasHandleSize, t);
    return out;
}

// --- Current theme and transition state -----------------------------------------------------

struct ThemeState
{
    // What T() returns: `shown` with the increased contrast variant applied while it is on.
    Theme current = Theme::Light();
    // The theme on screen as it was set (a blend step during a switch), standard border colors.
    Theme shown = Theme::Light();
    Theme from;
    Theme to;
    float elapsed = 0.0f;
    float duration = 0.0f;
    bool blending = false;

    ThemeMode mode = ThemeMode::Light;
    bool hasUserAccent = false;
    ImVec4 userAccent;
    bool systemDark = false;

    std::uint64_t revision = 1;

    // Change notification of the app mode registry value, armed while initialized.
    HKEY personalizeKey = nullptr;
    HANDLE personalizeEvent = nullptr;

    // The increased contrast variant: the application's switch, whether the Windows high contrast
    // setting counts, its last reading, and the time until the next one.
    bool increasedContrast = false;
    bool followSystemContrast = false;
    bool systemContrast = false;
    float systemContrastTimer = 0.0f;
};

ThemeState& State()
{
    static ThemeState state;
    return state;
}

// Accents pushed with PushAccent in this frame, and the themes T() returns while they are pushed.
// Each depth has its own theme, so a reference taken from T() in an outer scope keeps its accent
// while a nested scope is pushed, read and popped.
struct AccentScopes
{
    ImVec4 accents[kAccentStackDepth];
    int depth = 0;     // entries in `accents`
    int overflow = 0;  // pushes beyond the capacity, ignored but balanced by their pops
    Theme scoped[kAccentStackDepth];  // scoped[i]: the current theme with accents[i] applied
    // Theme revision scoped[i] was built from; 0 rebuilds it.
    std::uint64_t scopedRevision[kAccentStackDepth] = {};
};

AccentScopes& Scopes()
{
    static AccentScopes scopes;
    return scopes;
}

void ClearAccentScopes()
{
    AccentScopes& scopes = Scopes();
    scopes.depth = 0;
    scopes.overflow = 0;
    for (std::uint64_t& revision : scopes.scopedRevision)
        revision = 0;
}

// Reports pushes without a pop and clears them, so every frame starts unscoped.
void ThemeEndFrame()
{
    AccentScopes& scopes = Scopes();
    const int open = scopes.depth + scopes.overflow;
    if (open > 0)
        internal::ReportUsageError("PushAccent without PopAccent: %d accent(s) still pushed at EndFrame", open);
    ClearAccentScopes();
}

void Changed()
{
    ++State().revision;
    internal::NotifyThemeChanged();
}

bool ContrastVariantOn(const ThemeState& state)
{
    return state.increasedContrast || (state.followSystemContrast && state.systemContrast);
}

// Builds what T() returns from the theme on screen.
void Resolve()
{
    ThemeState& state = State();
    state.current = state.shown;
    if (ContrastVariantOn(state))
        ApplyIncreasedContrastColors(state.current.colors);
}

bool ReadSystemContrast()
{
    HIGHCONTRASTW contrast = {};
    contrast.cbSize = sizeof(contrast);
    if (!SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0))
        return false;
    return (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

bool SameColor(const ImVec4& a, const ImVec4& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

// A theme passed to SetTheme while the variant is on: border colors that equal what T() shows
// came back from T() and stand for the standard colors they replaced.
Theme WithStandardBorders(const Theme& theme)
{
    const ThemeState& state = State();
    Theme standard = theme;
    if (!ContrastVariantOn(state))
        return standard;
    const Colors& shown = state.current.colors;
    const Colors& original = state.shown.colors;
    Colors& c = standard.colors;
    if (SameColor(c.separator, shown.separator))
        c.separator = original.separator;
    if (SameColor(c.controlBorder, shown.controlBorder))
        c.controlBorder = original.controlBorder;
    if (SameColor(c.controlBorderHover, shown.controlBorderHover))
        c.controlBorderHover = original.controlBorderHover;
    return standard;
}

bool ReadSystemDark()
{
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, kPersonalizeKey, kAppsUseLightTheme, RRF_RT_REG_DWORD,
                                        nullptr, &value, &size);
    return status == ERROR_SUCCESS && value == 0;
}

// The theme a mode switch moves to: the colors and elevation of the light or dark preset on
// top of the app's current theme, so spacing, radii, metrics, type, motion and glass settings
// made with SetTheme survive the switch. During a running transition the theme it moves to
// counts as the current one.
Theme PresetFor(ThemeMode mode)
{
    ThemeState& state = State();
    bool dark = mode == ThemeMode::Dark;
    if (mode == ThemeMode::System)
        dark = state.systemDark;
    const Theme preset = dark ? Theme::Dark() : Theme::Light();
    Theme theme = state.blending ? state.to : state.shown;
    theme.dark = preset.dark;
    theme.colors = preset.colors;
    theme.elevation = preset.elevation;
    if (state.hasUserAccent)
        theme.ApplyAccent(state.userAccent);
    return theme;
}

void FinishTransition()
{
    ThemeState& state = State();
    if (!state.blending)
        return;
    state.blending = false;
    state.shown = state.to;
    Resolve();
}

void ApplyTheme(const Theme& theme, bool animate)
{
    ThemeState& state = State();
    const float duration = theme.motion.durationStandard * theme.motion.scale;
    const bool reduced = ReduceMotion() || theme.motion.reduceMotion;
    if (!animate || !IsInitialized() || reduced || duration <= 0.0f)
    {
        state.blending = false;
        state.shown = theme;
        Resolve();
        Changed();
        return;
    }
    // A new switch during a running one starts from what is on screen now.
    state.from = state.shown;
    state.to = theme;
    state.elapsed = 0.0f;
    state.duration = duration;
    state.blending = true;
}

void ArmSystemNotification()
{
    ThemeState& state = State();
    if (state.personalizeKey == nullptr || state.personalizeEvent == nullptr)
        return;
    if (RegNotifyChangeKeyValue(state.personalizeKey, FALSE, REG_NOTIFY_CHANGE_LAST_SET, state.personalizeEvent,
                                TRUE) != ERROR_SUCCESS)
    {
        CloseHandle(state.personalizeEvent);
        state.personalizeEvent = nullptr;
    }
}

bool ThemeInit()
{
    ThemeState& state = State();
    state.systemDark = ReadSystemDark();
    const bool contrastBefore = ContrastVariantOn(state);
    state.systemContrast = ReadSystemContrast();
    state.systemContrastTimer = kSystemContrastReadInterval;
    if (ContrastVariantOn(state) != contrastBefore)
    {
        Resolve();
        Changed();
    }
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kPersonalizeKey, 0, KEY_NOTIFY | KEY_QUERY_VALUE, &state.personalizeKey) !=
        ERROR_SUCCESS)
    {
        state.personalizeKey = nullptr;
        return true;  // no app mode on this system: System mode stays light
    }
    state.personalizeEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ArmSystemNotification();
    if (state.mode == ThemeMode::System && state.current.dark != state.systemDark)
        ApplyTheme(PresetFor(ThemeMode::System), false);
    return true;
}

void ThemeShutdown()
{
    ThemeState& state = State();
    ClearAccentScopes();
    FinishTransition();
    if (state.personalizeEvent != nullptr)
        CloseHandle(state.personalizeEvent);
    if (state.personalizeKey != nullptr)
        RegCloseKey(state.personalizeKey);
    state.personalizeEvent = nullptr;
    state.personalizeKey = nullptr;
}

void PollSystemMode()
{
    ThemeState& state = State();
    if (state.personalizeEvent == nullptr || WaitForSingleObject(state.personalizeEvent, 0) != WAIT_OBJECT_0)
        return;
    ArmSystemNotification();
    const bool dark = ReadSystemDark();
    if (dark == state.systemDark)
        return;
    state.systemDark = dark;
    if (state.mode == ThemeMode::System)
        ApplyTheme(PresetFor(ThemeMode::System), true);
}

void AdvanceTransition()
{
    ThemeState& state = State();
    if (!state.blending)
        return;
    state.elapsed += std::max(0.0f, internal::DeltaTime());
    const float progress = state.duration > 0.0f ? state.elapsed / state.duration : 1.0f;
    if (progress >= 1.0f)
    {
        state.blending = false;
        state.shown = state.to;
    }
    else
    {
        state.shown = MixThemes(state.from, state.to, EaseValue(Ease::InOutCubic, progress));
    }
    Resolve();
    Changed();
}

void PollSystemContrast()
{
    ThemeState& state = State();
    state.systemContrastTimer -= std::max(0.0f, internal::DeltaTime());
    if (state.systemContrastTimer > 0.0f)
        return;
    state.systemContrastTimer = kSystemContrastReadInterval;
    const bool before = ContrastVariantOn(state);
    state.systemContrast = ReadSystemContrast();
    if (ContrastVariantOn(state) == before)
        return;
    Resolve();
    Changed();
}

void ThemeNewFrame()
{
    PollSystemMode();
    PollSystemContrast();
    AdvanceTransition();
}

} // namespace

Theme Theme::Light()
{
    Theme theme;
    theme.dark = false;
    Colors& c = theme.colors;
    c.bg = Rgb(0xFFFFFF);
    c.bgSecondary = Rgb(0xF2F2F7);
    c.bgTertiary = Rgb(0xE5E5EA);
    c.bgElevated = Rgb(0xFFFFFF);
    c.bgElevatedLevels[0] = Rgb(0xFFFFFF);
    c.bgElevatedLevels[1] = c.bgElevated;
    c.bgElevatedLevels[2] = Rgb(0xFFFFFF);
    c.fill[0] = Rgb(0x787880, 0.20f);
    c.fill[1] = Rgb(0x787880, 0.16f);
    c.fill[2] = Rgb(0x787880, 0.12f);
    c.fill[3] = Rgb(0x787880, 0.08f);
    c.label = Rgb(0x000000);
    c.labelSecondary = Rgb(0x000000, 0.60f);
    c.labelTertiary = Rgb(0x000000, 0.30f);
    c.labelQuaternary = Rgb(0x000000, 0.16f);
    c.separator = Rgb(0x000000, 0.10f);
    c.separatorOpaque = Rgb(0xE5E5E7);
    c.controlBorder = Rgb(0x000000, 0.14f);
    c.controlBorderHover = Rgb(0x000000, 0.22f);
    c.knob = Rgb(0xFFFFFF);
    c.destructive = Rgb(0xC4000F);
    c.success = Rgb(0x146C2E);
    c.warning = Rgb(0x8A5300);
    c.shadow = Rgb(0x000000);
    c.glassTint = Rgb(0xFFFFFF);
    c.scrim = Rgb(0x000000, 0.20f);
    c.scrollThumb = Rgb(0x000000, 0.45f);
    c.scrollThumbHover = Rgb(0x000000, 0.60f);
    c.codeKeyword = Rgb(0x7A1FA2);
    c.codeString = Rgb(0x1A6B2B);
    c.codeNumber = Rgb(0x9A4A00);
    c.checker[0] = Rgb(0xFFFFFF);
    c.checker[1] = Rgb(0xD1D1D6);
    SetElevation(theme.elevation, 0.10f, 0.12f, 0.18f);
    theme.ApplyAccent(kLightAccent);
    return theme;
}

Theme Theme::Dark()
{
    Theme theme;
    theme.dark = true;
    Colors& c = theme.colors;
    // Near-black greys stepping up from black: depth comes from the lighter surface, not from an edge.
    c.bg = Rgb(0x000000);
    c.bgSecondary = Rgb(0x111113);
    c.bgTertiary = Rgb(0x19191C);
    c.bgElevated = Rgb(0x1E1E21);
    // A black shadow cannot darken the black background, so height shows as a lighter surface.
    c.bgElevatedLevels[0] = Rgb(0x18181B);
    c.bgElevatedLevels[1] = c.bgElevated;
    c.bgElevatedLevels[2] = Rgb(0x26262A);
    c.fill[0] = Rgb(0xFFFFFF, 0.14f);
    c.fill[1] = Rgb(0xFFFFFF, 0.10f);
    c.fill[2] = Rgb(0xFFFFFF, 0.07f);
    c.fill[3] = Rgb(0xFFFFFF, 0.04f);
    c.label = Rgb(0xFFFFFF);
    c.labelSecondary = Rgb(0xFFFFFF, 0.60f);
    c.labelTertiary = Rgb(0xFFFFFF, 0.32f);
    c.labelQuaternary = Rgb(0xFFFFFF, 0.18f);
    c.separator = Rgb(0xFFFFFF, 0.08f);
    c.separatorOpaque = Rgb(0x242426);
    c.controlBorder = Rgb(0xFFFFFF, 0.14f);
    c.controlBorderHover = Rgb(0xFFFFFF, 0.22f);
    c.knob = Rgb(0xFFFFFF);
    c.destructive = Rgb(0xFF6B5F);
    c.success = Rgb(0x32D74B);
    c.warning = Rgb(0xFFD60A);
    c.shadow = Rgb(0x000000);
    c.glassTint = Rgb(0x111113);
    c.scrim = Rgb(0x000000, 0.55f);
    c.scrollThumb = Rgb(0xFFFFFF, 0.45f);
    c.scrollThumbHover = Rgb(0xFFFFFF, 0.62f);
    c.codeKeyword = Rgb(0xD7A8FF);
    c.codeString = Rgb(0x7EE08F);
    c.codeNumber = Rgb(0xFFB86B);
    c.checker[0] = Rgb(0x2A2A2D);
    c.checker[1] = Rgb(0x19191C);
    SetElevation(theme.elevation, 0.36f, 0.44f, 0.56f);
    theme.ApplyAccent(kDarkAccent);
    return theme;
}

void Theme::ApplyAccent(const ImVec4& accent)
{
    Colors& c = colors;
    c.accent = WithAlpha(accent, 1.0f);
    const SolidFillStates states = DeriveSolidFillStates(c.accent);
    c.onAccent = states.on;
    c.accentHover = states.hover;
    c.accentPressed = states.pressed;

    c.accentText = MoveUntilContrast(c, c.accent, dark, kTextContrast, false);
    const Oklch text = ToOklch(c.accentText);
    c.accentTextVisited = MoveUntilContrast(c, FromOklch({text.L, text.C, std::fmod(text.h + kVisitedHueTurn, 360.0f)}),
                                            dark, kTextContrast, false);
    c.accentSubtle = WithAlpha(c.accent, dark ? kSubtleAlphaDark : kSubtleAlphaLight);
    // The ring sits beside the control on whatever surface holds it, so it needs the non-text
    // contrast on every surface, not only on the one the accent was chosen for. The same color marks
    // the filled part of a track and an on switch next to fill[0], so it reaches 3:1 against that too.
    c.focusRing = MoveUntilContrast(c, c.accent, dark, kBoundaryContrast, true, &MinMarkContrast);
    DeriveChartPalette(c);
    DeriveStatusStates(c);
}

Theme MixThemes(const Theme& a, const Theme& b, float t)
{
    if (!(t > 0.0f))
        return a;
    if (t >= 1.0f)
        return b;

    Theme out = Pick(a, b, t);  // switches and enums
    out.colors = MixColors(a.colors, b.colors, t);

    const Spacing& sa = a.spacing;
    const Spacing& sb = b.spacing;
    out.spacing = {Lerp(sa.xs, sb.xs, t),   Lerp(sa.sm, sb.sm, t),   Lerp(sa.md, sb.md, t),
                   Lerp(sa.lg, sb.lg, t),   Lerp(sa.xl, sb.xl, t),   Lerp(sa.xxl, sb.xxl, t),
                   Lerp(sa.xxxl, sb.xxxl, t)};

    out.radii.element = Lerp(a.radii.element, b.radii.element, t);
    out.radii.control = Lerp(a.radii.control, b.radii.control, t);
    out.radii.card = Lerp(a.radii.card, b.radii.card, t);
    out.radii.popover = Lerp(a.radii.popover, b.radii.popover, t);
    out.radii.window = Lerp(a.radii.window, b.radii.window, t);
    out.radii.continuousFrom = Lerp(a.radii.continuousFrom, b.radii.continuousFrom, t);

    const Metrics& ma = a.metrics;
    const Metrics& mb = b.metrics;
    Metrics& m = out.metrics;
    m.controlHeightSmall = Lerp(ma.controlHeightSmall, mb.controlHeightSmall, t);
    m.controlHeight = Lerp(ma.controlHeight, mb.controlHeight, t);
    m.controlHeightLarge = Lerp(ma.controlHeightLarge, mb.controlHeightLarge, t);
    m.toggleWidth = Lerp(ma.toggleWidth, mb.toggleWidth, t);
    m.toggleHeight = Lerp(ma.toggleHeight, mb.toggleHeight, t);
    m.toggleKnobInset = Lerp(ma.toggleKnobInset, mb.toggleKnobInset, t);
    m.checkboxSize = Lerp(ma.checkboxSize, mb.checkboxSize, t);
    m.sliderTrackHeight = Lerp(ma.sliderTrackHeight, mb.sliderTrackHeight, t);
    m.sliderKnobSize = Lerp(ma.sliderKnobSize, mb.sliderKnobSize, t);
    m.focusRingWidth = Lerp(ma.focusRingWidth, mb.focusRingWidth, t);
    m.hairline = Lerp(ma.hairline, mb.hairline, t);
    m.minHitTarget = Lerp(ma.minHitTarget, mb.minHitTarget, t);
    m.faderLength = Lerp(ma.faderLength, mb.faderLength, t);
    m.dialSweep = Lerp(ma.dialSweep, mb.dialSweep, t);
    m.padSize = Lerp(ma.padSize, mb.padSize, t);
    m.pickerWidth = Lerp(ma.pickerWidth, mb.pickerWidth, t);
    m.numberFieldWidth = Lerp(ma.numberFieldWidth, mb.numberFieldWidth, t);
    m.histogramHeight = Lerp(ma.histogramHeight, mb.histogramHeight, t);
    m.chartHeight = Lerp(ma.chartHeight, mb.chartHeight, t);
    m.panelHeight = Lerp(ma.panelHeight, mb.panelHeight, t);
    m.cardWidth = Lerp(ma.cardWidth, mb.cardWidth, t);
    m.thumbnailSize = Lerp(ma.thumbnailSize, mb.thumbnailSize, t);
    m.slotSize = Lerp(ma.slotSize, mb.slotSize, t);
    m.statusDotSize = Lerp(ma.statusDotSize, mb.statusDotSize, t);
    m.pageDotSize = Lerp(ma.pageDotSize, mb.pageDotSize, t);
    m.sliderTickSize = Lerp(ma.sliderTickSize, mb.sliderTickSize, t);
    m.segmentInset = Lerp(ma.segmentInset, mb.segmentInset, t);
    m.progressHeightSmall = Lerp(ma.progressHeightSmall, mb.progressHeightSmall, t);
    m.progressHeight = Lerp(ma.progressHeight, mb.progressHeight, t);
    m.progressHeightLarge = Lerp(ma.progressHeightLarge, mb.progressHeightLarge, t);
    m.emptyIconSize = Lerp(ma.emptyIconSize, mb.emptyIconSize, t);
    m.stepMarkerSize = Lerp(ma.stepMarkerSize, mb.stepMarkerSize, t);
    m.stepConnectorWidth = Lerp(ma.stepConnectorWidth, mb.stepConnectorWidth, t);
    m.checkerCell = Lerp(ma.checkerCell, mb.checkerCell, t);

    for (int i = 0; i < kTextStyleCount; ++i)
        out.type.styles[i].size = Lerp(a.type.styles[i].size, b.type.styles[i].size, t);

    Motion& motion = out.motion;
    motion.quick = Lerp(a.motion.quick, b.motion.quick, t);
    motion.standard = Lerp(a.motion.standard, b.motion.standard, t);
    motion.gentle = Lerp(a.motion.gentle, b.motion.gentle, t);
    motion.durationFast = Lerp(a.motion.durationFast, b.motion.durationFast, t);
    motion.durationStandard = Lerp(a.motion.durationStandard, b.motion.durationStandard, t);
    motion.durationSlow = Lerp(a.motion.durationSlow, b.motion.durationSlow, t);
    motion.scale = Lerp(a.motion.scale, b.motion.scale, t);
    motion.shimmerPeriod = Lerp(a.motion.shimmerPeriod, b.motion.shimmerPeriod, t);
    motion.spinnerPeriod = Lerp(a.motion.spinnerPeriod, b.motion.spinnerPeriod, t);
    motion.popupOpen = Lerp(a.motion.popupOpen, b.motion.popupOpen, t);
    motion.popupClose = Lerp(a.motion.popupClose, b.motion.popupClose, t);
    motion.popupScaleFrom = Lerp(a.motion.popupScaleFrom, b.motion.popupScaleFrom, t);
    motion.feedbackHold = Lerp(a.motion.feedbackHold, b.motion.feedbackHold, t);
    motion.pulsePeriod = Lerp(a.motion.pulsePeriod, b.motion.pulsePeriod, t);

    for (int i = 0; i < 3; ++i)
    {
        const ShadowLevel& la = a.elevation.levels[i];
        const ShadowLevel& lb = b.elevation.levels[i];
        out.elevation.levels[i] = {Lerp(la.offset, lb.offset, t), Lerp(la.blur, lb.blur, t),
                                   Lerp(la.spread, lb.spread, t), Lerp(la.opacity, lb.opacity, t)};
    }

    out.glass.blurRadius = Lerp(a.glass.blurRadius, b.glass.blurRadius, t);
    out.glass.tintOpacity = Lerp(a.glass.tintOpacity, b.glass.tintOpacity, t);
    out.glass.saturation = Lerp(a.glass.saturation, b.glass.saturation, t);

    out.layout.regularFrom = Lerp(a.layout.regularFrom, b.layout.regularFrom, t);
    out.layout.expandedFrom = Lerp(a.layout.expandedFrom, b.layout.expandedFrom, t);
    out.layout.labelColumn = Lerp(a.layout.labelColumn, b.layout.labelColumn, t);
    out.layout.minRowControl = Lerp(a.layout.minRowControl, b.layout.minRowControl, t);
    out.layout.dividerBand = Lerp(a.layout.dividerBand, b.layout.dividerBand, t);
    out.layout.panelHeaderHeight = Lerp(a.layout.panelHeaderHeight, b.layout.panelHeaderHeight, t);

    out.overlay = MixOverlay(a.overlay, b.overlay, t);
    out.containers = MixContainers(a.containers, b.containers, t);
    out.scroll = MixScroll(a.scroll, b.scroll, t);
    out.list = MixList(a.list, b.list, t);
    out.drag = MixDrag(a.drag, b.drag, t);
    out.field = MixField(a.field, b.field, t);
    out.tabs = MixTabs(a.tabs, b.tabs, t);
    out.table = MixTable(a.table, b.table, t);
    out.picker = MixPicker(a.picker, b.picker, t);
    out.charts = MixCharts(a.charts, b.charts, t);
    out.media = MixMedia(a.media, b.media, t);
    out.composites = MixComposites(a.composites, b.composites, t);
    return out;
}

const Theme& T()
{
    ThemeState& state = State();
    AccentScopes& scopes = Scopes();
    if (scopes.depth == 0)
        return state.current;
    // The slot of the innermost scope is rebuilt when it was pushed anew or the theme changed
    // inside the scope (SetTheme, SetAccent). The slots of outer scopes are left alone.
    const int top = scopes.depth - 1;
    if (scopes.scopedRevision[top] != state.revision)
    {
        scopes.scoped[top] = state.current;
        scopes.scoped[top].ApplyAccent(scopes.accents[top]);
        scopes.scopedRevision[top] = state.revision;
    }
    return scopes.scoped[top];
}

void PushAccent(const ImVec4& accent)
{
    AccentScopes& scopes = Scopes();
    if (scopes.depth >= kAccentStackDepth)
    {
        if (scopes.overflow == 0)
            internal::ReportUsageError("PushAccent nested deeper than %d; the extra accents are ignored",
                                       kAccentStackDepth);
        ++scopes.overflow;
        return;
    }
    scopes.scopedRevision[scopes.depth] = 0;
    scopes.accents[scopes.depth++] = accent;
}

void PushAccent(ImU32 accent)
{
    PushAccent(FromU32(accent));
}

void PopAccent()
{
    AccentScopes& scopes = Scopes();
    if (scopes.overflow > 0)
    {
        --scopes.overflow;
        return;
    }
    if (scopes.depth == 0)
    {
        internal::ReportUsageError("PopAccent without PushAccent");
        return;
    }
    --scopes.depth;
}

void SetTheme(const Theme& theme, bool animate)
{
    ApplyTheme(WithStandardBorders(theme), animate);
}

void SetAccent(const ImVec4& accent)
{
    ThemeState& state = State();
    FinishTransition();
    state.hasUserAccent = true;
    state.userAccent = WithAlpha(accent, 1.0f);
    state.shown.ApplyAccent(accent);
    Resolve();
    Changed();
}

void SetAccent(ImU32 accent)
{
    SetAccent(FromU32(accent));
}

void ClearAccent()
{
    State().hasUserAccent = false;
}

bool HasCustomAccent()
{
    return State().hasUserAccent;
}

bool ThemeTransitionActive()
{
    return State().blending;
}

void SetThemeMode(ThemeMode mode, bool animate)
{
    ThemeState& state = State();
    state.mode = mode;
    // Before Init nothing watches the registry; read it now.
    if (mode == ThemeMode::System && state.personalizeKey == nullptr)
        state.systemDark = ReadSystemDark();
    ApplyTheme(PresetFor(mode), animate);
}

ThemeMode GetThemeMode()
{
    return State().mode;
}

bool SystemPrefersDarkTheme()
{
    const ThemeState& state = State();
    return state.personalizeKey != nullptr ? state.systemDark : ReadSystemDark();
}

std::span<const AccentPreset> AccentPresets()
{
    return kAccentPresets;
}

ImVec4 ControlBoundaryColor(const Colors& c)
{
    return c.controlBorder;
}

void SetIncreasedContrast(bool enabled)
{
    State().increasedContrast = enabled;
    Resolve();
    Changed();
}

bool IncreasedContrastEnabled()
{
    return State().increasedContrast;
}

bool IncreasedContrast()
{
    return ContrastVariantOn(State());
}

bool SystemIncreasedContrast()
{
    return State().systemContrast;
}

void SetFollowSystemContrast(bool follow)
{
    ThemeState& state = State();
    const bool before = ContrastVariantOn(state);
    state.followSystemContrast = follow;
    // Before Init nothing reads the setting; read it now.
    if (follow && !IsInitialized())
        state.systemContrast = ReadSystemContrast();
    if (ContrastVariantOn(state) == before)
        return;
    Resolve();
    Changed();
}

bool FollowsSystemContrast()
{
    return State().followSystemContrast;
}

namespace internal
{

std::uint64_t ThemeRevision()
{
    return State().revision;
}

const Theme& UnscopedTheme()
{
    return State().current;
}

// The theme is plain state that exists before Init and after Shutdown. The hooks watch the
// Windows app mode, advance animated switches and close accent scopes left open.
Subsystem ThemeSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "theme";
    subsystem.init = &ThemeInit;
    subsystem.shutdown = &ThemeShutdown;
    subsystem.newFrame = &ThemeNewFrame;
    subsystem.endFrame = &ThemeEndFrame;
    return subsystem;
}

} // namespace internal

} // namespace facet
