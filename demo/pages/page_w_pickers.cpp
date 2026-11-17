// Demo page: Pickers. Calls every widget of facet/widgets/pickers.h once, grouped by purpose.
// The widgets are planned, so each call draws its placeholder at the widget's default size;
// Phase 2 replaces them one by one without touching this page's layout.

#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "pages/pages.h"

namespace
{

using facet::demo::LabeledRow;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

struct PickersUi
{
    // Picked colors are data, not theme colors; they start at the theme's accent.
    bool seeded = false;
    ImVec4 color = {};
    ImVec4 original = {};
    facet::ColorHistory history;
    facet::AnimatedColor animated;
    facet::Date date;
    facet::Date start;
    facet::Date end;
    facet::TimeOfDay time = {.hour = 12};
    float angle = 0.0f;
    facet::Icon icon = facet::Icon("star");
    int thumbnail = 0;
    std::string path;
};

PickersUi ui;

// Accent presets shown as swatches at most.
constexpr int kMaxSwatches = 8;

const facet::ChoiceItem kIcons[] = {
    {"Grid", facet::Icon("grid"), nullptr, "Layout"},
    {"List", facet::Icon("list"), nullptr, "Layout"},
    {"Sun", facet::Icon("sun"), nullptr, "Theme"},
    {"Moon", facet::Icon("moon"), nullptr, "Theme"},
};
const facet::ThumbnailItem kThumbnails[] = {
    {"Option 1", {}, facet::Icon("image")},
    {"Option 2", {}, facet::Icon("image")},
    {"Option 3", {}, facet::Icon("image"), true},
};
const facet::ChoiceItem kFileTypes[] = {
    {"Images", {}, "*.png;*.jpg"},
    {"All files", {}, "*.*"},
};

} // namespace

FACET_DEMO_PAGE("pickers", "Pickers", "Widgets", 50)
{
    facet::demo::PageHeader("Pickers",
                            "Color wells, pickers and inputs, swatch palettes, date and time pickers, angle, "
                            "icon and thumbnail pickers, and the file browser. Every widget on this page is a "
                            "placeholder until it is implemented: it has the widget's default size, cannot be "
                            "operated and writes nothing.");

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

    SectionHeader("Colors");
    LabeledRow("Color well");
    facet::ColorWell("##well", &ui.color, {.showHex = true, .swatches = palette});
    LabeledRow("Color inputs");
    facet::ColorInputs("##inputs", &ui.color);
    LabeledRow("Swatches");
    facet::SwatchPalette("##swatches", &ui.color, palette, &ui.history);
    LabeledRow("Channel");
    facet::ColorChannelSlider("##channel", &ui.color, {.channel = facet::ColorChannel::Hue});
    LabeledRow("Animated color");
    facet::ColorModeEditor("##animated", &ui.animated);
    Paragraph("The full picker and its saturation and value square on their own.");
    facet::ColorPicker("Color picker", &ui.color, {.original = &ui.original, .swatches = palette});
    ImGui::Spacing();
    facet::SatValSquare("Saturation and value", &ui.color);

    SectionHeader("Dates and times");
    LabeledRow("Date");
    facet::DatePicker("##date", &ui.date);
    LabeledRow("Date range");
    facet::DateRangePicker("##range", &ui.start, &ui.end);
    LabeledRow("Time");
    facet::TimePicker("##time", &ui.time);

    SectionHeader("Other pickers");
    LabeledRow("Angle");
    facet::AnglePicker("##angle", &ui.angle);
    LabeledRow("Icon");
    facet::IconPicker("##icon", &ui.icon, kIcons);
    LabeledRow("Thumbnails");
    facet::ThumbnailPicker("##thumbnails", &ui.thumbnail, kThumbnails);

    SectionHeader("Files");
    Paragraph("A browser drawn inside the app, for hosts that cannot show a system dialog.");
    facet::FileBrowser("Browse", &ui.path, {.fileTypes = kFileTypes});
}
