// UI scale modes and the font scale factors.

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
