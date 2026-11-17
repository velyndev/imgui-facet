// Layout tokens, width classes and the labelled row layout (core/layout.h).

#include <cmath>

#include <doctest/doctest.h>
#include <imgui.h>

#include <facet/core/layout.h>
#include <facet/facet.h>

#include "support/session.h"

namespace
{

// Restores the light preset and scale 1 after a case that changed them.
struct LayoutGuard
{
    ~LayoutGuard()
    {
        facet::SetTheme(facet::Theme::Light(), false);
        facet::SetUiScale(1.0f);
    }
};

} // namespace

TEST_CASE("Layout tokens hold their starting values and blend linearly")
{
    const facet::Theme light = facet::Theme::Light();
    CHECK(light.layout.regularFrom == 400.0f);
    CHECK(light.layout.expandedFrom == 720.0f);
    CHECK(light.layout.labelColumn == 112.0f);
    CHECK(light.layout.minRowControl == 128.0f);
    CHECK(facet::Theme::Dark().layout.labelColumn == light.layout.labelColumn);

    facet::Theme wide = light;
    wide.layout.labelColumn = 212.0f;
    wide.layout.regularFrom = 500.0f;
    const facet::Theme mid = facet::MixThemes(light, wide, 0.5f);
    CHECK(mid.layout.labelColumn == doctest::Approx(162.0f));
    CHECK(mid.layout.regularFrom == doctest::Approx(450.0f));
    CHECK(facet::MixThemes(light, wide, 1.0f).layout.labelColumn == 212.0f);
}

TEST_CASE("Width classes follow the layout tokens")
{
    LayoutGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    CHECK(facet::WidthClassOf(0.0f) == facet::WidthClass::Compact);
    CHECK(facet::WidthClassOf(-5.0f) == facet::WidthClass::Compact);
    CHECK(facet::WidthClassOf(std::nanf("")) == facet::WidthClass::Compact);
    CHECK(facet::WidthClassOf(399.9f) == facet::WidthClass::Compact);
    CHECK(facet::WidthClassOf(400.0f) == facet::WidthClass::Regular);
    CHECK(facet::WidthClassOf(719.9f) == facet::WidthClass::Regular);
    CHECK(facet::WidthClassOf(720.0f) == facet::WidthClass::Expanded);
    CHECK(facet::WidthClassOf(100000.0f) == facet::WidthClass::Expanded);

    facet::Theme theme = facet::Theme::Light();
    theme.layout.regularFrom = 300.0f;
    facet::SetTheme(theme, false);
    CHECK(facet::WidthClassOf(350.0f) == facet::WidthClass::Regular);
}

TEST_CASE("The available width is measured in design units at every scale")
{
    LayoutGuard guard;
    facet::test::Session session({.displaySize = ImVec2(1000.0f, 400.0f)});
    REQUIRE(session.Ready());
    facet::SetTheme(facet::Theme::Light(), false);
    float units = -1.0f;
    facet::WidthClass widthClass = facet::WidthClass::Compact;
    float childWidth = 0.0f;
    session.SetUi([&] {
        ImGui::BeginChild("region", ImVec2(childWidth, 100.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
        units = facet::AvailableWidthUnits();
        widthClass = facet::AvailableWidthClass();
        ImGui::EndChild();
    });

    // The child (no padding) is 600 pixels wide: 600 units at scale 1, Regular.
    childWidth = 600.0f;
    session.Frame();
    CHECK(units == doctest::Approx(600.0f));
    CHECK(widthClass == facet::WidthClass::Regular);

    // At scale 2 the same 600 pixels are 300 units: Compact.
    facet::SetUiScale(2.0f);
    REQUIRE(facet::Scale() == 2.0f);
    session.Frame();
    CHECK(units == doctest::Approx(300.0f));
    CHECK(widthClass == facet::WidthClass::Compact);

    // Outside a frame there is no window: 0.
    CHECK(facet::AvailableWidthUnits() == 0.0f);
}

TEST_CASE("Labelled rows keep the label beside the control until the control gets too narrow")
{
    LayoutGuard guard;
    facet::SetTheme(facet::Theme::Light(), false);
    facet::SetUiScale(1.0f);
    const facet::Theme& theme = facet::T();

    const facet::LabeledRowLayout wide = facet::LayoutLabeledRow(500.0f);
    CHECK_FALSE(wide.stacked);
    CHECK(wide.controlOffset == theme.layout.labelColumn);
    CHECK(wide.controlWidth == 500.0f - theme.layout.labelColumn);
    CHECK(wide.labelWidth == theme.layout.labelColumn - theme.spacing.sm);

    // Exactly the label column plus the narrowest control still fits beside.
    const float edge = theme.layout.labelColumn + theme.layout.minRowControl;
    CHECK_FALSE(facet::LayoutLabeledRow(edge).stacked);
    const facet::LabeledRowLayout narrow = facet::LayoutLabeledRow(edge - 1.0f);
    CHECK(narrow.stacked);
    CHECK(narrow.controlOffset == 0.0f);
    CHECK(narrow.controlWidth == edge - 1.0f);
    CHECK(narrow.labelWidth == edge - 1.0f);

    // A wider label column moves the control and the stacking point.
    const facet::LabeledRowLayout custom = facet::LayoutLabeledRow(500.0f, 200.0f);
    CHECK(custom.controlOffset == 200.0f);
    CHECK(facet::LayoutLabeledRow(200.0f + theme.layout.minRowControl - 1.0f, 200.0f).stacked);

    // Pixels follow the scale; negative widths count as 0.
    CHECK_FALSE(facet::LayoutLabeledRow(470.0f).stacked);
    facet::SetUiScale(2.0f);
    REQUIRE(facet::Scale() == 2.0f);
    CHECK(facet::LayoutLabeledRow(500.0f).controlOffset == theme.layout.labelColumn * 2.0f);
    CHECK(facet::LayoutLabeledRow(470.0f).stacked);
    const facet::LabeledRowLayout none = facet::LayoutLabeledRow(-10.0f);
    CHECK(none.stacked);
    CHECK(none.controlWidth == 0.0f);
}
