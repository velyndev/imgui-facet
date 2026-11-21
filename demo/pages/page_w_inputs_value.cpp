// Demo page: Value inputs. Every number-like widget of facet/widgets/inputs.h (NumberInput,
// DragNumber, UnitInput, VectorInput, SizeInput, HexInput, DurationInput, KeybindCapture, Rating)
// in a section of its own with its main variants and states, the values it wrote shown under it,
// capture states for the interaction looks and a selftest that operates each widget by mouse and
// by keyboard. The text-like inputs have their own page (page_w_inputs_text.cpp).

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>

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

struct ValueInputsUi
{
    // NumberInput
    int count = 3;
    float opacity = 0.75f;
    double delay = 250.0;
    int even = 3;
    int readOnly = 12;
    int disabled = 4;
    // DragNumber
    float amount = 0.5f;
    int steps = 20;
    double frequency = 440.0;
    int angle = 90;
    // UnitInput
    double duration = 0.25;
    int durationUnit = 0;
    double length = 1500.0;
    int lengthUnit = 1;
    // VectorInput
    float position[3] = {0.0f, 1.0f, 0.0f};
    ImVec2 scale = {1.0f, 1.0f};
    float direction[3] = {0.0f, 0.0f, 1.0f};
    int cells[2] = {8, 6};
    // SizeInput
    int canvasWidth = 1280;
    int canvasHeight = 720;
    ImVec2 region = {0.5f, 0.25f};
    // DurationInput
    double interval = 90.0;
    double clip = 12.5;
    // HexInput
    std::uint32_t word = 0x00FF7F3Fu;
    std::uint64_t wide = 0x0123456789ABCDEFull;
    std::uint32_t mask = 0x0F0Fu;
    // KeybindCapture
    facet::Keybind overlay = {.primary = ImGuiKey_F5, .mode = facet::BindMode::Toggle};
    facet::Keybind twoKeys = {.primary = ImGuiKey_F6, .alternate = ImGuiKey_F7 | ImGuiMod_Shift};
    facet::Keybind checked = {};
    bool overlayLatch = false;
    bool conflict = false;   // the checked bind uses the sample shortcut's chord
    // Rating
    int stars = 3;
    float halves = 2.5f;
    int hearts = 1;
    int writes = 0;          // every write on this page, so each one shows
};

ValueInputsUi ui;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kHovered,
    kFocused,
    kDragging,
    kOpen,
    kListening,
};

const facet::UnitItem kTimeUnits[] = {{"ms", 0.001}, {"s", 1.0}, {"min", 60.0}};
const facet::UnitItem kLengthUnits[] = {{"m", 1.0}, {"km", 1000.0}};
const char* const kCellAxes[] = {"C", "R"};
const ImVec2 kCanvasPresets[] = {{1280.0f, 720.0f}, {1920.0f, 1080.0f}, {2560.0f, 1440.0f}};

// The sample shortcut the checked bind is compared with; registered once, for the whole run.
facet::ShortcutId SampleShortcut()
{
    static const facet::ShortcutId id =
        facet::RegisterShortcut("demo.inputs.sample", ImGuiKey_K | ImGuiMod_Ctrl, {.label = "Sample shortcut"});
    return id;
}

void Count(bool written)
{
    if (written)
        ++ui.writes;
}

bool Capture(const facet::demo::PageCtx& ctx, CaptureState state)
{
    return ctx.capture && ctx.captureState == state;
}

void NumberSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("NumberInput");
    Paragraph("Typed numbers are written on Enter, Tab or focus loss; the steppers, Up and Down write at once. Escape "
              "restores the value from the start of the edit.");
    LabeledRow("Count, 0 to 10");
    if (Capture(ctx, kHovered))
        ctx.ForceNextItemState(ForcedItemState::Hovered);
    else if (Capture(ctx, kFocused))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    Count(facet::NumberInput("##count", &ui.count, {.min = 0, .max = 10}));
    ctx.Probe("inputs.count");
    if (Capture(ctx, kHovered))
        ctx.RevealInCapture(0.3f);
    LabeledRow("Opacity");
    Count(facet::NumberInput("##opacity", &ui.opacity, {.format = "%.2f", .min = 0.0, .max = 1.0, .step = 0.05,
                                                        .stepFast = 0.25}));
    ctx.Probe("inputs.opacity");
    LabeledRow("Delay, small");
    Count(facet::NumberInput("##delay", &ui.delay,
                             {.format = "%.0f", .min = 0.0, .step = 10.0, .stepFast = 100.0, .unit = "ms",
                              .size = facet::ControlSize::Small}));
    ctx.Probe("inputs.delay");
    LabeledRow("Even number");
    const bool odd = ui.even % 2 != 0;
    Count(facet::NumberInput("##even", &ui.even,
                             {.step = 2.0, .error = odd ? "Enter an even number" : nullptr,
                              .helpText = "The page checks the value; the field shows the message."}));
    ctx.Probe("inputs.even");
    LabeledRow("Read-only");
    Count(facet::NumberInput("##readonly", &ui.readOnly, {.readOnly = true}));
    ctx.Probe("inputs.readonly");
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    Count(facet::NumberInput("##disabled", &ui.disabled));
    ImGui::EndDisabled();
    Note("Count %d, opacity %.2f, delay %.0f ms, even number %d.", ui.count, ui.opacity, ui.delay, ui.even);
}

void DragSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("DragNumber");
    Paragraph("Drag the field or its label sideways (Shift finer, Ctrl coarser); a click without a drag, Enter or Space "
              "types a value. Left and Right step while it is focused.");
    LabeledRow("Amount, 0 to 1");
    if (Capture(ctx, kDragging))
        ctx.ForceNextItemState(ForcedItemState::Dragging);
    Count(facet::DragNumber("##amount", &ui.amount, {.format = "%.3f", .min = 0.0, .max = 1.0}));
    ctx.Probe("inputs.amount");
    if (Capture(ctx, kDragging))
        ctx.RevealInCapture(0.3f);
    LabeledRow("Steps, open range");
    Count(facet::DragNumber("##steps", &ui.steps, {.speed = 0.25}));
    ctx.Probe("inputs.steps");
    LabeledRow("Frequency, logarithmic");
    Count(facet::DragNumber("##frequency", &ui.frequency,
                            {.format = "%.0f Hz", .min = 20.0, .max = 20000.0, .scale = facet::ValueScale::Logarithmic}));
    ctx.Probe("inputs.frequency");
    LabeledRow("Angle, wraps");
    Count(facet::DragNumber("##angle", &ui.angle, {.format = "%d\xC2\xB0", .min = 0.0, .max = 360.0, .wrap = true}));
    ctx.Probe("inputs.angle");
    Note("Amount %.3f, steps %d, frequency %.0f Hz, angle %d.", ui.amount, ui.steps, ui.frequency, ui.angle);
}

void UnitSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("UnitInput");
    Paragraph("The value is kept in base units; the menu at the end picks the unit shown. A typed suffix such as "
              "\"250ms\" or \"2 min\" switches the unit.");
    LabeledRow("Duration");
    if (Capture(ctx, kOpen))
        ctx.ForceNextItemState(ForcedItemState::Open, 1);
    Count(facet::UnitInput("##duration", &ui.duration, &ui.durationUnit, kTimeUnits, {.format = "%g", .min = 0.0}));
    ctx.Probe("inputs.duration");
    if (Capture(ctx, kOpen))
        ctx.RevealInCapture(0.2f);
    LabeledRow("Length, keeps the number");
    Count(facet::UnitInput("##length", &ui.length, &ui.lengthUnit, kLengthUnits, {.format = "%g", .convert = false}));
    ctx.Probe("inputs.length");
    Note("Duration %g s in %s; length %g m in %s.", ui.duration, kTimeUnits[ui.durationUnit].label, ui.length,
         kLengthUnits[ui.lengthUnit].label);
}

void VectorSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("VectorInput");
    Paragraph("One drag field per component, each a Tab stop with its axis label as a drag handle. The link button "
              "scales every component alike while it is on; narrow rows stack the fields.");
    LabeledRow("Position");
    Count(facet::VectorInput("##position", std::span<float>(ui.position), {.format = "%.2f", .speed = 0.01}));
    ctx.Probe("inputs.position");
    LabeledRow("Scale, linked");
    Count(facet::VectorInput("##scale", &ui.scale, {.format = "%.2f", .min = 0.0, .speed = 0.01, .linkToggle = true}));
    ctx.Probe("inputs.scale");
    LabeledRow("Direction, length 1");
    Count(facet::VectorInput("##direction", std::span<float>(ui.direction),
                             {.format = "%.2f", .min = -1.0, .max = 1.0, .normalize = true}));
    LabeledRow("Grid cells");
    Count(facet::VectorInput("##cells", std::span<int>(ui.cells), {.min = 1.0, .max = 64.0, .axisLabels = kCellAxes}));
    ctx.Probe("inputs.cells");
    Note("Position %.2f, %.2f, %.2f; scale %.2f by %.2f; grid %d by %d.", ui.position[0], ui.position[1], ui.position[2],
         ui.scale.x, ui.scale.y, ui.cells[0], ui.cells[1]);
}

void SizeSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("SizeInput");
    Paragraph("Width and height with the aspect lock between them; the buttons at the end swap the values and offer "
              "common sizes.");
    LabeledRow("Canvas");
    Count(facet::SizeInput("##canvas", &ui.canvasWidth, &ui.canvasHeight,
                           {.min = {1.0f, 1.0f}, .max = {8192.0f, 8192.0f}, .swapButton = true,
                            .presets = kCanvasPresets}));
    ctx.Probe("inputs.canvas");
    {
        // The lock button for the selftest: at the end of the width field, side by side or stacked.
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const facet::Theme& t = facet::T();
        const float height = facet::ControlHeight(facet::ControlSize::Regular);
        const bool stacked = max.y - min.y > height * 1.5f;
        const float fieldEnd = stacked ? max.x : std::floor(min.x + (max.x - min.x - facet::Px(t.spacing.sm)) * 0.5f);
        const float gap = facet::Px(t.spacing.xs);
        facet::demo::ProbeRect(ctx, "inputs.canvas.lock", 0, ImVec2(fieldEnd - gap - (height - gap * 2.0f), min.y + gap),
                               ImVec2(fieldEnd - gap, min.y + height - gap));
    }
    LabeledRow("Region, without lock");
    Count(facet::SizeInput("##region", &ui.region, {.format = "%.2f", .step = 0.05, .aspectLock = false}));
    Note("Canvas %d by %d; region %.2f by %.2f.", ui.canvasWidth, ui.canvasHeight, ui.region.x, ui.region.y);
}

void DurationSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("DurationInput");
    Paragraph("Left and Right pick a segment, Up and Down step it with the carry, and a vertical drag moves it. Enter, "
              "a click or a digit types a duration such as \"1m30\", \"1h2m3s\" or \"90\".");
    LabeledRow("Interval");
    if (Capture(ctx, kFocused))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    else if (Capture(ctx, kDragging))
        ctx.ForceNextItemState(ForcedItemState::Dragging);
    Count(facet::DurationInput("##interval", &ui.interval));
    ctx.Probe("inputs.interval");
    if (Capture(ctx, kFocused))
        ctx.RevealInCapture(0.5f);
    LabeledRow("Clip, milliseconds");
    Count(facet::DurationInput("##clip", &ui.clip, {.showHours = false, .showMilliseconds = true}));
    ctx.Probe("inputs.clip");
    Note("Interval %.0f s; clip %.3f s.", ui.interval, ui.clip);
}

void HexSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("HexInput");
    Paragraph("A typed digit replaces the one after the caret; Up and Down change that digit by one.");
    LabeledRow("32-bit value");
    Count(facet::HexInput("##word", &ui.word, {.prefix = true, .groupBytes = true}));
    ctx.Probe("inputs.word");
    LabeledRow("64-bit value");
    Count(facet::HexInput("##wide", &ui.wide, {.prefix = true}));
    ctx.Probe("inputs.wide");
    LabeledRow("Four digits, lowercase");
    Count(facet::HexInput("##mask", &ui.mask, {.digits = 4, .uppercase = false}));
    Note("32-bit 0x%08X, 64-bit 0x%016llX, four digits 0x%04x.", ui.word, static_cast<unsigned long long>(ui.wide),
         ui.mask);
}

void KeybindSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("KeybindCapture");
    Paragraph("A click, Enter or Space listens for the next chord; Escape or a click elsewhere cancels; Backspace or "
              "Delete clears the focused slot.");
    LabeledRow("Overlay");
    if (Capture(ctx, kListening))
        ctx.ForceNextItemState(ForcedItemState::Pressed, 0);
    Count(facet::KeybindCapture("##overlay", &ui.overlay, {.showMode = true}));
    ctx.Probe("inputs.overlay");
    if (Capture(ctx, kListening))
        ctx.RevealInCapture(0.4f);
    const bool active = facet::IsBindActive(ui.overlay, &ui.overlayLatch);
    LabeledRow("Two slots");
    Count(facet::KeybindCapture("##twoKeys", &ui.twoKeys, {.allowModifierOnly = true, .showAlternate = true}));
    ctx.Probe("inputs.twokeys");
    LabeledRow("Checked bind");
    // Validation is the caller's: the page looks the chord up and passes what it found as the error.
    const facet::ShortcutId conflict = facet::FindChordConflict(ui.checked.primary, {});
    facet::ShortcutInfo info;
    ui.conflict = conflict.IsValid() && facet::GetShortcut(conflict, &info);
    static char conflictText[128];
    if (ui.conflict)
        std::snprintf(conflictText, sizeof(conflictText), "Also used by %s", info.label);
    Count(facet::KeybindCapture("##checked", &ui.checked,
                                {.error = ui.conflict ? conflictText : nullptr,
                                 .helpText = "The page compares the chord with the registered shortcuts."}));
    ctx.Probe("inputs.checked");
    Note("The overlay bind is %s (mode %s; press its chord to flip it).", active ? "active" : "inactive",
         ui.overlay.mode == facet::BindMode::Hold     ? "hold"
         : ui.overlay.mode == facet::BindMode::Toggle ? "toggle"
         : ui.overlay.mode == facet::BindMode::Always ? "always"
                                                      : "off");
}

void RatingSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("Rating");
    Paragraph("The pointer previews, a click writes, a click on the current value clears; Left, Right, Home and End "
              "work while it is focused.");
    LabeledRow("Stars");
    if (Capture(ctx, kHovered))
        ctx.ForceNextItemState(ForcedItemState::Hovered);
    else if (Capture(ctx, kFocused))
        ctx.ForceNextItemState(ForcedItemState::Focused);
    Count(facet::Rating("##stars", &ui.stars));
    ctx.Probe("inputs.stars");
    LabeledRow("Half steps");
    Count(facet::Rating("##halves", &ui.halves, {.halfSteps = true}));
    ctx.Probe("inputs.halves");
    LabeledRow("Own icon, large");
    Count(facet::Rating("##hearts", &ui.hearts, {.icon = facet::Icon("heart"), .size = facet::ControlSize::Large}));
    LabeledRow("Display only");
    Count(facet::Rating("##display", &ui.halves, {.readOnly = true}));
    Note("Stars %d, half steps %.1f, own icon %d. Writes on this page: %d.", ui.stars, ui.halves, ui.hearts, ui.writes);
}

} // namespace

FACET_DEMO_PAGE("inputs-value", "Value inputs", "Widgets", 45, "default", "hovered", "focused", "dragging", "open",
                "listening")
{
    facet::demo::PageHeader("Value inputs",
                            "Number, drag, unit, vector, size and duration fields, a hexadecimal field, key bind "
                            "capture and ratings.");
    facet::demo::PlannedFamilyNote("inputs");
    (void)SampleShortcut();

    NumberSection(ctx);
    DragSection(ctx);
    UnitSection(ctx);
    VectorSection(ctx);
    SizeSection(ctx);
    DurationSection(ctx);
    HexSection(ctx);
    KeybindSection(ctx);
    RatingSection(ctx);
}

FACET_DEMO_SELFTEST("inputs-value")
{
    using T = facet::demo::Target;
    test.Do([] { ui = {}; });

    // NumberInput: typing writes on Enter; Escape restores; the steppers write at once; Tab moves on.
    test.Click("inputs.count");
    test.Type("7");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("NumberInput writes a typed value on Enter", [] { return ui.count; }, 7.0, 0.0);
    test.Click("inputs.count");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up steps the focused number at once", [] { return ui.count; }, 8.0, 0.0);
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectNear("Escape restores the number from the start of the edit", [] { return ui.count; }, 7.0, 0.0);
    test.Click("inputs.count");
    test.Type("99");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectNear("a typed number is clamped to the range", [] { return ui.count; }, 10.0, 0.0);
    test.ExpectFocused("inputs.opacity");
    test.Type("0,5");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("a comma reads as the decimal separator", [] { return ui.opacity; }, 0.5, 1e-6);
    test.MouseMove(T::Item("inputs.count", 1.0f, 0.5f).Offset(-38.0f, 0.0f));  // the minus stepper
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.ExpectNear("the minus stepper steps at once", [] { return ui.count; }, 9.0, 0.0);
    test.Click("inputs.readonly");
    test.Type("5");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("a read-only number is never written", [] { return ui.readOnly; }, 12.0, 0.0);

    // DragNumber: a drag moves the value, a click types, Left and Right step.
    test.Drag({T::Item("inputs.amount", 0.3f), T::Item("inputs.amount", 0.6f)}, 0.3f);
    test.Expect("a drag to the right raises the amount", [] { return ui.amount > 0.6f; });
    test.Click("inputs.amount");
    test.Type("0.25");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("a click on a drag field types a value", [] { return ui.amount; }, 0.25, 1e-6);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right steps the focused drag field", [] { return ui.amount; }, 0.26, 1e-4);

    // UnitInput: a typed suffix switches the unit; the menu picks one by keyboard.
    test.Click("inputs.duration");
    test.Type("2 s");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("a typed suffix writes base units", [] { return ui.duration; }, 2.0, 1e-9);
    test.ExpectNear("and switches the shown unit", [] { return ui.durationUnit; }, 1.0, 0.0);
    test.MouseMove(T::Item("inputs.duration", 1.0f, 0.5f).Offset(-20.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_End);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("the unit menu picks a unit by keyboard", [] { return ui.durationUnit; }, 2.0, 0.0);
    test.ExpectNear("and keeps the quantity", [] { return ui.duration; }, 2.0, 1e-9);

    // VectorInput: each component is a Tab stop; one edit writes one component.
    test.MouseMove(T::Item("inputs.position", 0.15f, 0.0f).Offset(0.0f, 14.0f));
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.Type("4");
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectNear("the first component is written on Tab", [] { return ui.position[0]; }, 4.0, 1e-6);
    test.ExpectNear("the second one is untouched", [] { return ui.position[1]; }, 1.0, 1e-6);
    test.KeyPress(ImGuiKey_Enter);
    test.Type("5");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("Tab reached the second component, which types on Enter", [] { return ui.position[1]; }, 5.0, 1e-6);
    test.Drag({T::Item("inputs.cells", 0.02f, 0.0f).Offset(0.0f, 14.0f), T::Item("inputs.cells", 0.02f, 0.0f).Offset(12.0f, 14.0f)},
              0.3f);
    test.Expect("the axis label drags its component", [] { return ui.cells[0] > 8 && ui.cells[1] == 6; });

    // SizeInput: typed width; the lock keeps the ratio.
    test.MouseMove(T::Item("inputs.canvas.lock"));
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.MouseMove(T::Item("inputs.canvas", 0.1f, 0.0f).Offset(0.0f, 14.0f));
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.Type("640");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("a typed width is written", [] { return ui.canvasWidth; }, 640.0, 0.0);
    test.ExpectNear("the lock writes the height in the same frame", [] { return ui.canvasHeight; }, 360.0, 0.0);

    // DurationInput: typed units, then Up on the focused field steps the seconds.
    test.Click("inputs.interval");
    test.Wait(2);
    test.Type("1m30");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("a typed duration is written", [] { return ui.interval; }, 90.0, 0.0);
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up steps the seconds of the focused duration", [] { return ui.interval; }, 91.0, 0.0);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Left picks the minutes", [] { return ui.interval; }, 151.0, 0.0);

    // HexInput: overtype and Up on a digit.
    test.Click("inputs.word");
    test.KeyPress(ImGuiKey_Home);
    test.Type("AB");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("typed digits overtype", [] { return static_cast<double>(ui.word); },
                    static_cast<double>(0xABFF7F3Fu), 0.0);
    test.Click("inputs.word");
    test.KeyPress(ImGuiKey_Home);
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up changes the digit after the caret", [] { return static_cast<double>(ui.word); },
                    static_cast<double>(0xBBFF7F3Fu), 0.0);
    test.KeyPress(ImGuiKey_Enter);

    // KeybindCapture: a click listens; the chord is written on release; keys clear and listen again.
    test.MouseMove(T::Item("inputs.overlay", 0.05f, 0.0f).Offset(0.0f, 14.0f));  // the slot line
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.KeyPress(ImGuiKey_F8);
    test.Expect("a click listens and the next key is bound", [] { return ui.overlay.primary == ImGuiKey_F8; });
    test.KeyPress(ImGuiKey_Delete);
    test.Expect("Delete clears the focused slot", [] { return ui.overlay.primary == ImGuiKey_None; });
    // After a click the keyboard focus shows on the first key; Shift+Tab and Tab come back to the slot.
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_G | ImGuiMod_Ctrl);
    test.Expect("Enter listens; a chord is bound", [] { return ui.overlay.primary == (ImGuiKey_G | ImGuiMod_Ctrl); });
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape cancels and keeps the chord", [] { return ui.overlay.primary == (ImGuiKey_G | ImGuiMod_Ctrl); });
    test.KeyPress(ImGuiKey_G | ImGuiMod_Ctrl);
    test.Expect("the bound chord flips the toggle bind", [] { return ui.overlayLatch; });
    test.MouseMove(T::Item("inputs.checked", 0.05f, 0.0f).Offset(0.0f, 14.0f));
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.KeyPress(ImGuiKey_K | ImGuiMod_Ctrl);
    test.Wait(2);
    test.Expect("a chord of a registered shortcut is reported", [] { return ui.conflict; });

    // Rating: a click writes, keys step.
    test.MouseMove(T::Item("inputs.stars", 0.0f, 0.5f).Offset(3.5f * 28.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.ExpectNear("a click on the fourth star writes 4", [] { return ui.stars; }, 4.0, 0.0);
    test.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("Right steps the focused rating", [] { return ui.stars; }, 5.0, 0.0);
    test.KeyPress(ImGuiKey_Home);
    test.ExpectNear("Home goes to 0", [] { return ui.stars; }, 0.0, 0.0);
    test.MouseMove(T::Item("inputs.halves", 0.0f, 0.5f).Offset(3.75f * 28.0f, 0.0f));
    test.MouseDown();
    test.MouseUp();
    test.Wait(2);
    test.ExpectNear("a click on the right half of the fourth icon writes 4", [] { return ui.halves; }, 4.0, 0.0);
}
