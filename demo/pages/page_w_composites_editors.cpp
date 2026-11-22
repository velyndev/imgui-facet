// Demo page: Editors and canvas. Calls every editor, property grid, canvas and layer composite of
// facet/widgets/composites.h once with neutral arguments. A planned widget draws the placeholder of
// its declared size; when it is implemented, its call here grows into a section with its variants
// and states, capture states and selftest steps. The inventory and cards and the search and app
// controls have pages of their own (page_w_composites_cards.cpp and page_w_composites_settings.cpp).

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"
#include "selftest.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::SectionHeader;

// Reference resolution of the example preview canvas, in reference pixels.
const ImVec2 kReferenceSize(1920.0f, 1080.0f);

// Placeholder data: names like "Layer 1", nothing real.
struct EditorsUi
{
    ImVec2 curve[8] = {{0.0f, 0.0f}, {0.5f, 0.7f}, {1.0f, 1.0f}};
    int curveCount = 3;
    facet::CubicBezier easing{{0.25f, 0.1f}, {0.25f, 1.0f}};
    facet::GradientStop stops[8] = {{0.0f, {0.0f, 0.0f, 0.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}};
    int stopCount = 2;
    bool option = true;
    facet::CanvasElementState element{facet::CanvasAnchor::TopLeft, {64.0f, 64.0f}, {480.0f, 120.0f}};
    facet::LayerItem layers[3] = {{"Layer 1"}, {"Layer 2", {}, 1}, {"Layer 3", {}, 0, false}};
    int layer = 0;
};

EditorsUi ui;

} // namespace

FACET_DEMO_PAGE("composites-editors", "Editors and canvas", "Widgets", 144)
{
    facet::demo::PageHeader("Editors and canvas",
                            "Editors for curves, easing and gradients, property grids, preview canvases with their "
                            "elements, and layer lists.");
    facet::demo::PlannedFamilyNote("composites");

    SectionHeader("Editors");
    LabeledRow("Curve editor");
    facet::CurveEditor("Response", ui.curve, &ui.curveCount, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.marker = 0.5f});
    ctx.Probe("composites.curve");
    LabeledRow("Bezier editor");
    facet::BezierEditor("Easing", &ui.easing);
    LabeledRow("Gradient editor");
    facet::GradientEditor("Gradient", ui.stops, &ui.stopCount, {.alpha = true});

    SectionHeader("Inspector");
    LabeledRow("Property grid");
    ImGui::BeginGroup();  // keeps the grid in the control column of the row
    facet::BeginPropertyGrid("properties");
    if (facet::BeginPropertyCategory("Category 1"))
    {
        if (facet::BeginPropertyRow("Property 1", {.tooltip = "Placeholder description"}))
        {
            ImGui::Checkbox("##value", &ui.option);
            facet::ItemCursor();
            ctx.Probe("composites.property");
            facet::EndPropertyRow();
        }
        facet::EndPropertyCategory();
    }
    facet::EndPropertyGrid();
    ImGui::EndGroup();

    SectionHeader("Canvas and layers");
    LabeledRow("Preview canvas");
    ImGui::BeginGroup();
    facet::BeginPreviewCanvas("canvas", kReferenceSize, {.safeArea = 0.05f});
    facet::CanvasElement("Element 1", &ui.element);
    facet::EndPreviewCanvas();
    ImGui::EndGroup();
    LabeledRow("Layer list");
    facet::LayerList("layers", &ui.layer, ui.layers);
}

FACET_DEMO_SELFTEST("composites-editors")
{
    test.Do([] { ui = EditorsUi{}; });
    // The curve editor placeholder writes nothing.
    test.DoubleClick("composites.curve");
    test.Expect("a double click on the curve editor placeholder adds no point", [] { return ui.curveCount == 3; });

    // The stock checkbox inside the property row, by mouse and by keyboard.
    test.Do([] { ui.option = false; });
    test.Click("composites.property");
    test.Expect("a click checks the property checkbox", [] { return ui.option; });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.property");
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on it clears it again", [] { return !ui.option; });
}
