// Demo page: Value inputs. Calls every number-like widget of facet/widgets/inputs.h once
// (NumberInput, DragNumber, UnitInput, VectorInput, SizeInput, HexInput, DurationInput,
// KeybindCapture, Rating), grouped by purpose. The text-like inputs have their own page
// (page_w_inputs_text.cpp). A planned widget draws its placeholder at the widget's default size;
// implementing it changes nothing in this page's layout.

#include <cstdint>
#include <span>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct ValueInputsUi
{
    int count = 3;
    float amount = 0.5f;
    double duration = 0.25;
    int durationUnit = 0;
    float vector[3] = {0.0f, 1.0f, 0.0f};
    ImVec2 size = {640.0f, 480.0f};
    std::uint32_t hex = 0;
    double seconds = 90.0;
    facet::Keybind bind = {.primary = ImGuiKey_F5, .mode = facet::BindMode::Toggle};
    int rating = 3;
};

ValueInputsUi ui;

const facet::UnitItem kTimeUnits[] = {{"ms", 0.001}, {"s", 1.0}, {"min", 60.0}};

} // namespace

FACET_DEMO_PAGE("inputs-value", "Value inputs", "Widgets", 45)
{
    facet::demo::PageHeader("Value inputs",
                            "Number, drag, unit, vector, size and duration fields, a hexadecimal field, key bind "
                            "capture and ratings.");
    facet::demo::PlannedFamilyNote("inputs");

    SectionHeader("Numbers");
    Paragraph("Typed numbers are written on Enter, Tab or focus loss; steppers and drags write at once.");
    LabeledRow("Number");
    facet::NumberInput("##count", &ui.count, {.min = 0, .max = 10});
    ctx.Probe("inputs.count");
    LabeledRow("Drag number");
    facet::DragNumber("##amount", &ui.amount, {.min = 0.0, .max = 1.0, .speed = 0.005});
    LabeledRow("With units");
    facet::UnitInput("##duration", &ui.duration, &ui.durationUnit, kTimeUnits);
    LabeledRow("Vector");
    facet::VectorInput("##vector", std::span<float>(ui.vector), {.linkToggle = true});
    LabeledRow("Size");
    facet::SizeInput("##size", &ui.size);
    LabeledRow("Duration");
    facet::DurationInput("##seconds", &ui.seconds);

    SectionHeader("Formatted numbers");
    LabeledRow("Hexadecimal");
    facet::HexInput("##hex", &ui.hex, {.prefix = true});

    SectionHeader("Keys and ratings");
    LabeledRow("Key bind");
    facet::KeybindCapture("##bind", &ui.bind, {.showMode = true});
    LabeledRow("Rating");
    facet::Rating("##rating", &ui.rating);
    ctx.Probe("inputs.rating");
}

FACET_DEMO_SELFTEST("inputs-value")
{
    test.Do([] { ui = {}; });
    // The planned fields are placeholders: clicks and keys write nothing.
    test.Click("inputs.count");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("the number placeholder writes nothing", [] { return ui.count; }, 3.0, 0.0);
    test.Click("inputs.rating");
    test.ExpectNear("the rating placeholder writes nothing", [] { return ui.rating; }, 3.0, 0.0);
}
