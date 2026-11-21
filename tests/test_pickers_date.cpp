// Behaviour of the date, time and other pickers (DatePicker, DateRangePicker, TimePicker,
// AnglePicker, IconPicker, ThumbnailPicker and FileBrowser in facet/widgets/pickers.h).
//
// One test case per behaviour of an implemented widget; docs/writing-a-widget.md describes the
// pattern. The API checks, and the placeholder checks of the widgets that are still planned, are in
// tests/test_api_pickers_date.cpp. Popup levels and calendar cells are read inside the frame (the
// popup host and the state store answer there), so the ui code of a test records them.

#include <cmath>
#include <cstdio>
#include <span>
#include <string>

#include <doctest/doctest.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <facet/facet.h>

#include "internal/focus.h"
#include "internal/popup_host.h"
#include "support/session.h"
#include "widgets/pickers/date_picker_parts.h"

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