// Demo page: Color pickers. Calls every color widget of facet/widgets/pickers.h once: the color
// well, inputs, swatch palette, channel slider, the animated color editor, the full picker and its
// saturation and value square. The widgets are planned, so each call draws its placeholder at the
// widget's default size; Phase 2 replaces them one by one. The date, time and other pickers have a
// page of their own (page_w_pickers_date.cpp).

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

struct ColorPickersUi
{
    // Picked colors are data, not theme colors; they start at the theme's accent.
    bool seeded = false;
    ImVec4 color = {};
    ImVec4 original = {};
    facet::ColorHistory history;
    facet::AnimatedColor animated;
};

ColorPickersUi ui;

// Accent presets shown as swatches at most.
constexpr int kMaxSwatches = 8;

} // namespace

FACET_DEMO_PAGE("pickers-color", "Color pickers", "Widgets", 50)
{
    facet::demo::PageHeader("Color pickers",
                            "Color wells, inputs and swatch palettes, a channel slider, an editor for colors that "
                            "pulse or cycle, and the full picker with its saturation and value square.");
    facet::demo::PlannedFamilyNote("pickers");

    if (!ui.seeded)
    {
        ui.color = facet::T().colors.accent;
        ui.original = ui.color;
        ui.animated.color = ui.color;
        ui.seeded = true;
    }
    // Swatches offered by the palette: the accent presets of the theme.
    ImVec4 swatches[kMaxSwatches] = {};
    int swatchCount = 0;
    for (const facet::AccentPreset& preset : facet::AccentPresets())
    {
        if (swatchCount < kMaxSwatches)
            swatches[swatchCount++] = preset.color;
    }
    const std::span<const ImVec4> palette(swatches, static_cast<std::size_t>(swatchCount));

    SectionHeader("Fields");
    LabeledRow("Color well");
    facet::ColorWell("##well", &ui.color, {.showHex = true, .swatches = palette});
    ctx.Probe("pickers.well");
    LabeledRow("Color inputs");
    facet::ColorInputs("##inputs", &ui.color);
    LabeledRow("Swatches");
    facet::SwatchPalette("##swatches", &ui.color, palette, &ui.history);
    LabeledRow("Channel");
    facet::ColorChannelSlider("##channel", &ui.color, {.channel = facet::ColorChannel::Hue});
    ctx.Probe("pickers.channel");
    LabeledRow("Animated color");
    facet::ColorModeEditor("##animated", &ui.animated);

    SectionHeader("Panels");
    Paragraph("The full picker and its saturation and value square on their own.");
    facet::ColorPicker("Color picker", &ui.color, {.original = &ui.original, .swatches = palette});
    ImGui::Spacing();
    facet::SatValSquare("Saturation and value", &ui.color);
}

FACET_DEMO_SELFTEST("pickers-color")
{
    test.Do([] { ui = {}; });
    // The planned pickers are placeholders: clicks and keys write nothing.
    test.Click("pickers.well");
    test.KeyPress(ImGuiKey_Enter);
    test.Click("pickers.channel");
    test.KeyPress(ImGuiKey_RightArrow);
    test.Expect("the color placeholders write nothing", [] {
        const ImVec4& c = ui.color;
        const ImVec4& o = ui.original;
        return c.x == o.x && c.y == o.y && c.z == o.z && c.w == o.w;
    });
}
