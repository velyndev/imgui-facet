// Demo page: Other pickers. One section per widget of facet/widgets/pickers.h that is not a color
// picker: date, date range and time pickers, the angle, icon and thumbnail pickers and the file
// browser, with their important variants and states, capture states for the open and focused
// looks, and a selftest that operates every ready widget by mouse and keyboard. A widget that is
// still planned draws its placeholder. The color pickers have a page of their own
// (page_w_pickers_color.cpp). Sample values are neutral: fixed dates and times, built-in icons,
// "Option n" entries.

#include <cmath>
#include <cstdio>
#include <string>

#include <imgui.h>

#include <facet/facet.h>

#include "gallery.h"
#include "internal/popup_host.h"
#include "pages/pages.h"
#include "widgets/pickers/date_picker_parts.h"

namespace
{

using facet::demo::DemoWidth;
using facet::demo::ForcedItemState;
using facet::demo::LabeledRow;
using facet::demo::Note;
using facet::demo::Paragraph;
using facet::demo::SectionHeader;

// The capture states, in the order of the FACET_DEMO_PAGE line below.
enum CaptureState
{
    kDefault,
    kDateOpen,
    kDateFocused,
    kRangeOpen,
    kTimeOpen,
    kAngleFocused,
    kIconOpen,
};

// A fixed sample date, so captures show the same month on every day.
constexpr facet::Date kSampleDate{2026, 3, 14};

struct OtherPickersUi
{
    facet::Date date = kSampleDate;
    facet::Date limited = kSampleDate;
    facet::Date month{2026, 3, 1};
    facet::Date inlineDate = kSampleDate;
    facet::Date sized[3] = {kSampleDate, kSampleDate, kSampleDate};
    facet::Date empty;
    int weekStart = 0;  // 0 Monday, 1 Sunday
    int writes = 0;

    facet::Date start{2026, 3, 9};
    facet::Date end{2026, 3, 13};
    facet::Date presetStart;
    facet::Date presetEnd;
    facet::Date limitedStart;
    facet::Date limitedEnd;
    facet::TimeOfDay time = {.hour = 12};
    facet::TimeOfDay time12 = {.hour = 18, .minute = 45};
    facet::TimeOfDay timeSeconds = {.hour = 8, .minute = 30, .second = 15};
    facet::TimeOfDay timeInline = {.hour = 9, .minute = 30};
    float angle = 0.0f;
    float angleSigned = -1.5707964f;
    float angleLarge = 0.7853982f;
    facet::Icon icon = facet::Icon("star");
    facet::Icon iconSection = facet::Icon("sun");
    facet::Icon iconEmpty;
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

// The text of a date for the notes beside the pickers; `slot` (0 or 1) picks one of two buffers,
// so a note can show two dates.
const char* DateText(const facet::Date& date, int slot = 0)
{
    static char text[2][16];
    char* out = text[slot & 1];
    facet::internal::DateFormat(out, sizeof(text[0]), date, facet::DatePrecision::Day);
    return date.IsValid() ? out : "no date";
}

// Records a day cell of the calendar of the date picker submitted last (its popover, or its inline
// calendar with `inlinePanel`) as probe `name`, so the selftest clicks the day wherever the
// calendar opened.
void ProbeDay(facet::demo::PageCtx& ctx, const char* name, const facet::Date& day, bool inlinePanel = false)
{
    const ImGuiID field = ImGui::GetItemID();
    const ImGuiID panel = ImHashStr(inlinePanel ? "##inline" : "##calendar", 0, field);
    const ImRect rect = facet::internal::CalendarDayCellRect(panel, day);
    if (rect.GetWidth() > 0.0f)
        facet::demo::ProbeRect(ctx, name, facet::internal::CalendarDayCellId(panel, day), rect.Min, rect.Max);
}

void DateSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("DatePicker");
    Paragraph("A segmented date field. Left and Right pick the year, month or day; Up and Down step it; digits type "
              "it and Enter writes. A click, Space, Enter or Alt+Down opens the calendar: the arrows move by days and "
              "weeks, Page Up and Down by months, Enter or a click picks.");

    if (ctx.capture && ctx.captureState == kDateOpen)
        ctx.ForceNextItemState(ForcedItemState::Open);
    else if (ctx.capture && ctx.captureState == kDateFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    if (facet::DatePicker("Date", &ui.date, {.width = DemoWidth(260.0f)}))
        ++ui.writes;
    ctx.Probe("pickers.date");
    ProbeDay(ctx, "pickers.date.day20", facet::Date{2026, 3, 20});
    ImGui::SameLine();
    facet::demo::NoteBeside("%s", DateText(ui.date));
    if (ctx.capture && ctx.captureState == kDateOpen)
        ctx.RevealInCapture(0.2f);

    if (facet::DatePicker("Limited", &ui.limited,
                          {.min = {2026, 3, 10}, .max = {2026, 3, 20}, .width = DemoWidth(260.0f),
                           .helpText = "From 2026-03-10 to 2026-03-20"}))
        ++ui.writes;
    ctx.Probe("pickers.date.limited");
    if (facet::DatePicker("Month", &ui.month, {.precision = facet::DatePrecision::Month, .width = DemoWidth(260.0f)}))
        ++ui.writes;
    ctx.Probe("pickers.date.month");
    if (facet::DatePicker("Empty", &ui.empty, {.width = DemoWidth(260.0f)}))
        ++ui.writes;
    ctx.Probe("pickers.date.empty");

    LabeledRow("Sizes");
    {
        const facet::ControlSize sizes[] = {facet::ControlSize::Small, facet::ControlSize::Regular,
                                            facet::ControlSize::Large};
        const char* names[] = {"##small", "##regular", "##large"};
        for (int i = 0; i < 3; ++i)
        {
            if (i > 0)
                LabeledRow("");
            facet::DatePicker(names[i], &ui.sized[i], {.size = sizes[i], .width = DemoWidth(200.0f)});
        }
    }
    facet::Date fixed = kSampleDate;
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    facet::DatePicker("##disabled", &fixed, {.width = DemoWidth(200.0f)});
    ImGui::EndDisabled();
    LabeledRow("Read-only");
    facet::DatePicker("##readonly", &fixed, {.width = DemoWidth(200.0f), .readOnly = true});
    LabeledRow("Error");
    facet::DatePicker("##error", &fixed, {.width = DemoWidth(200.0f), .error = "Pick a later date"});
    LabeledRow("Mixed");
    facet::DatePicker("##mixed", &fixed, {.width = DemoWidth(200.0f), .mixed = true});

    LabeledRow("Week starts on");
    const char* const weekStarts[] = {"Monday", "Sunday"};
    facet::SegmentedControl("##weekstart", &ui.weekStart, weekStarts);
    ctx.Probe("pickers.date.weekstart");
    LabeledRow("Inline");
    const facet::Weekday first = ui.weekStart == 1 ? facet::Weekday::Sunday : facet::Weekday::Monday;
    if (facet::DatePicker("##inline", &ui.inlineDate, {.firstWeekday = first, .inlineCalendar = true}))
        ++ui.writes;
    ctx.Probe("pickers.date.inline");
    ProbeDay(ctx, "pickers.date.inline.day2", facet::Date{2026, 3, 2}, true);
    Note("Inline value: %s. Writes so far: %d.", DateText(ui.inlineDate), ui.writes);
}

// Two neutral presets: halves of the sample month.
const facet::DateRangePreset kRangePresets[] = {
    {"First half of March 2026", {2026, 3, 1}, {2026, 3, 15}},
    {"Second half of March 2026", {2026, 3, 16}, {2026, 3, 31}},
};

void RangeSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("DateRangePicker");
    Paragraph("A field that opens two months side by side. The first pick sets the start, the second the end (in "
              "order, whichever comes first); the band follows the pointer or the arrows in between. The ends of a "
              "range can be dragged. Presets write both ends at once.");
    if (ctx.capture && ctx.captureState == kRangeOpen)
        ctx.ForceNextItemState(ForcedItemState::Open);
    facet::DateRangePicker("Range", &ui.start, &ui.end, {.width = DemoWidth(320.0f)});
    ctx.Probe("pickers.range");
    ProbeDay(ctx, "pickers.range.day17", facet::Date{2026, 3, 17});
    ProbeDay(ctx, "pickers.range.day24", facet::Date{2026, 3, 24});
    ProbeDay(ctx, "pickers.range.day13", facet::Date{2026, 3, 13});
    if (ctx.capture && ctx.captureState == kRangeOpen)
        ctx.RevealInCapture(0.2f);
    facet::DateRangePicker("With presets", &ui.presetStart, &ui.presetEnd,
                           {.presets = kRangePresets, .width = DemoWidth(320.0f)});
    ctx.Probe("pickers.range.presets");
    facet::DateRangePicker("Limited", &ui.limitedStart, &ui.limitedEnd,
                           {.min = {2026, 3, 1}, .max = {2026, 4, 30}, .firstWeekday = facet::Weekday::Sunday,
                            .width = DemoWidth(320.0f), .helpText = "March and April 2026, weeks from Sunday"});
    facet::Date fixedStart{2026, 3, 9};
    facet::Date fixedEnd{2026, 3, 13};
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    facet::DateRangePicker("##disabled", &fixedStart, &fixedEnd, {.width = DemoWidth(260.0f)});
    ImGui::EndDisabled();
    LabeledRow("Read-only");
    facet::DateRangePicker("##readonly", &fixedStart, &fixedEnd, {.width = DemoWidth(260.0f), .readOnly = true});
    LabeledRow("Error");
    facet::DateRangePicker("##error", &fixedStart, &fixedEnd, {.width = DemoWidth(260.0f), .error = "Pick a longer range"});
    Note("Range: %s to %s.", DateText(ui.start, 0), DateText(ui.end, 1));
}

// The text of a time for the notes.
const char* TimeText(const facet::TimeOfDay& time)
{
    static char text[16];
    std::snprintf(text, sizeof(text), "%02d:%02d:%02d", time.hour, time.minute, time.second);
    return text;
}

void TimeSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("TimePicker");
    Paragraph("A segmented time field. Up and Down step the hour, minute or second and wrap around midnight; digits "
              "type it and Enter writes; A or P sets the half of the day on a 12-hour clock. A click, Space, Enter or "
              "Alt+Down opens scrolling columns that write at once; Enter closes them.");
    if (ctx.capture && ctx.captureState == kTimeOpen)
        ctx.ForceNextItemState(ForcedItemState::Open);
    facet::TimePicker("24 hours", &ui.time, {.minuteStep = 5, .width = DemoWidth(220.0f), .helpText = "Minutes in steps of 5"});
    ctx.Probe("pickers.time");
    if (ctx.capture && ctx.captureState == kTimeOpen)
        ctx.RevealInCapture(0.2f);
    facet::TimePicker("12 hours", &ui.time12, {.twelveHour = true, .width = DemoWidth(220.0f)});
    ctx.Probe("pickers.time.twelve");
    facet::TimePicker("Seconds", &ui.timeSeconds, {.showSeconds = true, .width = DemoWidth(220.0f)});
    ctx.Probe("pickers.time.seconds");
    facet::TimeOfDay fixed{.hour = 14, .minute = 10};
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    facet::TimePicker("##disabled", &fixed, {.width = DemoWidth(200.0f)});
    ImGui::EndDisabled();
    LabeledRow("Read-only");
    facet::TimePicker("##readonly", &fixed, {.width = DemoWidth(200.0f), .readOnly = true});
    LabeledRow("Inline");
    facet::TimePicker("##inline", &ui.timeInline, {.minuteStep = 15, .inlineColumns = true, .width = DemoWidth(200.0f)});
    ctx.Probe("pickers.time.inline");
    Note("Values: %s, %s.", TimeText(ui.time), [] {
        static char text[16];
        std::snprintf(text, sizeof(text), "%02d:%02d", ui.timeInline.hour, ui.timeInline.minute);
        return text;
    }());
}

// Records the dial of the AnglePicker submitted last (one without a visible label, so the dial
// starts at the left of the widget) as probe `name`.
void ProbeDial(facet::demo::PageCtx& ctx, const char* name, float diameterUnits = 0.0f)
{
    const ImVec2 min = ImGui::GetItemRectMin();
    const float diameter = diameterUnits > 0.0f ? std::round(facet::Px(diameterUnits))
                                                : std::round(facet::ControlHeight(facet::ControlSize::Inherit) * 2.0f);
    facet::demo::ProbeRect(ctx, name, ImGui::GetItemID(), min, ImVec2(min.x + diameter, min.y + diameter));
}

void AngleSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("AnglePicker");
    Paragraph("A dial with a needle over an angle, counter-clockwise from the right. Dragging points the needle at "
              "the pointer (Shift snaps to 15 degrees); the arrows turn it by a degree, Shift by the snap. The field "
              "beside it takes typed degrees.");
    LabeledRow("Angle", std::round(facet::ControlHeight(facet::ControlSize::Inherit) * 2.0f));
    if (ctx.capture && ctx.captureState == kAngleFocused)
        ctx.ForceNextItemState(ForcedItemState::Focused);
    facet::AnglePicker("##angle", &ui.angle);
    ctx.Probe("pickers.angle");
    ProbeDial(ctx, "pickers.angle.dial");
    if (ctx.capture && ctx.captureState == kAngleFocused)
        ctx.RevealInCapture(0.4f);
    LabeledRow("Signed", std::round(facet::ControlHeight(facet::ControlSize::Inherit) * 2.0f));
    facet::AnglePicker("##signed", &ui.angleSigned, {.signedRange = true, .helpText = "From -180 to 180 degrees"});
    ctx.Probe("pickers.angle.signed");
    LabeledRow("Large dial", std::round(facet::Px(96.0f)));
    facet::AnglePicker("##large", &ui.angleLarge, {.field = false, .diameter = 96.0f});
    ProbeDial(ctx, "pickers.angle.large", 96.0f);
    float fixed = 2.0f;
    LabeledRow("Small", facet::ControlHeight(facet::ControlSize::Small) * 2.0f);
    facet::AnglePicker("##small", &fixed, {.size = facet::ControlSize::Small});
    LabeledRow("Disabled", std::round(facet::ControlHeight(facet::ControlSize::Inherit) * 2.0f));
    ImGui::BeginDisabled();
    facet::AnglePicker("##disabled", &fixed);
    ImGui::EndDisabled();
    LabeledRow("Read-only", std::round(facet::ControlHeight(facet::ControlSize::Inherit) * 2.0f));
    facet::AnglePicker("##readonly", &fixed, {.readOnly = true});
    LabeledRow("Error", std::round(facet::ControlHeight(facet::ControlSize::Inherit) * 2.0f));
    facet::AnglePicker("##error", &fixed, {.error = "Pick an angle below 90 degrees"});
    Note("Angle: %.1f degrees.", ui.angle * 180.0f / 3.14159265f);
}

void IconSection(facet::demo::PageCtx& ctx)
{
    SectionHeader("IconPicker");
    Paragraph("A field that opens a grid of icons: the recently picked ones first, then every icon, in sections when "
              "the items have groups. Typing filters by name; Down moves from the search field into the grid, where "
              "the arrows move in two dimensions and Enter picks.");
    if (ctx.capture && ctx.captureState == kIconOpen)
        ctx.ForceNextItemState(ForcedItemState::Open);
    facet::IconPicker("Built-in icons", &ui.icon, {.width = DemoWidth(320.0f)});
    ctx.Probe("pickers.icon");
    if (ctx.capture && ctx.captureState == kIconOpen)
        ctx.RevealInCapture(0.2f);
    facet::IconPicker("Sections", &ui.iconSection, kIcons, {.searchable = false, .columns = 2, .width = DemoWidth(320.0f)});
    ctx.Probe("pickers.icon.sections");
    facet::IconPicker("No icon yet", &ui.iconEmpty, {.width = DemoWidth(320.0f)});
    facet::Icon fixed = facet::Icon("bell");
    LabeledRow("Disabled");
    ImGui::BeginDisabled();
    facet::IconPicker("##disabled", &fixed, {.width = DemoWidth(240.0f)});
    ImGui::EndDisabled();
    LabeledRow("Read-only");
    facet::IconPicker("##readonly", &fixed, {.width = DemoWidth(240.0f), .readOnly = true});
    LabeledRow("Error");
    facet::IconPicker("##error", &fixed, {.width = DemoWidth(240.0f), .error = "Pick another icon"});
    const char* name = facet::IconName(ui.icon);
    Note("Picked: %s.", name != nullptr ? name : "none");
}

} // namespace

FACET_DEMO_PAGE("pickers-date", "Other pickers", "Widgets", 52, "default", "date-open", "date-focused", "range-open",
                "time-open", "angle-focused", "icon-open")
{
    facet::demo::PageHeader("Other pickers",
                            "Date, date range and time pickers, angle, icon and thumbnail pickers, and the file "
                            "browser. The color pickers have a page of their own.");
    facet::demo::PlannedFamilyNote("pickers");

    DateSection(ctx);
    RangeSection(ctx);
    TimeSection(ctx);

    AngleSection(ctx);

    IconSection(ctx);

    SectionHeader("Other pickers");
    LabeledRow("Thumbnails");
    facet::ThumbnailPicker("##thumbnails", &ui.thumbnail, kThumbnails);

    SectionHeader("Files");
    Paragraph("A browser drawn inside the app, for hosts that cannot show a system dialog.");
    facet::FileBrowser("Browse", &ui.path, {.fileTypes = kFileTypes});
}

FACET_DEMO_SELFTEST("pickers-date")
{
    using facet::Date;
    const auto same = [](const Date& a, const Date& b) { return a.year == b.year && a.month == b.month && a.day == b.day; };
    test.Do([] { ui = {}; });

    // DatePicker by mouse: the field opens the calendar, a day writes and closes it.
    test.Reveal("pickers.date");
    test.WaitSeconds(0.4f);
    test.Click("pickers.date");
    test.WaitSeconds(0.3f);
    test.Click("pickers.date.day20");
    test.Expect("a click on a day writes it", [same] { return same(ui.date, Date{2026, 3, 20}); });
    // By keyboard: Space opens on the date, Right moves a day, Enter picks and gives the focus back.
    test.Click("pickers.date");
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectFocused("pickers.date");
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Right and Enter in the calendar pick the next day", [same] { return same(ui.date, Date{2026, 3, 21}); });
    test.ExpectFocused("pickers.date");
    test.KeyPress(ImGuiKey_UpArrow);
    test.Expect("Up steps the day segment", [same] { return same(ui.date, Date{2026, 3, 22}); });
    test.KeyPress(ImGuiKey_LeftArrow);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Type("2025");
    test.Type("12");
    test.Type("25");
    test.Expect("typed parts wait for Enter", [same] { return same(ui.date, Date{2026, 3, 22}); });
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter writes the typed date", [same] { return same(ui.date, Date{2025, 12, 25}); });
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_PageDown);
    test.KeyPress(ImGuiKey_Escape);
    test.Expect("Escape closes the calendar without writing", [same] { return same(ui.date, Date{2025, 12, 25}); });
    test.ExpectFocused("pickers.date");

    // The inline calendar picks in place.
    test.Click("pickers.date.inline.day2");
    test.Expect("a click on an inline day writes it", [same] { return same(ui.inlineDate, Date{2026, 3, 2}); });
    test.MouseMove(facet::demo::Target::Item("pickers.date.weekstart", 0.8f));
    test.MouseDown();
    test.MouseUp();
    test.Expect("the week start control switches to Sunday", [] { return ui.weekStart == 1; });

    // DateRangePicker by mouse: two picks in reverse order make the range in order.
    test.Reveal("pickers.range");
    test.WaitSeconds(0.4f);
    test.Click("pickers.range");
    test.WaitSeconds(0.3f);
    test.Click("pickers.range.day24");
    test.Expect("the first pick sets the start and clears the end",
                [same] { return same(ui.start, Date{2026, 3, 24}) && !ui.end.IsValid(); });
    test.Click("pickers.range.day17");
    test.Expect("the second pick before the first completes the range in order",
                [same] { return same(ui.start, Date{2026, 3, 17}) && same(ui.end, Date{2026, 3, 24}); });
    // Dragging the end of the range moves it.
    test.Click("pickers.range");
    test.WaitSeconds(0.3f);
    test.Drag({facet::demo::Target::Item("pickers.range.day24"), facet::demo::Target::Item("pickers.range.day13")}, 0.3f);
    test.Expect("the end dragged before the start swaps the ends",
                [same] { return same(ui.start, Date{2026, 3, 13}) && same(ui.end, Date{2026, 3, 17}); });
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectFocused("pickers.range");
    // By keyboard: Space opens on the start, Enter picks it, a week on, Enter ends the range.
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Enter, Down and Enter pick a week by keyboard",
                [same] { return same(ui.start, Date{2026, 3, 14}) && same(ui.end, Date{2026, 3, 21}); });
    test.ExpectFocused("pickers.range");

    // TimePicker by keyboard: the hour steps around midnight, the columns write at once.
    test.Reveal("pickers.time");
    test.WaitSeconds(0.4f);
    test.Do([] { ui.time = {.hour = 23, .minute = 55}; });
    test.Click("pickers.time");
    test.KeyPress(ImGuiKey_Escape);
    test.ExpectFocused("pickers.time");
    test.KeyPress(ImGuiKey_LeftArrow);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.KeyPress(ImGuiKey_UpArrow);
    test.Expect("Up on the hour wraps from 23 to 0", [] { return ui.time.hour == 0 && ui.time.minute == 55; });
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_UpArrow);
    test.Expect("Up on the minute carries into the hour", [] { return ui.time.hour == 1 && ui.time.minute == 0; });
    test.KeyPress(ImGuiKey_LeftArrow);
    test.Type("0930");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("typed digits are written on Enter", [] { return ui.time.hour == 9 && ui.time.minute == 30; });
    test.ExpectFocused("pickers.time");
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);

    test.KeyPress(ImGuiKey_DownArrow);
    test.ExpectNear("Down in the hour column writes at once", [] { return ui.time.hour * 100 + ui.time.minute; }, 1030.0, 0.0);
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectFocused("pickers.time");
    // By mouse: a vertical drag on the minute steps it.
    test.Drag({facet::demo::Target::Item("pickers.time", 0.12f), facet::demo::Target::Item("pickers.time", 0.12f).Offset(0.0f, -24.0f)},
              0.3f);
    test.ExpectNear("a drag up on the field steps the focused segment up",
                    [] { return ui.time.hour * 100 + ui.time.minute; }, 1045.0, 0.0);
    test.Click("pickers.time.twelve");
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_Escape);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_RightArrow);
    test.Type("a");
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("A on a 12-hour clock moves the time before noon", [] { return ui.time12.hour == 6 && ui.time12.minute == 45; });

    // AnglePicker: a press points the needle, the arrows turn it, the field takes typed degrees.
    using T = facet::demo::Target;
    const auto degrees = [](float radians) { return static_cast<double>(radians) * 180.0 / 3.14159265358979; };
    test.Reveal("pickers.angle");
    test.WaitSeconds(0.4f);
    test.MouseMove(T::Item("pickers.angle.dial", 0.5f, 0.02f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("a press at the top of the dial points to 90 degrees", [degrees] { return degrees(ui.angle); }, 90.0, 0.01);
    test.Drag({T::Item("pickers.angle.dial", 0.5f, 0.02f), T::Item("pickers.angle.dial", 0.02f, 0.5f)}, 0.3f);
    test.ExpectNear("a drag to the left edge points to 180 degrees", [degrees] { return degrees(ui.angle); }, 180.0, 0.01);
    test.KeyPress(ImGuiKey_UpArrow);
    test.ExpectNear("Up turns one degree counter-clockwise", [degrees] { return degrees(ui.angle); }, 181.0, 0.01);
    test.KeyPress(ImGuiKey_LeftArrow | ImGuiMod_Shift);
    test.ExpectNear("Shift+Left turns back by the snap", [degrees] { return degrees(ui.angle); }, 166.0, 0.01);
    test.KeyPress(ImGuiKey_Tab);
    test.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    test.Type("30");
    test.KeyPress(ImGuiKey_Enter);
    test.ExpectNear("the degrees field writes typed degrees", [degrees] { return degrees(ui.angle); }, 30.0, 0.01);
    test.MouseMove(T::Item("pickers.angle.large", 0.98f, 0.5f));
    test.MouseDown();
    test.MouseUp();
    test.ExpectNear("the large dial points at the pointer", [degrees] { return degrees(ui.angleLarge); }, 0.0, 0.01);

    // IconPicker: typing filters, Down enters the grid, Enter picks; the arrows move in two dimensions.
    test.Reveal("pickers.icon");
    test.WaitSeconds(0.4f);
    test.Click("pickers.icon");
    test.WaitSeconds(0.3f);
    test.Type("moo");
    test.Wait(2);
    test.KeyPress(ImGuiKey_DownArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("typing, Down and Enter pick the matching icon", [] { return ui.icon == facet::Icon("moon"); });
    test.ExpectFocused("pickers.icon");
    test.Click("pickers.icon.sections");
    test.KeyPress(ImGuiKey_Escape);
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_RightArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Right from Sun picks Moon in the next column", [] { return ui.iconSection == facet::Icon("moon"); });
    test.KeyPress(ImGuiKey_Space);
    test.WaitSeconds(0.3f);
    test.KeyPress(ImGuiKey_UpArrow);
    test.KeyPress(ImGuiKey_LeftArrow);
    test.KeyPress(ImGuiKey_Enter);
    test.Expect("Up past the section header and Left reach Grid", [] { return ui.iconSection == facet::Icon("grid"); });

    // The planned pickers are placeholders: clicks and keys write nothing.

}
