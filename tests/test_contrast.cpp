// WCAG contrast of the theme tokens: every text token on every surface in both presets,
// and the derived accent colors for accents across 24 hues.

#include <vector>

#include <doctest/doctest.h>

#include <facet/facet.h>

namespace
{

constexpr float kTextContrast = 4.5f;      // WCAG AA, normal text
constexpr float kNonTextContrast = 3.0f;   // WCAG 2.1 non-text contrast (control boundaries)
constexpr int kHueCount = 24;

struct Named
{
    const char* name;
    ImVec4 color;
};

std::vector<Named> Surfaces(const facet::Colors& c)
{
    return {{"bg", c.bg},
            {"bgSecondary", c.bgSecondary},
            {"bgTertiary", c.bgTertiary},
            {"bgElevated", c.bgElevated},
            {"bgElevatedLevels[0]", c.bgElevatedLevels[0]},
            {"bgElevatedLevels[1]", c.bgElevatedLevels[1]},
            {"bgElevatedLevels[2]", c.bgElevatedLevels[2]}};
}

std::vector<Named> ContentSurfaces(const facet::Colors& c)
{
    return {{"bg", c.bg}, {"bgSecondary", c.bgSecondary}, {"bgTertiary", c.bgTertiary}};
}

void CheckAccentDerivation(const facet::Theme& theme)
{
    const facet::Colors& c = theme.colors;
    CHECK(facet::ContrastRatio(c.onAccent, c.accent) >= kTextContrast);
    CHECK(facet::ContrastRatio(c.onAccent, c.accentHover) >= kTextContrast);
    CHECK(facet::ContrastRatio(c.onAccent, c.accentPressed) >= kTextContrast);
    for (const Named& surface : Surfaces(c))
    {
        CAPTURE(surface.name);
        CHECK(facet::ContrastRatio(c.accentText, surface.color) >= kTextContrast);
    }
    CHECK(c.accent.w == doctest::Approx(1.0f));
    CHECK(c.accentText.w == doctest::Approx(1.0f));
}

} // namespace

TEST_CASE("label and labelSecondary reach 4.5:1 on every surface and control fill")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        const facet::Colors& c = theme.colors;
        for (const Named& surface : Surfaces(c))
        {
            CAPTURE(surface.name);
            CHECK(facet::ContrastRatio(c.label, surface.color) >= kTextContrast);
            CHECK(facet::ContrastRatio(c.labelSecondary, surface.color) >= kTextContrast);
            // Text on a control: each fill drawn over the surface.
            for (int i = 0; i < 4; ++i)
            {
                CAPTURE(i);
                const ImVec4 control = facet::Over(c.fill[i], surface.color);
                CHECK(facet::ContrastRatio(c.label, control) >= kTextContrast);
            }
        }
    }
}

TEST_CASE("labelTertiary and labelQuaternary stay below body text contrast as documented")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        const facet::Colors& c = theme.colors;
        for (const Named& surface : Surfaces(c))
        {
            CAPTURE(surface.name);
            CHECK(facet::ContrastRatio(c.labelTertiary, surface.color) < kTextContrast);
            CHECK(facet::ContrastRatio(c.labelQuaternary, surface.color) <
                  facet::ContrastRatio(c.labelTertiary, surface.color));
            // Still distinguishable from the surface (disabled text must stay visible).
            CHECK(facet::ContrastRatio(c.labelTertiary, surface.color) > 1.5f);
        }
    }
}

TEST_CASE("Status colors reach 4.5:1 as text on the content surfaces")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        const facet::Colors& c = theme.colors;
        for (const Named& surface : ContentSurfaces(c))
        {
            CAPTURE(surface.name);
            CHECK(facet::ContrastRatio(c.destructive, surface.color) >= kTextContrast);
            CHECK(facet::ContrastRatio(c.success, surface.color) >= kTextContrast);
            CHECK(facet::ContrastRatio(c.warning, surface.color) >= kTextContrast);
        }
    }
}

TEST_CASE("The control boundary color reaches 3:1 on the content surfaces")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        const ImVec4 boundary = facet::ControlBoundaryColor(theme.colors);
        for (const Named& surface : ContentSurfaces(theme.colors))
        {
            CAPTURE(surface.name);
            CHECK(facet::ContrastRatio(boundary, surface.color) >= kNonTextContrast);
        }
    }
}

TEST_CASE("The default accents give readable accent states in both themes")
{
    CheckAccentDerivation(facet::Theme::Light());
    CheckAccentDerivation(facet::Theme::Dark());
}

TEST_CASE("Accents on 24 hues at five lightness levels stay readable in both themes")
{
    for (const bool dark : {false, true})
    {
        facet::Theme theme = dark ? facet::Theme::Dark() : facet::Theme::Light();
        for (int step = 0; step < kHueCount; ++step)
        {
            const float hue = 360.0f * static_cast<float>(step) / static_cast<float>(kHueCount);
            for (const float lightness : {0.35f, 0.5f, 0.62f, 0.7f, 0.85f})
            {
                CAPTURE(dark);
                CAPTURE(hue);
                CAPTURE(lightness);
                theme.ApplyAccent(facet::FromOklch({lightness, 0.15f, hue}));
                CheckAccentDerivation(theme);
            }
        }
    }
}

TEST_CASE("accentText keeps the hue of the accent")
{
    for (const facet::Theme& theme : {facet::Theme::Light(), facet::Theme::Dark()})
    {
        CAPTURE(theme.dark);
        const facet::Oklch accent = facet::ToOklch(theme.colors.accent);
        const facet::Oklch text = facet::ToOklch(theme.colors.accentText);
        CHECK(text.h == doctest::Approx(accent.h).epsilon(0.02));
    }
}

TEST_CASE("Every accent preset is readable in both themes")
{
    const auto presets = facet::AccentPresets();
    REQUIRE(presets.size() >= 2);
    for (const bool dark : {false, true})
    {
        facet::Theme theme = dark ? facet::Theme::Dark() : facet::Theme::Light();
        for (const facet::AccentPreset& preset : presets)
        {
            CAPTURE(dark);
            CAPTURE(preset.name);
            theme.ApplyAccent(preset.color);
            CheckAccentDerivation(theme);
        }
    }
    CHECK(facet::ToU32(presets[0].color) == facet::ToU32(facet::Theme::Light().colors.accent));
}
