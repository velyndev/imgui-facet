// Demo page: Sliders. One section per widget of facet/widgets/sliders.h with its important
// variants, capture states for the looks that need input (hovered, focused, dragging), and one
// selftest that operates the ready widgets by mouse and keyboard. A widget whose docs entry is not
// ready yet draws its placeholder at its default size.

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

    // The planned widgets below.
    float histogramLow = 0.3f;
    float histogramHigh = 0.6f;
    float handles[3] = {0.2f, 0.5f, 0.8f};
    float knob = 0.4f;
    ImVec2 point = {0.5f, 0.5f};
    ImVec2 stick = {0.0f, 0.0f};
    float arcLow = 45.0f;
    float arcHigh = 135.0f;
    ImVec2 boxLow = {0.25f, 0.25f};
    ImVec2 boxHigh = {0.75f, 0.75f};
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
};

const float kZoomStops[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f};
const char* const kZoomLabels[] = {"0.25x", "0.5x", "1x", "2x", "4x", "8x"};
const float kOpacityStops[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
// Placeholder distribution for the histogram: an even ramp, not measured data.
const float kBins[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f};
const char* const kSegments[] = {"Option 1", "Option 2", "Option 3"};

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
    ForceCaptureLook(ctx);
    if (facet::Slider("Level", &ui.level, 0.0f, 1.0f, {.format = "%.2f", .valueBubble = true}))
        ++ui.levelEdits;
    if (facet::IsItemEditCommitted())
        ++ui.levelCommits;
    ctx.Probe("slider.level");
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
// widget's rectangle, so a selftest can check which handle has the keyboard focus.
void ProbeHandles(facet::demo::PageCtx& ctx, const char* label, const char* probe, int count)
{
    const ImGuiID widget = ImGui::GetID(label);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    char name[64];
    for (int i = 0; i < count; ++i)
    {
        ImGui::PushOverrideID(widget);
        const ImGuiID id = ImGui::GetID(i);
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
    ForceCaptureLook(ctx, 1);
    facet::RangeSlider("Price", &ui.low, &ui.high, 0.0f, 100.0f,
                       {.format = "%.0f", .minGap = 10.0, .valueBubble = true});
    ctx.Probe("range.price");
    ProbeHandles(ctx, "Price", "range.price", 2);
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

} // namespace

FACET_DEMO_PAGE("sliders", "Sliders", "Widgets", 30, "default", "hovered", "focused", "dragging")
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

    SectionHeader("Knob");
    LabeledRow("Knob");
    facet::Knob("##knob", &ui.knob, 0.0f, 1.0f);

    RangeSection(ctx);

    SectionHeader("Ranges");
    Paragraph("Every range widget shares one handle model: handles never cross, keep a minimum gap, "
              "and either stop at each other or push each other.");
    LabeledRow("Range with fields");
    facet::RangeInput("##rangeInput", &ui.lowInt, &ui.highInt, 0, 100, {.collision = facet::HandleCollision::Push});
    LabeledRow("Range over bars");
    facet::RangeSliderHistogram("##histogram", &ui.histogramLow, &ui.histogramHigh, 0.0f, 1.0f, kBins);
    LabeledRow("Several handles");
    facet::MultiSlider("##multi", std::span<float>(ui.handles), 0.0f, 1.0f, {.minGap = 0.05});
    LabeledRow("Arc range");
    facet::ArcRangeSlider("##arc", &ui.arcLow, &ui.arcHigh, 0.0f, 360.0f, {.wrap = true, .sweep = 360.0f});
    LabeledRow("Shares");
    facet::AllocationBar("##shares", std::span<float>(ui.shares), kSegments, {.minGap = 0.05});

    SectionHeader("Two dimensions");
    LabeledRow("XY pad");
    facet::XYPad("##pad", &ui.point, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), {.grid = true});
    LabeledRow("Joystick");
    facet::JoystickPad("##stick", &ui.stick, {.deadzone = 0.1f, .showDeadzone = true});
    LabeledRow("Box range");
    facet::BoxRangePad("##box", &ui.boxLow, &ui.boxHigh, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
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
}
