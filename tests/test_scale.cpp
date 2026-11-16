// UI scale modes and the pixel snapping helpers.

#include <cmath>

#include <doctest/doctest.h>

#include <facet/facet.h>

namespace
{

// Restores the global scale state after a test changes it.
struct ScaleGuard
{
    float ui = facet::UiScale();
    float dpi = facet::DpiScale();
    facet::ScaleMode mode = facet::GetScaleMode();
    ~ScaleGuard()
    {
        facet::SetScaleMode(mode);
        facet::SetDpiScale(dpi);
        facet::SetUiScale(ui);
    }
};

bool IsWhole(float value)
{
    return value == std::floor(value);
}

} // namespace

TEST_CASE("Scale combines the DPI and user scale according to the mode")
{
    ScaleGuard guard;
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
    facet::SetDpiScale(100.0f);
    CHECK(facet::DpiScale() == doctest::Approx(8.0f));
}

TEST_CASE("The scale is written into the Dear ImGui font scale factors")
{
    ScaleGuard guard;
    ImGuiContext* context = ImGui::CreateContext();
    facet::SetScaleMode(facet::ScaleMode::FollowDpi);
    facet::SetDpiScale(1.25f);
    facet::SetUiScale(1.5f);
    const ImGuiStyle& style = ImGui::GetStyle();
    CHECK(style.FontScaleMain == doctest::Approx(1.5f));
    CHECK(style.FontScaleDpi == doctest::Approx(1.25f));
    facet::SetScaleMode(facet::ScaleMode::Manual);
    CHECK(style.FontScaleDpi == doctest::Approx(1.0f));
    CHECK(style.FontScaleMain * style.FontScaleDpi == doctest::Approx(facet::Scale()));
    ImGui::DestroyContext(context);
}

TEST_CASE("A host that manages the font scale keeps its own factors")
{
    ScaleGuard guard;
    ImGuiContext* context = ImGui::CreateContext();
    REQUIRE(facet::ManagesFontScale());
    facet::SetManageFontScale(false);
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontScaleMain = 0.9f;
    style.FontScaleDpi = 1.1f;
    facet::SetUiScale(2.0f);
    ImGuiStyle applied = style;
    facet::ApplyImGuiStyle(&applied);
    CHECK(style.FontScaleMain == doctest::Approx(0.9f));
    CHECK(style.FontScaleDpi == doctest::Approx(1.1f));
    CHECK(applied.FontScaleMain == doctest::Approx(0.9f));
    CHECK(applied.FontScaleDpi == doctest::Approx(1.1f));
    // Turning it back on writes the scale at once.
    facet::SetManageFontScale(true);
    CHECK(style.FontScaleMain * style.FontScaleDpi == doctest::Approx(facet::Scale()));
    ImGui::DestroyContext(context);
}

TEST_CASE("Snap rounds to whole pixels")
{
    CHECK(facet::Snap(10.4f) == 10.0f);
    CHECK(facet::Snap(10.6f) == 11.0f);
    CHECK(facet::Snap(-2.6f) == -3.0f);
    const ImVec2 p = facet::Snap(ImVec2(1.49f, 7.51f));
    CHECK(p.x == 1.0f);
    CHECK(p.y == 8.0f);

    ImVec2 a(0.4f, 3.6f);
    ImVec2 b(10.5f, 20.2f);
    facet::SnapRect(a, b);
    CHECK(a.x == 0.0f);
    CHECK(a.y == 4.0f);
    CHECK(b.x == 11.0f);
    CHECK(b.y == 20.0f);
}

TEST_CASE("PxSnapped gives whole pixels and never rounds a positive length away")
{
    ScaleGuard guard;
    facet::SetScaleMode(facet::ScaleMode::Manual);
    for (const float scale : {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f})
    {
        CAPTURE(scale);
        facet::SetUiScale(scale);
        for (const float units : {0.25f, 1.0f, 3.0f, 8.0f, 13.0f})
        {
            CAPTURE(units);
            const float snapped = facet::PxSnapped(units);
            CHECK(IsWhole(snapped));
            CHECK(snapped >= 1.0f);
            CHECK(std::fabs(snapped - facet::Px(units)) <= 0.5f + (facet::Px(units) < 1.0f ? 1.0f : 0.0f));
        }
        CHECK(facet::PxSnapped(0.0f) == 0.0f);
    }
}

TEST_CASE("Hairlines are whole pixels at fractional scales")
{
    ScaleGuard guard;
    facet::SetScaleMode(facet::ScaleMode::Manual);
    for (const float scale : {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.25f, 3.0f})
    {
        CAPTURE(scale);
        facet::SetUiScale(scale);
        const float thickness = facet::HairlineThickness();
        CHECK(IsWhole(thickness));
        CHECK(thickness >= 1.0f);
        CHECK(thickness <= std::max(1.0f, std::round(scale)));
    }
}

TEST_CASE("SnapStroke centers odd strokes on pixel centers and even ones on pixel edges")
{
    CHECK(facet::SnapStroke(10.0f, 1.0f) == 10.5f);
    CHECK(facet::SnapStroke(10.7f, 1.0f) == 10.5f);
    CHECK(facet::SnapStroke(10.7f, 3.0f) == 10.5f);
    CHECK(facet::SnapStroke(10.7f, 2.0f) == 10.0f);
    CHECK(facet::SnapStroke(-0.2f, 1.0f) == -0.5f);
    // A stroke of the snapped thickness around the center covers whole pixels.
    for (const float thickness : {1.0f, 2.0f, 3.0f, 4.0f})
    {
        CAPTURE(thickness);
        const float center = facet::SnapStroke(5.3f, thickness);
        CHECK(IsWhole(center - thickness * 0.5f));
        CHECK(IsWhole(center + thickness * 0.5f));
    }
}
