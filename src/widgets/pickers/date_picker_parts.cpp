// Parts of the date and time pickers: calendar arithmetic, the calendar panel and the segment
// stepper (date_picker_parts.h).

#include "widgets/pickers/date_picker_parts.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <limits>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/list_view.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "internal/value_drag.h"

namespace facet::internal
{
namespace
{

// Days from 1970-01-01 to a civil date (the proleptic Gregorian calendar, any year).
long long DateDaysFromCivil(int year, int month, int day)
{
    const long long y = static_cast<long long>(year) - (month <= 2 ? 1 : 0);
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const long long yoe = y - era * 400;
    const long long mp = month > 2 ? month - 3 : month + 9;
    const long long doy = (153 * mp + 2) / 5 + day - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

// The serial of 0001-01-01 counted from 1970-01-01.
constexpr long long kDateSerialOrigin = -719162;
// The serial of 9999-12-31.
constexpr int kDateSerialLast = 3652058;

// Months in one year view block and in one row of the month and year grids.
constexpr int kCalendarBlock = 12;
constexpr int kCalendarGridColumns = 4;
constexpr int kCalendarWeeks = 6;
constexpr int kCalendarDays = 7;

struct CalendarState
{
    CalendarView view = CalendarView::Day;
    int year = 0;   // day view: the first shown month's year; month view: the year shown
    int month = 1;  // day view: the first shown month
    Date cursor;    // the keyboard cursor
    bool started = false;
    // The layout of the last frame, for the rect queries.
    ImVec2 gridOrigin[2];
    int months = 1;
    float cell = 0.0f;
    float monthGap = 0.0f;
    Weekday firstWeekday = Weekday::Monday;
    ImRect buttons[3];
};

int CalendarMonthIndex(int year, int month)
{
    return year * 12 + (month - 1);
}

int CalendarBlockStart(int year)
{
    return year - (year - 1) % kCalendarBlock;
}

ImGuiID CalendarSeed(ImGuiID panelId, const char* kind)
{
    return ImHashStr(kind, 0, panelId);
}

ImGuiID CalendarKeyId(ImGuiID seed, int key)
{
    return ImHashData(&key, sizeof(key), seed);
}

// Layout of one frame of the panel, in pixels.
struct CalendarLayout
{
    float cell = 0.0f;
    float header = 0.0f;
    float headerGap = 0.0f;
    float weekdays = 0.0f;
    float monthGap = 0.0f;
    float monthWidth = 0.0f;
    ImVec2 size;
};

CalendarLayout CalendarMeasure(const CalendarPanelDesc& desc)
{
    const Theme& t = T();
    CalendarLayout layout;
    layout.cell = std::max(1.0f, std::floor(desc.cell > 0.0f ? desc.cell : Px(t.picker.dayCell)));
    layout.header = GetControlMetrics(ControlSize::Inherit).height;
    layout.headerGap = PxSnapped(t.spacing.xs);
    layout.weekdays = std::round(MeasureText(TextStyle::Footnote, "Ag").y + Px(t.spacing.sm));
    layout.monthGap = PxSnapped(t.spacing.lg);
    layout.monthWidth = layout.cell * kCalendarDays;
    const int months = std::clamp(desc.months, 1, 2);
    layout.size = ImVec2(layout.monthWidth * months + layout.monthGap * (months - 1),
                         layout.header + layout.headerGap + layout.weekdays + layout.cell * kCalendarWeeks);
    return layout;
}

Date CalendarDefaultCursor(const Date& focus, const CalendarPanelDesc& desc)
{
    Date cursor = DateIsReal(focus) ? focus : DateToday();
    return DateClampTo(cursor, desc.min, desc.max);
}

// The month a day view shows at index `m` (0 or 1).
void CalendarShownMonth(const CalendarState& state, int m, int* year, int* month)
{
    const int index = std::clamp(CalendarMonthIndex(state.year, state.month) + m, CalendarMonthIndex(kDateMinYear, 1),
                                 CalendarMonthIndex(kDateMaxYear, 12));
    *year = index / 12;
    *month = index % 12 + 1;
}

// Moves the shown months so the cursor is in them.
void CalendarRevealCursor(CalendarState& state, int months)
{
    const int cursor = CalendarMonthIndex(state.cursor.year, state.cursor.month);
    int first = CalendarMonthIndex(state.year, state.month);
    if (cursor < first)
        first = cursor;
    else if (cursor > first + months - 1)
        first = cursor - (months - 1);
    first = std::max(first, CalendarMonthIndex(kDateMinYear, 1));
    state.year = first / 12;
    state.month = first % 12 + 1;
}

// The rect of a day cell in the layout of the shown months; empty when the day is not shown.
ImRect CalendarDayRect(const CalendarState& state, const Date& date)
{
    for (int m = 0; m < state.months; ++m)
    {
        int year = 0;
        int month = 0;
        CalendarShownMonth(state, m, &year, &month);
        if (year != date.year || month != date.month)
            continue;
        const int index = DateMonthStartColumn(year, month, state.firstWeekday) + date.day - 1;
        const ImVec2 at(state.gridOrigin[m].x + static_cast<float>(index % kCalendarDays) * state.cell,
                        state.gridOrigin[m].y + static_cast<float>(index / kCalendarDays) * state.cell);
        return ImRect(at, ImVec2(at.x + state.cell, at.y + state.cell));
    }
    return ImRect();
}

// The rect of entry `index` (0 to 11) of a month or year grid.
ImRect CalendarGridRect(const ImRect& area, int index)
{
    const float w = area.GetWidth() / kCalendarGridColumns;
    const float h = area.GetHeight() / (kCalendarBlock / kCalendarGridColumns);
    const float x = std::round(area.Min.x + static_cast<float>(index % kCalendarGridColumns) * w);
    const float y = std::round(area.Min.y + static_cast<float>(index / kCalendarGridColumns) * h);
    return ImRect(x, y, std::round(x + w), std::round(y + h));
}

bool CalendarMonthAllowed(int year, int month, const Date& min, const Date& max)
{
    const Date first{year, month, 1};
    const Date last{year, month, DateMonthLength(year, month)};
    if (min.IsValid() && DateCompare(last, min) < 0)
        return false;
    return !(max.IsValid() && DateCompare(first, max) > 0);
}

bool CalendarYearAllowed(int year, const Date& min, const Date& max)
{
    if (year < kDateMinYear || year > kDateMaxYear)
        return false;
    if (min.IsValid() && year < min.year)
        return false;
    return !(max.IsValid() && year > max.year);
}

// Draws the number or name of a cell centered in `rect`.
void CalendarCellText(ImDrawList* drawList, const ImRect& rect, const char* text, ImU32 color, bool strong)
{
    PushPillLabelStyle(TextStyle::Body, strong);
    const FittedText shown = FitText(text, nullptr, rect.GetWidth() - PxSnapped(T().spacing.xs));
    const ImVec2 at(std::round(rect.GetCenter().x - shown.width * 0.5f), CenteredTextY(rect.Min.y, rect.GetHeight()));
    drawList->AddText(at, color, shown.text);
    PopTextStyle();
}

} // namespace

// ---- Calendar arithmetic --------------------------------------------------------------------

bool DateIsLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int DateMonthLength(int year, int month)
{
    static constexpr int kLengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12)
        return 0;
    return month == 2 && DateIsLeapYear(year) ? 29 : kLengths[month - 1];
}

bool DateIsReal(const Date& date)
{
    return date.year >= kDateMinYear && date.year <= kDateMaxYear && date.month >= 1 && date.month <= 12 &&
           date.day >= 1 && date.day <= DateMonthLength(date.year, date.month);
}

int DateSerial(const Date& date)
{
    return static_cast<int>(DateDaysFromCivil(date.year, date.month, date.day) - kDateSerialOrigin);
}

Date DateFromSerial(int serial)
{
    serial = std::clamp(serial, 0, kDateSerialLast);
    const long long z = static_cast<long long>(serial) + kDateSerialOrigin + 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const long long doe = z - era * 146097;
    const long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long long mp = (5 * doy + 2) / 153;
    const int day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    const int month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    const int year = static_cast<int>(yoe + era * 400 + (month <= 2 ? 1 : 0));
    return Date{year, month, day};
}

Weekday DateWeekdayOf(const Date& date)
{
    return static_cast<Weekday>(DateSerial(date) % 7);
}

Date DateAddDays(const Date& date, int days)
{
    const long long serial = static_cast<long long>(DateSerial(date)) + days;
    return DateFromSerial(static_cast<int>(std::clamp<long long>(serial, 0, kDateSerialLast)));
}

Date DateAddMonths(const Date& date, int months)
{
    const long long index = static_cast<long long>(CalendarMonthIndex(date.year, date.month)) + months;
    const long long clamped = std::clamp<long long>(index, CalendarMonthIndex(kDateMinYear, 1), CalendarMonthIndex(kDateMaxYear, 12));
    Date out;
    out.year = static_cast<int>(clamped / 12);
    out.month = static_cast<int>(clamped % 12) + 1;
    out.day = std::min(date.day, DateMonthLength(out.year, out.month));
    return out;
}

int DateCompare(const Date& a, const Date& b)
{
    if (a.year != b.year)
        return a.year < b.year ? -1 : 1;
    if (a.month != b.month)
        return a.month < b.month ? -1 : 1;
    if (a.day != b.day)
        return a.day < b.day ? -1 : 1;
    return 0;
}

bool DateWithin(const Date& date, const Date& min, const Date& max)
{
    if (min.IsValid() && DateCompare(date, min) < 0)
        return false;
    return !(max.IsValid() && DateCompare(date, max) > 0);
}

Date DateClampTo(const Date& date, const Date& min, const Date& max)
{
    if (max.IsValid() && DateCompare(date, max) > 0)
        return max;
    if (min.IsValid() && DateCompare(date, min) < 0)
        return min;
    return date;
}

Date DateToday()
{
    const std::time_t now = std::time(nullptr);
    std::tm local{};
    if (localtime_s(&local, &now) != 0)
        return Date{1970, 1, 1};
    return Date{local.tm_year + 1900, local.tm_mon + 1, local.tm_mday};
}

int DateMonthStartColumn(int year, int month, Weekday first)
{
    const int weekday = static_cast<int>(DateWeekdayOf(Date{year, month, 1}));
    return (weekday - static_cast<int>(first) + kCalendarDays) % kCalendarDays;
}

const char* DateMonthName(int month)
{
    const Strings& s = GetStrings();
    const char* const names[12] = {s.monthJanuary, s.monthFebruary, s.monthMarch,     s.monthApril,
                                   s.monthMay,     s.monthJune,     s.monthJuly,      s.monthAugust,
                                   s.monthSeptember, s.monthOctober, s.monthNovember, s.monthDecember};
    return month >= 1 && month <= 12 ? names[month - 1] : "";
}

const char* DateWeekdayName(Weekday day)
{
    const Strings& s = GetStrings();
    const char* const names[7] = {s.weekdayMonday, s.weekdayTuesday,  s.weekdayWednesday, s.weekdayThursday,
                                  s.weekdayFriday, s.weekdaySaturday, s.weekdaySunday};
    return names[std::clamp(static_cast<int>(day), 0, 6)];
}

void DateFormat(char* out, std::size_t size, const Date& date, DatePrecision precision)
{
    if (size == 0)
        return;
    out[0] = 0;
    if (!date.IsValid())
        return;
    switch (precision)
    {
    case DatePrecision::Year:
        std::snprintf(out, size, "%04d", date.year);
        break;
    case DatePrecision::Month:
        std::snprintf(out, size, "%04d-%02d", date.year, date.month);
        break;
    default:
        std::snprintf(out, size, "%04d-%02d-%02d", date.year, date.month, date.day);
        break;
    }
}

Date DateAtPrecision(const Date& date, DatePrecision precision)
{
    Date out = date;
    if (precision != DatePrecision::Day)
        out.day = 1;
    if (precision == DatePrecision::Year)
        out.month = 1;
    return out;
}

// ---- Time of day arithmetic -----------------------------------------------------------------------

int TimeToSeconds(const TimeOfDay& time)
{
    return std::clamp(time.hour, 0, 23) * 3600 + std::clamp(time.minute, 0, 59) * 60 + std::clamp(time.second, 0, 59);
}

TimeOfDay TimeFromSeconds(long long seconds)
{
    const long long wrapped = ((seconds % kTimeSecondsPerDay) + kTimeSecondsPerDay) % kTimeSecondsPerDay;
    const int day = static_cast<int>(wrapped);
    return TimeOfDay{day / 3600, (day / 60) % 60, day % 60};
}

TimeOfDay TimeStep(const TimeOfDay& time, TimeSegment segment, int steps, int minuteStep)
{
    const long long total = TimeToSeconds(time);
    switch (segment)
    {
    case TimeSegment::Hour:
        return TimeFromSeconds(total + 3600LL * steps);
    case TimeSegment::Second:
        return TimeFromSeconds(total + steps);
    case TimeSegment::HalfDay:
        return TimeFromSeconds(total + 43200LL * steps);
    case TimeSegment::Minute:
        break;
    }
    const int step = std::clamp(minuteStep, 1, 60);
    const int slots = 59 / step + 1;
    long long hour = total / 3600;
    const int minute = static_cast<int>((total / 60) % 60);
    const int second = static_cast<int>(total % 60);
    long long slot = minute / step;
    long long remaining = steps;
    if (minute % step != 0 && remaining != 0)
    {
        // Off the grid: the first step lands on the next slot in its direction.
        if (remaining > 0)
        {
            ++slot;
            --remaining;
        }
        else
        {
            ++remaining;
        }
    }
    slot += remaining;
    const long long carry = slot >= 0 ? slot / slots : -((-slot + slots - 1) / slots);
    hour += carry;
    slot -= carry * slots;
    return TimeFromSeconds(hour * 3600 + slot * step * 60 + second);
}

int TimeHour12(int hour24)
{
    const int hour = ((hour24 % 12) + 12) % 12;
    return hour == 0 ? 12 : hour;
}

// ---- Calendar panel -----------------------------------------------------------------------------

void CalendarPanelOpen(ImGuiID panelId, const Date& focus, const CalendarPanelDesc& desc)
{
    CalendarState& state = GetState<CalendarState>(panelId);
    state.cursor = CalendarDefaultCursor(focus, desc);
    state.view = desc.months > 1 || desc.precision == DatePrecision::Day ? CalendarView::Day
                 : desc.precision == DatePrecision::Month               ? CalendarView::Month
                                                                        : CalendarView::Year;
    state.year = state.cursor.year;
    state.month = state.cursor.month;
    // A range shows the cursor's month first unless that would push the last allowed month out.
    if (desc.months > 1 && desc.max.IsValid() &&
        CalendarMonthIndex(state.year, state.month) + 1 > CalendarMonthIndex(desc.max.year, desc.max.month))
        CalendarRevealCursor(state, desc.months);
    state.started = true;
}

ImVec2 CalendarPanelSize(const CalendarPanelDesc& desc)
{
    return CalendarMeasure(desc).size;
}

ImGuiID CalendarDayCellId(ImGuiID panelId, const Date& date)
{
    if (!DateIsReal(date))
        return 0;
    return CalendarKeyId(CalendarSeed(panelId, "day"), DateSerial(date));
}

ImRect CalendarDayCellRect(ImGuiID panelId, const Date& date)
{
    CalendarState* state = static_cast<CalendarState*>(GlobalStateStore().Find(panelId, StateTypeOf<CalendarState>()));
    if (state == nullptr || state->view != CalendarView::Day || !DateIsReal(date))
        return ImRect();
    return CalendarDayRect(*state, date);
}

Date CalendarPanelCursor(ImGuiID panelId)
{
    const CalendarState* state =
        static_cast<const CalendarState*>(GlobalStateStore().Find(panelId, StateTypeOf<CalendarState>()));
    return state != nullptr ? state->cursor : Date{};
}

Date CalendarPanelShownMonth(ImGuiID panelId)
{
    const CalendarState* state =
        static_cast<const CalendarState*>(GlobalStateStore().Find(panelId, StateTypeOf<CalendarState>()));
    return state != nullptr ? Date{state->year, state->month, 1} : Date{};
}

CalendarView CalendarPanelView(ImGuiID panelId)
{
    const CalendarState* state =
        static_cast<const CalendarState*>(GlobalStateStore().Find(panelId, StateTypeOf<CalendarState>()));
    return state != nullptr ? state->view : CalendarView::Day;
}

ImRect CalendarPanelButtonRect(ImGuiID panelId, int button)
{
    const CalendarState* state =
        static_cast<const CalendarState*>(GlobalStateStore().Find(panelId, StateTypeOf<CalendarState>()));
    if (state == nullptr || button < 0 || button > 2)
        return ImRect();
    return state->buttons[button];
}

CalendarPanelResult CalendarPanel(ImGuiID panelId, const CalendarPanelDesc& desc)
{
    CalendarPanelResult result;
    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context == nullptr || context->CurrentWindow == nullptr || panelId == 0)
        return result;
    ImGuiContext& g = *context;
    ImGuiWindow* window = g.CurrentWindow;
    const Theme& t = T();
    const Colors& c = t.colors;
    const int months = std::clamp(desc.months, 1, 2);
    const CalendarLayout layout = CalendarMeasure(desc);
    CalendarState& state = GetState<CalendarState>(panelId);
    if (!state.started)
        CalendarPanelOpen(panelId, desc.picked.IsValid() ? desc.picked : desc.rangeStart, desc);
    if (months > 1)
        state.view = CalendarView::Day;
    state.cursor = DateClampTo(DateIsReal(state.cursor) ? state.cursor : DateToday(), desc.min, desc.max);
    if (state.view == CalendarView::Day)
    {
        // The keyboard cursor stays in the shown months (it follows the header buttons).
        const int cursor = CalendarMonthIndex(state.cursor.year, state.cursor.month);
        const int first = CalendarMonthIndex(state.year, state.month);
        if (cursor < first || cursor > first + months - 1)
        {
            Date moved{state.year, state.month, std::min(state.cursor.day, DateMonthLength(state.year, state.month))};
            if (cursor > first + months - 1)
                moved = DateAddMonths(Date{state.year, state.month, state.cursor.day}, months - 1);
            state.cursor = DateClampTo(moved, desc.min, desc.max);
            CalendarRevealCursor(state, months);
        }
    }

    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImRect panel(origin, ImVec2(origin.x + layout.size.x, origin.y + layout.size.y));
    ImDrawList* drawList = window->DrawList;
    const ImGuiID daySeed = CalendarSeed(panelId, "day");
    const ImGuiID monthSeed = CalendarSeed(panelId, "month");
    const ImGuiID yearSeed = CalendarSeed(panelId, "year");
    const Date today = DateToday();
    const bool live = desc.live;
    const float inset = PxSnapped(t.metrics.segmentInset);
    const float radius = Px(t.radii.control);

    // ---- Header: previous, the title, next ----
    ImGui::PushOverrideID(panelId);
    const Strings& strings = GetStrings();
    const float button = layout.header;
    const ImRect gridArea(origin.x, origin.y + layout.header + layout.headerGap, panel.Max.x, panel.Max.y);
    bool prevAllowed = true;
    bool nextAllowed = true;
    char title[96] = {};
    int blockStart = CalendarBlockStart(state.cursor.year);
    switch (state.view)
    {
    case CalendarView::Day:
    {
        int lastYear = 0;
        int lastMonth = 0;
        CalendarShownMonth(state, months - 1, &lastYear, &lastMonth);
        const int first = CalendarMonthIndex(state.year, state.month);
        const int last = CalendarMonthIndex(lastYear, lastMonth);
        prevAllowed = first > CalendarMonthIndex(kDateMinYear, 1) &&
                      !(desc.min.IsValid() && first <= CalendarMonthIndex(desc.min.year, desc.min.month));
        nextAllowed = last < CalendarMonthIndex(kDateMaxYear, 12) &&
                      !(desc.max.IsValid() && last >= CalendarMonthIndex(desc.max.year, desc.max.month));
        std::snprintf(title, sizeof(title), "%s %d###title", DateMonthName(state.month), state.year);
        break;
    }
    case CalendarView::Month:
        prevAllowed = state.cursor.year > kDateMinYear && !(desc.min.IsValid() && state.cursor.year <= desc.min.year);
        nextAllowed = state.cursor.year < kDateMaxYear && !(desc.max.IsValid() && state.cursor.year >= desc.max.year);
        std::snprintf(title, sizeof(title), "%d###title", state.cursor.year);
        break;
    case CalendarView::Year:
        prevAllowed = blockStart > kDateMinYear && !(desc.min.IsValid() && blockStart <= desc.min.year);
        nextAllowed = blockStart + kCalendarBlock - 1 < kDateMaxYear &&
                      !(desc.max.IsValid() && blockStart + kCalendarBlock - 1 >= desc.max.year);
        std::snprintf(title, sizeof(title), "%d - %d###title", blockStart, blockStart + kCalendarBlock - 1);
        break;
    }
    char prevLabel[64];
    char nextLabel[64];
    std::snprintf(prevLabel, sizeof(prevLabel), "%s###prev", strings.previous);
    std::snprintf(nextLabel, sizeof(nextLabel), "%s###next", strings.next);
    int moveBy = 0;  // -1 previous, +1 next
    // A panel narrower than three buttons keeps only the title; Page Up and Down still page.
    const bool buttons = layout.size.x >= button * 3.0f;
    state.buttons[0] = ImRect();
    state.buttons[2] = ImRect();
    if (buttons)
    {
        ImGui::SetCursorScreenPos(origin);
        ImGui::BeginDisabled(!prevAllowed);
        if (IconButton(prevLabel, Icon("chevron-left")))
            moveBy = -1;
        state.buttons[0] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImGui::EndDisabled();
        ImGui::SetCursorScreenPos(ImVec2(panel.Max.x - button, origin.y));
        ImGui::BeginDisabled(!nextAllowed);
        if (IconButton(nextLabel, Icon("chevron-right")))
            moveBy = 1;
        state.buttons[2] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImGui::EndDisabled();
    }
    const float titleGap = buttons ? PxSnapped(t.spacing.xs) : 0.0f;
    const float titleLeft = origin.x + (buttons ? button : 0.0f) + titleGap;
    const float titleRight = panel.Max.x - (buttons ? button : 0.0f) - titleGap;
    if (months > 1)
    {
        // Two months: their names over each grid, no view switch.
        state.buttons[1] = ImRect();
        for (int m = 0; m < months; ++m)
        {
            int year = 0;
            int month = 0;
            CalendarShownMonth(state, m, &year, &month);
            char name[80];
            std::snprintf(name, sizeof(name), "%s %d", DateMonthName(month), year);
            const float left = std::max(titleLeft, origin.x + static_cast<float>(m) * (layout.monthWidth + layout.monthGap));
            const float right = std::min(titleRight, left + layout.monthWidth);
            ControlTextScope headline(TextStyle::Headline);
            const FittedText shown = FitText(name, nullptr, std::max(1.0f, right - left));
            const float x = std::round(std::max(left, (left + right) * 0.5f - shown.width * 0.5f));
            drawList->AddText(ImVec2(x, CenteredTextY(origin.y, layout.header)), ToU32(c.label), shown.text);
        }
    }
    else if (state.view == CalendarView::Year)
    {
        // The year view's title is text: there is no wider view to switch to.
        ControlTextScope headline(TextStyle::Headline);
        const char* end = ImGui::FindRenderedTextEnd(title);
        const FittedText shown = FitText(title, end, std::max(1.0f, titleRight - titleLeft));
        drawList->AddText(ImVec2(std::round((titleLeft + titleRight) * 0.5f - shown.width * 0.5f),
                                 CenteredTextY(origin.y, layout.header)),
                          ToU32(c.label), shown.text);
        state.buttons[1] = ImRect(titleLeft, origin.y, titleRight, origin.y + layout.header);
    }
    else
    {
        ImGui::SetCursorScreenPos(ImVec2(titleLeft, origin.y));
        const float widthUnits = std::max(1.0f, titleRight - titleLeft) / Scale();
        if (Button(title, {.variant = ButtonVariant::Plain, .width = widthUnits}) && live)
            state.view = state.view == CalendarView::Day ? CalendarView::Month : CalendarView::Year;
        state.buttons[1] = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    }
    ImGui::PopID();
    if (!live)
        moveBy = 0;

    if (moveBy != 0)
    {
        switch (state.view)
        {
        case CalendarView::Day:
        {
            const Date first = DateAddMonths(Date{state.year, state.month, 1}, moveBy);
            state.year = first.year;
            state.month = first.month;
            state.cursor = DateClampTo(DateAddMonths(state.cursor, moveBy), desc.min, desc.max);
            break;
        }
        case CalendarView::Month:
            state.cursor = DateClampTo(DateAddMonths(state.cursor, moveBy * 12), desc.min, desc.max);
            break;
        case CalendarView::Year:
            state.cursor = DateClampTo(DateAddMonths(state.cursor, moveBy * 12 * kCalendarBlock), desc.min, desc.max);
            break;
        }
        blockStart = CalendarBlockStart(state.cursor.year);
    }

    // ---- The grid ----
    state.months = months;
    state.cell = layout.cell;
    state.monthGap = layout.monthGap;
    state.firstWeekday = desc.firstWeekday;
    const ImGuiID focusScope = ImGui::GetCurrentFocusScope();
    ListRowGroup group;
    group.Begin(desc.forced);
    bool cellFocused = false;
    const bool mouseOver = live && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                           ImGui::IsMousePosValid(&g.IO.MousePos);
    int part = 1;

    if (state.view == CalendarView::Day)
    {
        // Range ends in order, and the band between them.
        Date lo;
        Date hi;
        if (desc.range && DateIsReal(desc.rangeStart))
        {
            lo = desc.rangeStart;
            hi = DateIsReal(desc.rangeEnd) ? desc.rangeEnd : (DateIsReal(desc.bandTo) ? desc.bandTo : desc.rangeStart);
            if (DateCompare(hi, lo) < 0)
                std::swap(lo, hi);
        }
        const bool band = lo.IsValid() && DateCompare(lo, hi) < 0;
        for (int m = 0; m < months; ++m)
        {
            int year = 0;
            int month = 0;
            CalendarShownMonth(state, m, &year, &month);
            const float left = origin.x + static_cast<float>(m) * (layout.monthWidth + layout.monthGap);
            // Weekday headings.
            {
                ControlTextScope footnote(TextStyle::Footnote);
                for (int col = 0; col < kCalendarDays; ++col)
                {
                    const Weekday day = static_cast<Weekday>((static_cast<int>(desc.firstWeekday) + col) % kCalendarDays);
                    const char* name = DateWeekdayName(day);
                    const FittedText shown = FitText(name, nullptr, layout.cell);
                    const float x = std::round(left + (static_cast<float>(col) + 0.5f) * layout.cell - shown.width * 0.5f);
                    drawList->AddText(ImVec2(x, CenteredTextY(gridArea.Min.y, layout.weekdays)), ToU32(c.labelSecondary),
                                      shown.text);
                }
            }
            const ImVec2 grid(left, gridArea.Min.y + layout.weekdays);
            state.gridOrigin[m] = grid;
            const int start = DateMonthStartColumn(year, month, desc.firstWeekday);
            const int length = DateMonthLength(year, month);
            const ImRect gridRect(grid, ImVec2(grid.x + layout.monthWidth, grid.y + layout.cell * kCalendarWeeks));
            if (mouseOver && gridRect.Contains(g.IO.MousePos))
            {
                const int col = static_cast<int>((g.IO.MousePos.x - grid.x) / layout.cell);
                const int row = static_cast<int>((g.IO.MousePos.y - grid.y) / layout.cell);
                const int day = row * kCalendarDays + col - start + 1;
                if (day >= 1 && day <= length)
                    result.pointer = Date{year, month, day};
            }
            for (int day = 1; day <= length; ++day)
            {
                const Date date{year, month, day};
                const int index = start + day - 1;
                const ImVec2 at(grid.x + static_cast<float>(index % kCalendarDays) * layout.cell,
                                grid.y + static_cast<float>(index / kCalendarDays) * layout.cell);
                const ImRect rect(at, ImVec2(at.x + layout.cell, at.y + layout.cell));
                const bool disabled = !DateWithin(date, desc.min, desc.max);
                const bool cursor = date == state.cursor;
                const bool end = lo.IsValid() && (date == lo || date == hi);
                const bool picked = (!desc.range && date == desc.picked) || end;
                // The band joins the ends: full cells between them, half cells under each end.
                if (band && rect.Max.x > 0.0f)
                {
                    const int vsLo = DateCompare(date, lo);
                    const int vsHi = DateCompare(date, hi);
                    if (vsLo >= 0 && vsHi <= 0)
                    {
                        ImRect strip(rect.Min.x, rect.Min.y + inset, rect.Max.x, rect.Max.y - inset);
                        if (vsLo == 0)
                            strip.Min.x = rect.GetCenter().x;
                        if (vsHi == 0)
                            strip.Max.x = rect.GetCenter().x;
                        FillRect(drawList, strip.Min, strip.Max, ToU32(c.accentSubtle));
                    }
                }
                ListRowDesc row;
                row.selected = picked;
                row.disabled = disabled;
                row.tabStop = cursor;
                row.part = cursor ? 0 : ++part;
                const ImGuiID cellId = CalendarKeyId(daySeed, DateSerial(date));
                const ListRowState cellRow = group.Row(cellId, rect, row);
                if (cursor)
                    ImGui::SetItemDefaultFocus();
                cellFocused = cellFocused || cellRow.focused;
                if (cellRow.item.visible)
                {
                    if (picked)
                        FillRect(drawList, ImVec2(rect.Min.x + inset, rect.Min.y + inset),
                                 ImVec2(rect.Max.x - inset, rect.Max.y - inset),
                                 ToU32(disabled ? c.fill[0] : c.accent), radius);
                    // Today: the accent hairline at the one outline width (the selected day shows by
                    // its fill instead).
                    if (date == today && !picked)
                    {
                        OutlineLook todayEdge;
                        todayEdge.selected = true;
                        todayEdge.disabled = disabled;
                        DrawOutline(drawList, ImRect(rect.Min.x + inset, rect.Min.y + inset, rect.Max.x - inset, rect.Max.y - inset),
                                    radius, ControlOutlineColor(todayEdge));
                    }
                    char number[8];
                    std::snprintf(number, sizeof(number), "%d", day);
                    const ImVec4 ink = disabled ? c.labelTertiary
                                       : picked ? c.onAccent
                                       : date == today ? c.accentText
                                                       : c.label;
                    CalendarCellText(drawList, rect, number, ToU32(ink), picked);
                }
                if (live && cellRow.item.activated && !cellRow.item.keyboardPressed && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    result.pressed = true;
                    result.pressedDate = date;
                }
                if (live && cellRow.item.pressed && !disabled)
                {
                    state.cursor = date;
                    if (!desc.readOnly)
                    {
                        result.picked = true;
                        result.date = date;
                        result.byKey = cellRow.item.keyboardPressed;
                    }
                }
            }
        }
    }
    else
    {
        // Month or year grid: twelve entries in the room of the day grid.
        const ImRect area(origin.x, gridArea.Min.y, origin.x + layout.monthWidth, panel.Max.y);
        state.gridOrigin[0] = area.Min;
        for (int i = 0; i < kCalendarBlock; ++i)
        {
            const bool months12 = state.view == CalendarView::Month;
            const int year = months12 ? state.cursor.year : blockStart + i;
            const int month = months12 ? i + 1 : 1;
            if (!months12 && (year < kDateMinYear || year > kDateMaxYear))
                continue;
            const ImRect rect = CalendarGridRect(area, i);
            const bool disabled = months12 ? !CalendarMonthAllowed(year, month, desc.min, desc.max)
                                           : !CalendarYearAllowed(year, desc.min, desc.max);
            const bool cursor = months12 ? month == state.cursor.month : year == state.cursor.year;
            const bool picked = desc.picked.IsValid() && desc.picked.year == year &&
                                (!months12 || desc.picked.month == month);
            ListRowDesc row;
            row.selected = picked;
            row.disabled = disabled;
            row.tabStop = cursor;
            row.part = cursor ? 0 : ++part;
            const ImGuiID cellId = months12 ? CalendarKeyId(monthSeed, CalendarMonthIndex(year, month))
                                            : CalendarKeyId(yearSeed, year);
            const ListRowState cellRow = group.Row(cellId, rect, row);
            if (cursor)
                ImGui::SetItemDefaultFocus();
            cellFocused = cellFocused || cellRow.focused;
            if (cellRow.item.visible)
            {
                if (picked)
                    FillRect(drawList, ImVec2(rect.Min.x + inset, rect.Min.y + inset),
                             ImVec2(rect.Max.x - inset, rect.Max.y - inset), ToU32(disabled ? c.fill[0] : c.accent), radius);
                char text[16];
                if (!months12)
                    std::snprintf(text, sizeof(text), "%d", year);
                const ImVec4 ink = disabled ? c.labelTertiary : picked ? c.onAccent : c.label;
                CalendarCellText(drawList, rect, months12 ? DateMonthName(month) : text, ToU32(ink), picked);
            }
            if (live && cellRow.item.pressed && !disabled)
            {
                Date chosen = state.cursor;
                chosen.year = year;
                if (months12)
                    chosen.month = month;
                chosen.day = std::min(chosen.day, DateMonthLength(chosen.year, chosen.month));
                chosen = DateClampTo(chosen, desc.min, desc.max);
                state.cursor = chosen;
                const bool pickHere = (months12 && desc.precision == DatePrecision::Month) ||
                                      (!months12 && desc.precision == DatePrecision::Year);
                if (pickHere)
                {
                    if (!desc.readOnly)
                    {
                        result.picked = true;
                        result.date = DateAtPrecision(chosen, desc.precision);
                        result.byKey = cellRow.item.keyboardPressed;
                    }
                }
                else
                {
                    // A wider view hands over to the narrower one at the chosen month or year.
                    state.view = months12 ? CalendarView::Day : CalendarView::Month;
                    state.year = chosen.year;
                    state.month = chosen.month;
                    if (cellRow.item.keyboardPressed)
                    {
                        const ImGuiID target = state.view == CalendarView::Day
                                                   ? CalendarKeyId(daySeed, DateSerial(chosen))
                                                   : CalendarKeyId(monthSeed, CalendarMonthIndex(chosen.year, chosen.month));
                        ListFocusRow(window, target, focusScope, rect);
                    }
                }
            }
        }
    }
    group.End();

    // ---- Keys on a focused cell: the cursor moves by days, weeks, months and years ----
    result.focused = cellFocused;
    if (live && cellFocused)
    {
        const ImGuiKey keys[] = {ImGuiKey_LeftArrow, ImGuiKey_RightArrow, ImGuiKey_UpArrow, ImGuiKey_DownArrow,
                                 ImGuiKey_PageUp,    ImGuiKey_PageDown,   ImGuiKey_Home,    ImGuiKey_End};
        for (const ImGuiKey key : keys)
            ImGui::SetKeyOwner(key, panelId);
        const auto pressed = [&](ImGuiKey key) { return ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, panelId); };
        const bool shift = g.IO.KeyShift;
        Date next = state.cursor;
        bool moved = false;
        if (state.view == CalendarView::Day)
        {
            const int column = (static_cast<int>(DateWeekdayOf(state.cursor)) - static_cast<int>(desc.firstWeekday) +
                                kCalendarDays) % kCalendarDays;
            if (pressed(ImGuiKey_LeftArrow)) { next = DateAddDays(next, -1); moved = true; }
            if (pressed(ImGuiKey_RightArrow)) { next = DateAddDays(next, 1); moved = true; }
            if (pressed(ImGuiKey_UpArrow)) { next = DateAddDays(next, -kCalendarDays); moved = true; }
            if (pressed(ImGuiKey_DownArrow)) { next = DateAddDays(next, kCalendarDays); moved = true; }
            if (pressed(ImGuiKey_PageUp)) { next = DateAddMonths(next, shift ? -12 : -1); moved = true; }
            if (pressed(ImGuiKey_PageDown)) { next = DateAddMonths(next, shift ? 12 : 1); moved = true; }
            if (pressed(ImGuiKey_Home)) { next = DateAddDays(next, -column); moved = true; }
            if (pressed(ImGuiKey_End)) { next = DateAddDays(next, kCalendarDays - 1 - column); moved = true; }
        }
        else
        {
            const int unit = state.view == CalendarView::Month ? 1 : 12;  // months per entry
            const int index = state.view == CalendarView::Month ? state.cursor.month - 1 : state.cursor.year - blockStart;
            if (pressed(ImGuiKey_LeftArrow)) { next = DateAddMonths(next, -unit); moved = true; }
            if (pressed(ImGuiKey_RightArrow)) { next = DateAddMonths(next, unit); moved = true; }
            if (pressed(ImGuiKey_UpArrow)) { next = DateAddMonths(next, -unit * kCalendarGridColumns); moved = true; }
            if (pressed(ImGuiKey_DownArrow)) { next = DateAddMonths(next, unit * kCalendarGridColumns); moved = true; }
            if (pressed(ImGuiKey_PageUp)) { next = DateAddMonths(next, -unit * kCalendarBlock); moved = true; }
            if (pressed(ImGuiKey_PageDown)) { next = DateAddMonths(next, unit * kCalendarBlock); moved = true; }
            if (pressed(ImGuiKey_Home)) { next = DateAddMonths(next, -unit * index); moved = true; }
            if (pressed(ImGuiKey_End)) { next = DateAddMonths(next, unit * (kCalendarBlock - 1 - index)); moved = true; }
        }
        next = DateClampTo(next, desc.min, desc.max);
        if (moved && !(next == state.cursor))
        {
            state.cursor = next;
            ImRect rect;
            ImGuiID target = 0;
            if (state.view == CalendarView::Day)
            {
                CalendarRevealCursor(state, months);
                rect = CalendarDayRect(state, next);
                target = CalendarKeyId(daySeed, DateSerial(next));
            }
            else
            {
                const ImRect area(origin.x, gridArea.Min.y, origin.x + layout.monthWidth, panel.Max.y);
                const int block = CalendarBlockStart(next.year);
                const int index = state.view == CalendarView::Month ? next.month - 1 : next.year - block;
                rect = CalendarGridRect(area, index);
                target = state.view == CalendarView::Month ? CalendarKeyId(monthSeed, CalendarMonthIndex(next.year, next.month))
                                                           : CalendarKeyId(yearSeed, next.year);
            }
            ListFocusRow(window, target, focusScope, rect);
        }
    }

    // The panel is one block of layout, whatever its parts did with the cursor.
    ImGui::SetCursorScreenPos(origin);
    ImGui::ItemSize(layout.size);
    return result;
}

// ---- Segment stepping ---------------------------------------------------------------------------

PickerSegmentStep PickerSegmentStepRun(PickerSegmentStepper* stepper, const ItemState& item, const ImRect& frame,
                                       bool readOnly)
{
    PickerSegmentStep out;
    if (stepper == nullptr || ImGui::GetCurrentContext() == nullptr)
        return out;
    ImGuiContext& g = *GImGui;
    const Theme& t = T();
    const float threshold = Px(t.drag.startThreshold);
    const bool pastThreshold = g.IO.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= threshold * threshold;
    // A press only becomes a drag past the threshold: a plain click is the field's own action.
    ItemState input = item;
    if (input.held && !pastThreshold)
        input.held = false;
    input.pressed = false;
    ValueDragOptions drag;
    drag.mapping.min = -std::numeric_limits<double>::infinity();
    drag.mapping.max = std::numeric_limits<double>::infinity();
    drag.mapping.integer = true;
    drag.orientation = Orientation::Vertical;
    drag.grabRadius = -1.0f;
    drag.speed = 1.0 / std::max(1.0, static_cast<double>(Px(t.spacing.sm)));
    drag.keyStep = 1.0;
    drag.readOnly = readOnly;
    const ValueDragResult edit = ValueDragBehavior(input, &stepper->counter, frame, drag);
    const bool inSession = edit.dragging || edit.keyboard || g.ActiveId == item.id;
    if (inSession && !stepper->session)
        out.started = true;
    stepper->session = inSession;
    out.dragging = edit.dragging;
    out.canceled = edit.canceled;
    const long long target = std::llround(stepper->counter);
    if (!edit.canceled)
        out.steps = static_cast<int>(std::clamp<long long>(target - stepper->applied, -100000, 100000));
    stepper->applied = target;
    if (!inSession || edit.canceled)
    {
        // Between sessions the counter starts again from zero, so it never grows large.
        stepper->counter = 0.0;
        stepper->applied = 0;
    }
    return out;
}

} // namespace facet::internal
