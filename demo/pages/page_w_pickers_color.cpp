// Demo page: Color pickers. One section per color widget of facet/widgets/pickers.h with its important
// variants: the color well and its popover, the full picker in both modes, the saturation and value
// square, the channel sliders, the inputs, the swatch palettes and the editor of an animated color.
// Capture states show the open popover, the focus ring, a drag in progress and the Wheel mode; one
// selftest operates every widget by mouse and keyboard. The date, time and other pickers have a
// page of their own (page_w_pickers_date.cpp).

#include <span>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "page_helpers.h"
#include "internal/popup_host.h"
#include "pages/pages.h"

namespace
{

using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct ColorPickersUi
{
    // Picked colors are data, not theme colors; they start at the theme's accent.
    bool seeded = false;
    ImVec4 color = {};
    ImVec4 original = {};
    ImVec4 translucent = {};
    ImVec4 small = {}, regular = {}, large = {};
    ImVec4 readOnly = {}, disabled = {}, mixed = {}, invalid = {};
    bool mixedSet = false;
    ImVec4 channels = {};
    ImVec4 floats = {};
    int pickerMode = 0;     // 0 Square, 1 Wheel
    bool pickerAlpha = true;
    bool pickerInputs = true;
    facet::ColorHistory history;
    std::vector<ImVec4> editable;
    facet::AnimatedColor animated;
    bool wellOpen = false;  // the popover of the first well, read each frame
};

ColorPickersUi ui;

// Accent presets shown as swatches at most.
constexpr int kMaxSwatches = 8;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kWellOpen,
    kFocused,
    kDragging,
    kWheel,
    kChannels,  // the channel sliders and the inputs
    kPalettes,  // the swatch palettes and the animated color
};

const char* const kModes[] = {"Square", "Wheel"};

void Seed()
{
    if (ui.seeded)
        return;
    const ImVec4 accent = facet::T().colors.accent;
    ui.color = accent;
    ui.original = accent;
    ui.translucent = facet::WithAlpha(accent, 0.5f);
    ui.small = ui.regular = ui.large = accent;
    ui.readOnly = ui.disabled = ui.mixed = accent;
    // The lightest accent preset, so the error of the last well shows from the start.
    ui.invalid = accent;
    for (const facet::AccentPreset& preset : facet::AccentPresets())
    {
        if (facet::RelativeLuminance(preset.color) > facet::RelativeLuminance(ui.invalid))
            ui.invalid = preset.color;
    }
    ui.channels = accent;
    ui.floats = accent;
    ui.animated = {facet::ColorAnimation::Pulse, accent, facet::T().colors.success, 2.0f};
    // The editable palette starts with the first two accent presets of the theme.
    for (const facet::AccentPreset& preset : facet::AccentPresets())
    {
        if (ui.editable.size() < 2)
            ui.editable.push_back(preset.color);
    }
    ui.seeded = true;
}

// Probes a part of a widget made of several items: its id under the widget and its rectangle.
void ProbePart(facet::demo::PageCtx& ctx, const char* name, ImGuiID widget, const char* part, ImVec2 min, ImVec2 max)
{
    facet::demo::ProbeRect(ctx, name, ImHashStr(part, 0, widget), min, max);
}

} // namespace

FACET_DEMO_PAGE("pickers-color", "Color pickers", "Widgets", 50, "default", "well-open", "focused", "dragging", "wheel",
                 "channels", "palettes")
{
    facet::demo::PageHeader("Color pickers",
                            "Color wells, the full picker and its square, channel sliders, inputs, swatch palettes and "
                            "an editor for colors that pulse or cycle.");
    facet::demo::ReadyFamilyNote("pickers");
    Seed();

    // Swatches offered by the palettes: the accent presets of the theme.
    ImVec4 swatches[kMaxSwatches] = {};
    int swatchCount = 0;
    for (const facet::AccentPreset& preset : facet::AccentPresets())
    {
        if (swatchCount < kMaxSwatches)
            swatches[swatchCount++] = preset.color;
    }
    const std::span<const ImVec4> palette(swatches, static_cast<std::size_t>(swatchCount));
    const bool capture = ctx.capture;

    // ---- ColorWell ----
    SectionHeader("Color well");
    Paragraph("A click, Enter, Space or Alt+Down opens the picker beside the well. Escape closes it and restores the "
              "color from before it opened; a click outside keeps the change. Ctrl+C and Ctrl+V copy and paste the "
              "color as hex text, and the well takes colors dropped from another well.");
    LabeledRow("With hex text");
    if (capture && ctx.captureState == kWellOpen)
        ctx.ForceNextItemState(ForcedItemState::Open);
    facet::ColorWell("##well", &ui.color, {.showHex = true, .swatches = palette});
    ctx.Probe("pickers.well");
    ui.wellOpen = facet::internal::IsPopupLevelOpen(ImHashStr("##popover", 0, ImGui::GetItemID()));
    if (capture && ctx.captureState == kWellOpen)
        ctx.RevealInCapture(0.2f);
    LabeledRow("With alpha");
    facet::ColorWell("##translucent", &ui.translucent, {.alpha = true, .showHex = true});
    ctx.Probe("pickers.wellAlpha");
    const float large = facet::ControlHeight(facet::ControlSize::Large);
    LabeledRow("Sizes", large);
    {
        facet::demo::MixedHeightRow row(large);
        row.Item(facet::ControlHeight(facet::ControlSize::Small));
        facet::ColorWell("##small", &ui.small, {.size = facet::ControlSize::Small});
        row.Item(facet::ControlHeight(facet::ControlSize::Regular));
        facet::ColorWell("##regular", &ui.regular);
        row.Item(large);
        facet::ColorWell("##large", &ui.large, {.size = facet::ControlSize::Large});
    }
    LabeledRow("States");
    facet::ColorWell("Read-only##wellReadOnly", &ui.readOnly, {.showHex = true, .readOnly = true});
    ImGui::SameLine();
    ImGui::BeginDisabled();
    facet::ColorWell("Disabled##wellDisabled", &ui.disabled, {.showHex = true});
    ImGui::EndDisabled();
    LabeledRow("");
    if (facet::ColorWell("Mixed##wellMixed", &ui.mixed, {.showHex = true, .mixed = !ui.mixedSet}))
        ui.mixedSet = true;
    LabeledRow("");
    // White text reads on a color when white is the more readable of white and black on it.
    const bool tooLight = facet::RelativeLuminance(facet::ReadableOn(ui.invalid)) < 0.5f;
    facet::ColorWell("Background##wellError", &ui.invalid,
                     {.showHex = true, .width = facet::demo::DemoWidth(260.0f), .error = tooLight ? "White text does not read on this color" : nullptr,
                      .helpText = "White text goes on this color"});

    // ---- ColorPicker ----
    SectionHeader("Color picker");
    Paragraph("Each part is a Tab stop. Arrow keys move the square handle on both axes and step the bars, Shift finer "
              "and Ctrl coarser. The right half of the compare strip restores the original color.");
    if (capture)
        ui.pickerMode = ctx.captureState == kWheel ? 1 : 0;
    LabeledRow("Mode");
    facet::SegmentedControl("##mode", &ui.pickerMode, std::span<const char* const>(kModes));
    LabeledRow("Alpha");
    facet::Toggle("##alpha", &ui.pickerAlpha);
    LabeledRow("Inputs");
    facet::Toggle("##inputs", &ui.pickerInputs);
    LabeledRow("Original");
    if (facet::Button("Keep the current color"))
        ui.original = ui.color;
    ImGui::Spacing();
    const bool wheel = ui.pickerMode == 1;
    if (capture && ctx.captureState == kFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused, 0);
    if (capture && ctx.captureState == kDragging)
        ctx.ForceNextItemState(ForcedItemState::Hovered | ForcedItemState::Dragging, 0);
    const ImGuiID pickerId = ImGui::GetID("Color picker");
    const ImVec2 pickerAt = ImGui::GetCursorScreenPos();
    facet::ColorPicker("Color picker", &ui.color,
                       {.alpha = ui.pickerAlpha,
                        .mode = wheel ? facet::ColorPickerMode::Wheel : facet::ColorPickerMode::Square,
                        .inputs = ui.pickerInputs,
                        .original = &ui.original,
                        .swatches = palette});
    ctx.Probe("pickers.picker");
    {
        // The parts the selftest operates: the square (or wheel) and the hue bar under it.
        const float side = ImGui::GetItemRectSize().x;
        ProbePart(ctx, "pickers.picker.sv", pickerId, "##sv", pickerAt, ImVec2(pickerAt.x + side, pickerAt.y + side));
        const float gap = facet::Px(facet::T().spacing.sm);
        const float bar = facet::ControlHeight(facet::ControlSize::Inherit);
        ProbePart(ctx, "pickers.picker.hue", pickerId, "##hue", ImVec2(pickerAt.x, pickerAt.y + side + gap),
                  ImVec2(pickerAt.x + side, pickerAt.y + side + gap + bar));
    }
    if (capture && (ctx.captureState == kFocused || ctx.captureState == kDragging || ctx.captureState == kWheel))
        ctx.RevealInCapture(0.5f);

    // ---- SatValSquare ----
    SectionHeader("Saturation and value square");
    Paragraph("The square of the picker on its own: it writes saturation and value and keeps the hue, also while the "
              "color passes through grey and black.");
    facet::SatValSquare("Saturation and value", &ui.color, {.width = facet::demo::DemoWidth(200.0f)});
    ctx.Probe("pickers.square");

    // ---- ColorChannelSlider ----
    SectionHeader("Channel sliders");
    Paragraph("One slider per channel over its gradient, the other channels held; the hue wraps at the ends and the "
              "alpha track shows a checkerboard. The keys are those of a slider.");
    struct ChannelRow
    {
        const char* label;
        facet::ColorChannel channel;
        const char* probe;
    };
    const facet::Strings& strings = facet::GetStrings();
    const ChannelRow rows[] = {
        {strings.colorHue, facet::ColorChannel::Hue, "pickers.channel.hue"},
        {strings.colorSaturation, facet::ColorChannel::Saturation, "pickers.channel.saturation"},
        {strings.colorValue, facet::ColorChannel::Value, "pickers.channel.value"},
        {strings.colorRed, facet::ColorChannel::Red, "pickers.channel.red"},
        {strings.colorGreen, facet::ColorChannel::Green, "pickers.channel.green"},
        {strings.colorBlue, facet::ColorChannel::Blue, "pickers.channel.blue"},
        {strings.colorAlpha, facet::ColorChannel::Alpha, "pickers.channel.alpha"},
    };
    for (const ChannelRow& row : rows)
    {
        LabeledRow(row.label);
        ImGui::PushID(row.probe);
        facet::ColorChannelSlider("##channel", &ui.channels, {.channel = row.channel});
        ImGui::PopID();
        ctx.Probe(row.probe);
    }
    LabeledRow("Vertical", facet::Px(facet::T().metrics.faderLength));
    facet::ColorChannelSlider("Red##vertical", &ui.channels, {.channel = facet::ColorChannel::Red, .orientation = facet::Orientation::Vertical});
    ImGui::SameLine();
    facet::ColorChannelSlider("Green##vertical", &ui.channels, {.channel = facet::ColorChannel::Green, .orientation = facet::Orientation::Vertical});
    ImGui::SameLine();
    facet::ColorChannelSlider("Blue##vertical", &ui.channels, {.channel = facet::ColorChannel::Blue, .orientation = facet::Orientation::Vertical});
    LabeledRow("States");
    facet::ColorChannelSlider("Read-only##channel", &ui.readOnly, {.readOnly = true});
    LabeledRow("");
    ImGui::BeginDisabled();
    facet::ColorChannelSlider("Disabled##channel", &ui.disabled);
    ImGui::EndDisabled();

    // ---- ColorInputs ----
    SectionHeader("Color inputs");
    Paragraph("One field per channel of the model the menu picks, or one hex field. Drag a field sideways or type a "
              "value; pasting color text such as #3366FF, rgb(51, 102, 255) or 0.2, 0.4, 1 into a field sets the whole "
              "color.");
    LabeledRow("RGB with alpha");
    facet::ColorInputs("##inputs", &ui.color, {.alpha = true});
    ctx.Probe("pickers.inputs");
    LabeledRow("Fractions");
    facet::ColorInputs("##floats", &ui.floats, {.floats = true});
    LabeledRow("Hex");
    facet::ColorInputs("##hex", &ui.floats, {.model = facet::ColorModel::Hex, .modeSwitch = false});
    if (capture && ctx.captureState == kChannels)
        ctx.RevealInCapture(0.5f);
    LabeledRow("States");
    facet::ColorInputs("##inputsReadOnly", &ui.readOnly, {.model = facet::ColorModel::Hsv, .readOnly = true});
    LabeledRow("");
    facet::ColorInputs("##inputsError", &ui.invalid,
                       {.modeSwitch = false, .error = tooLight ? "White text does not read on this color" : nullptr,
                        .helpText = "White text goes on this color"});

    // ---- SwatchPalette ----
    SectionHeader("Swatch palette");
    Paragraph("One Tab stop; the arrow keys move between the swatches in both directions and Enter picks. The chosen "
              "swatch has the accent outline and a check mark. The recent row keeps the colors picked here.");
    LabeledRow("Accent presets");
    facet::SwatchPalette("Accent presets", &ui.color, palette, &ui.history);
    ctx.Probe("pickers.swatches");
    LabeledRow("Editable");
    facet::SwatchPalette("Editable swatches", &ui.color, &ui.editable, nullptr);
    ctx.Probe("pickers.editable");
    Note("The + swatch adds the current color, Delete removes the focused swatch, Alt with Left or Right or a drag "
         "moves it.");

    // ---- ColorModeEditor ----
    SectionHeader("Animated color");
    Paragraph("A color that stays, pulses toward a second color, runs through the hues or sweeps a gradient, with its "
              "period in seconds. The preview at the end stands still while motion is reduced.");
    LabeledRow("Marker");
    facet::ColorModeEditor("##animated", &ui.animated, {.alpha = true});
    ctx.Probe("pickers.animated");
    if (capture && ctx.captureState == kPalettes)
        ctx.RevealInCapture(0.7f);
}

FACET_DEMO_SELFTEST("pickers-color")
{
    using facet::demo::Target;
    test.Do([] {
        ui = {};
        Seed();
    });
    const auto same = [](const ImVec4& a, const ImVec4& b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; };

    // ColorWell: mouse opens, the keys edit in the popover, Escape restores and gives the focus back.
    test.Reveal("pickers.well");
    test.WaitSeconds(0.4f);
    test.Click("pickers.well");
    test.WaitSeconds(0.3f);
    test.Expect("a click on the well opens its picker", [] { return ui.wellOpen; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_UpArrow);
    test.Expect("the arrow keys edit the color in the popover", [same] { return !same(ui.color, ui.original); });
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.3f);
    test.Expect("Escape closes the popover", [] { return !ui.wellOpen; });
    test.Expect("Escape restores the color from before it opened", [same] { return same(ui.color, ui.original); });
    test.ExpectFocused("pickers.well");
    test.KeyPress(ImGuiKey_Enter);
    test.WaitSeconds(0.3f);
    test.Expect("Enter opens the popover again", [] { return ui.wellOpen; });
    test.KeyPress(ImGuiKey_Escape);
    test.WaitSeconds(0.3f);
    test.SetClipboard("#3366FF");
    test.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    test.Expect("Ctrl+V pastes hex text into the well", [] {
        return ui.color.x == 0x33 / 255.0f && ui.color.y == 0x66 / 255.0f && ui.color.z == 1.0f;
    });
    test.KeyPress(ImGuiKey_C | ImGuiMod_Ctrl);
    test.ExpectClipboard("Ctrl+C copies the well as hex text", "#3366FF");

    // ColorPicker: a click in the square, then the hue bar by keys.
    test.Reveal("pickers.picker.sv");
    test.WaitSeconds(0.4f);
    test.MouseMove(Target::Item("pickers.picker.sv", 0.95f, 0.05f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click near the top right of the square picks a bright saturated color", [] {
        const float max = ImMax(ImMax(ui.color.x, ui.color.y), ui.color.z);
        const float min = ImMin(ImMin(ui.color.x, ui.color.y), ui.color.z);
        return max > 0.9f && max - min > 0.8f;
    });
    test.ExpectFocused("pickers.picker.sv");
    test.Do([] { ui.original = ui.color; });
    test.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_DownArrow | ImGuiMod_Ctrl);
    test.Expect("Ctrl and the arrows move the square handle on both axes", [] {
        const float max = ImMax(ImMax(ui.color.x, ui.color.y), ui.color.z);
        const float min = ImMin(ImMin(ui.color.x, ui.color.y), ui.color.z);
        const float before = ImMax(ImMax(ui.original.x, ui.original.y), ui.original.z);
        return max < before && max - min < before - ImMin(ImMin(ui.original.x, ui.original.y), ui.original.z);
    });
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("pickers.picker.hue");
    test.Do([] { ui.original = ui.color; });
    test.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Ctrl);
    test.Expect("the hue bar turns the hue", [same] { return !same(ui.color, ui.original); });

    // SatValSquare on its own: the handle follows a drag.
    test.Reveal("pickers.square");
    test.WaitSeconds(0.4f);
    test.Drag({Target::Item("pickers.square", 0.5f, 0.5f), Target::Item("pickers.square", 0.1f, 0.9f)}, 0.25f);
    test.Expect("a drag to the lower left of the square gives a dark, pale color", [] {
        return ImMax(ImMax(ui.color.x, ui.color.y), ui.color.z) < 0.3f;
    });
    test.ExpectFocused("pickers.square");
    test.KeyPress(ImGuiKey_End);
    test.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Ctrl);
    test.KeyPress(ImGuiKey_UpArrow | ImGuiMod_Ctrl);
    test.Expect("End and Ctrl+Up move the square handle by keys", [] {
        return ImMax(ImMax(ui.color.x, ui.color.y), ui.color.z) > 0.25f;
    });

    // ColorChannelSlider: a click, the keys, and Tab to the next channel.
    test.Reveal("pickers.channel.red");
    test.WaitSeconds(0.4f);
    test.Click("pickers.channel.red");
    test.KeyPress(ImGuiKey_Home);
    test.Expect("Home puts the red channel at 0", [] { return ui.channels.x == 0.0f; });
    test.KeyPress(ImGuiKey_PageUp);
    test.ExpectNear("Page Up steps red by a tenth", [] { return ui.channels.x; }, 0.1, 1e-5);
    test.KeyPress(ImGuiKey_Tab);
    test.ExpectFocused("pickers.channel.green");
    test.KeyPress(ImGuiKey_End);
    test.Expect("End puts the green channel at 1", [] { return ui.channels.y == 1.0f; });
    test.Click("pickers.channel.alpha");
    test.KeyPress(ImGuiKey_Home);
    test.Expect("the alpha slider writes only alpha", [] { return ui.channels.w == 0.0f && ui.channels.y == 1.0f; });

    // ColorInputs: paste color text into a focused field.
    test.Reveal("pickers.inputs");
    test.WaitSeconds(0.4f);
    // The last field: near the bottom, where the fields are also when the menu takes a line of its own.
    test.MouseMove(Target::Item("pickers.inputs", 0.97f, 0.85f));
    test.MouseDown();
    test.MouseUp();
    test.KeyPress(ImGuiKey_Escape);
    test.SetClipboard("rgb(51, 102, 255)");
    test.KeyPress(ImGuiKey_V | ImGuiMod_Ctrl);
    test.Expect("pasted rgb() text sets the whole color", [] {
        return ui.color.x == 51.0f / 255.0f && ui.color.y == 102.0f / 255.0f && ui.color.z == 1.0f;
    });

    // SwatchPalette: a click on the first swatch, then the keys.
    test.Reveal("pickers.swatches");
    test.WaitSeconds(0.4f);
    test.MouseMove(Target::Item("pickers.swatches", 0.0f, 0.0f).Offset(8.0f, 8.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("a click picks the first swatch", [same] { return same(ui.color, facet::AccentPresets()[0].color); });
    test.Expect("the picked color goes into the recent row", [] { return ui.history.count == 1; });
    test.KeyPress(ImGuiKey_Home);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Down and Right move to the second swatch and Enter picks it",
                [same] { return same(ui.color, facet::AccentPresets()[1].color); });
    test.Expect("the recent row has both colors", [] { return ui.history.count == 2; });

    // The editable palette: the + swatch adds the color, Delete removes it.
    test.Reveal("pickers.editable");
    test.WaitSeconds(0.4f);
    test.MouseMove(Target::Item("pickers.editable", 0.0f, 0.0f)
                       .Offset((facet::T().metrics.controlHeight + facet::T().spacing.xs) * 2.0f + 8.0f, 8.0f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("the + swatch adds the current color", [] { return ui.editable.size() == 3; });
    test.KeyPress(ImGuiKey_Delete);
    test.Expect("Delete removes the focused swatch", [] { return ui.editable.size() == 2; });

    // ColorModeEditor: the animation by keys, then the period field by Tab.
    test.Reveal("pickers.animated");
    test.WaitSeconds(0.4f);
    test.MouseMove(Target::Item("pickers.animated", 0.4f, 0.0f).Offset(0.0f, 10.0f));
    test.MouseDown();
    test.MouseUp();
    for (int i = 0; i < 3; ++i)
        test.KeyPress(ImGuiKey_LeftArrow);
    test.Expect("Left moves to the static animation", [] { return ui.animated.animation == facet::ColorAnimation::Static; });
    for (int i = 0; i < 3; ++i)
        test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("Right moves to the gradient sweep", [] { return ui.animated.animation == facet::ColorAnimation::Gradient; });
}
