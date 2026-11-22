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
#include "internal/popup_host.h"
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
    kGradientOpen,
    kGradientFocused,
};

// Placeholder data: names like "Layer 1" and property values that only show the widgets working.
struct EditorsUi
{
    ImVec2 curve[8] = {{0.0f, 0.0f}, {0.5f, 0.7f}, {1.0f, 1.0f}};
    int curveCount = 3;
    facet::CubicBezier easing{{0.25f, 0.1f}, {0.25f, 1.0f}};
    // Gradients: seeded from the chart palette of the theme on the first frame (data colors).
    bool gradientSeeded = false;
    facet::GradientStop stops[8] = {};
    int stopCount = 0;
    facet::GradientStop alphaStops[8] = {};
    int alphaCount = 0;
    facet::GradientStop fullStops[2] = {};
    int fullCount = 0;
    facet::GradientStop fixedStops[2] = {};  // the read-only and disabled examples
    int gradientWrites = 0;
    ImGuiID gradientId = 0;
    bool gradientPopover = false;
    float savedPosition = 0.0f;   // a value a selftest step compares with
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

void SeedGradients()
{
    if (ui.gradientSeeded)
        return;
    ui.gradientSeeded = true;
    const facet::Colors& c = facet::T().colors;
    ui.stops[0] = {0.0f, c.chart[0]};
    ui.stops[1] = {0.5f, c.chart[1]};
    ui.stops[2] = {1.0f, c.chart[2]};
    ui.stopCount = 3;
    ui.alphaStops[0] = {0.0f, facet::WithAlpha(c.chart[3], 0.0f)};
    ui.alphaStops[1] = {1.0f, c.chart[3]};
    ui.alphaCount = 2;
    ui.fullStops[0] = {0.0f, c.chart[4]};
    ui.fullStops[1] = {1.0f, c.chart[5]};
    ui.fullCount = 2;
    ui.fixedStops[0] = {0.0f, c.chart[0]};
    ui.fixedStops[1] = {1.0f, c.chart[2]};
}

// A probe for the bar and for each marker of the gradient editor `id`.
void ProbeGradient(facet::demo::PageCtx& ctx, ImGuiID id, const char* prefix, int count)
{
    const ImRect bar = facet::internal::GradientEditorBarRect(id);
    char name[64];
    std::snprintf(name, sizeof(name), "%s.bar", prefix);
    facet::demo::ProbeRect(ctx, name, ImHashStr("##bar", 0, id), bar.Min, bar.Max);
    for (int i = 0; i < count; ++i)
    {
        ImGuiID itemId = 0;
        const ImRect stop = facet::internal::GradientEditorStopRect(id, i, &itemId);
        std::snprintf(name, sizeof(name), "%s.stop%d", prefix, i);
        facet::demo::ProbeRect(ctx, name, itemId, stop.Min, stop.Max);
    }
}

void GradientSection(facet::demo::PageCtx& ctx)
{
    using namespace facet;
    const bool capture = ctx.capture;
    SeedGradients();
    SectionHeader("Gradient editor");
    Paragraph("A click on the bar adds a stop with the color shown there. Drag a stop along the row under the bar; it "
              "passes the others, and dragged far above or below the editor it is removed on release. Each stop is a "
              "Tab stop: the arrows move it, Insert adds one half way to its neighbour, Delete removes it. A click or "
              "Enter on a stop opens its color picker; Escape closes it and restores the color.");
    Note("Placeholder colors from the chart palette of the theme.");
    LabeledRow("Opaque");
    if (capture && ctx.captureState == kGradientOpen)
        ctx.ForceNextItemState(demo::ForcedItemState::Open, 1);
    if (capture && ctx.captureState == kGradientFocused)
        ctx.ForceNextItemState(demo::ForcedItemState::Focused, 1);
    ui.gradientId = ImGui::GetID("Gradient");
    if (GradientEditor("Gradient", ui.stops, &ui.stopCount, {.width = demo::DemoWidth(480.0f)}))
        ++ui.gradientWrites;
    ctx.Probe("composites.gradient");
    ProbeGradient(ctx, ui.gradientId, "composites.gradient", ui.stopCount);
    ui.gradientPopover = internal::IsPopupLevelOpen(internal::GradientEditorPopoverLevel(ui.gradientId));
    if (capture && (ctx.captureState == kGradientOpen || ctx.captureState == kGradientFocused))
    {
        ctx.RevealInCapture(0.2f);
        if (ctx.captureFramesLeft == 0 && ctx.captureState == kGradientOpen && !ui.gradientPopover)
            ctx.captureError = "the gradient stop popover did not open";
    }
    {
        char positions[96] = "";
        int used = 0;
        for (int i = 0; i < ui.stopCount && used < static_cast<int>(sizeof(positions)) - 8; ++i)
            used += std::snprintf(positions + used, sizeof(positions) - static_cast<std::size_t>(used), "%s%.0f %%",
                                  i == 0 ? "" : ", ", ui.stops[i].position * 100.0f);
        LabeledRow("");
        Note("%d stops at %s.", ui.stopCount, positions);
    }
    LabeledRow("With alpha");
    GradientEditor("Translucent", ui.alphaStops, &ui.alphaCount, {.alpha = true, .width = demo::DemoWidth(480.0f)});
    LabeledRow("Storage full");
    GradientEditor("Two stops at most", ui.fullStops, &ui.fullCount, {.width = demo::DemoWidth(480.0f)});
    ctx.Probe("composites.gradientFull");
    ProbeGradient(ctx, ImGui::GetItemID(), "composites.gradientFull", ui.fullCount);
    LabeledRow("");
    Note("The storage holds two stops: a click on the bar adds nothing.");
    int fixedCount = 2;
    LabeledRow("Read-only");
    GradientEditor("Read-only gradient", ui.fixedStops, &fixedCount, {.width = demo::DemoWidth(480.0f), .readOnly = true});
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    GradientEditor("Disabled gradient", ui.fixedStops, &fixedCount, {.width = demo::DemoWidth(480.0f)});
    ImGui::EndDisabled();
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
                "grid-no-results", "grid-column", "gradient-open", "gradient-focused")
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

    GradientSection(ctx);
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

    // ---- Gradient editor ----
    test.Reveal("composites.gradient");
    test.WaitSeconds(0.4f);
    test.Click("composites.gradient.bar");  // the middle of the bar
    test.Expect("a click on the bar adds a stop", [] { return ui.stopCount == 4; });
    test.KeyPress(ImGuiKey_Delete);  // the new stop has the focus
    test.Expect("Delete removes the focused new stop",
                [] { return ui.stopCount == 3 && ui.stops[1].position == 0.5f && ui.stops[2].position == 1.0f; });
    // A stop dragged past its neighbour passes it and keeps its drag.
    test.Drag({T::Item("composites.gradient.stop0"), T::Item("composites.gradient.stop1"),
               T::Item("composites.gradient.stop2").Offset(-40.0f, 0.0f)},
              0.4f);
    test.Expect("the dragged first stop passed the middle one and the stops stay sorted", [] {
        return ui.stopCount == 3 && ui.stops[0].position == 0.5f && ui.stops[1].position > 0.5f &&
               ui.stops[1].position < ui.stops[2].position;
    });
    // Keyboard: the arrows move the focused stop, Insert adds one, Delete removes it.
    test.KeyPress(ImGuiKey_Tab);  // shows the focus the drag gave the stop
    test.ExpectFocused("composites.gradient.stop1");
    test.Do([] { ui.savedPosition = ui.stops[1].position; });
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Expect("Left moves the focused stop", [] { return ui.stops[1].position < ui.savedPosition && ui.stopCount == 3; });
    test.KeyPress(ImGuiKey_Insert);
    test.Expect("Insert adds a stop", [] { return ui.stopCount == 4; });
    test.KeyPress(ImGuiKey_Delete);
    test.Expect("Delete removes it again", [] { return ui.stopCount == 3; });
    // A click on a stop opens its color popover; Escape closes it and writes nothing.
    test.Do([] { ui.gradientWrites = 0; });
    test.Click("composites.gradient.stop2");
    test.WaitSeconds(0.3f);
    test.Expect("a click on a stop opens its color popover", [] { return ui.gradientPopover; });
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.3f);
    test.Expect("Escape closes it without a change", [] { return !ui.gradientPopover && ui.gradientWrites == 0; });
    // A stop dragged far below the editor is removed on release.
    test.Drag({T::Item("composites.gradient.stop1"), T::Item("composites.gradient.stop1").Offset(0.0f, 90.0f)}, 0.3f);
    test.Expect("a stop dragged off the editor is removed", [] { return ui.stopCount == 2; });
    // The storage of two stops is full.
    test.Click("composites.gradientFull.bar");
    test.Expect("a full storage takes no new stop", [] { return ui.fullCount == 2; });

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
