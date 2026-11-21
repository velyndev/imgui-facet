// Demo page: Other pickers. Calls every widget of facet/widgets/pickers.h that is not a color
// picker once: date, date range and time pickers, the angle, icon and thumbnail pickers and the
// file browser. The widgets are planned, so each call draws its placeholder at the widget's default
// size; Phase 2 replaces them one by one. The color pickers have a page of their own
// (page_w_pickers_color.cpp).

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

struct OtherPickersUi
{
    facet::Date date;
    facet::Date start;
    facet::Date end;
    facet::TimeOfDay time = {.hour = 12};
    float angle = 0.0f;
    facet::Icon icon = facet::Icon("star");
    int thumbnail = 0;
    std::string path;
};

OtherPickersUi ui;

// Placeholder entries, labelled as such.
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
const facet::FileType kFileTypes[] = {
    {"Images", "*.png;*.jpg"},
    {"All files"},
};

} // namespace

FACET_DEMO_PAGE("pickers-date", "Other pickers", "Widgets", 52)
{
    facet::demo::PageHeader("Other pickers",
                            "Date, date range and time pickers, angle, icon and thumbnail pickers, and the file "
                            "browser. The color pickers have a page of their own.");
    facet::demo::PlannedFamilyNote("pickers");

    SectionHeader("Dates and times");
    LabeledRow("Date");
    facet::DatePicker("##date", &ui.date);
    ctx.Probe("pickers.date");
    LabeledRow("Date range");
    facet::DateRangePicker("##range", &ui.start, &ui.end);
    LabeledRow("Time");
    facet::TimePicker("##time", &ui.time);
    ctx.Probe("pickers.time");

    SectionHeader("Other pickers");
    LabeledRow("Angle");
    facet::AnglePicker("##angle", &ui.angle);
    ctx.Probe("pickers.angle");
    LabeledRow("Icon");
    facet::IconPicker("##icon", &ui.icon, kIcons);
    LabeledRow("Thumbnails");
    facet::ThumbnailPicker("##thumbnails", &ui.thumbnail, kThumbnails);

    SectionHeader("Files");
    Paragraph("A browser drawn inside the app, for hosts that cannot show a system dialog.");
    facet::FileBrowser("Browse", &ui.path, {.fileTypes = kFileTypes});
}

FACET_DEMO_SELFTEST("pickers-date")
{
    test.Do([] { ui = {}; });
    // The planned pickers are placeholders: clicks and keys write nothing.
    test.Click("pickers.date");
    test.KeyPress(ImGuiKey_UpArrow);
    test.Expect("the date placeholder writes nothing", [] { return !ui.date.IsValid(); });
    test.Click("pickers.time");
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("the time placeholder writes nothing", [] { return ui.time.hour; }, 12.0, 0.0);
    test.Click("pickers.angle");
    test.KeyPress(ImGuiKey_RightArrow);
    test.ExpectNear("the angle placeholder writes nothing", [] { return ui.angle; }, 0.0, 0.0);
}
