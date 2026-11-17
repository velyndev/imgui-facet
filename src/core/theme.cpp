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

// Lightness steps (OKLCH L) of the hover and pressed accent states.
constexpr float kHoverLightnessStep = 0.04f;
constexpr float kPressedLightnessStep = 0.08f;

// WCAG AA contrast for normal text, the target of accentText.
constexpr float kTextContrast = 4.5f;
// Lightness step (OKLCH L) of the accentText search.
constexpr float kTextLightnessStep = 0.01f;

// WCAG 2 non-text contrast for the boundary of a control, and the mix step of its search.
constexpr float kBoundaryContrast = 3.0f;
constexpr float kBoundaryMixStep = 0.05f;

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

// The accent with its lightness moved away from the surfaces (darker on light surfaces,
// lighter on dark ones) in small steps until it is readable on all of them. Black and white
// end the search, and one of them always reaches 4.5:1 on the preset surfaces.
ImVec4 DeriveAccentText(const Colors& c, bool dark)
{
    const Oklch lch = ToOklch(c.accent);
    const float direction = dark ? 1.0f : -1.0f;
    for (int step = 0;; ++step)
    {
        const float L = lch.L + direction * kTextLightnessStep * static_cast<float>(step);
        if (L < 0.0f || L > 1.0f)
            break;
        const ImVec4 candidate = FromOklch({L, lch.C, lch.h});
        if (MinSurfaceContrast(c, candidate) >= kTextContrast)
            return candidate;
    }
    return dark ? Rgb(0xFFFFFF) : Rgb(0x000000);
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
    return out;
}

// --- Current theme and transition state -----------------------------------------------------

struct ThemeState
{
    Theme current = Theme::Light();
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
    Theme theme = state.blending ? state.to : state.current;
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
    state.current = state.to;
}

void ApplyTheme(const Theme& theme, bool animate)
{
    ThemeState& state = State();
    const float duration = theme.motion.durationStandard * theme.motion.scale;
    const bool reduced = ReduceMotion() || theme.motion.reduceMotion;
    if (!animate || !IsInitialized() || reduced || duration <= 0.0f)
    {
        state.blending = false;
        state.current = theme;
        Changed();
        return;
    }
    // A new switch during a running one starts from what is on screen now.
    state.from = state.current;
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
        state.current = state.to;
    }
    else
    {
        state.current = MixThemes(state.from, state.to, EaseValue(Ease::InOutCubic, progress));
    }
    Changed();
}

void ThemeNewFrame()
{
    PollSystemMode();
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
    c.separator = Rgb(0x3C3C43, 0.29f);
    c.separatorOpaque = Rgb(0xC6C6C8);
    c.destructive = Rgb(0xC4000F);
    c.success = Rgb(0x146C2E);
    c.warning = Rgb(0x8A5300);
    c.shadow = Rgb(0x000000);
    c.glassTint = Rgb(0xFFFFFF);
    c.scrim = Rgb(0x000000, 0.20f);
    SetElevation(theme.elevation, 0.10f, 0.12f, 0.18f);
    theme.ApplyAccent(kLightAccent);
    return theme;
}

Theme Theme::Dark()
{
    Theme theme;
    theme.dark = true;
    Colors& c = theme.colors;
    c.bg = Rgb(0x000000);
    c.bgSecondary = Rgb(0x1C1C1E);
    c.bgTertiary = Rgb(0x2C2C2E);
    c.bgElevated = Rgb(0x3A3A3C);
    // A black shadow cannot darken the black background, so height shows as a lighter surface.
    c.bgElevatedLevels[0] = Rgb(0x2C2C2E);
    c.bgElevatedLevels[1] = c.bgElevated;
    c.bgElevatedLevels[2] = Rgb(0x444446);
    c.fill[0] = Rgb(0x787880, 0.36f);
    c.fill[1] = Rgb(0x787880, 0.32f);
    c.fill[2] = Rgb(0x787880, 0.24f);
    c.fill[3] = Rgb(0x787880, 0.18f);
    c.label = Rgb(0xFFFFFF);
    c.labelSecondary = Rgb(0xFFFFFF, 0.60f);
    c.labelTertiary = Rgb(0xFFFFFF, 0.30f);
    c.labelQuaternary = Rgb(0xFFFFFF, 0.16f);
    c.separator = Rgb(0x545458, 0.60f);
    c.separatorOpaque = Rgb(0x38383A);
    c.destructive = Rgb(0xFF6B5F);
    c.success = Rgb(0x32D74B);
    c.warning = Rgb(0xFFD60A);
    c.shadow = Rgb(0x000000);
    c.glassTint = Rgb(0x1C1C1E);
    c.scrim = Rgb(0x000000, 0.48f);
    SetElevation(theme.elevation, 0.36f, 0.44f, 0.56f);
    theme.ApplyAccent(kDarkAccent);
    return theme;
}

void Theme::ApplyAccent(const ImVec4& accent)
{
    Colors& c = colors;
    c.accent = WithAlpha(accent, 1.0f);
    c.onAccent = ReadableOn(c.accent);

    // Hover and pressed move away from the text color, which only raises the contrast.
    const bool whiteText = c.onAccent.x > 0.5f;
    const float direction = whiteText ? -1.0f : 1.0f;
    const Oklch lch = ToOklch(c.accent);
    const auto shifted = [&](float step) {
        return FromOklch({std::clamp(lch.L + direction * step, 0.0f, 1.0f), lch.C, lch.h});
    };
    c.accentHover = shifted(kHoverLightnessStep);
    c.accentPressed = shifted(kPressedLightnessStep);

    c.accentText = DeriveAccentText(c, dark);
    c.accentSubtle = WithAlpha(c.accent, dark ? 0.24f : 0.14f);
    c.focusRing = WithAlpha(c.accent, dark ? 0.80f : 0.70f);
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
    ApplyTheme(theme, animate);
}

void SetAccent(const ImVec4& accent)
{
    ThemeState& state = State();
    FinishTransition();
    state.hasUserAccent = true;
    state.userAccent = WithAlpha(accent, 1.0f);
    state.current.ApplyAccent(accent);
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
    const ImVec4 surfaces[] = {c.bg, c.bgSecondary, c.bgTertiary};
    for (int step = 0; static_cast<float>(step) * kBoundaryMixStep <= 1.0f; ++step)
    {
        const ImVec4 candidate = Mix(c.separatorOpaque, c.label, static_cast<float>(step) * kBoundaryMixStep);
        bool readable = true;
        for (const ImVec4& surface : surfaces)
            readable = readable && ContrastRatio(candidate, surface) >= kBoundaryContrast;
        if (readable)
            return candidate;
    }
    return c.label;
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
