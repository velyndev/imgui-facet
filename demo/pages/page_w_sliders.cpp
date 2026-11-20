// Demo page: Sliders. One section per widget of facet/widgets/sliders.h with its important
// variants, capture states for the looks that need input (hovered, focused, dragging), and one
// selftest that operates the ready widgets by mouse and keyboard. A widget whose docs entry is not
// ready yet draws its placeholder at its default size.

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

    // The planned widgets below.
    float steppedStops = 0.25f;
    float field = 0.75f;
    float low = 0.2f;
    float high = 0.8f;
    int lowInt = 20;
    int highInt = 80;
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

const float kStops[] = {0.0f, 0.25f, 0.5f, 1.0f};
const char* const kStopLabels[] = {"Option 1", "Option 2", "Option 3", "Option 4"};
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

} // namespace

FACET_DEMO_PAGE("sliders", "Sliders", "Widgets", 30, "default", "hovered", "focused", "dragging")
{
    facet::demo::PageHeader("Sliders",
                            "Sliders, range sliders, multi-handle sliders, knobs and two-dimensional pads.");
    facet::demo::PlannedFamilyNote("sliders");

    SliderSection(ctx);

    SectionHeader("Stepped and with a field");
    LabeledRow("Stepped");
    facet::SliderStepped("##stepped", &ui.steppedStops, kStops, {.stopLabels = kStopLabels});
    LabeledRow("With a field");
    facet::SliderWithInput("##field", &ui.field, 0.0f, 1.0f);
    LabeledRow("Knob");
    facet::Knob("##knob", &ui.knob, 0.0f, 1.0f);

    SectionHeader("Ranges");
    Paragraph("Every range widget shares one handle model: handles never cross, keep a minimum gap, "
              "and either stop at each other or push each other.");
    LabeledRow("Range");
    facet::RangeSlider("##range", &ui.low, &ui.high, 0.0f, 1.0f, {.minGap = 0.1});
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
    test.Do([] { ui = {}; });

    // Slider, pointer: a click on the track moves the value there, a drag follows the pointer, a
    // drag out of the window and back to the press point ends on the start value.
    test.Drag({T::Item("slider.count", 0.45f), T::Item("slider.count", 0.99f)}, 0.3f);
    test.ExpectNear("a drag past the end of the track reaches 10", [] { return ui.count; }, 10.0, 0.0);
    test.Click("slider.count");
    test.Expect("a click on the track moves the count under the pointer", [] { return ui.count > 0 && ui.count < 10; });
    static float pressed = 0.0f;
    test.MouseMove(T::Item("slider.level", 0.6f));
    test.MouseDown();
    test.Do([] { pressed = ui.level; });
    test.MouseMove(T::Display(1.5f, 0.5f));
    test.Expect("a drag out of the window to the right reaches the maximum", [] { return ui.level == 1.0f; });
    test.MouseMove(T::Display(-0.5f, 0.5f));
    test.MouseMove(T::Item("slider.level", 0.6f));
    test.MouseUp();
    test.Expect("a drag out of the window and back ends on the value under the pointer",
                [] { return ui.level == pressed; });
    test.Expect("the drag was one finished edit", [] { return ui.levelCommits == 1; });
    test.Drag({T::Item("slider.frequency", 0.3f), T::Item("slider.frequency", 0.1f)}, 0.3f);
    test.Expect("the logarithmic slider follows the pointer down", [] { return ui.frequency < 440.0f; });

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
    test.Click("slider.limit");
    test.KeyPress(ImGuiKey_Home);
    test.Expect("Home clears the error of the limit", [] { return ui.invalid <= 0.8f; });
    test.MouseMove(T::Item("slider.mixed", 0.8f));
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
}
