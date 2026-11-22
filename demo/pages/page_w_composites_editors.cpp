// Demo page: Editors and canvas. One section per editor, property grid, canvas and layer composite of
// facet/widgets/composites.h, each with its variants and states, capture states and selftest steps.
// A widget that is still planned draws the placeholder of its declared size. The inventory and cards
// and the search and app controls have pages of their own (page_w_composites_cards.cpp and
// page_w_composites_settings.cpp).
//
// The values on this page are placeholders: property names and numbers that only show the widgets
// working, nothing real.

#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"
#include "selftest.h"
#include "widgets/composites/editors_frame.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// Reference resolution of the example preview canvas, in reference pixels.
const ImVec2 kReferenceSize(1920.0f, 1080.0f);

const char* const kBlendModes[] = {"Normal", "Multiply", "Screen"};

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kGridFiltered,
    kGridNoResults,
    kGridColumn,
};

// Placeholder data: names like "Layer 1" and property values that only show the widgets working.
struct EditorsUi
{
    ImVec2 curve[8] = {{0.0f, 0.0f}, {0.5f, 0.7f}, {1.0f, 1.0f}};
    int curveCount = 3;
    facet::CubicBezier easing{{0.25f, 0.1f}, {0.25f, 1.0f}};
    facet::GradientStop stops[8] = {{0.0f, {0.0f, 0.0f, 0.0f, 1.0f}}, {1.0f, {1.0f, 1.0f, 1.0f, 1.0f}}};
    int stopCount = 2;
    facet::CanvasElementState element{facet::CanvasAnchor::TopLeft, {64.0f, 64.0f}, {480.0f, 120.0f}};
    facet::LayerItem layers[3] = {{"Layer 1"}, {"Layer 2", {}, 1}, {"Layer 3", {}, 0, false}};
    int layer = 0;

    // Property grid.
    char filter[64] = "";
    ImVec2 position = {0.0f, 0.0f};
    ImVec2 size = {100.0f, 100.0f};
    float rotation = 0.0f;
    bool visible = true;
    float opacity = 1.0f;
    ImVec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
    int blend = 0;
    char name[32] = "Item 1";
    int copies = 1;
    int changes = 0;              // change signals of the grid's controls
    char lastEdited[32] = "";     // the row whose control reported the last change
    ImGuiID gridId = 0;
    int gridRows = 0;             // rows the main grid showed
    bool textOpen = false;        // the "Text" category returned true this frame
};

EditorsUi ui;
// Each selftest run starts a new grid id, so the open categories and the column a run leaves behind
// (kept under the id like ini records) do not carry into the next run.
int gridGeneration = 0;

void NoteChange(bool changed, const char* row)
{
    if (!changed)
        return;
    ++ui.changes;
    std::snprintf(ui.lastEdited, sizeof(ui.lastEdited), "%s", row);
}

void PropertyGridSection(facet::demo::PageCtx& ctx)
{
    using namespace facet;
    const bool capture = ctx.capture;
    SectionHeader("Property grid");
    Paragraph("Rows share one label column. Drag its edge between the labels and the controls, or reach it with Tab "
              "after the last row and use the arrows, Home and End; a double click returns it to the default. A "
              "category header opens and closes with a click, Enter, Space, Left and Right. The filter matches the "
              "labels and the keywords of the rows.");
    Note("Placeholder properties: the names and values only show the grid working.");
    if (capture)
    {
        // The capture state sets itself up again in each theme's pass.
        const char* filter = ctx.captureState == kGridFiltered ? "op" : (ctx.captureState == kGridNoResults ? "zzz" : "");
        std::snprintf(ui.filter, sizeof(ui.filter), "%s", filter);
    }
    LabeledRow("Filter");
    SearchField("##filter", ui.filter, sizeof(ui.filter), {.width = demo::DemoWidth(240.0f)});
    ctx.Probe("composites.grid.filter");
    ImGui::Spacing();

    BeginStack("##properties", {.width = demo::DemoWidth(560.0f)});
    if (capture && ctx.captureState == kGridColumn)
        ctx.ForceNextItemState(demo::ForcedItemState::Hovered | demo::ForcedItemState::Dragging, 0);
    char gridId[48];
    std::snprintf(gridId, sizeof(gridId), "properties##%d", gridGeneration);
    BeginPropertyGrid(gridId, {.filter = ui.filter});
    ui.gridId = ImGui::GetCurrentWindow()->IDStack.back();
    if (BeginPropertyCategory("Transform"))
    {
        if (BeginPropertyRow("Position", {.modified = ui.position.x != 0.0f || ui.position.y != 0.0f}))
        {
            NoteChange(VectorInput("##value", &ui.position), "Position");
            EndPropertyRow();
        }
        if (BeginPropertyRow("Size"))
        {
            NoteChange(VectorInput("##value", &ui.size), "Size");
            EndPropertyRow();
        }
        if (BeginPropertyRow("Rotation", {.tooltip = "Degrees, clockwise", .keywords = "angle"}))
        {
            NoteChange(Slider("##value", &ui.rotation, -180.0f, 180.0f, {.format = "%.0f"}), "Rotation");
            EndPropertyRow();
        }
        EndPropertyCategory();
    }
    if (BeginPropertyCategory("Appearance"))
    {
        if (BeginPropertyRow("Visible", {.keywords = "show hide"}))
        {
            NoteChange(Toggle("##value", &ui.visible), "Visible");
            ctx.Probe("composites.grid.visible");
            EndPropertyRow();
        }
        if (BeginPropertyRow("Opacity", {.tooltip = "0 is transparent, 1 is opaque", .modified = ui.opacity != 1.0f}))
        {
            NoteChange(Slider("##value", &ui.opacity, 0.0f, 1.0f, {.format = "%.2f"}), "Opacity");
            ctx.Probe("composites.grid.opacity");
            EndPropertyRow();
        }
        if (BeginPropertyRow("Tint"))
        {
            NoteChange(ColorWell("##value", &ui.tint, {.showHex = true}), "Tint");
            EndPropertyRow();
        }
        if (BeginPropertyRow("Blend mode"))
        {
            NoteChange(Dropdown("##value", &ui.blend, std::span<const char* const>(kBlendModes)), "Blend mode");
            EndPropertyRow();
        }
        EndPropertyCategory();
    }
    ui.textOpen = BeginPropertyCategory("Text", {.defaultOpen = false});
    ctx.Probe("composites.grid.text");
    if (ui.textOpen)
    {
        if (BeginPropertyRow("Name"))
        {
            NoteChange(TextField("##value", ui.name, sizeof(ui.name)), "Name");
            EndPropertyRow();
        }
        if (BeginPropertyRow("Copies"))
        {
            NoteChange(NumberInput("##value", &ui.copies, {.min = 0.0, .max = 99.0}), "Copies");
            EndPropertyRow();
        }
        EndPropertyCategory();
    }
    EndPropertyGrid();
    const internal::PropertyGridDividerRecord divider = internal::LastPropertyGridDivider();
    ui.gridRows = internal::LastPropertyGridRowCount();
    if (divider.id != 0)
        // The probe covers the edge beside the first row, so its centre is never on a category header.
        demo::ProbeRect(ctx, "composites.grid.column", divider.id, divider.band.Min,
                        ImVec2(divider.band.Max.x, divider.band.Min.y + ControlHeight(ControlSize::Regular)));
    EndStack();
    if (capture && ctx.captureState != kDefault && ctx.captureState <= kGridColumn)
        ctx.RevealInCapture(0.75f);
    if (ui.changes == 0)
        Note("No change yet.");
    else
        Note("%d changes, the last one in %s.", ui.changes, ui.lastEdited);

    // Too narrow for a label column beside the controls: every row stacks its control under the label.
    LabeledRow("Narrow");
    BeginStack("##narrowGrid", {.width = demo::DemoWidth(200.0f)});
    BeginPropertyGrid("narrow properties");
    if (BeginPropertyRow("Visible"))
    {
        NoteChange(Toggle("##value", &ui.visible), "Visible");
        EndPropertyRow();
    }
    if (BeginPropertyRow("Opacity"))
    {
        NoteChange(Slider("##value", &ui.opacity, 0.0f, 1.0f, {.format = "%.2f"}), "Opacity");
        EndPropertyRow();
    }
    EndPropertyGrid();
    EndStack();

    if (capture && ctx.captureState != kDefault && ctx.captureState <= kGridColumn)
    {
        if (ctx.captureFramesLeft == 0)
        {
            if (ctx.captureState == kGridNoResults && ui.gridRows != 0)
                ctx.captureError = "the property grid filter still shows rows";
            if (ctx.captureState == kGridFiltered && ui.gridRows == 0)
                ctx.captureError = "the property grid filter shows no row";
            if (ctx.captureState == kGridColumn && divider.id == 0)
                ctx.captureError = "the property grid has no column edge";
        }
    }
}

} // namespace

FACET_DEMO_PAGE("composites-editors", "Editors and canvas", "Widgets", 144, "default", "grid-filtered",
                "grid-no-results", "grid-column")
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

    PropertyGridSection(ctx);

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
    using T = facet::demo::Target;
    test.Do([] {
        ui = EditorsUi{};
        ++gridGeneration;  // a grid of its own: no open or column state from an earlier run
    });
    // The curve editor placeholder writes nothing.
    test.DoubleClick("composites.curve");
    test.Expect("a double click on the curve editor placeholder adds no point", [] { return ui.curveCount == 3; });

    // ---- Property grid ----
    // Each control edits through the finished widget and reports one change per edit.
    test.Reveal("composites.grid.visible");
    test.WaitSeconds(0.4f);
    test.Click("composites.grid.visible");
    test.Expect("a click on the Visible toggle turns it off", [] { return !ui.visible; });
    test.Expect("the toggle reported one change", [] { return ui.changes == 1; });
    test.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.grid.opacity");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("Left steps the opacity down", [] { return ui.opacity; }, 0.99, 0.001);
    test.Expect("the step reported one change", [] { return ui.changes == 2; });

    // The column edge: a drag moves it, the arrows step it, a double click resets it.
    test.Reveal("composites.grid.column");
    test.WaitSeconds(0.4f);
    test.Drag({T::Item("composites.grid.column"), T::Item("composites.grid.column").Offset(10.0f, 0.0f)}, 0.3f);
    test.ExpectNear("a drag widens the label column by 10 units",
                    [] { return facet::internal::PropertyGridColumnUnits(ui.gridId); },
                    facet::T().layout.labelColumn + 10.0, 1.0);
    test.KeyPress(ImGuiKey_Tab);  // shows the focus the press gave the edge
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("Left on the focused edge steps it by spacing.lg",
                    [] { return facet::internal::PropertyGridColumnUnits(ui.gridId); },
                    facet::T().layout.labelColumn + 10.0 - facet::T().spacing.lg, 1.0);
    test.DoubleClick("composites.grid.column");
    test.ExpectNear("a double click returns the column to the default",
                    [] { return facet::internal::PropertyGridColumnUnits(ui.gridId); }, facet::T().layout.labelColumn,
                    0.01);

    // A category header by mouse and keyboard.
    test.Reveal("composites.grid.text");
    test.WaitSeconds(0.4f);
    test.Click("composites.grid.text");
    test.WaitSeconds(0.4f);
    test.Expect("a click opens the Text category", [] { return ui.textOpen; });
    test.KeyPress(ImGuiKey_Tab);  // shows the focus on the header
    test.ExpectFocused("composites.grid.text");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.WaitSeconds(0.4f);
    test.Expect("Left closes it", [] { return !ui.textOpen; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.WaitSeconds(0.4f);
    test.Expect("Right opens it", [] { return ui.textOpen; });
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.4f);
    test.Expect("Space closes it", [] { return !ui.textOpen; });

    // The filter: no hit, Escape, a keyword, matches in a closed category.
    test.Reveal("composites.grid.filter");
    test.WaitSeconds(0.4f);
    test.Click("composites.grid.filter");
    test.Type("zzz");
    test.Wait(2);
    test.Expect("a filter without a hit shows no row", [] { return ui.gridRows == 0; });
    test.KeyPress(ImGuiKey_Escape);  // clears the query
    test.Wait(2);
    test.Expect("Escape clears the filter", [] { return ui.filter[0] == '\0'; });
    test.Type("angle");
    test.Wait(2);
    test.Expect("a keyword finds its row", [] { return ui.gridRows == 1; });
    test.KeyPress(ImGuiKey_Escape);
    test.Type("op");
    test.Wait(2);
    test.Expect("the filter shows the matching rows, also in a closed category",
                [] { return ui.gridRows == 2; });
    test.KeyPress(ImGuiKey_Escape);
    test.Wait(2);
}
