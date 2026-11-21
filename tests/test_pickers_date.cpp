// Behaviour of the date, time and other pickers (DatePicker, DateRangePicker, TimePicker,
// AnglePicker, IconPicker, ThumbnailPicker and FileBrowser in facet/widgets/pickers.h).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_pickers_date.cpp. Popup levels and calendar cells are read inside the frame (the
// popup host and the state store answer there), so the ui code of a test records them.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <thread>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/popup_host.h"
#include "support/session.h"
#include "widgets/pickers/date_picker_parts.h"
#include "widgets/pickers/file_browser_parts.h"
#include "widgets/pickers/icon_picker_parts.h"
#include "internal/list_view.h"

namespace
{

namespace fi = facet::internal;
using facet::Date;

// The calendar of a DatePicker as the last frame left it.
struct CalendarProbe
{
    ImGuiID field = 0;
    ImGuiID level = 0;
    bool open = false;
    Date cursor;
    Date shown;

    // Call right after the picker `label`, inside the ui code.
    void Record(const char* label)
    {
        field = ImGui::GetID(label);
        level = ImHashStr("##calendar", 0, field);
        open = fi::IsPopupLevelOpen(level);
        cursor = fi::CalendarPanelCursor(level);
        shown = fi::CalendarPanelShownMonth(level);
    }
    ImGuiID Cell(const Date& date) const { return fi::CalendarDayCellId(level, date); }
};

// The rect of a day cell of panel `panel`, read inside the ui code.
struct CellProbe
{
    ImRect rect;
    void Record(ImGuiID panel, const Date& date) { rect = fi::CalendarDayCellRect(panel, date); }
};

bool SameDate(const Date& a, const Date& b)
{
    return a.year == b.year && a.month == b.month && a.day == b.day;
}

} // namespace

// ---- Calendar arithmetic -------------------------------------------------------------------------

TEST_CASE("Calendar arithmetic knows leap years, month lengths and weekdays")
{
    CHECK(fi::DateIsLeapYear(2024));
    CHECK(fi::DateIsLeapYear(2000));
    CHECK_FALSE(fi::DateIsLeapYear(1900));
    CHECK_FALSE(fi::DateIsLeapYear(2023));
    CHECK(fi::DateMonthLength(2024, 2) == 29);
    CHECK(fi::DateMonthLength(2023, 2) == 28);
    CHECK(fi::DateMonthLength(1900, 2) == 28);
    CHECK(fi::DateMonthLength(2026, 4) == 30);
    CHECK(fi::DateMonthLength(2026, 12) == 31);
    CHECK(fi::DateMonthLength(2026, 13) == 0);
    CHECK(fi::DateIsReal(Date{2024, 2, 29}));
    CHECK_FALSE(fi::DateIsReal(Date{2023, 2, 29}));
    CHECK_FALSE(fi::DateIsReal(Date{2026, 2, 31}));
    CHECK_FALSE(fi::DateIsReal(Date{0, 1, 1}));
    CHECK_FALSE(fi::DateIsReal(Date{10000, 1, 1}));

    CHECK(fi::DateWeekdayOf(Date{1, 1, 1}) == facet::Weekday::Monday);
    CHECK(fi::DateWeekdayOf(Date{2000, 1, 1}) == facet::Weekday::Saturday);
    CHECK(fi::DateWeekdayOf(Date{2026, 11, 21}) == facet::Weekday::Saturday);
    CHECK(fi::DateWeekdayOf(Date{2024, 2, 29}) == facet::Weekday::Thursday);
    // November 2026 starts on a Sunday: the last column of a Monday week, the first of a Sunday week.
    CHECK(fi::DateMonthStartColumn(2026, 11, facet::Weekday::Monday) == 6);
    CHECK(fi::DateMonthStartColumn(2026, 11, facet::Weekday::Sunday) == 0);
    CHECK(fi::DateMonthStartColumn(2026, 11, facet::Weekday::Saturday) == 1);

    // Serials run without gaps over the whole range.
    for (int serial = 0; serial <= 3652058; serial += 97)
    {
        const Date date = fi::DateFromSerial(serial);
        REQUIRE(fi::DateIsReal(date));
        REQUIRE(fi::DateSerial(date) == serial);
    }
    CHECK(SameDate(fi::DateFromSerial(3652058), Date{9999, 12, 31}));
}

TEST_CASE("Adding days and months crosses month, year and leap day borders")
{
    CHECK(SameDate(fi::DateAddDays(Date{2024, 2, 28}, 1), Date{2024, 2, 29}));
    CHECK(SameDate(fi::DateAddDays(Date{2024, 2, 28}, 2), Date{2024, 3, 1}));
    CHECK(SameDate(fi::DateAddDays(Date{2023, 2, 28}, 1), Date{2023, 3, 1}));
    CHECK(SameDate(fi::DateAddDays(Date{2026, 12, 31}, 1), Date{2027, 1, 1}));
    CHECK(SameDate(fi::DateAddDays(Date{2027, 1, 1}, -1), Date{2026, 12, 31}));
    CHECK(SameDate(fi::DateAddDays(Date{9999, 12, 31}, 5), Date{9999, 12, 31}));
    CHECK(SameDate(fi::DateAddDays(Date{1, 1, 1}, -5), Date{1, 1, 1}));
    CHECK(SameDate(fi::DateAddMonths(Date{2024, 1, 31}, 1), Date{2024, 2, 29}));
    CHECK(SameDate(fi::DateAddMonths(Date{2023, 1, 31}, 1), Date{2023, 2, 28}));
    CHECK(SameDate(fi::DateAddMonths(Date{2026, 12, 15}, 1), Date{2027, 1, 15}));
    CHECK(SameDate(fi::DateAddMonths(Date{2026, 1, 15}, -1), Date{2025, 12, 15}));
    CHECK(SameDate(fi::DateAddMonths(Date{2024, 2, 29}, 12), Date{2025, 2, 28}));
    CHECK(fi::DateCompare(Date{2026, 1, 2}, Date{2026, 1, 3}) < 0);
    CHECK(fi::DateCompare(Date{2026, 2, 1}, Date{2026, 1, 31}) > 0);
    CHECK(SameDate(fi::DateClampTo(Date{2026, 1, 1}, Date{2026, 3, 10}, Date{}), Date{2026, 3, 10}));
    CHECK(SameDate(fi::DateClampTo(Date{2026, 9, 1}, Date{}, Date{2026, 3, 20}), Date{2026, 3, 20}));
    CHECK(fi::DateWithin(Date{2026, 3, 15}, Date{}, Date{}));

    char text[16];
    fi::DateFormat(text, sizeof(text), Date{2026, 3, 4}, facet::DatePrecision::Day);
    CHECK(std::string(text) == "2026-03-04");
    fi::DateFormat(text, sizeof(text), Date{2026, 3, 4}, facet::DatePrecision::Month);
    CHECK(std::string(text) == "2026-03");
    fi::DateFormat(text, sizeof(text), Date{}, facet::DatePrecision::Day);
    CHECK(std::string(text).empty());
}

// ---- DatePicker ---------------------------------------------------------------------------------

TEST_CASE("A date picker opens its calendar on a click and a click on a day writes it and closes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date date{2026, 3, 14};
    int writes = 0;
    bool edited = false;
    CalendarProbe calendar;
    CellProbe day20;
    session.SetUi([&] {
        if (facet::DatePicker("Date", &date))
            ++writes;
        edited = edited || ImGui::IsItemEdited();
        session.Track("date");
        calendar.Record("Date");
        day20.Record(calendar.level, Date{2026, 3, 20});
    });
    session.Frame();
    session.Click(session.ItemCenter("date"));
    session.Frames(2);
    REQUIRE(calendar.open);
    CHECK(SameDate(calendar.shown, Date{2026, 3, 1}));
    REQUIRE(day20.rect.GetWidth() > 1.0f);
    CHECK(day20.rect.Min.y >= session.ItemRect("date").Max.y);
    session.Click(day20.rect.GetCenter());
    CHECK(SameDate(date, Date{2026, 3, 20}));
    CHECK(writes == 1);
    CHECK(edited);
    CHECK_FALSE(calendar.open);
}

TEST_CASE("The calendar cursor moves with the keys across month and year borders and Enter picks")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date date{2026, 12, 31};
    CalendarProbe calendar;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::DatePicker("Date", &date);
        session.Track("date");
        calendar.Record("Date");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("date"));
    session.KeyPress(ImGuiKey_Space);
    session.Frames(2);
    REQUIRE(calendar.open);
    CHECK(ImGui::GetFocusID() == calendar.Cell(Date{2026, 12, 31}));

    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(SameDate(calendar.cursor, Date{2027, 1, 1}));
    CHECK(SameDate(calendar.shown, Date{2027, 1, 1}));
    CHECK(ImGui::GetFocusID() == calendar.Cell(Date{2027, 1, 1}));
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(SameDate(calendar.cursor, Date{2026, 12, 31}));
    CHECK(SameDate(calendar.shown, Date{2026, 12, 1}));
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(SameDate(calendar.cursor, Date{2027, 1, 7}));
    session.KeyPress(ImGuiKey_PageUp);
    CHECK(SameDate(calendar.cursor, Date{2026, 12, 7}));
    session.KeyPress(ImGuiKey_PageDown | ImGuiMod_Shift);
    CHECK(SameDate(calendar.cursor, Date{2027, 12, 7}));
    // 7 December 2027 is a Tuesday: Home goes to Monday the 6th, End to Sunday the 12th.
    session.KeyPress(ImGuiKey_Home);
    CHECK(SameDate(calendar.cursor, Date{2027, 12, 6}));
    session.KeyPress(ImGuiKey_End);
    CHECK(SameDate(calendar.cursor, Date{2027, 12, 12}));
    CHECK(SameDate(date, Date{2026, 12, 31}));  // moving writes nothing

    session.KeyPress(ImGuiKey_Enter);
    CHECK(SameDate(date, Date{2027, 12, 12}));
    CHECK_FALSE(calendar.open);
    CHECK(ImGui::GetFocusID() == session.ItemId("date"));
}

TEST_CASE("The calendar cursor passes the leap day and Page Down keeps the day inside the month")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date date{2024, 2, 28};
    CalendarProbe calendar;
    session.SetUi([&] {
        facet::DatePicker("Date", &date);
        session.Track("date");
        calendar.Record("Date");
    });
    session.Frame();
    session.Click(session.ItemCenter("date"));
    session.Frames(2);
    REQUIRE(calendar.open);
    fi::SetKeyboardModality(true);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(SameDate(calendar.cursor, Date{2024, 2, 29}));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(SameDate(calendar.cursor, Date{2024, 3, 1}));
    session.KeyPress(ImGuiKey_PageUp);
    session.KeyPress(ImGuiKey_PageUp);
    CHECK(SameDate(calendar.cursor, Date{2024, 1, 1}));
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(SameDate(calendar.cursor, Date{2023, 12, 31}));
    session.KeyPress(ImGuiKey_PageDown);
    session.KeyPress(ImGuiKey_PageDown);
    CHECK(SameDate(calendar.cursor, Date{2024, 2, 29}));
    session.KeyPress(ImGuiKey_PageDown | ImGuiMod_Shift);
    CHECK(SameDate(calendar.cursor, Date{2025, 2, 28}));
}

TEST_CASE("Days outside the minimum and maximum cannot be picked or reached with the keys")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date date{2026, 3, 14};
    CalendarProbe calendar;
    CellProbe early;
    session.SetUi([&] {
        facet::DatePicker("Date", &date, {.min = {2026, 3, 10}, .max = {2026, 3, 20}});
        session.Track("date");
        calendar.Record("Date");
        early.Record(calendar.level, Date{2026, 3, 5});
    });
    session.Frame();
    session.Click(session.ItemCenter("date"));
    session.Frames(2);
    REQUIRE(calendar.open);
    session.Click(early.rect.GetCenter());
    CHECK(SameDate(date, Date{2026, 3, 14}));
    CHECK(calendar.open);
    // The cursor stops at the ends; Page Up cannot leave the allowed days.
    fi::SetKeyboardModality(true);
    session.KeyPress(ImGuiKey_PageUp);
    CHECK(SameDate(calendar.cursor, Date{2026, 3, 10}));
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(SameDate(calendar.cursor, Date{2026, 3, 10}));
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(SameDate(calendar.cursor, Date{2026, 3, 20}));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(SameDate(date, Date{2026, 3, 20}));
}

TEST_CASE("The week start option moves the first day of the month to its column")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date date{2026, 11, 21};
    CellProbe mondayFirst;
    CellProbe sundayFirst;
    CellProbe mondaySecond;
    session.SetUi([&] {
        facet::DatePicker("##monday", &date, {.inlineCalendar = true});
        const ImGuiID monday = ImHashStr("##inline", 0, ImGui::GetID("##monday"));
        mondayFirst.Record(monday, Date{2026, 11, 1});
        mondaySecond.Record(monday, Date{2026, 11, 2});
        facet::DatePicker("##sunday", &date, {.firstWeekday = facet::Weekday::Sunday, .inlineCalendar = true});
        sundayFirst.Record(ImHashStr("##inline", 0, ImGui::GetID("##sunday")), Date{2026, 11, 1});
    });
    session.Frames(2);
    const float cell = mondayFirst.rect.GetWidth();
    REQUIRE(cell > 1.0f);
    // Monday weeks: the 1st (a Sunday) is the last column of the first row, the 2nd starts row two.
    CHECK(mondaySecond.rect.Min.x == doctest::Approx(mondayFirst.rect.Min.x - 6.0f * cell));
    CHECK(mondaySecond.rect.Min.y == doctest::Approx(mondayFirst.rect.Max.y));
    // Sunday weeks: the 1st is the first column.
    CHECK(sundayFirst.rect.Min.x == doctest::Approx(mondaySecond.rect.Min.x));
}

TEST_CASE("Typed date parts are written on Enter, and an invalid date waits for its correction")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date date{2026, 3, 14};
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::DatePicker("Date", &date))
            ++writes;
        session.Track("date");
        ImGui::Button("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("date"));
    // The last segment (day) is focused first: Left twice reaches the year.
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.Type("2023");
    session.Type("02");
    session.Type("31");
    CHECK(SameDate(date, Date{2026, 3, 14}));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(SameDate(date, Date{2026, 3, 14}));  // 31 February is not written
    CHECK(writes == 0);
    session.Type("28");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(SameDate(date, Date{2023, 2, 28}));
    CHECK(writes == 1);

    // Escape drops what was typed; Tab commits.
    session.Type("5");
    session.KeyPress(ImGuiKey_Escape);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(SameDate(date, Date{2023, 2, 28}));
    session.KeyPress(ImGuiKey_Tab | ImGuiMod_Shift);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("date"));
    session.Type("9");
    session.KeyPress(ImGuiKey_Tab);
    CHECK(SameDate(date, Date{2023, 2, 9}));
    CHECK(writes == 2);
}

TEST_CASE("Up and Down step the focused date segment, wrapping inside it and keeping the date real")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date date{2024, 1, 31};
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::DatePicker("Date", &date))
            ++writes;
        session.Track("date");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("date"));
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(SameDate(date, Date{2024, 1, 1}));  // day 31 wraps to 1 in its month
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(SameDate(date, Date{2024, 1, 31}));
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(SameDate(date, Date{2024, 2, 29}));  // the day follows the length of February
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(SameDate(date, Date{2025, 2, 28}));
    CHECK(writes == 4);
    CHECK(ImGui::GetFocusID() == session.ItemId("date"));
}

TEST_CASE("A read-only date picker focuses but never opens or writes")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date date{2026, 3, 14};
    CalendarProbe calendar;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::DatePicker("Date", &date, {.readOnly = true});
        session.Track("date");
        calendar.Record("Date");
    });
    session.Frame();
    session.Click(session.ItemCenter("date"));
    CHECK_FALSE(calendar.open);
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("date"));
    session.KeyPress(ImGuiKey_UpArrow);
    session.Type("1");
    session.KeyPress(ImGuiKey_Enter);
    CHECK_FALSE(calendar.open);
    CHECK(SameDate(date, Date{2026, 3, 14}));
}

TEST_CASE("An inline calendar picks in place and fits a short row")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date date{2026, 3, 14};
    float available = 0.0f;
    CellProbe day2;
    session.SetUi([&] {
        facet::DatePicker("Date##wide", &date, {.inlineCalendar = true});
        session.Track("wide");
        day2.Record(ImHashStr("##inline", 0, ImGui::GetID("Date##wide")), Date{2026, 3, 2});
        ImGui::BeginChild("short row", ImVec2(facet::Px(40.0f), facet::Px(600.0f)));
        available = ImGui::GetContentRegionAvail().x;
        facet::DatePicker("Date", &date, {.inlineCalendar = true});
        session.Track("short");
        ImGui::EndChild();
    });
    session.Frames(2);
    REQUIRE(available > 1.0f);
    CHECK(session.ItemRect("wide").GetWidth() > available);
    CHECK(session.ItemRect("wide").GetWidth() >= facet::Px(facet::T().picker.dayCell) * 7.0f - 1.0f);
    CHECK(session.ItemRect("short").GetWidth() == doctest::Approx(available));
    session.Click(day2.rect.GetCenter());
    CHECK(SameDate(date, Date{2026, 3, 2}));
}

// ---- DateRangePicker ----------------------------------------------------------------------------

namespace
{

// The calendar of a DateRangePicker and a few of its day cells, as the last frame left them.
struct RangeProbe
{
    ImGuiID level = 0;
    bool open = false;
    Date shown;
    ImRect cells[4];

    void Record(const char* label, const Date (&days)[4])
    {
        level = ImHashStr("##calendar", 0, ImGui::GetID(label));
        open = fi::IsPopupLevelOpen(level);
        shown = fi::CalendarPanelShownMonth(level);
        for (int i = 0; i < 4; ++i)
            cells[i] = fi::CalendarDayCellRect(level, days[i]);
    }
};

} // namespace

TEST_CASE("Two picks make a range in order, and the second pick before the first swaps them")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date start{2026, 3, 2};
    Date end{2026, 3, 6};
    int writes = 0;
    RangeProbe range;
    const Date days[4] = {{2026, 3, 10}, {2026, 3, 18}, {2026, 4, 3}, {2026, 3, 5}};
    session.SetUi([&] {
        if (facet::DateRangePicker("Dates", &start, &end))
            ++writes;
        session.Track("dates");
        range.Record("Dates", days);
    });
    session.Frame();
    session.Click(session.ItemCenter("dates"));
    session.Frames(2);
    REQUIRE(range.open);
    CHECK(SameDate(range.shown, Date{2026, 3, 1}));
    // Two months side by side: April's 3rd is to the right of March.
    REQUIRE(range.cells[2].GetWidth() > 1.0f);
    CHECK(range.cells[2].Min.x > range.cells[1].Max.x);

    session.Click(range.cells[1].GetCenter());
    CHECK(SameDate(start, Date{2026, 3, 18}));
    CHECK_FALSE(end.IsValid());
    CHECK(writes == 1);
    CHECK(range.open);
    session.Click(range.cells[0].GetCenter());
    CHECK(SameDate(start, Date{2026, 3, 10}));
    CHECK(SameDate(end, Date{2026, 3, 18}));
    CHECK(writes == 2);
    CHECK_FALSE(range.open);

    // By keyboard: the arrows show the cursor, Enter on it, then three weeks on, Enter.
    session.Click(session.ItemCenter("dates"));
    session.Frames(2);
    REQUIRE(range.open);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(SameDate(start, Date{2026, 3, 10}));
    CHECK_FALSE(end.IsValid());
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(SameDate(start, Date{2026, 3, 10}));
    CHECK(SameDate(end, Date{2026, 3, 31}));
    CHECK_FALSE(range.open);
    CHECK(ImGui::GetFocusID() == session.ItemId("dates"));
}

TEST_CASE("Dragging an end of a range moves it, and past the other end the two swap")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date start{2026, 3, 10};
    Date end{2026, 3, 18};
    RangeProbe range;
    const Date days[4] = {{2026, 3, 10}, {2026, 3, 18}, {2026, 3, 25}, {2026, 3, 5}};
    session.SetUi([&] {
        facet::DateRangePicker("Dates", &start, &end);
        session.Track("dates");
        range.Record("Dates", days);
    });
    session.Frame();
    session.Click(session.ItemCenter("dates"));
    session.Frames(2);
    REQUIRE(range.open);
    session.Drag({range.cells[1].GetCenter(), range.cells[2].GetCenter()}, 6);
    CHECK(SameDate(start, Date{2026, 3, 10}));
    CHECK(SameDate(end, Date{2026, 3, 25}));
    CHECK(range.open);
    // The start dragged past the end: the end becomes the start.
    session.Drag({range.cells[0].GetCenter(), range.cells[2].GetCenter(), range.cells[3].GetCenter()}, 6);
    CHECK(SameDate(start, Date{2026, 3, 5}));
    CHECK(SameDate(end, Date{2026, 3, 25}));
}

TEST_CASE("A range preset writes both ends at once and closes the calendar")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    Date start;
    Date end;
    int writes = 0;
    bool open = false;
    ImRect presetRect;
    const facet::DateRangePreset presets[] = {{"Option 1", {2026, 1, 5}, {2026, 1, 9}},
                                              {"Option 2", {2026, 2, 20}, {2026, 2, 10}}};
    session.SetUi([&] {
        if (facet::DateRangePicker("Dates", &start, &end, {.presets = presets}))
            ++writes;
        session.Track("dates");
        const ImGuiID level = ImHashStr("##calendar", 0, ImGui::GetID("Dates"));
        open = fi::IsPopupLevelOpen(level);
        if (open)
            presetRect = fi::PopupLevelRect(level);
    });
    session.Frame();
    session.Click(session.ItemCenter("dates"));
    session.Frames(2);
    REQUIRE(open);
    // The presets column starts at the level's content: a caption line, then the buttons.
    const float pad = facet::Px(facet::T().spacing.xs);
    const float row = facet::ControlHeight(facet::ControlSize::Inherit);
    const ImVec2 second(presetRect.Min.x + pad + facet::Px(20.0f), presetRect.Min.y + pad + row * 2.5f);
    session.Click(second);
    // An end before its start is put in order.
    CHECK(SameDate(start, Date{2026, 2, 10}));
    CHECK(SameDate(end, Date{2026, 2, 20}));
    CHECK(writes == 1);
    CHECK_FALSE(open);
}

TEST_CASE("A range picker shows one month when the display has no room for two")
{
    facet::test::Session session(facet::test::SessionOptions{.displaySize = ImVec2(300.0f, 600.0f)});
    REQUIRE(session.Ready());
    Date start{2026, 3, 10};
    Date end{2026, 3, 18};
    RangeProbe range;
    const Date days[4] = {{2026, 3, 10}, {2026, 3, 18}, {2026, 4, 3}, {2026, 3, 5}};
    session.SetUi([&] {
        facet::DateRangePicker("Dates", &start, &end);
        session.Track("dates");
        range.Record("Dates", days);
    });
    session.Frame();
    session.Click(session.ItemCenter("dates"));
    session.Frames(2);
    REQUIRE(range.open);
    CHECK(range.cells[0].GetWidth() > 1.0f);
    CHECK(range.cells[2].GetWidth() == 0.0f);  // April is not shown
}
// ---- TimePicker ---------------------------------------------------------------------------------

namespace
{

using facet::TimeOfDay;

bool SameTime(const TimeOfDay& a, int hour, int minute, int second = 0)
{
    return a.hour == hour && a.minute == minute && a.second == second;
}

} // namespace

TEST_CASE("Time steps wrap around midnight, carry into the next unit and keep the minute slots")
{
    using fi::TimeSegment;
    CHECK(SameTime(fi::TimeStep({23, 0, 0}, TimeSegment::Hour, 1, 1), 0, 0));
    CHECK(SameTime(fi::TimeStep({0, 30, 0}, TimeSegment::Hour, -1, 1), 23, 30));
    CHECK(SameTime(fi::TimeStep({23, 59, 0}, TimeSegment::Minute, 1, 1), 0, 0));
    CHECK(SameTime(fi::TimeStep({0, 0, 0}, TimeSegment::Minute, -1, 1), 23, 59));
    CHECK(SameTime(fi::TimeStep({23, 59, 59}, TimeSegment::Second, 1, 1), 0, 0, 0));
    CHECK(SameTime(fi::TimeStep({10, 59, 59}, TimeSegment::Second, 1, 1), 11, 0, 0));
    CHECK(SameTime(fi::TimeStep({11, 0, 0}, TimeSegment::HalfDay, 1, 1), 23, 0));
    CHECK(SameTime(fi::TimeStep({23, 0, 0}, TimeSegment::HalfDay, 1, 1), 11, 0));
    // Minute slots of five: off the grid the first step lands on the next slot in its direction.
    CHECK(SameTime(fi::TimeStep({10, 7, 0}, TimeSegment::Minute, 1, 5), 10, 10));
    CHECK(SameTime(fi::TimeStep({10, 7, 0}, TimeSegment::Minute, 2, 5), 10, 15));
    CHECK(SameTime(fi::TimeStep({10, 7, 0}, TimeSegment::Minute, -1, 5), 10, 5));
    CHECK(SameTime(fi::TimeStep({10, 55, 0}, TimeSegment::Minute, 1, 5), 11, 0));
    CHECK(SameTime(fi::TimeStep({23, 50, 0}, TimeSegment::Minute, 1, 15), 0, 0));
    CHECK(SameTime(fi::TimeStep({0, 0, 0}, TimeSegment::Minute, -1, 15), 23, 45));
    CHECK(fi::TimeHour12(0) == 12);
    CHECK(fi::TimeHour12(12) == 12);
    CHECK(fi::TimeHour12(13) == 1);
    CHECK(fi::TimeHour12(11) == 11);
}

TEST_CASE("A time field steps its segments with Up and Down and wraps around midnight")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TimeOfDay time{23, 59, 0};
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::TimePicker("Time", &time))
            ++writes;
        session.Track("time");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("time"));
    session.KeyPress(ImGuiKey_UpArrow);  // the hour segment comes first
    CHECK(SameTime(time, 0, 59));
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(SameTime(time, 1, 0));
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(SameTime(time, 0, 58));
    CHECK(writes == 4);
}

TEST_CASE("A twelve-hour time field shows 12 at noon and midnight and switches the half of the day")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TimeOfDay time{0, 30, 0};
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::TimePicker("Time", &time, {.twelveHour = true});
        session.Track("time");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("time"));
    // Hour 12 AM, then 11 PM the day before when stepped down.
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(SameTime(time, 23, 30));
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_UpArrow);  // the AM and PM segment
    CHECK(SameTime(time, 11, 30));
    session.Type("p");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(SameTime(time, 23, 30));
    // Typed 12-hour digits: 12 typed with AM is midnight.
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.Type("12");
    session.Type("05");
    session.Type("a");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(SameTime(time, 0, 5));
}

TEST_CASE("Typed time digits are written on Enter or Tab, and an invalid hour waits")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TimeOfDay time{12, 0, 0};
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::TimePicker("Time", &time, {.showSeconds = true}))
            ++writes;
        session.Track("time");
        ImGui::Button("after");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("time"));
    session.Type("093015");
    CHECK(SameTime(time, 12, 0));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(SameTime(time, 9, 30, 15));
    CHECK(writes == 1);
    // A first digit of 3 to 9 fills the hour alone and moves on.
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    session.Type("745");
    session.KeyPress(ImGuiKey_Tab);
    CHECK(SameTime(time, 7, 45, 15));
    CHECK(writes == 2);
}

TEST_CASE("The time columns write at once and Enter closes them with the focus back on the field")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TimeOfDay time{1, 0, 0};
    bool open = false;
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::TimePicker("Time", &time, {.minuteStep = 15}))
            ++writes;
        session.Track("time");
        open = fi::IsPopupLevelOpen(ImHashStr("##columns", 0, ImGui::GetID("Time")));
    });
    session.Frame();
    session.Click(session.ItemCenter("time"));
    session.Frames(2);
    REQUIRE(open);
    session.KeyPress(ImGuiKey_Escape);
    CHECK_FALSE(open);
    CHECK(ImGui::GetFocusID() == session.ItemId("time"));
    session.KeyPress(ImGuiKey_Space);
    session.Frames(2);
    REQUIRE(open);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(SameTime(time, 2, 0));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(SameTime(time, 2, 15));
    CHECK(open);
    session.KeyPress(ImGuiKey_Enter);
    CHECK_FALSE(open);
    CHECK(SameTime(time, 2, 15));
    CHECK(ImGui::GetFocusID() == session.ItemId("time"));
    CHECK(writes == 2);
}
// ---- AnglePicker --------------------------------------------------------------------------------

namespace
{

constexpr float kPi = 3.14159265358979f;

float Degrees(float radians)
{
    return radians * 180.0f / kPi;
}

// The dial of an AnglePicker without a visible label: a square at the left of the widget, two
// control heights across.
ImRect AngleDial(const facet::test::Session& session, const char* name)
{
    const ImRect widget = session.ItemRect(name);
    const float diameter = std::round(facet::ControlHeight(facet::ControlSize::Inherit) * 2.0f);
    return ImRect(widget.Min, ImVec2(widget.Min.x + diameter, widget.Min.y + diameter));
}

} // namespace

TEST_CASE("The angle dial turns with the arrows of both axes, Shift by the snap, and wraps")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float radians = 359.0f * kPi / 180.0f;
    float signedRadians = kPi;
    int writes = 0;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::AnglePicker("##angle", &radians, {.snapDegrees = 15.0f}))
            ++writes;
        session.Track("angle");
        facet::AnglePicker("##signed", &signedRadians, {.signedRange = true, .field = false});
        session.Track("signed");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("angle"));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(Degrees(radians) == doctest::Approx(0.0f).epsilon(0.001));
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(Degrees(radians) == doctest::Approx(1.0f).epsilon(0.001));
    session.KeyPress(ImGuiKey_DownArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(Degrees(radians) == doctest::Approx(359.0f).epsilon(0.001));
    session.KeyPress(ImGuiKey_RightArrow | ImGuiMod_Shift);
    CHECK(Degrees(radians) == doctest::Approx(14.0f).epsilon(0.001));
    CHECK(writes == 5);
    CHECK(ImGui::GetFocusID() == session.ItemId("angle"));

    // A signed range writes -180 to 180: one degree past 180 is -179.
    session.Click(AngleDial(session, "signed").GetCenter());
    const float before = signedRadians;
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(Degrees(signedRadians) == doctest::Approx(-179.0f).epsilon(0.001));
    CHECK(before == kPi);
}

TEST_CASE("The angle dial points its needle at the pointer, and Escape during a drag restores it")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float radians = 1.0f;
    session.SetUi([&] {
        facet::AnglePicker("##angle", &radians, {.field = false});
        session.Track("angle");
    });
    session.Frame();
    const ImRect dial = AngleDial(session, "angle");
    // A press at the right edge points to 0 degrees, a drag to the top to 90.
    session.Click(ImVec2(dial.Max.x - 2.0f, dial.GetCenter().y));
    CHECK(Degrees(radians) == doctest::Approx(0.0f).epsilon(0.001));
    session.Drag({ImVec2(dial.Max.x - 2.0f, dial.GetCenter().y), ImVec2(dial.GetCenter().x, dial.Min.y - 20.0f)}, 4);
    CHECK(Degrees(radians) == doctest::Approx(90.0f).epsilon(0.001));
    // Left of the center is 180; Escape before the release restores the angle from the press.
    session.MouseMove(ImVec2(dial.Min.x + 2.0f, dial.GetCenter().y));
    session.MouseDown();
    session.MouseMove(ImVec2(dial.GetCenter().x, dial.Max.y + 20.0f));
    CHECK(Degrees(radians) == doctest::Approx(270.0f).epsilon(0.001));
    session.KeyPress(ImGuiKey_Escape);
    session.MouseUp();
    CHECK(Degrees(radians) == doctest::Approx(90.0f).epsilon(0.001));
}

TEST_CASE("The degrees field writes a typed angle, and a read-only angle picker writes nothing")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    float radians = 0.0f;
    float fixed = 0.5f;
    session.SetUi([&] {
        facet::AnglePicker("##angle", &radians);
        session.Track("angle");
        facet::AnglePicker("##fixed", &fixed, {.readOnly = true});
        session.Track("fixed");
    });
    session.Frame();
    const ImRect widget = session.ItemRect("angle");
    const ImRect dial = AngleDial(session, "angle");
    session.Click(ImVec2((dial.Max.x + widget.Max.x) * 0.5f, dial.GetCenter().y));
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.Type("45");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(Degrees(radians) == doctest::Approx(45.0f).epsilon(0.001));

    const ImRect fixedDial = AngleDial(session, "fixed");
    session.Click(ImVec2(fixedDial.Min.x + 2.0f, fixedDial.GetCenter().y));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(fixed == 0.5f);
}
// ---- IconPicker ---------------------------------------------------------------------------------

namespace
{

// The popover of an IconPicker as the last frame left it.
struct IconProbe
{
    ImGuiID level = 0;
    ImGuiID grid = 0;
    bool open = false;
    ImRect rect;

    void Record(const char* label)
    {
        level = ImHashStr("##icons", 0, ImGui::GetID(label));
        grid = ImHashStr("##tiles", 0, level);
        open = fi::IsPopupLevelOpen(level);
        rect = fi::PopupLevelRect(level);
    }
    // The id of the tile of item `item` (a recent tile when `recent`).
    ImGuiID Tile(int item, bool recent = false) const
    {
        const ImGuiID key = static_cast<ImGuiID>(item) * 2u + (recent ? 1u : 0u) + 1u;
        return ImHashData(&key, sizeof(key), ImHashStr("##tile", 0, grid));
    }
};

const facet::ChoiceItem kIconItems[] = {
    {"Grid", facet::Icon("grid")},  {"List", facet::Icon("list")},     {"Sun", facet::Icon("sun")},
    {"Moon", facet::Icon("moon")},  {"Star", facet::Icon("star")},     {"Heart", facet::Icon("heart")},
    {"Bell", facet::Icon("bell")},  {"Clock", facet::Icon("clock")},   {"File", facet::Icon("file")},
    {"Folder", facet::Icon("folder")},
};

} // namespace

TEST_CASE("An icon picker filters by name and Down and Enter pick the match")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Icon icon = facet::Icon("grid");
    int writes = 0;
    IconProbe popup;
    session.SetUi([&] {
        if (facet::IconPicker("Icon", &icon, kIconItems))
            ++writes;
        session.Track("icon");
        popup.Record("Icon");
    });
    session.Frame();
    session.Click(session.ItemCenter("icon"));
    session.Frames(2);
    REQUIRE(popup.open);
    session.Type("moo");
    session.Frames(2);
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(ImGui::GetFocusID() == popup.Tile(3));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(icon == facet::Icon("moon"));
    CHECK(writes == 1);
    CHECK_FALSE(popup.open);
    CHECK(ImGui::GetFocusID() == session.ItemId("icon"));
}

TEST_CASE("The icon grid moves in two dimensions and keeps the recent icons first")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Icon icon = facet::Icon("grid");
    IconProbe popup;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::IconPicker("Icon", &icon, kIconItems, {.searchable = false, .columns = 4});
        session.Track("icon");
        popup.Record("Icon");
    });
    session.Frame();
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    REQUIRE(ImGui::GetFocusID() == session.ItemId("icon"));
    session.KeyPress(ImGuiKey_Space);
    session.Frames(2);
    REQUIRE(popup.open);
    CHECK(ImGui::GetFocusID() == popup.Tile(0));
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ImGui::GetFocusID() == popup.Tile(1));
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(ImGui::GetFocusID() == popup.Tile(5));
    session.KeyPress(ImGuiKey_DownArrow);  // the last line holds 8 and 9
    CHECK(ImGui::GetFocusID() == popup.Tile(9));
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(ImGui::GetFocusID() == popup.Tile(9));
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(ImGui::GetFocusID() == popup.Tile(5));
    session.KeyPress(ImGuiKey_Home);
    CHECK(ImGui::GetFocusID() == popup.Tile(0));
    session.KeyPress(ImGuiKey_End);
    CHECK(ImGui::GetFocusID() == popup.Tile(9));
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(icon == facet::Icon("file"));
    CHECK_FALSE(popup.open);

    // Opened again: the picked icon leads under Recent, and the focus starts on the chosen tile.
    session.KeyPress(ImGuiKey_Space);
    session.Frames(2);
    REQUIRE(popup.open);
    CHECK(ImGui::GetFocusID() == popup.Tile(8));
    session.KeyPress(ImGuiKey_Home);
    CHECK(ImGui::GetFocusID() == popup.Tile(8, true));
}

TEST_CASE("An icon search without a match shows the empty state and keeps the icon")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Icon icon = facet::Icon("sun");
    IconProbe popup;
    ImRect tile;
    session.SetUi([&] {
        facet::IconPicker("Icon", &icon, kIconItems);
        session.Track("icon");
        popup.Record("Icon");
        tile = fi::PickerGridTileRect(popup.grid, 0);
    });
    session.Frame();
    session.Click(session.ItemCenter("icon"));
    session.Frames(2);
    REQUIRE(popup.open);
    CHECK(tile.GetWidth() > 0.0f);
    session.Type("qqqq");
    session.Frames(3);
    CHECK(popup.open);
    CHECK(tile.GetWidth() == 0.0f);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(icon == facet::Icon("sun"));
}

TEST_CASE("An icon grid over a very large set submits only its visible lines and reaches the last icon")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static constexpr int kCount = 20000;
    static facet::ChoiceItem items[kCount];
    static char labels[kCount][16];
    for (int i = 0; i < kCount; ++i)
    {
        std::snprintf(labels[i], sizeof(labels[i]), "Option %d", i + 1);
        items[i] = facet::ChoiceItem{labels[i], facet::Icon(i % 2 == 0 ? "grid" : "list")};
    }
    facet::Icon icon;
    IconProbe popup;
    ImRect first;
    ImRect last;
    int drawn = 0;
    session.SetUi([&] {
        facet::IconPicker("Icon", &icon, std::span<const facet::ChoiceItem>(items, kCount), {.searchable = false});
        session.Track("icon");
        popup.Record("Icon");
        first = fi::PickerGridTileRect(popup.grid, 0);
        last = fi::PickerGridTileRect(popup.grid, kCount - 1);
        drawn = 0;
        for (int i = 0; i < 200; ++i)
            drawn += fi::PickerGridTileRect(popup.grid, i).GetWidth() > 0.0f ? 1 : 0;
    });
    session.Frame();
    session.Click(session.ItemCenter("icon"));
    session.Frames(2);
    REQUIRE(popup.open);
    CHECK(first.GetWidth() > 0.0f);
    CHECK(last.GetWidth() == 0.0f);
    CHECK(drawn < 200);  // the visible lines, not every tile
    session.KeyPress(ImGuiKey_RightArrow);
    session.KeyPress(ImGuiKey_End);
    session.Frames(3);
    CHECK(ImGui::GetFocusID() == popup.Tile(kCount - 1));
    CHECK(last.GetWidth() > 0.0f);
    CHECK(first.GetWidth() == 0.0f);
}

TEST_CASE("A read-only icon picker does not open")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    facet::Icon icon = facet::Icon("sun");
    IconProbe popup;
    session.SetUi([&] {
        facet::IconPicker("Icon", &icon, kIconItems, {.readOnly = true});
        session.Track("icon");
        popup.Record("Icon");
    });
    session.Frame();
    session.Click(session.ItemCenter("icon"));
    session.Frames(2);
    CHECK_FALSE(popup.open);
    CHECK(icon == facet::Icon("sun"));
}
// ---- ThumbnailPicker ----------------------------------------------------------------------------

namespace
{

const facet::ThumbnailItem kTiles[] = {
    {"Option 1", {}, facet::Icon("image")},
    {"Option 2", {}, facet::Icon("image")},
    {"Option 3", {}, facet::Icon("image"), true},
    {"Option 4", {}, facet::Icon("image"), false, true},
    {"Option 5", {}, facet::Icon("image")},
    {"Option 6", {}, facet::Icon("image")},
    {"Option 7", {}, facet::Icon("image")},
};

// The tiles of a ThumbnailPicker, read inside the ui code.
struct TileProbe
{
    ImGuiID grid = 0;
    ImRect rects[8];

    void Record(const char* label)
    {
        grid = ImHashStr("##tiles", 0, ImGui::GetID(label));
        for (int i = 0; i < 8; ++i)
            rects[i] = fi::PickerGridTileRect(grid, i);
    }
    ImGuiID Tile(int index) const
    {
        const ImGuiID key = static_cast<ImGuiID>(index);
        return ImHashData(&key, sizeof(key), ImHashStr("##tile", 0, grid));
    }
};

} // namespace

TEST_CASE("A thumbnail picker picks by click and moves in two dimensions past locked and disabled tiles")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    int writes = 0;
    TileProbe tiles;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        if (facet::ThumbnailPicker("Pictures", &index, kTiles, {.thumbnailSize = {64.0f, 48.0f}, .columns = 3}))
            ++writes;
        session.Track("pictures");
        tiles.Record("Pictures");
        ImGui::Button("after");
        session.Track("after");
    });
    session.Frames(2);
    session.Click(tiles.rects[1].GetCenter());
    CHECK(index == 1);
    CHECK(writes == 1);
    // A locked tile takes the focus but is not picked; a disabled one ignores the click.
    session.Click(tiles.rects[2].GetCenter());
    session.Click(tiles.rects[3].GetCenter());
    CHECK(index == 1);
    CHECK(writes == 1);

    // One Tab stop: Tab from the button lands on the tile focused last (the locked one the click
    // focused), the arrows move from there, and the next Tab leaves the grid.
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == tiles.Tile(2));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(index == 1);
    session.KeyPress(ImGuiKey_RightArrow);
    CHECK(ImGui::GetFocusID() == tiles.Tile(4));  // the disabled tile 3 is passed over
    session.KeyPress(ImGuiKey_DownArrow);
    CHECK(ImGui::GetFocusID() == tiles.Tile(6));  // the last line holds only tile 6
    session.KeyPress(ImGuiKey_UpArrow);
    CHECK(ImGui::GetFocusID() == tiles.Tile(2));  // tile 3 above 6 is disabled: the scan goes on back
    session.KeyPress(ImGuiKey_LeftArrow);
    session.KeyPress(ImGuiKey_LeftArrow);
    CHECK(ImGui::GetFocusID() == tiles.Tile(0));
    session.KeyPress(ImGuiKey_End);
    session.KeyPress(ImGuiKey_Space);
    CHECK(index == 6);
    CHECK(writes == 2);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == session.ItemId("after"));
}

TEST_CASE("The multi thumbnail picker flips one flag per tile and makes each tile a Tab stop")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    bool checked[7] = {false, true, false, false, false, false, false};
    TileProbe tiles;
    session.SetUi([&] {
        ImGui::Button("before");
        session.Track("before");
        facet::ThumbnailPicker("Pictures", std::span<bool>(checked), kTiles, {.thumbnailSize = {64.0f, 48.0f}, .columns = 3});
        tiles.Record("Pictures");
    });
    session.Frames(2);
    session.Click(tiles.rects[0].GetCenter());
    CHECK(checked[0]);
    session.Click(tiles.rects[1].GetCenter());
    CHECK_FALSE(checked[1]);
    session.Click(tiles.rects[2].GetCenter());
    CHECK_FALSE(checked[2]);  // locked
    session.Click(session.ItemCenter("before"));
    session.KeyPress(ImGuiKey_Tab);
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == tiles.Tile(0));
    session.KeyPress(ImGuiKey_Tab);
    CHECK(ImGui::GetFocusID() == tiles.Tile(1));
    session.KeyPress(ImGuiKey_Space);
    CHECK(checked[1]);
}

TEST_CASE("A thumbnail picker shows its loading, error and empty states and writes nothing in them")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    int index = 0;
    facet::DataState state = facet::DataState::Loading;
    std::span<const facet::ThumbnailItem> items(kTiles);
    session.SetUi([&] {
        if (facet::ThumbnailPicker("Pictures", &index, items, {.thumbnailSize = {64.0f, 48.0f}, .columns = 3, .state = state}))
            index = 99;
        session.Track("pictures");
    });
    session.Frames(2);
    const float loadingHeight = session.ItemRect("pictures").GetHeight();
    CHECK(loadingHeight > facet::Px(48.0f) * 2.0f);  // three lines of skeleton tiles
    session.Click(session.ItemCenter("pictures"));
    CHECK(index == 0);
    state = facet::DataState::Error;
    session.Frames(2);
    CHECK(session.ItemRect("pictures").GetHeight() < loadingHeight);
    session.Click(session.ItemCenter("pictures"));
    CHECK(index == 0);
    state = facet::DataState::Ready;
    items = {};
    session.Frames(2);
    session.Click(session.ItemCenter("pictures"));
    CHECK(index == 0);
}

TEST_CASE("A thumbnail grid of fifty thousand tiles submits only the lines in view")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    static constexpr int kCount = 50000;
    static facet::ThumbnailItem many[kCount];
    for (int i = 0; i < kCount; ++i)
        many[i] = facet::ThumbnailItem{"Option", {}, facet::Icon("image")};
    int index = 0;
    ImRect first;
    ImRect last;
    int laidOut = 0;
    session.SetUi([&] {
        ImGui::BeginChild("area", ImVec2(facet::Px(400.0f), facet::Px(300.0f)));
        facet::ThumbnailPicker("Pictures", &index, std::span<const facet::ThumbnailItem>(many, kCount),
                               {.thumbnailSize = {64.0f, 48.0f}});
        const ImGuiID grid = ImHashStr("##tiles", 0, ImGui::GetID("Pictures"));
        first = fi::PickerGridTileRect(grid, 0);
        last = fi::PickerGridTileRect(grid, kCount - 1);
        laidOut = 0;
        for (int i = 0; i < 400; ++i)
            laidOut += fi::PickerGridTileRect(grid, i).GetWidth() > 0.0f ? 1 : 0;
        ImGui::EndChild();
    });
    session.Frames(2);
    CHECK(first.GetWidth() > 0.0f);
    CHECK(last.GetWidth() == 0.0f);
    CHECK(laidOut < 100);
    session.Click(first.GetCenter());
    session.KeyPress(ImGuiKey_End);
    session.Frames(4);
    CHECK(last.GetWidth() > 0.0f);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(index == kCount - 1);
}
// ---- FileBrowser --------------------------------------------------------------------------------

namespace
{

namespace fs = std::filesystem;

// A folder tree under the temporary directory, removed again at the end of the test.
struct TempTree
{
    fs::path root;

    TempTree()
    {
        static int counter = 0;
        root = fs::temp_directory_path() / ("facet_file_browser_" + std::to_string(++counter) + "_" +
                                            std::to_string(reinterpret_cast<std::uintptr_t>(this)));
        std::error_code error;
        fs::remove_all(root, error);
        fs::create_directories(root, error);
    }
    ~TempTree()
    {
        std::error_code error;
        fs::remove_all(root, error);
    }
    void File(const fs::path& relative) const
    {
        std::ofstream out(root / relative);
        out << "placeholder";
    }
    void Folder(const fs::path& relative) const
    {
        std::error_code error;
        fs::create_directories(root / relative, error);
    }
    std::string Utf8(const fs::path& relative = {}) const
    {
        const std::u8string text = (relative.empty() ? root : root / relative).u8string();
        return std::string(reinterpret_cast<const char*>(text.data()), text.size());
    }
};

// Runs frames until the browser `label` is not loading, for at most two seconds of real time.
fi::FileBrowserView WaitForBrowser(facet::test::Session& session, ImGuiID id)
{
    fi::FileBrowserView view;
    for (int i = 0; i < 400; ++i)
    {
        session.Frame();
        if (fi::FileBrowserViewOf(id, &view) && view.state != 1 && !fi::FileBrowserBusy(id))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return view;
}

// The center of listed row `row` of a browser whose widget rect is `widget`: the path bar, the gap,
// the area padding, then the rows.
ImVec2 BrowserRow(const ImRect& widget, int row)
{
    const float bar = facet::ControlHeight(facet::ControlSize::Inherit);
    const float top = widget.Min.y + bar + facet::PxSnapped(facet::T().spacing.sm) + fi::FocusRingOutset();
    const float height = fi::ListRowHeight(facet::ControlSize::Inherit);
    return ImVec2(widget.Min.x + facet::Px(60.0f), top + height * (static_cast<float>(row) + 0.5f));
}

const facet::FileType kBrowserTypes[] = {{"All files", nullptr}, {"Images", "*.png;*.JPG"}};

} // namespace

TEST_CASE("File browser patterns match wildcards without case")
{
    CHECK(fi::FileBrowserMatches("photo.PNG", "*.png;*.jpg"));
    CHECK(fi::FileBrowserMatches("photo.jpg", "*.png; *.jpg"));
    CHECK_FALSE(fi::FileBrowserMatches("photo.jpeg", "*.png;*.jpg"));
    CHECK(fi::FileBrowserMatches("a1.txt", "a?.txt"));
    CHECK_FALSE(fi::FileBrowserMatches("a12.txt", "a?.txt"));
    CHECK(fi::FileBrowserMatches("anything", nullptr));
    CHECK(fi::FileBrowserMatches("anything", ""));
    CHECK(fi::FileBrowserMatches("archive.tar.gz", "*.gz"));
}

TEST_CASE("A file browser lists folders first, opens a folder, writes a file and goes back with Backspace")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TempTree tree;
    tree.Folder("Folder A");
    tree.Folder("Folder B");
    tree.File("Folder B/inner.txt");
    tree.File("image.png");
    tree.File("notes.txt");
    tree.File(".hidden.txt");
    std::string path = tree.Utf8("notes.txt");
    int writes = 0;
    session.SetUi([&] {
        if (facet::FileBrowser("Files", &path, {.fileTypes = kBrowserTypes}))
            ++writes;
        session.Track("files");
    });
    session.Frame();
    const ImGuiID browser = session.ItemId("files");
    fi::FileBrowserView view = WaitForBrowser(session, browser);
    REQUIRE(view.state == 0);
    CHECK(view.folder == tree.Utf8());
    REQUIRE(view.rows == 4);  // the hidden file is left out
    CHECK(std::string(fi::FileBrowserRowName(browser, 0)) == "Folder A");
    CHECK(std::string(fi::FileBrowserRowName(browser, 1)) == "Folder B");
    CHECK(std::string(fi::FileBrowserRowName(browser, 2)) == "image.png");

    session.DoubleClick(BrowserRow(session.ItemRect("files"), 1));
    view = WaitForBrowser(session, browser);
    CHECK(view.folder == tree.Utf8() + "\\Folder B");
    REQUIRE(view.rows == 1);
    session.DoubleClick(BrowserRow(session.ItemRect("files"), 0));
    CHECK(path == tree.Utf8() + "\\Folder B\\inner.txt");
    CHECK(writes == 1);
    session.KeyPress(ImGuiKey_Backspace);
    view = WaitForBrowser(session, browser);
    CHECK(view.folder == tree.Utf8());
    CHECK(view.rows == 4);
}

TEST_CASE("A file browser shows an empty folder, and a folder that disappears turns into the error state")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TempTree tree;
    tree.Folder("Empty");
    tree.Folder("Going");
    tree.File("Going/file.txt");
    std::string path = tree.Utf8("Empty");
    std::string second = tree.Utf8("Going");
    session.SetUi([&] {
        facet::FileBrowser("Empty", &path, {.mode = facet::FileBrowserMode::Folder});
        session.Track("empty");
        facet::FileBrowser("Going", &second);
        session.Track("going");
    });
    session.Frame();
    fi::FileBrowserView view = WaitForBrowser(session, session.ItemId("empty"));
    CHECK(view.state == 0);
    CHECK(view.rows == 0);
    view = WaitForBrowser(session, session.ItemId("going"));
    CHECK(view.folder == tree.Utf8() + "\\Going");
    CHECK(view.rows == 1);

    std::error_code error;
    fs::remove_all(tree.root / "Going", error);
    REQUIRE_FALSE(error);
    // The next check (every two seconds of frame time) finds the folder gone.
    session.Seconds(2.2f);
    for (int i = 0; i < 400 && view.state != 2; ++i)
    {
        session.Frame();
        fi::FileBrowserViewOf(session.ItemId("going"), &view);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    CHECK(view.state == 2);
    // The parent folder button still works.
    const ImRect widget = session.ItemRect("going");
    const float bar = facet::ControlHeight(facet::ControlSize::Inherit);
    session.Click(ImVec2(widget.Min.x + bar * 0.5f, widget.Min.y + bar * 0.5f));
    view = WaitForBrowser(session, session.ItemId("going"));
    CHECK(view.state == 0);
    CHECK(view.folder == tree.Utf8());
}

TEST_CASE("A file browser lists a folder of three thousand files and reaches the last with End")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TempTree tree;
    for (int i = 0; i < 3000; ++i)
    {
        char name[32];
        std::snprintf(name, sizeof(name), "file_%05d.txt", i);
        tree.File(name);
    }
    const std::string longName(180, 'n');
    tree.File(longName + ".txt");
    std::string path = tree.Utf8("file_00000.txt");
    session.SetUi([&] {
        facet::FileBrowser("Files", &path);
        session.Track("files");
    });
    session.Frame();
    const ImGuiID browser = session.ItemId("files");
    const fi::FileBrowserView view = WaitForBrowser(session, browser);
    REQUIRE(view.rows == 3001);
    CHECK(std::string(fi::FileBrowserRowName(browser, 3000)) == longName + ".txt");
    session.Click(BrowserRow(session.ItemRect("files"), 0));
    session.KeyPress(ImGuiKey_End);
    session.KeyPress(ImGuiKey_Enter);
    CHECK(path.size() > longName.size());
    CHECK(path.substr(path.size() - longName.size() - 4) == longName + ".txt");
}

TEST_CASE("Save mode writes the typed name, and a path that does not fit the buffer is never written")
{
    facet::test::Session session;
    REQUIRE(session.Ready());
    TempTree tree;
    tree.File("existing.txt");
    std::string path = tree.Utf8("existing.txt");
    char tiny[8] = "x";
    session.SetUi([&] {
        facet::FileBrowser("Save", &path, {.mode = facet::FileBrowserMode::Save});
        session.Track("save");
        facet::FileBrowser("Small", tiny, sizeof(tiny));
        session.Track("small");
    });
    session.Frame();
    WaitForBrowser(session, session.ItemId("save"));
    const ImRect widget = session.ItemRect("save");
    // The name field is the line under the list; typing there and Enter save.
    const float bar = facet::ControlHeight(facet::ControlSize::Inherit);
    const float gap = facet::PxSnapped(facet::T().spacing.sm);
    const float listHeight = std::round(facet::Px(facet::T().metrics.panelHeight));
    session.Click(ImVec2(widget.Max.x - facet::Px(40.0f), widget.Min.y + bar + gap + listHeight + gap + bar * 0.5f));
    session.KeyPress(ImGuiKey_A | ImGuiMod_Ctrl);
    session.Type("new name.txt");
    session.KeyPress(ImGuiKey_Enter);
    CHECK(path == tree.Utf8() + "\\new name.txt");

    // The small buffer: a file is chosen and Enter pressed, but the path does not fit.
    WaitForBrowser(session, session.ItemId("small"));
    session.Click(BrowserRow(session.ItemRect("small"), 0));
    session.KeyPress(ImGuiKey_Enter);
    CHECK(std::string(tiny) == "x");
}

TEST_CASE("A folder the system does not let the user read shows the error state")
{
    // Every NTFS volume has this folder, and a normal user may not list it.
    const fs::path locked = "C:\\System Volume Information";
    std::error_code error;
    if (!fs::is_directory(locked, error))
    {
        MESSAGE("no locked system folder on this machine; skipped");
        return;
    }
    facet::test::Session session;
    REQUIRE(session.Ready());
    std::string path = "C:\\System Volume Information";
    session.SetUi([&] {
        facet::FileBrowser("Files", &path);
        session.Track("files");
    });
    session.Frame();
    const fi::FileBrowserView view = WaitForBrowser(session, session.ItemId("files"));
    fs::directory_iterator probe(locked, error);
    if (!error)
    {
        MESSAGE("this user may read the system folder; skipped");
        return;
    }
    CHECK(view.state == 2);
    CHECK(path == "C:\\System Volume Information");
}