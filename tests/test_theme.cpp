// Theme presets: contrast of text tokens on every surface, accent derivation, scale.

#include <vector>

#include <doctest/doctest.h>

#include <facet/facet.h>

namespace
{

constexpr float kTextContrast = 4.5f;  // WCAG AA for normal text

struct Surface
{
    const char* name;
    ImVec4 color;
};

std::vector<Surface> Surfaces(const facet::Theme& theme)
{
    const facet::Colors& c = theme.colors;
    return {{"bg", c.bg}, {"bgSecondary", c.bgSecondary}, {"bgTertiary", c.bgTertiary},
            {"bgElevated", c.bgElevated}};
}

void CheckReadableText(const facet::Theme& theme)
{
    const facet::Colors& c = theme.colors;
    for (const Surface& surface : Surfaces(theme))
    {
        CAPTURE(surface.name);
        CHECK(facet::ContrastRatio(c.label, surface.color) >= kTextContrast);
        CHECK(facet::ContrastRatio(c.labelSecondary, surface.color) >= kTextContrast);
        // Text on a control: the default control fill drawn over the surface.
        const ImVec4 control = facet::Over(c.fill[1], surface.color);
        CHECK(facet::ContrastRatio(c.label, control) >= kTextContrast);
    }
    // Status colors are used as text on the three content surfaces.
    const ImVec4 content[] = {c.bg, c.bgSecondary, c.bgTertiary};
    for (const ImVec4& surface : content)
    {
        CHECK(facet::ContrastRatio(c.destructive, surface) >= kTextContrast);
        CHECK(facet::ContrastRatio(c.success, surface) >= kTextContrast);
        CHECK(facet::ContrastRatio(c.warning, surface) >= kTextContrast);
    }
}

void CheckAccentStates(const facet::Colors& c)
{
    CHECK(facet::ContrastRatio(c.onAccent, c.accent) >= kTextContrast);
    CHECK(facet::ContrastRatio(c.onAccent, c.accentHover) >= kTextContrast);
    CHECK(facet::ContrastRatio(c.onAccent, c.accentPressed) >= kTextContrast);
}

} // namespace

TEST_CASE("Text tokens reach 4.5:1 on every surface of the light theme")
{
    const facet::Theme theme = facet::Theme::Light();
    CHECK_FALSE(theme.dark);
    CheckReadableText(theme);
}

TEST_CASE("Text tokens reach 4.5:1 on every surface of the dark theme")
{
    const facet::Theme theme = facet::Theme::Dark();
    CHECK(theme.dark);
    CheckReadableText(theme);
}

TEST_CASE("onAccent reaches 4.5:1 on the default accent states of both themes")
{
    CheckAccentStates(facet::Theme::Light().colors);
    CheckAccentStates(facet::Theme::Dark().colors);
}

TEST_CASE("onAccent stays readable for accents across hues and lightness")
{
    for (const bool dark : {false, true})
    {
        facet::Theme theme = dark ? facet::Theme::Dark() : facet::Theme::Light();
        for (int hue = 0; hue < 360; hue += 15)
        {
            for (const float lightness : {0.35f, 0.5f, 0.62f, 0.7f, 0.85f})
            {
                CAPTURE(dark);
                CAPTURE(hue);
                CAPTURE(lightness);
                theme.ApplyAccent(facet::FromOklch({lightness, 0.15f, static_cast<float>(hue)}));
                CheckAccentStates(theme.colors);
                CHECK(theme.colors.accent.w == doctest::Approx(1.0f));
            }
        }
    }
}

TEST_CASE("The token groups hold the documented starting values")
{
    const facet::Theme theme = facet::Theme::Light();
    CHECK(theme.spacing.xs == 4.0f);
    CHECK(theme.spacing.xxxl == 32.0f);
    CHECK(theme.radii.control == 8.0f);
    CHECK(theme.radii.window == 16.0f);
    CHECK(theme.metrics.controlHeight == 28.0f);
    CHECK(theme.type[facet::TextStyle::Body].size == 13.0f);
    CHECK(theme.type[facet::TextStyle::Headline].weight == facet::FontWeight::Semibold);
    CHECK(theme.type[facet::TextStyle::LargeTitle].size == 26.0f);
    CHECK(theme.type[facet::TextStyle::Caption].size == 11.0f);
    CHECK(theme.motion.durationStandard == doctest::Approx(0.2f));
    CHECK(theme.elevation.Level(0).blur == theme.elevation.Level(1).blur);
    CHECK(theme.elevation.Level(9).blur == theme.elevation.Level(3).blur);
    CHECK(theme.elevation.Level(3).blur > theme.elevation.Level(1).blur);
}

TEST_CASE("SetTheme and SetAccent change the current theme")
{
    const facet::Theme saved = facet::T();

    facet::SetTheme(facet::Theme::Dark(), false);
    CHECK(facet::T().dark);
    facet::SetAccent(IM_COL32(0xFF, 0xD6, 0x0A, 0xFF));
    CHECK(facet::T().colors.accent.x == doctest::Approx(1.0f));
    CHECK(facet::ContrastRatio(facet::T().colors.onAccent, facet::T().colors.accent) >= kTextContrast);
    facet::SetTheme(facet::Theme::Light(), false);
    CHECK_FALSE(facet::T().dark);
    CHECK(facet::T().colors.accent.z == doctest::Approx(facet::Theme::Light().colors.accent.z));

    facet::SetTheme(saved, false);
}

TEST_CASE("Scale combines the DPI and user scale according to the mode")
{
    const float savedUi = facet::UiScale();
    const float savedDpi = facet::DpiScale();
    const facet::ScaleMode savedMode = facet::GetScaleMode();

    facet::SetScaleMode(facet::ScaleMode::FollowDpi);
    facet::SetDpiScale(1.5f);
    facet::SetUiScale(2.0f);
    CHECK(facet::Scale() == doctest::Approx(3.0f));
    CHECK(facet::Px(4.0f) == doctest::Approx(12.0f));
    const ImVec2 size = facet::Px(ImVec2(10.0f, 20.0f));
    CHECK(size.x == doctest::Approx(30.0f));
    CHECK(size.y == doctest::Approx(60.0f));

    facet::SetScaleMode(facet::ScaleMode::Manual);
    CHECK(facet::Scale() == doctest::Approx(2.0f));

    facet::SetUiScale(100.0f);
    CHECK(facet::UiScale() == doctest::Approx(4.0f));
    facet::SetUiScale(0.0f);
    CHECK(facet::UiScale() == doctest::Approx(0.5f));

    facet::SetScaleMode(savedMode);
    facet::SetDpiScale(savedDpi);
    facet::SetUiScale(savedUi);
}
