// Theme tokens: the light and dark presets, accent derivation and the current theme.
//
// SetTheme switches at once; the cross-fade of the animate flag is not implemented yet.

#include "facet/core/theme.h"

#include <algorithm>

#include "facet/core/color.h"
#include "internal/subsystem.h"

namespace facet
{
namespace
{

// Default accents: the same blue hue, each dark enough for white text on it in its theme.
constexpr ImVec4 kLightAccent = Rgb(0x0066CC);
constexpr ImVec4 kDarkAccent = Rgb(0x1F6FEB);

// Lightness steps (OKLCH L) of the hover and pressed accent states.
constexpr float kHoverLightnessStep = 0.04f;
constexpr float kPressedLightnessStep = 0.08f;

Theme& Current()
{
    static Theme theme = Theme::Light();
    return theme;
}

void SetElevation(Elevation& elevation, float opacity1, float opacity2, float opacity3)
{
    elevation.levels[0] = {{0.0f, 1.0f}, 3.0f, 0.0f, opacity1};
    elevation.levels[1] = {{0.0f, 4.0f}, 16.0f, 0.0f, opacity2};
    elevation.levels[2] = {{0.0f, 12.0f}, 40.0f, 0.0f, opacity3};
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

    c.accentSubtle = WithAlpha(c.accent, dark ? 0.24f : 0.14f);
    c.focusRing = WithAlpha(c.accent, dark ? 0.80f : 0.70f);
}

const Theme& T()
{
    return Current();
}

void SetTheme(const Theme& theme, bool animate)
{
    (void)animate;
    Current() = theme;
    internal::NotifyThemeChanged();
}

void SetAccent(const ImVec4& accent)
{
    Current().ApplyAccent(accent);
    internal::NotifyThemeChanged();
}

void SetAccent(ImU32 accent)
{
    SetAccent(FromU32(accent));
}

namespace internal
{

// No hooks: the theme is plain state that exists before Init and after Shutdown.
Subsystem ThemeSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "theme";
    return subsystem;
}

} // namespace internal

} // namespace facet
