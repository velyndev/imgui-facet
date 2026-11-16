// Theme presets, switching, blending and the Dear ImGui style mapping.

#include <doctest/doctest.h>

#include <facet/facet.h>

namespace
{

bool SameColor(const ImVec4& a, const ImVec4& b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

bool SameColors(const facet::Colors& a, const facet::Colors& b)
{
    bool same = SameColor(a.bg, b.bg) && SameColor(a.bgSecondary, b.bgSecondary) &&
                SameColor(a.bgTertiary, b.bgTertiary) && SameColor(a.bgElevated, b.bgElevated) &&
                SameColor(a.label, b.label) && SameColor(a.labelSecondary, b.labelSecondary) &&
                SameColor(a.accent, b.accent) && SameColor(a.accentText, b.accentText) &&
                SameColor(a.onAccent, b.onAccent) && SameColor(a.scrim, b.scrim) &&
                SameColor(a.separator, b.separator) && SameColor(a.focusRing, b.focusRing);
    for (int i = 0; i < 4; ++i)
        same = same && SameColor(a.fill[i], b.fill[i]);
    return same;
}

// Restores the global theme state after a test changes it.
struct ThemeGuard
{
    facet::Theme saved = facet::T();
    facet::ThemeMode mode = facet::GetThemeMode();
    ~ThemeGuard()
    {
        facet::ClearAccent();
        facet::SetThemeMode(mode, false);
        facet::SetTheme(saved, false);
    }
};

} // namespace

TEST_CASE("The token groups hold the documented starting values")
{
    const facet::Theme theme = facet::Theme::Light();
    CHECK(theme.spacing.xs == 4.0f);
    CHECK(theme.spacing.xxxl == 32.0f);
    CHECK(theme.radii.control == 8.0f);
    CHECK(theme.radii.window == 16.0f);
    CHECK(theme.radii.continuousFrom == 10.0f);
    CHECK(theme.metrics.controlHeight == 28.0f);
    CHECK(theme.type[facet::TextStyle::Body].size == 13.0f);
    CHECK(theme.type[facet::TextStyle::Headline].weight == facet::FontWeight::Semibold);
    CHECK(theme.type[facet::TextStyle::LargeTitle].size == 26.0f);
    CHECK(theme.type[facet::TextStyle::Caption].size == 11.0f);
    CHECK(theme.motion.durationFast == doctest::Approx(0.12f));
    CHECK(theme.motion.durationStandard == doctest::Approx(0.2f));
    CHECK(theme.motion.durationSlow == doctest::Approx(0.35f));
    CHECK(theme.elevation.Level(0).blur == theme.elevation.Level(1).blur);
    CHECK(theme.elevation.Level(9).blur == theme.elevation.Level(3).blur);
    CHECK(theme.elevation.Level(3).blur > theme.elevation.Level(1).blur);
    CHECK_FALSE(theme.dark);
    CHECK(facet::Theme::Dark().dark);
}

TEST_CASE("SetTheme and SetAccent change the current theme")
{
    ThemeGuard guard;
    facet::SetTheme(facet::Theme::Dark(), false);
    CHECK(facet::T().dark);
    facet::SetAccent(IM_COL32(0xFF, 0xD6, 0x0A, 0xFF));
    CHECK(facet::T().colors.accent.x == doctest::Approx(1.0f));
    CHECK(facet::ContrastRatio(facet::T().colors.onAccent, facet::T().colors.accent) >= 4.5f);
    facet::SetTheme(facet::Theme::Light(), false);
    CHECK_FALSE(facet::T().dark);
    CHECK(facet::T().colors.accent.z == doctest::Approx(facet::Theme::Light().colors.accent.z));
}

TEST_CASE("An animated SetTheme switches at once while Facet is not running")
{
    ThemeGuard guard;
    REQUIRE_FALSE(facet::IsInitialized());
    facet::SetTheme(facet::Theme::Dark(), true);
    CHECK_FALSE(facet::ThemeTransitionActive());
    CHECK(facet::T().dark);
    CHECK(SameColors(facet::T().colors, facet::Theme::Dark().colors));
}

TEST_CASE("MixThemes returns exact copies at the endpoints")
{
    const facet::Theme light = facet::Theme::Light();
    facet::Theme dark = facet::Theme::Dark();
    dark.spacing.md = 20.0f;
    dark.radii.card = 4.0f;

    const facet::Theme start = facet::MixThemes(light, dark, 0.0f);
    CHECK(SameColors(start.colors, light.colors));
    CHECK(start.spacing.md == light.spacing.md);
    CHECK(start.radii.card == light.radii.card);
    CHECK_FALSE(start.dark);

    const facet::Theme end = facet::MixThemes(light, dark, 1.0f);
    CHECK(SameColors(end.colors, dark.colors));
    CHECK(end.spacing.md == 20.0f);
    CHECK(end.radii.card == 4.0f);
    CHECK(end.dark);

    // Values outside 0 to 1 are clamped.
    CHECK(SameColors(facet::MixThemes(light, dark, -2.0f).colors, light.colors));
    CHECK(SameColors(facet::MixThemes(light, dark, 3.0f).colors, dark.colors));
}

TEST_CASE("MixThemes blends colors in OKLab and lengths linearly")
{
    const facet::Theme light = facet::Theme::Light();
    facet::Theme dark = facet::Theme::Dark();
    dark.spacing.md = 20.0f;

    const facet::Theme mid = facet::MixThemes(light, dark, 0.5f);
    CHECK(mid.spacing.md == doctest::Approx((light.spacing.md + 20.0f) * 0.5f));
    // White to black at the midpoint is the OKLab midpoint: lightness 0.5.
    CHECK(facet::ToOklab(mid.colors.bg).L == doctest::Approx(0.5f).epsilon(0.01));
    const ImVec4 expected = facet::Mix(light.colors.accent, dark.colors.accent, 0.5f);
    CHECK(mid.colors.accent.x == doctest::Approx(expected.x));
    CHECK(mid.colors.accent.z == doctest::Approx(expected.z));
    // Switches flip at the halfway point.
    CHECK(mid.dark);
    CHECK_FALSE(facet::MixThemes(light, dark, 0.49f).dark);

    // Lightness of the background falls monotonically along the blend.
    float previous = 2.0f;
    for (int i = 0; i <= 10; ++i)
    {
        const float L = facet::ToOklab(facet::MixThemes(light, dark, static_cast<float>(i) / 10.0f).colors.bg).L;
        CHECK(L <= previous + 1e-5f);
        previous = L;
    }
}

TEST_CASE("SetThemeMode selects the presets and keeps a custom accent")
{
    ThemeGuard guard;
    facet::SetThemeMode(facet::ThemeMode::Dark, false);
    CHECK(facet::GetThemeMode() == facet::ThemeMode::Dark);
    CHECK(facet::T().dark);

    const ImVec4 purple = facet::AccentPresets()[1].color;
    facet::SetAccent(purple);
    facet::SetThemeMode(facet::ThemeMode::Light, false);
    CHECK_FALSE(facet::T().dark);
    CHECK(facet::ToU32(facet::T().colors.accent) == facet::ToU32(purple));

    facet::SetThemeMode(facet::ThemeMode::System, false);
    CHECK(facet::T().dark == facet::SystemPrefersDarkTheme());

    // SetTheme replaces the accent with the one of the theme passed.
    facet::SetTheme(facet::Theme::Light(), false);
    CHECK(facet::ToU32(facet::T().colors.accent) == facet::ToU32(facet::Theme::Light().colors.accent));
}
