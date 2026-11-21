// Demo page: Sliders. One section per widget of facet/widgets/sliders.h with its important
// variants, capture states for the looks that need input (hovered, focused, dragging), and one
// selftest that operates the ready widgets by mouse and keyboard. A widget whose docs entry is not
// ready yet draws its placeholder at its default size.

#include <cmath>
#include <cstdio>
#include <span>
#include <vector>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct SlidersUi
{
    // Slider
    float level = 0.5f;
    int count = 4;
    float gain = 0.0f;       // bipolar, -12 to 12
    float frequency = 440.0f; // logarithmic, 20 to 20000
    float stepped = 0.25f;
    float small = 0.3f;
    float large = 0.7f;
    float fader = 0.6f;
    float faderB = 0.3f;
    float readOnly = 0.4f;
    float disabled = 0.6f;
    float invalid = 0.9f;
    float mixed = 0.5f;
    bool mixedSet = false;
    int levelEdits = 0;      // frames that wrote level
    int levelCommits = 0;    // finished drags and key presses of level

    // RangeSlider
    float low = 20.0f;
    float high = 80.0f;
    int lowInt = 20;
    int highInt = 80;
    float spreadLow = -0.5f;
    float spreadHigh = 0.5f;
    int spanLow = 30;
    int spanHigh = 50;
    float bandLow = 200.0f;
    float bandHigh = 2000.0f;
    int filterLow = 10;
    int filterHigh = 90;
    int roLow = 25, roHigh = 75;
    int offLow = 25, offHigh = 75;
    int errLow = 40, errHigh = 50;
    int mixLow = 25, mixHigh = 75;
    bool mixRangeSet = false;
    float vLow = 0.25f;
    float vHigh = 0.75f;

    // SliderStepped
    float zoom = 1.0f;
    float opacity = 0.5f;
    float steppedV = 0.25f;

    // SliderWithInput
    int amount = 40;
    float field = 0.75f;

    // RangeInput
    int yearLow = 1990;
    int yearHigh = 2020;
    float fieldLow = 0.25f;
    float fieldHigh = 0.75f;

    // MultiSlider
    float handles[3] = {0.25f, 0.5f, 0.75f};
    std::vector<float> marks = {20.0f, 60.0f};
    int marksAdded = 0;
    int marksRemoved = 0;
    int levels[3] = {2, 5, 8};

    // Knob
    float knobArc = 0.4f;
    int knobDot = 3;
    float knobWiper = 0.0f;
    float knobPan = 0.0f;
    float knobFixed = 0.6f;

    // RangeSliderHistogram
    float histogramLow = 0.3f;
    float histogramHigh = 0.6f;
    int emptyLow = 2;
    int emptyHigh = 8;

    // XYPad
    ImVec2 point = {0.5f, 0.5f};
    ImVec2 cell = {2.0f, 5.0f};
    ImVec2 padFixed = {0.3f, 0.6f};
    ImVec2 padOff = {0.7f, 0.4f};
    ImVec2 padLimit = {0.9f, 0.9f};
    ImVec2 padMixed = {0.5f, 0.5f};
    bool padMixedSet = false;
    int pointEdits = 0;    // frames that wrote point
    int pointCommits = 0;  // finished drags and key presses of point

    // BoxRangePad
    ImVec2 boxLow = {0.25f, 0.25f};
    ImVec2 boxHigh = {0.75f, 0.75f};
    ImVec2 cropLow = {20.0f, 10.0f};
    ImVec2 cropHigh = {70.0f, 60.0f};
    ImVec2 boxFixedLow = {0.2f, 0.3f}, boxFixedHigh = {0.6f, 0.8f};
    ImVec2 boxOffLow = {0.4f, 0.2f}, boxOffHigh = {0.8f, 0.7f};
    ImVec2 boxMixLow = {0.25f, 0.25f}, boxMixHigh = {0.75f, 0.75f};
    bool boxMixedSet = false;
    ImVec2 boxSmallLow = {0.1f, 0.1f}, boxSmallHigh = {0.9f, 0.9f};

    // The planned widgets below.
    ImVec2 stick = {0.0f, 0.0f};
    float arcLow = 45.0f;
    float arcHigh = 135.0f;
    float shares[3] = {0.5f, 0.3f, 0.2f};
};

SlidersUi ui;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kHovered,
    kFocused,
    kDragging,
    kPads,       // the two-dimensional widgets in use: a pad dragged, a box handle focused
};

const float kZoomStops[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f};
const char* const kZoomLabels[] = {"0.25x", "0.5x", "1x", "2x", "4x", "8x"};
const float kOpacityStops[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
// Placeholder distribution for the histogram: an even ramp, not measured data.
const float kBins[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f};
const char* const kSegments[] = {"Option 1", "Option 2", "Option 3"};
// Segment colors of the fixed splits: 0 takes the chart palette color of each segment.
const ImU32 kSegmentColors[] = {0, 0, 0, 0};

// Asks the next widget for the look of the current capture state (part -1: the widget, or a
// handle index for widgets with several).
void ForceCaptureLook(facet::demo::PageCtx& ctx, int part = -1)
{
    if (!ctx.capture)
        return;
    if (ctx.captureState == kHovered)
        ctx.ForceNextItemState(ForcedItemState::Hovered, part);
    else if (ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused, part);
    else if (ctx.captureState == kDragging)
        ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Dragging, part);
}

void SliderSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Slider");
    Paragraph("Drag the knob or click the track. Arrow keys step, Shift finer and Ctrl coarser, Page Up and Down "
              "move a tenth, Home and End go to the ends; Ctrl+click or Enter types a value, Escape restores it.");

    LabeledRow("Continuous");
    // Hovered and focused show on this slider, dragging on the range below.
    const bool sliderState = ctx.capture && (ctx.captureState == kHovered || ctx.captureState == kFocused);
    if (sliderState)
        ForceCaptureLook(ctx);
    if (facet::Slider("Level", &ui.level, 0.0f, 1.0f, {.format = "%.2f", .valueBubble = true}))
        ++ui.levelEdits;
    if (facet::IsItemEditCommitted())
        ++ui.levelCommits;
    ctx.Probe("slider.level");
    if (sliderState)
        ctx.RevealInCapture(0.3f);
    LabeledRow("Whole numbers");
    facet::Slider("Count", &ui.count, 0, 10, {.step = 1.0});
    ctx.Probe("slider.count");
    LabeledRow("From the center");
    facet::Slider("Gain", &ui.gain, -12.0f, 12.0f, {.format = "%+.1f dB", .fillFromCenter = true});
    ctx.Probe("slider.gain");
    LabeledRow("Logarithmic");
    facet::Slider("Frequency", &ui.frequency, 20.0f, 20000.0f, {.format = "%.0f Hz", .scale = facet::ValueScale::Logarithmic});
    ctx.Probe("slider.frequency");
    LabeledRow("Stepped");
    facet::Slider("Stepped", &ui.stepped, 0.0f, 1.0f, {.format = "%.2f", .step = 0.125});
    ctx.Probe("slider.stepped");

    LabeledRow("Sizes");
    facet::Slider("Small", &ui.small, 0.0f, 1.0f, {.format = "%.2f", .size = facet::ControlSize::Small});
    ctx.Probe("slider.small");
    LabeledRow("");
    facet::Slider("Large", &ui.large, 0.0f, 1.0f, {.format = "%.2f", .size = facet::ControlSize::Large});
    ctx.Probe("slider.large");

    LabeledRow("States");
    facet::Slider("Read-only", &ui.readOnly, 0.0f, 1.0f, {.format = "%.2f", .readOnly = true});
    ctx.Probe("slider.readOnly");
    LabeledRow("");
    ImGui::BeginDisabled();
    facet::Slider("Disabled", &ui.disabled, 0.0f, 1.0f, {.format = "%.2f"});
    ImGui::EndDisabled();
    ctx.Probe("slider.disabled");
    LabeledRow("");
    facet::Slider("Limit", &ui.invalid, 0.0f, 1.0f,
                  {.format = "%.2f", .error = ui.invalid > 0.8f ? "Keep the limit at 0.80 or below" : nullptr,
                   .helpText = "Drag below 0.80 to clear the error"});
    ctx.Probe("slider.limit");
    LabeledRow("");
    if (facet::Slider("Mixed", &ui.mixed, 0.0f, 1.0f, {.format = "%.2f", .mixed = !ui.mixedSet}))
        ui.mixedSet = true;
    ctx.Probe("slider.mixed");

    LabeledRow("Vertical");
    facet::Slider("Fader A", &ui.fader, 0.0f, 1.0f, {.format = "%.2f", .valueBubble = true,
                                                     .orientation = facet::Orientation::Vertical});
    ctx.Probe("slider.faderA");
    ImGui::SameLine();
    facet::Slider("Fader B", &ui.faderB, 0.0f, 1.0f, {.format = "%.2f", .fillFromCenter = true,
                                                      .orientation = facet::Orientation::Vertical});
    ctx.Probe("slider.faderB");

    Note("Level %.2f: written on %d frames, %d finished edits. Count %d, gain %+.1f dB, frequency %.0f Hz.", ui.level,
         ui.levelEdits, ui.levelCommits, ui.count, ui.gain, ui.frequency);
}

void ProbeHandles(facet::demo::PageCtx& ctx, const char* label, const char* probe, int count, int keyBase = 0);

void MultiSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("MultiSlider");
    Paragraph("Several handles on one track, in order and a minimum gap apart. The growable form adds a handle on a "
              "double click on the empty track and removes the focused one with Delete, or one dragged off the "
              "track.");
    LabeledRow("Fixed splits");
    facet::MultiSlider("Splits", std::span<float>(ui.handles), 0.0f, 1.0f,
                       {.format = "%.2f", .minGap = 0.05, .segmentColors = kSegmentColors});
    ctx.Probe("multi.splits");
    ProbeHandles(ctx, "Splits", "multi.splits", 3, 1);
    LabeledRow("Growable");
    facet::MultiSlider("Marks", &ui.marks, 0.0f, 100.0f,
                       {.format = "%.0f", .minGap = 2.0, .allowAdd = true, .allowRemove = true, .minHandles = 1,
                        .maxHandles = 8});
    if (facet::MultiSliderAddedHandle() >= 0)
        ++ui.marksAdded;
    if (facet::MultiSliderRemovedHandle() >= 0)
        ++ui.marksRemoved;
    ctx.Probe("multi.marks");
    LabeledRow("Whole numbers");
    facet::MultiSlider("Levels", std::span<int>(ui.levels), 0, 10, {.step = 1.0, .collision = facet::HandleCollision::Push});
    ctx.Probe("multi.levels");
    Note("Splits %.2f, %.2f, %.2f. Marks: %d handles, %d added, %d removed.", ui.handles[0], ui.handles[1],
         ui.handles[2], static_cast<int>(ui.marks.size()), ui.marksAdded, ui.marksRemoved);
}

void KnobSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Knob");
    Paragraph("Drag up to turn a knob up, or around it with circular drag. The arrows step while it is focused (Up "
              "and Right up, Down and Left down), the wheel turns it while it is focused or with Alt, a double click "
              "undoes the last click.");
    // Knobs side by side, wrapping onto the next line in a narrow window.
    const float knobWidth = facet::ControlHeight(facet::ControlSize::Large) * 2.0f;
    float right = facet::demo::ContentRight();
    const auto KnobNext = [&] { facet::demo::SameLineIfFits(right, knobWidth); };
    LabeledRow("Variants");
    right = facet::demo::ContentRight();
    facet::Knob("Arc", &ui.knobArc, 0.0f, 1.0f, {.format = "%.2f"});
    ctx.Probe("knob.arc");
    KnobNext();
    facet::Knob("Dot", &ui.knobDot, 0, 10, {.variant = facet::KnobVariant::Dot});
    ctx.Probe("knob.dot");
    KnobNext();
    facet::Knob("Wiper", &ui.knobWiper, -1.0f, 1.0f, {.format = "%+.2f", .variant = facet::KnobVariant::Wiper,
                                                     .fillFromCenter = true});
    ctx.Probe("knob.wiper");
    KnobNext();
    facet::Knob("Pan", &ui.knobPan, -1.0f, 1.0f, {.format = "%+.2f", .fillFromCenter = true, .circularDrag = true});
    ctx.Probe("knob.pan");
    LabeledRow("Sizes and states");
    right = facet::demo::ContentRight();
    facet::Knob("Small", &ui.knobArc, 0.0f, 1.0f, {.format = "%.2f", .size = facet::ControlSize::Small});
    KnobNext();
    facet::Knob("Large", &ui.knobArc, 0.0f, 1.0f, {.format = "%.2f", .size = facet::ControlSize::Large});
    KnobNext();
    facet::Knob("Read-only", &ui.knobFixed, 0.0f, 1.0f, {.format = "%.2f", .readOnly = true});
    KnobNext();
    ImGui::BeginDisabled();
    facet::Knob("Disabled", &ui.knobFixed, 0.0f, 1.0f, {.format = "%.2f"});
    ImGui::EndDisabled();
    Note("Arc %.2f, dot %d, wiper %+.2f, pan %+.2f.", ui.knobArc, ui.knobDot, ui.knobWiper, ui.knobPan);
}

void SteppedSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("SliderStepped");
    Paragraph("Stops of any spacing, evenly placed on the track. The arrows go to the next stop; magnetic moves "
              "freely and snaps only near a stop.");
    LabeledRow("On stops");
    facet::SliderStepped("Zoom", &ui.zoom, kZoomStops, {.format = "%.2gx", .stopLabels = kZoomLabels});
    ctx.Probe("stepped.zoom");
    LabeledRow("Magnetic");
    facet::SliderStepped("Opacity", &ui.opacity, kOpacityStops, {.format = "%.2f", .magnetic = true});
    ctx.Probe("stepped.opacity");
    LabeledRow("Vertical");
    facet::SliderStepped("Steps", &ui.steppedV, kOpacityStops, {.format = "%.2f", .orientation = facet::Orientation::Vertical,
                                                                .height = 96.0f});
    ctx.Probe("stepped.vertical");
    Note("Zoom %.2gx, opacity %.2f.", ui.zoom, ui.opacity);
}

// Probes for the handles of the widget submitted last under `label`: each handle's id with the
// widget's rectangle, so a selftest can check which handle has the keyboard focus. Handles are
// numbered from `keyBase` (a multi slider keys its handles from 1).
void ProbeHandles(facet::demo::PageCtx& ctx, const char* label, const char* probe, int count, int keyBase)
{
    const ImGuiID widget = ImGui::GetID(label);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    char name[64];
    for (int i = 0; i < count; ++i)
    {
        ImGui::PushOverrideID(widget);
        const ImGuiID id = ImGui::GetID(i + keyBase);
        ImGui::PopID();
        std::snprintf(name, sizeof(name), "%s.h%d", probe, i);
        facet::demo::ProbeRect(ctx, name, id, min, max);
    }
}

void RangeSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("RangeSlider");
    Paragraph("Two handles, each its own Tab stop. Handles never cross and keep the minimum gap; dragging the filled "
              "span moves both, a click on the empty track moves the nearer handle. Ctrl+click or Enter on a handle "
              "types its value.");

    LabeledRow("Minimum gap");
    const bool rangeState = ctx.capture && ctx.captureState == kDragging;
    if (rangeState)
        ForceCaptureLook(ctx, 1);
    facet::RangeSlider("Price", &ui.low, &ui.high, 0.0f, 100.0f,
                       {.format = "%.0f", .minGap = 10.0, .valueBubble = true});
    ctx.Probe("range.price");
    ProbeHandles(ctx, "Price", "range.price", 2);
    if (rangeState)
        ctx.RevealInCapture(0.4f);
    LabeledRow("Push, steps of 5");
    facet::RangeSlider("Window", &ui.lowInt, &ui.highInt, 0, 100,
                       {.step = 5.0, .minGap = 10.0, .collision = facet::HandleCollision::Push});
    ctx.Probe("range.window");
    ProbeHandles(ctx, "Window", "range.window", 2);
    LabeledRow("Mirror");
    facet::RangeSlider("Spread", &ui.spreadLow, &ui.spreadHigh, -1.0f, 1.0f, {.format = "%+.2f", .mirror = true});
    ctx.Probe("range.spread");
    ProbeHandles(ctx, "Spread", "range.spread", 2);
    LabeledRow("Fixed span");
    facet::RangeSlider("Span", &ui.spanLow, &ui.spanHigh, 0, 100, {.fixedSpan = true});
    ctx.Probe("range.span");
    ProbeHandles(ctx, "Span", "range.span", 2);
    LabeledRow("Logarithmic");
    facet::RangeSlider("Band", &ui.bandLow, &ui.bandHigh, 20.0f, 20000.0f,
                       {.format = "%.0f", .scale = facet::ValueScale::Logarithmic, .minGap = 10.0});
    ctx.Probe("range.band");
    LabeledRow("Reset and typing");
    facet::RangeSlider("Filter", &ui.filterLow, &ui.filterHigh, 0, 100,
                       {.allowOutOfRange = true, .doubleClickReset = true,
                        .helpText = "Double-click a handle to clear its bound; typed values may leave the range"});
    ctx.Probe("range.filter");
    ProbeHandles(ctx, "Filter", "range.filter", 2);

    LabeledRow("States");
    facet::RangeSlider("Read-only", &ui.roLow, &ui.roHigh, 0, 100, {.readOnly = true});
    ctx.Probe("range.readOnly");
    LabeledRow("");
    ImGui::BeginDisabled();
    facet::RangeSlider("Disabled", &ui.offLow, &ui.offHigh, 0, 100);
    ImGui::EndDisabled();
    ctx.Probe("range.disabled");
    LabeledRow("");
    facet::RangeSlider("Limits", &ui.errLow, &ui.errHigh, 0, 100,
                       {.error = ui.errHigh - ui.errLow < 20 ? "Leave at least 20 between the limits" : nullptr});
    ctx.Probe("range.limits");
    LabeledRow("");
    if (facet::RangeSlider("Mixed", &ui.mixLow, &ui.mixHigh, 0, 100, {.mixed = !ui.mixRangeSet}))
        ui.mixRangeSet = true;
    ctx.Probe("range.mixed");

    LabeledRow("Vertical");
    facet::RangeSlider("Vertical range", &ui.vLow, &ui.vHigh, 0.0f, 1.0f,
                       {.format = "%.2f", .minGap = 0.1, .valueBubble = true, .orientation = facet::Orientation::Vertical});
    ctx.Probe("range.vertical");
    ProbeHandles(ctx, "Vertical range", "range.vertical", 2);

    Note("Price %.0f to %.0f, window %d to %d, spread %+.2f to %+.2f, filter %d to %d.", ui.low, ui.high, ui.lowInt,
         ui.highInt, ui.spreadLow, ui.spreadHigh, ui.filterLow, ui.filterHigh);
}

// A row of small pads that wraps in a narrow window.
constexpr float kSmallPad = 112.0f;

void XYPadSection(facet::demo::PageCtx& ctx)
{
    // The state labels repeat those of the sliders above: an id scope of its own keeps them apart.
    ImGui::PushID("xy");
    SectionHeader("XYPad");
    Paragraph("A point on two axes. Drag it, or click to move it there; Shift keeps the axis the drag goes along "
              "and Ctrl snaps to tenths. The arrows step x and y, Page Up and Down move y by a tenth, Home and End "
              "send x to its ends, Escape restores the point.");
    LabeledRow("Continuous");
    const bool padState = ctx.capture && ctx.captureState == kPads;
    // Dragging without the hover: a hovered pad would show its label tooltip, which a capture
    // without a pointer cannot place.
    if (padState)
        ctx.ForceNextItemState(ForcedItemState::Dragging);
    if (facet::XYPad("Position", &ui.point, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.format = "%.2f", .grid = true}))
        ++ui.pointEdits;
    if (facet::IsItemEditCommitted())
        ++ui.pointCommits;
    ctx.Probe("xy.position");
    LabeledRow("Steps, y down");
    facet::XYPad("Cell", &ui.cell, ImVec2(0.0f, 0.0f), ImVec2(8.0f, 8.0f),
                 {.format = "%.0f", .step = {1.0f, 1.0f}, .grid = true, .yDown = true, .width = facet::demo::DemoWidth(160.0f)});
    ctx.Probe("xy.cell");

    LabeledRow("States");
    float right = facet::demo::ContentRight();
    const float small = facet::Px(kSmallPad);
    facet::XYPad("Read-only", &ui.padFixed, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                 {.format = "%.1f", .width = facet::demo::DemoWidth(kSmallPad), .readOnly = true});
    ctx.Probe("xy.readOnly");
    facet::demo::SameLineIfFits(right, small);
    ImGui::BeginDisabled();
    facet::XYPad("Disabled", &ui.padOff, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                 {.format = "%.1f", .width = facet::demo::DemoWidth(kSmallPad)});
    ImGui::EndDisabled();
    ctx.Probe("xy.disabled");
    facet::demo::SameLineIfFits(right, small);
    if (facet::XYPad("Mixed", &ui.padMixed, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                     {.format = "%.1f", .width = facet::demo::DemoWidth(kSmallPad), .mixed = !ui.padMixedSet}))
        ui.padMixedSet = true;
    ctx.Probe("xy.mixed");
    LabeledRow("");
    facet::XYPad("Limit", &ui.padLimit, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                 {.format = "%.1f", .crosshair = false, .width = facet::demo::DemoWidth(kSmallPad),
                  .error = ui.padLimit.x + ui.padLimit.y > 1.5f ? "Keep x + y at 1.5 or below" : nullptr,
                  .helpText = "Move the point toward the lower left"});
    ctx.Probe("xy.limit");
    Note("Position %.2f, %.2f: written on %d frames, %d finished edits. Cell %.0f, %.0f.", ui.point.x, ui.point.y,
         ui.pointEdits, ui.pointCommits, ui.cell.x, ui.cell.y);
    ImGui::PopID();
}

void BoxRangeSection(facet::demo::PageCtx& ctx)
{
    ImGui::PushID("box");
    SectionHeader("BoxRangePad");
    Paragraph("A range on two axes: the box between a low and a high corner. The edges move one axis and the corners "
              "both, each its own Tab stop moved by the arrows; drag the inside to move the whole box. Each axis keeps "
              "its minimum gap, and Escape restores the box.");
    LabeledRow("Minimum gap");
    if (ctx.capture && ctx.captureState == kPads)
        ctx.ForceNextItemState(ForcedItemState::Focused, 5);
    facet::BoxRangePad("Area", &ui.boxLow, &ui.boxHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.minGap = {0.1f, 0.1f}});
    ctx.Probe("box.area");
    ProbeHandles(ctx, "Area", "box.area", 8);
    if (ctx.capture && ctx.captureState == kPads)
        ctx.RevealInCapture(0.4f);
    LabeledRow("Push, steps, y down");
    facet::BoxRangePad("Crop", &ui.cropLow, &ui.cropHigh, ImVec2(0.0f, 0.0f), ImVec2(100.0f, 100.0f),
                       {.step = {5.0f, 5.0f}, .minGap = {10.0f, 10.0f}, .collision = facet::HandleCollision::Push,
                        .yDown = true, .width = facet::demo::DemoWidth(160.0f)});
    ctx.Probe("box.crop");
    ProbeHandles(ctx, "Crop", "box.crop", 8);

    LabeledRow("States");
    float right = facet::demo::ContentRight();
    const float small = facet::Px(kSmallPad);
    facet::BoxRangePad("Read-only", &ui.boxFixedLow, &ui.boxFixedHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                       {.width = facet::demo::DemoWidth(kSmallPad), .readOnly = true});
    ctx.Probe("box.readOnly");
    facet::demo::SameLineIfFits(right, small);
    ImGui::BeginDisabled();
    facet::BoxRangePad("Disabled", &ui.boxOffLow, &ui.boxOffHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                       {.width = facet::demo::DemoWidth(kSmallPad)});
    ImGui::EndDisabled();
    ctx.Probe("box.disabled");
    facet::demo::SameLineIfFits(right, small);
    if (facet::BoxRangePad("Mixed", &ui.boxMixLow, &ui.boxMixHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                           {.width = facet::demo::DemoWidth(kSmallPad), .mixed = !ui.boxMixedSet}))
        ui.boxMixedSet = true;
    ctx.Probe("box.mixed");
    LabeledRow("");
    const float boxArea = (ui.boxSmallHigh.x - ui.boxSmallLow.x) * (ui.boxSmallHigh.y - ui.boxSmallLow.y);
    facet::BoxRangePad("Limit", &ui.boxSmallLow, &ui.boxSmallHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                       {.minGap = {0.05f, 0.05f}, .width = facet::demo::DemoWidth(kSmallPad),
                        .error = boxArea > 0.5f ? "Keep the box at half the pad or less" : nullptr,
                        .helpText = "Drag a corner inward"});
    ctx.Probe("box.limit");
    ProbeHandles(ctx, "Limit", "box.limit", 8);
    Note("Area x %.2f to %.2f, y %.2f to %.2f. Crop x %.0f to %.0f, y %.0f to %.0f.", ui.boxLow.x, ui.boxHigh.x, ui.boxLow.y,
         ui.boxHigh.y, ui.cropLow.x, ui.cropHigh.x, ui.cropLow.y, ui.cropHigh.y);
    ImGui::PopID();
}

} // namespace

FACET_DEMO_PAGE("sliders", "Sliders", "Widgets", 30, "default", "hovered", "focused", "dragging", "pads")
{
    facet::demo::PageHeader("Sliders",
                            "Sliders, range sliders, multi-handle sliders, knobs and two-dimensional pads.");
    facet::demo::PlannedFamilyNote("sliders");

    SliderSection(ctx);

    SteppedSection(ctx);

    SectionHeader("SliderWithInput");
    Paragraph("A slider and a number field on one value. The field writes on Enter, Tab or focus loss; Up and Down "
              "in the field step like the arrows on the slider.");
    LabeledRow("Whole numbers");
    facet::SliderWithInput("Amount", &ui.amount, 0, 100);
    ctx.Probe("withInput.amount");
    LabeledRow("Out of range");
    facet::SliderWithInput("Scale", &ui.field, 0.0f, 2.0f,
                           {.format = "%.2f", .allowOutOfRange = true,
                            .helpText = "The field takes values past 2; the slider then shows its end"});
    ctx.Probe("withInput.scale");
    Note("Amount %d, scale %.2f.", ui.amount, ui.field);

    RangeSection(ctx);

    SectionHeader("RangeInput");
    Paragraph("The range track between two number fields. A typed value keeps the order and the gap like a dragged "
              "handle; Tab goes through the low field, both handles and the high field.");
    LabeledRow("With the track");
    facet::RangeInput("Years", &ui.yearLow, &ui.yearHigh, 1900, 2100,
                      {.minGap = 1.0, .collision = facet::HandleCollision::Push});
    ctx.Probe("rangeInput.years");
    LabeledRow("Fields only");
    facet::RangeInput("Limits", &ui.fieldLow, &ui.fieldHigh, 0.0f, 1.0f, {.format = "%.2f", .minGap = 0.05, .slider = false});
    ctx.Probe("rangeInput.fields");
    Note("Years %d to %d, limits %.2f to %.2f.", ui.yearLow, ui.yearHigh, ui.fieldLow, ui.fieldHigh);

    MultiSection(ctx);
    KnobSection(ctx);

    SectionHeader("RangeSliderHistogram");
    Paragraph("A range over a bar chart of the caller's counts; bars outside the range are dimmed, and hovering a bar "
              "shows its interval and count.");
    LabeledRow("Over bars");
    facet::RangeSliderHistogram("Spread", &ui.histogramLow, &ui.histogramHigh, 0.0f, 1.0f, kBins,
                                {.format = "%.2f", .minGap = 0.05});
    ctx.Probe("histogram.spread");
    LabeledRow("No data");
    facet::RangeSliderHistogram("Empty", &ui.emptyLow, &ui.emptyHigh, 0, 10, {}, {.helpText = "No bins: the track alone"});
    ctx.Probe("histogram.empty");

    XYPadSection(ctx);

    BoxRangeSection(ctx);

    SectionHeader("AllocationBar");
    LabeledRow("Shares");
    facet::AllocationBar("##shares", std::span<float>(ui.shares), kSegments, {.minGap = 0.05});

    SectionHeader("ArcRangeSlider");
    LabeledRow("Arc range");
    facet::ArcRangeSlider("##arc", &ui.arcLow, &ui.arcHigh, 0.0f, 360.0f, {.wrap = true, .sweep = 360.0f});

    SectionHeader("JoystickPad");
    LabeledRow("Joystick");
    facet::JoystickPad("##stick", &ui.stick, {.deadzone = 0.1f, .showDeadzone = true});
}

FACET_DEMO_SELFTEST("sliders")
{
    using T = facet::demo::Target;
    // A point on the control row of a horizontal slider: half a regular control height below the
    // top of the item, so a message line under the control does not move it. Fractions from 0.55
    // on lie on the track in every layout: the label takes at most half the width.
    const auto Row = [](const char* probe, float fx) { return T::Item(probe, fx, 0.0f).Offset(0.0f, 14.0f); };
    test.Do([] { ui = {}; });

    // Slider, pointer: a click on the track moves the value there, a drag follows the pointer, a
    // drag out of the window and back to the press point ends on the start value.
    test.Drag({Row("slider.count", 0.6f), Row("slider.count", 0.99f)}, 0.3f);
    test.ExpectNear("a drag past the end of the track reaches 10", [] { return ui.count; }, 10.0, 0.0);
    test.MouseMove(Row("slider.count", 0.6f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click on the track moves the count under the pointer", [] { return ui.count > 0 && ui.count < 10; });
    static float pressed = 0.0f;
    test.MouseMove(Row("slider.level", 0.6f));
    test.MouseDown();
    test.Do([] { pressed = ui.level; });
    test.MouseMove(T::Display(1.5f, 0.5f));
    test.Expect("a drag out of the window to the right reaches the maximum", [] { return ui.level == 1.0f; });
    test.MouseMove(T::Display(-0.5f, 0.5f));
    test.MouseMove(Row("slider.level", 0.6f));
    test.MouseUp();
    test.Expect("a drag out of the window and back ends on the value under the pointer",
                [] { return ui.level == pressed; });
    test.Expect("the drag was one finished edit", [] { return ui.levelCommits == 1; });
    test.MouseMove(Row("slider.frequency", 0.9f));
    test.MouseDown();
    test.Do([] { pressed = ui.frequency; });
    test.MouseMove(Row("slider.frequency", 0.6f));
    test.MouseUp();
    test.Expect("the logarithmic slider follows the pointer down", [] { return ui.frequency < pressed; });

    // Slider, keyboard: Tab reaches it, the arrows step, End and Home reach the ends.
    test.Click("slider.gain");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("slider.gain");
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End moves the gain to its maximum", [] { return ui.gain; }, 12.0, 0.0);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("Left steps one hundredth of the range down", [] { return ui.gain; }, 11.76, 0.001);
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home moves the gain to its minimum", [] { return ui.gain; }, -12.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("slider.frequency");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("slider.stepped");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("the stepped slider moves one step", [] { return ui.stepped; }, 0.375, 0.0);

    // Typed value: Enter on the focused knob, then the number and Enter.
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.Type("0.5");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.ExpectNear("a typed value is written on Enter", [] { return ui.stepped; }, 0.5, 0.0);
    test.ExpectFocused("slider.stepped");

    // States: read-only and disabled write nothing, the error clears when the value is valid.
    test.Drag({T::Item("slider.readOnly", 0.5f), T::Item("slider.readOnly", 0.95f)}, 0.2f);
    test.ExpectNear("the read-only slider keeps its value", [] { return ui.readOnly; }, 0.4, 1e-6);
    test.Drag({T::Item("slider.disabled", 0.5f), T::Item("slider.disabled", 0.95f)}, 0.2f);
    test.ExpectNear("the disabled slider keeps its value", [] { return ui.disabled; }, 0.6, 1e-6);
    test.MouseMove(Row("slider.limit", 0.7f));
    test.MouseDown();
    test.MouseUp();
    test.KeyPress(ImGuiKey_Home);
    test.Expect("Home clears the error of the limit", [] { return ui.invalid <= 0.8f; });
    test.MouseMove(Row("slider.mixed", 0.8f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("the first edit of a mixed slider sets its value", [] { return ui.mixedSet; });

    // Vertical: Up moves the fader up.
    test.Click("slider.faderA");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("slider.faderA");
    test.KeyPress(ImGuiKey_Home);
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up raises the vertical fader one step", [] { return ui.fader; }, 0.01, 0.0001);
    test.Drag({T::Item("slider.faderB", 0.5f, 0.9f), T::Item("slider.faderB", 0.5f, 0.1f)}, 0.3f);
    test.Expect("a drag up raises the second fader", [] { return ui.faderB > 0.8f; });

    // RangeSlider, keyboard, one Tab chain through every range on the page: each handle is a Tab
    // stop, the arrows move the focused handle, the gap holds, Push, mirror and fixed span move the
    // other handle, Enter types a value.
    test.Click("slider.faderB");
    test.KeyPress(ImGuiKey_Tab);
    // SliderStepped on the way: the arrows go from stop to stop, magnetic too.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("stepped.zoom");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right moves the zoom to the next stop", [] { return ui.zoom; }, 2.0, 0.0);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End moves the zoom to the last stop", [] { return ui.zoom; }, 8.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("stepped.opacity");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right moves the magnetic slider to the next stop", [] { return ui.opacity; }, 0.75, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("stepped.vertical");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up moves the vertical stepped slider to the next stop", [] { return ui.steppedV; }, 0.5, 0.0);
    // SliderWithInput: the slider, then its field; the field writes on Enter.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("withInput.amount");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right on the slider part steps the amount", [] { return ui.amount; }, 41.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.Type("64");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("the field writes the typed amount on Enter", [] { return ui.amount; }, 64.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.Type("3.5");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectNear("the field writes a value past the range on Tab", [] { return ui.field; }, 3.5, 1e-6);
    test.ExpectFocused("range.price.h0");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right moves the low handle one hundredth", [] { return ui.low; }, 21.0, 1e-4);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End stops the low handle one gap before the high one", [] { return ui.low; }, 70.0, 1e-4);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("range.price.h1");
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home stops the high handle one gap after the low one", [] { return ui.high; }, 80.0, 1e-4);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End moves the high handle to the maximum", [] { return ui.high; }, 100.0, 1e-4);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("range.window.h0");
    test.KeyPress(ImGuiKey_End);
    test.Expect("End on the low handle pushes the high one to 100 and keeps the gap",
                [] { return ui.lowInt == 90 && ui.highInt == 100; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("range.spread.h0");
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("mirror moves the high handle the other way", [] { return ui.spreadLow > -0.5f && ui.spreadHigh < 0.5f; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("range.span.h0");
    test.KeyPress(ImGuiKey_End);
    test.Expect("fixed span moves both handles and stops at the end", [] { return ui.spanLow == 80 && ui.spanHigh == 100; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("range.filter.h0");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.Type("-5");
    test.KeyPress(ImGuiKey_Enter);
    test.Wait(2);
    test.ExpectNear("a typed value outside the range is written with allowOutOfRange", [] { return ui.filterLow; }, -5.0, 0.0);
    test.ExpectFocused("range.filter.h0");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("range.filter.h1");
    // The read-only range keeps its two Tab stops, the disabled one has none.
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("range.vertical.h0");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up raises the low handle of the vertical range", [] { return ui.vLow; }, 0.26, 1e-6);

    // RangeInput: low field, low handle, high handle, high field; typed values keep the order.
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.Type("2050");
    test.KeyPress(ImGuiKey_Tab);
    test.Expect("a typed low year past the high one pushes it along",
                [] { return ui.yearLow == 2050 && ui.yearHigh == 2051; });
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("the low handle follows the arrows", [] { return ui.yearLow; }, 2049.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.Type("1950");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("a typed high year below the low one pushes it down",
                [] { return ui.yearHigh == 1950 && ui.yearLow == 1949; });    test.KeyPress(ImGuiKey_Tab);
    test.Type("0.9");
    test.KeyPress(ImGuiKey_Tab);
    test.Expect("the fields-only form stops the low value one gap before the high one",
                [] { return ui.fieldLow > 0.69f && ui.fieldLow < 0.71f; });
    // MultiSlider: three handle stops that keep the gap; Delete removes the focused growable handle.
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("multi.splits.h0");
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End stops the first split one gap before the second", [] { return ui.handles[0]; }, 0.45, 1e-6);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("multi.splits.h2");
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home stops the last split one gap after the second", [] { return ui.handles[2]; }, 0.55, 1e-6);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Delete);
    test.Expect("Delete removes the focused mark", [] { return ui.marks.size() == 1 && ui.marksRemoved == 1; });
    test.KeyPress(ImGuiKey_Delete);
    test.Expect("the last mark stays: minHandles is 1", [] { return ui.marks.size() == 1; });
    // Knob: a click grabs it without moving it, the first Tab after it shows the focus there.
    test.Click("knob.arc");
    test.ExpectNear("a click on the knob does not move it", [] { return ui.knobArc; }, 0.4, 1e-6);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("knob.arc");
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End turns the knob to its maximum", [] { return ui.knobArc; }, 1.0, 0.0);
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectNear("Down turns it one hundredth down", [] { return ui.knobArc; }, 0.99, 1e-6);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("knob.dot");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up turns the whole-number knob one step up", [] { return ui.knobDot; }, 4.0, 0.0);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right turns it one more step up", [] { return ui.knobDot; }, 5.0, 0.0);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.ExpectNear("Left turns it one step down", [] { return ui.knobDot; }, 4.0, 0.0);
    test.ExpectFocused("knob.dot");
    test.MouseMove(Row("multi.marks", 0.92f));
    test.MouseDown();
    test.MouseUp();
    test.MouseDown();
    test.MouseUp();
    test.Expect("a double click on the empty track adds a mark", [] { return ui.marks.size() == 2 && ui.marksAdded >= 1; });
    test.Do([] { ui.knobArc = 0.5f; });
    test.Drag({T::Item("knob.arc", 0.5f, 0.3f), T::Item("knob.arc", 0.5f, 0.3f).Offset(0.0f, 50.0f)}, 0.3f);
    test.Expect("a downward drag turns the knob down", [] { return ui.knobArc < 0.5f; });
    test.Drag({T::Item("knob.pan", 0.5f, 0.3f), T::Item("knob.pan", 0.5f, 0.3f).Offset(-60.0f, 0.0f)}, 0.3f);
    test.Expect("a drag to the left of the circular knob turns it left", [] { return ui.knobPan < -0.3f; });    // RangeSliderHistogram: a click on the track row left of the low handle moves it there and
    // focuses it; End then stops it one gap before the high handle.
    test.Do([] { ui.histogramLow = 0.7f; ui.histogramHigh = 0.9f; });
    test.MouseMove(T::Item("histogram.spread", 0.55f, 1.0f).Offset(0.0f, -14.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click left of the low handle moves it down", [] { return ui.histogramLow < 0.7f; });
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End stops the low handle one gap before the high one", [] { return ui.histogramLow; }, 0.85, 1e-6);    test.Drag({Row("multi.levels", 0.97f), Row("multi.levels", 0.6f)}, 0.3f);
    test.Expect("a drag pushes the whole-number levels down", [] { return ui.levels[2] < 8; });

    test.Do([] { ui.yearLow = 1990; ui.yearHigh = 2020; });
    test.Drag({Row("rangeInput.years", 0.55f), Row("rangeInput.years", 0.5f)}, 0.2f);
    test.Expect("a drag on the track of the range input moves the years",
                [] { return ui.yearLow != 1990 || ui.yearHigh != 2020; });

    // Pointer: the span drags both handles and keeps their distance; a click between the handles
    // grabs the span without moving it; a click on the empty track moves the nearer handle.
    // The values put 0.55 of the row between the handles, or just before the low one, in the wide
    // and the narrow layout alike.
    test.Do([] { ui.lowInt = 10; ui.highInt = 90; });
    test.Drag({Row("range.window", 0.55f), Row("range.window", 0.48f)}, 0.3f);
    test.Expect("dragging the span moves both handles by the same amount",
                [] { return ui.highInt - ui.lowInt == 80 && ui.lowInt < 10; });
    test.Do([] { ui.lowInt = 10; ui.highInt = 90; });
    test.MouseMove(Row("range.window", 0.55f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click between the handles grabs the span without moving it",
                [] { return ui.lowInt == 10 && ui.highInt == 90; });
    test.Do([] { ui.lowInt = 60; ui.highInt = 90; });
    test.MouseMove(Row("range.window", 0.55f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click before the low handle moves it there", [] { return ui.lowInt < 60 && ui.highInt == 90; });
    test.Do([] { ui.low = 20.0f; ui.high = 80.0f; });
    test.Drag({T::Item("range.price", 0.5f), T::Display(1.5f, 0.5f), T::Item("range.price", 0.5f)}, 0.4f);
    test.Expect("a span drag out of the window and back ends where it started",
                [] { return ui.low == 20.0f && ui.high == 80.0f; });

    // SliderWithInput by pointer: the slider part follows a drag.
    test.Do([] { ui.amount = 0; });
    test.Drag({Row("withInput.amount", 0.55f), Row("withInput.amount", 0.6f)}, 0.2f);
    test.Expect("a drag on the slider part writes the amount", [] { return ui.amount > 0; });

    // SliderStepped by pointer: a drag always ends on a stop.
    test.Drag({Row("stepped.zoom", 0.9f), Row("stepped.zoom", 0.63f)}, 0.3f);
    test.Expect("a drag on the stepped slider ends on a stop", [] {
        for (const float stop : kZoomStops)
            if (ui.zoom == stop)
                return ui.zoom < 8.0f;
        return false;
    });

    // States.
    test.Drag({T::Item("range.readOnly", 0.5f), T::Item("range.readOnly", 0.9f)}, 0.2f);
    test.Expect("the read-only range keeps its values", [] { return ui.roLow == 25 && ui.roHigh == 75; });
    test.Drag({T::Item("range.disabled", 0.5f), T::Item("range.disabled", 0.9f)}, 0.2f);
    test.Expect("the disabled range keeps its values", [] { return ui.offLow == 25 && ui.offHigh == 75; });
    test.MouseMove(Row("range.mixed", 0.9f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("the first edit of a mixed range sets it", [] { return ui.mixRangeSet; });

    // XYPad: a drag moves both axes, a click moves the point there, the arrows step both axes,
    // one Tab stop per pad.
    test.Do([] { ui.point = {0.5f, 0.5f}; ui.pointCommits = 0; });
    test.Drag({T::Item("xy.position", 0.5f, 0.5f), T::Item("xy.position", 0.8f, 0.2f)}, 0.3f);
    test.Expect("a drag up and to the right moves the point on both axes",
                [] { return ui.point.x > 0.65f && ui.point.y > 0.65f; });
    test.Expect("the drag was one finished edit", [] { return ui.pointCommits == 1; });
    test.MouseMove(T::Item("xy.position", 0.2f, 0.8f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click moves the point under the pointer", [] { return ui.point.x < 0.35f && ui.point.y < 0.35f; });
    test.ExpectFocused("xy.position");
    test.Do([] { ui.point = {0.5f, 0.5f}; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right steps x one hundredth", [] { return ui.point.x; }, 0.51, 1e-6);
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up steps y one hundredth", [] { return ui.point.y; }, 0.51, 1e-6);
    test.KeyPress(ImGuiKey_PageDown);
    test.ExpectNear("Page Down moves y a tenth down", [] { return ui.point.y; }, 0.5, 1e-6);
    test.KeyPress(ImGuiKey_End);
    test.ExpectNear("End sends x to its maximum", [] { return ui.point.x; }, 1.0, 0.0);
    test.ExpectFocused("xy.position");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("xy.cell");
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectNear("Down on a pad with y down moves to the next cell row", [] { return ui.cell.y; }, 6.0, 0.0);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("xy.readOnly");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("xy.mixed");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Expect("the first edit of a mixed pad sets it", [] { return ui.padMixedSet; });
    test.Drag({T::Item("xy.readOnly", 0.5f, 0.5f), T::Item("xy.readOnly", 0.9f, 0.1f)}, 0.2f);
    test.ExpectNear("the read-only pad keeps its value", [] { return ui.padFixed.x; }, 0.3, 1e-6);
    test.Drag({T::Item("xy.disabled", 0.5f, 0.5f), T::Item("xy.disabled", 0.1f, 0.9f)}, 0.2f);
    test.ExpectNear("the disabled pad keeps its value", [] { return ui.padOff.x; }, 0.7, 1e-6);
    test.MouseMove(T::Item("xy.limit", 0.15f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click toward the lower left clears the error", [] { return ui.padLimit.x + ui.padLimit.y <= 1.5f; });

    // BoxRangePad: a corner moves both axes and stops one gap short, the inside moves the whole
    // box, each handle is a Tab stop moved by the arrows, Push moves the other side along.
    // A value v of a pad W wide lies at 9 + v * (W - 18) units: the knob is 18 units and its
    // centre travels inside the frame inset by half a knob. That is the fraction v of the width
    // plus (1 - 2v) * 9 units, whatever W is.
    const auto BoxAt = [](const char* probe, float vx, float vy) {
        return T::Item(probe, vx, 1.0f - vy).Offset((1.0f - 2.0f * vx) * 9.0f, (2.0f * vy - 1.0f) * 9.0f);
    };
    // The pad in the middle of the window first, so nothing scrolls between a probe and a press.
    test.Reveal("box.area");
    test.WaitSeconds(0.4f);
    test.Do([] { ui.boxLow = {0.25f, 0.25f}; ui.boxHigh = {0.75f, 0.75f}; });
    test.Drag({BoxAt("box.area", 0.75f, 0.75f), BoxAt("box.area", 0.05f, 0.05f)}, 0.3f);
    test.ExpectNear("the top right corner stops one gap right of the low x", [] { return ui.boxHigh.x; }, 0.35, 1e-4);
    test.ExpectNear("and one gap above the low y", [] { return ui.boxHigh.y; }, 0.35, 1e-4);
    test.Do([] { ui.boxLow = {0.25f, 0.25f}; ui.boxHigh = {0.75f, 0.75f}; });
    test.Drag({BoxAt("box.area", 0.5f, 0.5f), BoxAt("box.area", 0.6f, 0.4f)}, 0.3f);
    test.Expect("a drag inside the box moves it whole and keeps its size", [] {
        return ui.boxLow.x > 0.3f && ui.boxLow.y < 0.2f && std::fabs(ui.boxHigh.x - ui.boxLow.x - 0.5f) < 1e-4f &&
               std::fabs(ui.boxHigh.y - ui.boxLow.y - 0.5f) < 1e-4f;
    });
    test.Do([] { ui.boxLow = {0.25f, 0.25f}; ui.boxHigh = {0.75f, 0.75f}; });
    test.MouseMove(BoxAt("box.area", 0.25f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectFocused("box.area.h0");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right moves the left edge", [] { return ui.boxLow.x; }, 0.26, 1e-6);
    test.ExpectFocused("box.area.h0");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("box.area.h1");
    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectNear("Down moves the top edge down", [] { return ui.boxHigh.y; }, 0.74, 1e-6);
    for (int i = 0; i < 4; ++i)
        test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("box.area.h5");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.KeyPress(ImGuiKey_UpArrow);
    test.Expect("the top right corner moves on both axes",
                [] { return std::fabs(ui.boxHigh.x - 0.74f) < 1e-6f && std::fabs(ui.boxHigh.y - 0.75f) < 1e-6f; });
    for (int i = 0; i < 3; ++i)
        test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("box.crop.h0");
    test.KeyPress(ImGuiKey_End);
    test.Expect("End on the left edge pushes the right one to the end, a gap apart",
                [] { return ui.cropLow.x == 90.0f && ui.cropHigh.x == 100.0f; });
    test.Drag({T::Item("box.readOnly", 0.5f, 0.5f), T::Item("box.readOnly", 0.9f, 0.1f)}, 0.2f);
    test.Expect("the read-only box keeps its values", [] { return ui.boxFixedLow.x == 0.2f && ui.boxFixedHigh.x == 0.6f; });
    test.Drag({T::Item("box.disabled", 0.5f, 0.5f), T::Item("box.disabled", 0.1f, 0.9f)}, 0.2f);
    test.Expect("the disabled box keeps its values", [] { return ui.boxOffLow.x == 0.4f && ui.boxOffHigh.x == 0.8f; });
    test.Drag({T::Item("box.mixed", 0.5f, 0.5f), T::Item("box.mixed", 0.6f, 0.5f)}, 0.2f);
    test.Expect("the first edit of a mixed box sets it", [] { return ui.boxMixedSet; });
    // The limit pad has a message line under it, so its y comes from the top: the corner at 0.9 of
    // the 112 unit pad is 9 + 0.1 * 94 units down.
    test.Drag({T::Item("box.limit", 0.9f, 0.0f).Offset(-7.2f, 18.4f), T::Item("box.limit", 0.5f, 0.0f).Offset(0.0f, 56.0f)},
              0.3f);
    test.Expect("dragging the corner inward clears the error", [] {
        return (ui.boxSmallHigh.x - ui.boxSmallLow.x) * (ui.boxSmallHigh.y - ui.boxSmallLow.y) <= 0.5f;
    });
}
