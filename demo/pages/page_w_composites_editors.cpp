// Demo page: Editors and canvas. One section per editor, property grid, canvas and layer composite of
// facet/widgets/composites.h, each with its variants and states, capture states and selftest steps.
// The inventory and cards and the search and app controls have pages of their own
// (page_w_composites_cards.cpp and page_w_composites_settings.cpp).
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
    kCurveDragging,
    kCurveFocused,
    kEasingDragging,
    kLayersFocused,
    kCanvasSelected,
};

const char* const kInterpolations[] = {"Smooth", "Linear", "Step"};

// Placeholder data: names like "Layer 1" and property values that only show the widgets working.
struct EditorsUi
{
    ImVec2 curve[8] = {{0.0f, 0.0f}, {0.3f, 0.6f}, {0.7f, 0.4f}, {1.0f, 1.0f}};
    int curveCount = 4;
    int interpolation = 0;
    bool lockEnds = true;
    bool curveGrid = true;
    float curveInput = 0.5f;
    int curveWrites = 0;
    ImGuiID curveId = 0;
    ImVec2 fixedCurve[3] = {{0.0f, 0.2f}, {0.5f, 0.8f}, {1.0f, 0.6f}};  // the read-only and disabled examples
    facet::CubicBezier easing{{0.25f, 0.1f}, {0.25f, 1.0f}};
    bool overshoot = false;
    bool easingFields = true;
    bool easingPresets = true;
    int easingWrites = 0;
    ImGuiID easingId = 0;
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
    facet::CanvasElementState element{facet::CanvasAnchor::TopLeft, {192.0f, 128.0f}, {480.0f, 270.0f}};
    facet::CanvasElementState corner{facet::CanvasAnchor::BottomRight, {-96.0f, -96.0f}, {320.0f, 180.0f}};
    facet::CanvasElementState oversized{facet::CanvasAnchor::Center, {0.0f, 0.0f}, {2400.0f, 1400.0f}};
    bool canvasGrid = true;
    bool canvasSafeArea = true;
    bool canvasSnap = true;
    bool canvasResizable = true;
    bool canvasOversized = false;
    int canvasWrites = 0;
    ImGuiID elementId = 0;
    facet::LayerItem layers[5] = {{"Layer 1", {}, 0, true, false, 1},
                                  {"Group 1", {}, 0, true, false, 2},
                                  {"Layer 2", {}, 1, true, false, 3},
                                  {"Layer 3", {}, 1, false, false, 4},
                                  {"Background", {}, 0, true, true, 5}};
    int layer = 0;
    int layerState = 0;           // the data state shown: Ready, Loading, Error, no layers
    char layerAction[64] = "";
    ImGuiID layersId = 0;
    facet::LayerItem fixedLayers[2] = {{"Layer 1"}, {"Layer 2", {}, 0, false}};  // the read-only example
    int fixedLayer = 0;

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

void CurveSection(facet::demo::PageCtx& ctx)
{
    using namespace facet;
    const bool capture = ctx.capture;
    SectionHeader("Curve editor");
    Paragraph("A double click on empty space adds a point; a double click on a point or Delete removes it, never below "
              "two points and never a locked end. Each point is a Tab stop: the arrows move it, Shift finer, Ctrl "
              "coarser. A point never passes its neighbours. The guide shows the input below on the curve.");
    LabeledRow("Interpolation");
    SegmentedControl("##interpolation", &ui.interpolation, std::span<const char* const>(kInterpolations));
    LabeledRow("Lock ends");
    Toggle("##lockEnds", &ui.lockEnds);
    LabeledRow("Grid");
    Toggle("##grid", &ui.curveGrid);
    LabeledRow("Input");
    Slider("##input", &ui.curveInput, 0.0f, 1.0f, {.format = "%.2f", .width = demo::DemoWidth(240.0f)});
    ctx.Probe("composites.curve.input");
    LabeledRow("Response");
    if (capture && ctx.captureState == kCurveDragging)
        ctx.ForceNextItemState(demo::ForcedItemState::Hovered | demo::ForcedItemState::Dragging, 1);
    if (capture && ctx.captureState == kCurveFocused)
        ctx.ForceNextItemState(demo::ForcedItemState::Focused, 1);
    ui.curveId = ImGui::GetID("Response");
    CurveEditorOptions options;
    options.interpolation = static_cast<CurveInterpolation>(ui.interpolation);
    options.lockEnds = ui.lockEnds;
    options.grid = ui.curveGrid;
    options.marker = ui.curveInput;
    options.width = demo::DemoWidth(480.0f);
    if (CurveEditor("Response", ui.curve, &ui.curveCount, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), options))
        ++ui.curveWrites;
    ctx.Probe("composites.curve");
    {
        const float handle = Px(T().composites.editorHandleSize);
        for (int i = 0; i < ui.curveCount; ++i)
        {
            const ImVec2 at = internal::CurveEditorPointPos(ui.curveId, ui.curve[i]);
            char name[32];
            std::snprintf(name, sizeof(name), "composites.curve.p%d", i);
            demo::ProbeRect(ctx, name, internal::CurveEditorPointId(ui.curveId, i), ImVec2(at.x - handle * 0.5f, at.y - handle * 0.5f),
                            ImVec2(at.x + handle * 0.5f, at.y + handle * 0.5f));
        }
    }
    if (capture && (ctx.captureState == kCurveDragging || ctx.captureState == kCurveFocused))
        ctx.RevealInCapture(0.6f);
    LabeledRow("");
    Note("%d of %d points in the storage.", ui.curveCount, static_cast<int>(IM_ARRAYSIZE(ui.curve)));
    LabeledRow("Read-only");
    int fixedCount = 3;
    CurveEditor("Read-only curve", ui.fixedCurve, &fixedCount, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                {.width = demo::DemoWidth(240.0f), .height = 96.0f, .readOnly = true});
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    CurveEditor("Disabled curve", ui.fixedCurve, &fixedCount, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                {.width = demo::DemoWidth(240.0f), .height = 96.0f});
    ImGui::EndDisabled();
}

void BezierSection(facet::demo::PageCtx& ctx)
{
    using namespace facet;
    const bool capture = ctx.capture;
    SectionHeader("Bezier editor");
    Paragraph("The two control points of an easing curve are Tab stops: drag them or move them with the arrows. The "
              "menu under the curve sets a common easing, the fields type the four numbers, and after each edit a dot "
              "runs the curve once.");
    LabeledRow("Overshoot");
    Toggle("##overshoot", &ui.overshoot);
    ctx.Probe("composites.easing.overshoot");
    LabeledRow("Number fields");
    Toggle("##fields", &ui.easingFields);
    LabeledRow("Presets");
    Toggle("##presets", &ui.easingPresets);
    LabeledRow("Easing");
    if (capture && ctx.captureState == kEasingDragging)
        ctx.ForceNextItemState(demo::ForcedItemState::Hovered | demo::ForcedItemState::Dragging, 0);
    ui.easingId = ImGui::GetID("Easing");
    if (BezierEditor("Easing", &ui.easing,
                     {.presets = ui.easingPresets, .allowOvershoot = ui.overshoot, .numericFields = ui.easingFields,
                      .width = demo::DemoWidth(320.0f)}))
        ++ui.easingWrites;
    ctx.Probe("composites.easing");
    {
        const float handle = Px(T().composites.editorHandleSize);
        const ImVec2 points[2] = {ui.easing.p1, ui.easing.p2};
        for (int i = 0; i < 2; ++i)
        {
            const ImVec2 at = internal::BezierEditorPointPos(ui.easingId, points[i]);
            demo::ProbeRect(ctx, i == 0 ? "composites.easing.p0" : "composites.easing.p1",
                            internal::BezierEditorPointId(ui.easingId, i), ImVec2(at.x - handle * 0.5f, at.y - handle * 0.5f),
                            ImVec2(at.x + handle * 0.5f, at.y + handle * 0.5f));
        }
    }
    if (capture && ctx.captureState == kEasingDragging)
        ctx.RevealInCapture(0.6f);
    LabeledRow("");
    Note("Control points %.2f, %.2f and %.2f, %.2f.", ui.easing.p1.x, ui.easing.p1.y, ui.easing.p2.x, ui.easing.p2.y);
    LabeledRow("Read-only");
    CubicBezier fixed{{0.42f, 0.0f}, {0.58f, 1.0f}};
    BezierEditor("Read-only easing", &fixed,
                 {.presets = false, .numericFields = false, .width = demo::DemoWidth(240.0f), .height = 96.0f, .readOnly = true});
}

const char* const kLayerStates[] = {"Ready", "Loading", "Error", "Empty"};

void LayerSection(facet::demo::PageCtx& ctx)
{
    using namespace facet;
    const bool capture = ctx.capture;
    SectionHeader("Layer list");
    Paragraph("Rows are one Tab stop: Up and Down move the selection, Alt+Up and Alt+Down move the layer, Space picks "
              "it up for the arrows and Space or Enter puts it down. Tab goes on to the focused row's visibility and "
              "lock toggles. Drag across the toggles to set several; Alt+click on a visibility toggle shows that layer "
              "alone.");
    Note("Placeholder layers.");
    LabeledRow("Data state");
    SegmentedControl("##layerState", &ui.layerState, std::span<const char* const>(kLayerStates));
    LabeledRow("Layers");
    if (capture && ctx.captureState == kLayersFocused)
        ctx.ForceNextItemState(demo::ForcedItemState::Focused, 1);
    ui.layersId = ImGui::GetID("layers");
    const DataState states[] = {DataState::Ready, DataState::Loading, DataState::Error, DataState::Ready};
    const std::span<LayerItem> rows = ui.layerState == 3 ? std::span<LayerItem>() : std::span<LayerItem>(ui.layers);
    const LayerListResult result =
        LayerList("layers", &ui.layer, rows, {.state = states[ui.layerState], .width = demo::DemoWidth(320.0f)});
    ctx.Probe("composites.layers");
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        const char* parts[] = {"row", "visible", "lock"};
        for (int part = 0; part < 3; ++part)
        {
            ImGuiID itemId = 0;
            const ImRect rect = internal::LayerListRowRect(ui.layersId, i, part, rows[i].id, &itemId);
            char name[48];
            std::snprintf(name, sizeof(name), "composites.layers.%s%d", parts[part], i);
            demo::ProbeRect(ctx, name, itemId, rect.Min, rect.Max);
        }
    }
    if (capture && ctx.captureState == kLayersFocused)
        ctx.RevealInCapture(0.5f);
    switch (result.action)
    {
    case LayerAction::Moved:
        ApplyReorderMove(ui.layers, result.move);
        std::snprintf(ui.layerAction, sizeof(ui.layerAction), "Moved from row %d to row %d.", result.move.from + 1,
                      result.move.to + 1);
        break;
    case LayerAction::Visibility:
        std::snprintf(ui.layerAction, sizeof(ui.layerAction), "Visibility of row %d.", result.index + 1);
        break;
    case LayerAction::Lock:
        std::snprintf(ui.layerAction, sizeof(ui.layerAction), "Lock of row %d.", result.index + 1);
        break;
    case LayerAction::Selected:
        std::snprintf(ui.layerAction, sizeof(ui.layerAction), "Selected row %d.", ui.layer + 1);
        break;
    case LayerAction::None:
        break;
    }
    LabeledRow("");
    Note("%s", ui.layerAction[0] != '\0' ? ui.layerAction : "No action yet.");
    LabeledRow("Read-only");
    LayerList("read-only layers", &ui.fixedLayer, ui.fixedLayers, {.width = demo::DemoWidth(320.0f), .readOnly = true});
}

const char* const kAnchorNames[] = {"top left", "top", "top right", "left", "center", "right", "bottom left", "bottom",
                                    "bottom right"};

void CanvasSection(facet::demo::PageCtx& ctx)
{
    using namespace facet;
    const bool capture = ctx.capture;
    SectionHeader("Preview canvas");
    Paragraph("A 1920 by 1080 screen, scaled to fit. Drag an element, or select it with a click or Tab and move it "
              "with the arrows (Shift: ten reference pixels); Ctrl with the arrows resizes it. A selected element "
              "shows its handles and the anchor control. Moved elements snap to the edges, the center, the safe area "
              "and each other; Alt moves freely.");
    Note("Placeholder elements.");
    LabeledRow("Grid");
    Toggle("##canvasGrid", &ui.canvasGrid);
    LabeledRow("Safe area");
    Toggle("##canvasSafe", &ui.canvasSafeArea);
    LabeledRow("Snap");
    Toggle("##canvasSnap", &ui.canvasSnap);
    LabeledRow("Resizable");
    Toggle("##canvasResizable", &ui.canvasResizable);
    LabeledRow("Larger than the canvas");
    Toggle("##canvasOversized", &ui.canvasOversized);
    ctx.Probe("composites.canvas.oversized");
    LabeledRow("Canvas");
    BeginPreviewCanvas("canvas", kReferenceSize,
                       {.gridSpacing = ui.canvasGrid ? 120.0f : 0.0f, .safeArea = ui.canvasSafeArea ? 0.05f : 0.0f,
                        .width = demo::DemoWidth(560.0f)});
    if (ui.canvasOversized)
        CanvasElement("Oversized", &ui.oversized, {.snap = ui.canvasSnap});
    if (capture && ctx.captureState == kCanvasSelected)
        ctx.ForceNextItemState(demo::ForcedItemState::Focused, 0);
    ui.elementId = ImGui::GetID("Element 1");
    if (CanvasElement("Element 1", &ui.element, {.resizable = ui.canvasResizable, .snap = ui.canvasSnap}))
        ++ui.canvasWrites;
    ctx.Probe("composites.canvas.e1");
    CanvasElement("Element 2 (read-only)", &ui.corner, {.readOnly = true});
    {
        const char* parts[] = {"composites.canvas.e1.h5", "composites.canvas.e1.center"};
        const int indices[] = {5, 9 + static_cast<int>(CanvasAnchor::Center)};
        for (int k = 0; k < 2; ++k)
        {
            ImGuiID itemId = 0;
            const ImRect rect = internal::CanvasElementPartRect(ui.elementId, indices[k], &itemId);
            if (rect.GetWidth() > 0.0f)
                demo::ProbeRect(ctx, parts[k], itemId, rect.Min, rect.Max);
        }
    }
    EndPreviewCanvas();
    ctx.Probe("composites.canvas");
    if (capture && ctx.captureState == kCanvasSelected)
        ctx.RevealInCapture(0.6f);
    LabeledRow("");
    Note("Element 1: anchor %s, offset %.0f, %.0f, size %.0f by %.0f.", kAnchorNames[static_cast<int>(ui.element.anchor)],
         ui.element.offset.x, ui.element.offset.y, ui.element.extent.x, ui.element.extent.y);
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
                "grid-no-results", "grid-column", "gradient-open", "gradient-focused", "curve-dragging", "curve-focused",
                "easing-dragging", "layers-focused",
                "canvas-selected")
{
    facet::demo::PageHeader("Editors and canvas",
                            "Editors for curves, easing and gradients, property grids, preview canvases with their "
                            "elements, and layer lists.");
    facet::demo::ReadyFamilyNote("composites");

    CurveSection(ctx);
    BezierSection(ctx);

    GradientSection(ctx);
    PropertyGridSection(ctx);

    LayerSection(ctx);

    CanvasSection(ctx);
}

FACET_DEMO_SELFTEST("composites-editors")
{
    using T = facet::demo::Target;
    test.Do([] {
        ui = EditorsUi{};
        ++gridGeneration;  // a grid of its own: no open or column state from an earlier run
    });
    // ---- Curve editor ----
    test.Reveal("composites.curve");
    test.WaitSeconds(0.4f);
    test.Click("composites.curve.input");  // a click before the editor, so Tab reaches its first point
    test.KeyPress(ImGuiKey_Tab);           // after a click the first Tab shows the focus
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.curve.p0");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.curve.p1");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up moves the focused point up", [] { return ui.curve[1].y; }, 0.61, 0.001);
    test.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    test.ExpectNear("Ctrl+Right moves it a tenth of the range", [] { return ui.curve[1].x; }, 0.4, 0.001);
    // A drag far to the right stops at the right neighbour.
    test.Drag({T::Item("composites.curve.p1"), T::Item("composites.curve.p1").Offset(900.0f, 0.0f)}, 0.3f);
    test.Expect("a point never passes its neighbour", [] { return ui.curve[1].x == ui.curve[2].x; });
    // A double click on empty space adds a point; a double click on a point removes it.
    test.DoubleClick("composites.curve");
    test.Expect("a double click on the canvas adds a point", [] { return ui.curveCount == 5; });
    test.Expect("the points stay sorted by x", [] {
        for (int i = 1; i < ui.curveCount; ++i)
            if (ui.curve[i - 1].x > ui.curve[i].x)
                return false;
        return true;
    });
    test.KeyPress(ImGuiKey_Delete);  // the new point has the focus
    test.Expect("Delete removes the focused point", [] { return ui.curveCount == 4; });
    test.DoubleClick("composites.curve.p2");
    test.Expect("a double click on a point removes it", [] { return ui.curveCount == 3; });
    test.DoubleClick("composites.curve.p0");
    test.Expect("a locked end stays", [] { return ui.curveCount == 3 && ui.curve[0].x == 0.0f; });

    // ---- Bezier editor ----
    test.Reveal("composites.easing");
    test.WaitSeconds(0.4f);
    test.Click("composites.easing.overshoot");  // turns overshoot on; Tab then reaches the first point
    test.Expect("the overshoot toggle turned on", [] { return ui.overshoot; });
    test.KeyPress(ImGuiKey_Tab);  // shows the focus on the toggle
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.easing.p0");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right moves the first control point", [] { return ui.easing.p1.x; }, 0.26, 0.001);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("composites.easing.p1");
    test.KeyPress(ImGuiKey_PageUp);
    test.Expect("with overshoot Page Up takes the second point above 1", [] { return ui.easing.p2.y > 1.0f; });
    test.Drag({T::Item("composites.easing.p1"), T::Item("composites.easing.p1").Offset(2000.0f, 0.0f)}, 0.3f);
    test.Expect("a drag keeps x at 1 at most", [] { return ui.easing.p2.x == 1.0f; });
    // The preset menu after the points: open it, take the first easing.
    test.KeyPress(ImGuiKey_Tab);  // after the drag's click the first Tab shows the focus on the point
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_Home);
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.3f);
    test.Expect("the first preset makes the curve linear", [] {
        return ui.easing.p1.x == 0.0f && ui.easing.p1.y == 0.0f && ui.easing.p2.x == 1.0f && ui.easing.p2.y == 1.0f;
    });

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

    // ---- Layer list ----
    test.Reveal("composites.layers");
    test.WaitSeconds(0.4f);
    test.Click("composites.layers.row2");
    test.Expect("a click selects a row", [] { return ui.layer == 2; });
    test.KeyPress(ImGuiKey_Tab);  // after a click the first Tab shows the focus
    test.KeyPress(ImGuiKey_DownArrow);
    test.Expect("Down moves the selection", [] { return ui.layer == 3; });
    test.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Alt);
    test.Expect("Alt+Up moves the layer up and the selection with it", [] {
        return std::strcmp(ui.layers[2].label, "Layer 3") == 0 && ui.layer == 2;
    });
    test.KeyPress(ImGuiKey_Tab);  // the focused row's visibility toggle
    test.KeyPress(ImGuiKey_Space);
    test.Expect("Space on the visibility toggle shows the hidden layer", [] { return ui.layers[2].visible; });
    test.KeyPress(ImGuiKey_Tab);  // its lock toggle
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter on the lock toggle locks it", [] { return ui.layers[2].locked; });
    test.Drag({T::Item("composites.layers.visible0"), T::Item("composites.layers.visible1"),
               T::Item("composites.layers.visible2")},
              0.4f);
    test.Expect("a drag down the visibility toggles hides three layers",
                [] { return !ui.layers[0].visible && !ui.layers[1].visible && !ui.layers[2].visible && ui.layers[3].visible; });
    test.KeyDown(ImGuiMod_Alt);
    test.Click("composites.layers.visible4");
    test.KeyUp(ImGuiMod_Alt);
    test.Expect("Alt+click shows that layer alone", [] {
        return ui.layers[4].visible && !ui.layers[0].visible && !ui.layers[1].visible && !ui.layers[2].visible &&
               !ui.layers[3].visible;
    });
    test.Drag({T::Item("composites.layers.row4"), T::Item("composites.layers.row2"),
               T::Item("composites.layers.row0", 0.5f, 0.1f)},
              0.5f);
    test.Expect("a drag moves the last layer to the top", [] { return std::strcmp(ui.layers[0].label, "Background") == 0; });
    test.Do([] { ui.layerState = 1; });
    test.Wait(2);
    test.Do([] { ui.layer = 0; });
    test.Click("composites.layers");
    test.Expect("a loading list takes no click", [] { return ui.layer == 0; });
    test.Do([] { ui.layerState = 0; });
    test.Wait(2);

    // ---- Preview canvas ----
    test.Reveal("composites.canvas");
    test.WaitSeconds(0.4f);
    test.KeyDown(ImGuiMod_Alt);  // no snapping, so the distances are exact
    test.Drag({T::Item("composites.canvas.e1"), T::Item("composites.canvas.e1").Offset(40.0f, 0.0f)}, 0.3f);
    test.KeyUp(ImGuiMod_Alt);
    test.Expect("a drag moves the element to the right", [] {
        return ui.element.offset.x > 192.0f && ui.element.offset.y == 128.0f && ui.canvasWrites > 0;
    });
    test.Do([] { ui.savedPosition = ui.element.offset.x; });
    test.KeyPress(ImGuiKey_Tab);  // shows the focus the drag gave the element
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right nudges it by one reference pixel", [] { return ui.element.offset.x == ui.savedPosition + 1.0f; });
    test.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Shift);
    test.Expect("Shift+Down nudges it by ten", [] { return ui.element.offset.y == 138.0f; });
    test.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    test.Expect("Ctrl+Right makes it one wider", [] { return ui.element.extent.x == 481.0f; });
    test.KeyDown(ImGuiMod_Alt);
    test.Drag({T::Item("composites.canvas.e1.h5"), T::Item("composites.canvas.e1.h5").Offset(20.0f, 10.0f)}, 0.3f);
    test.KeyUp(ImGuiMod_Alt);
    test.Expect("the corner handle resizes it", [] { return ui.element.extent.x > 481.0f && ui.element.extent.y > 270.0f; });
    test.Do([] { ui.savedPosition = ui.element.offset.x; });
    test.Click("composites.canvas.e1.center");
    test.Expect("the anchor control sets the center anchor and keeps the place", [] {
        const float minX = ui.element.offset.x + 960.0f - ui.element.extent.x * 0.5f;
        return ui.element.anchor == facet::CanvasAnchor::Center && std::fabs(minX - ui.savedPosition) < 0.01f;
    });
    test.Do([] { ui.savedPosition = ui.element.offset.x; });
    // The lower left part of the element: the anchor control sits beside its top right.
    test.MouseMove(T::Item("composites.canvas.e1", 0.2f, 0.8f));
    test.MouseDown();
    test.MouseMove(T::Item("composites.canvas.e1", 0.2f, 0.8f).Offset(-60.0f, 0.0f));
    test.KeyPress(ImGuiKey_Escape);
    test.MouseUp();
    test.Expect("Escape puts a dragged element back", [] { return ui.element.offset.x == ui.savedPosition; });
    test.Click("composites.canvas.oversized");
    test.Expect("an element larger than the canvas is shown", [] { return ui.canvasOversized; });
    test.Click("composites.canvas.oversized");

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
