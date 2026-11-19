// Demo page: Sliders. Calls every widget of facet/widgets/sliders.h once, grouped by purpose.
// The widgets are planned, so each call draws its placeholder at the widget's default size;
// Phase 2 replaces them one by one without touching this page's layout.

#include <vector>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct SlidersUi
{
    float value = 0.5f;
    int count = 4;
    float stepped = 0.25f;
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

const float kStops[] = {0.0f, 0.25f, 0.5f, 1.0f};
const char* const kStopLabels[] = {"Option 1", "Option 2", "Option 3", "Option 4"};
// Placeholder distribution for the histogram: an even ramp, not measured data.
const float kBins[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f};
const char* const kSegments[] = {"Option 1", "Option 2", "Option 3"};

} // namespace

FACET_DEMO_PAGE("sliders", "Sliders", "Widgets", 30)
{
    facet::demo::PageHeader("Sliders",
                            "Sliders, range sliders, multi-handle sliders, knobs and two-dimensional pads.");
    facet::demo::PlannedFamilyNote("sliders");

    SectionHeader("One value");
    LabeledRow("Slider");
    facet::Slider("##value", &ui.value, 0.0f, 1.0f, {.valueBubble = true});
    LabeledRow("Whole numbers");
    facet::Slider("##count", &ui.count, 0, 10);
    LabeledRow("Stepped");
    facet::SliderStepped("##stepped", &ui.stepped, kStops, {.stopLabels = kStopLabels});
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
