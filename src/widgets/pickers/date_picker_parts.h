#pragma once

// Parts that DatePicker, DateRangePicker and TimePicker (pickers.h) share: plain calendar
// arithmetic on the proleptic Gregorian calendar, the text of a date, the calendar panel with
// its day, month and year views, and the stepping of one segment of a segmented field. Names
// carry the widgets they belong to ("Date", "Calendar", "PickerSegment"), because every family
// links into one library.
//
// The calendar panel, every frame while it is shown (inside a popup level or in the page):
//
//     CalendarPanelDesc desc{.min = options.min, .max = options.max, .picked = *date};
//     if (opened) CalendarPanelOpen(panelId, *date, desc);    // the view starts at the date
//     const CalendarPanelResult result = CalendarPanel(panelId, desc);
//     if (result.picked) *date = result.date;
//
// The panel is a header (previous, the title, next) over a grid. In the day view the grid is the
// weekday headings and six weeks of day cells, one or two months side by side; the month and
// year views put twelve months or twelve years in the same room, so the panel keeps its size.
// The cells are rows of the list core's row block (ListRowGroup), so they hover, focus and draw
// the focus ring like list rows; the panel moves the focus itself with the arrows, Page Up and
// Down, Home and End, across month and year borders, and keeps it inside [min, max].

#include <cstddef>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/widgets/pickers.h"
#include "internal/item.h"

namespace facet::internal
{

// ---- Calendar arithmetic --------------------------------------------------------------------

inline constexpr int kDateMinYear = 1;
inline constexpr int kDateMaxYear = 9999;

// Leap years of the Gregorian calendar: every fourth year, except centuries not divisible by 400.
bool DateIsLeapYear(int year);
// Days in `month` (1 to 12) of `year`: 28 to 31; 0 for a month outside 1 to 12.
int DateMonthLength(int year, int month);
// A real calendar date: year 1 to 9999, month 1 to 12, day within the month.
bool DateIsReal(const Date& date);
// Days from 0001-01-01 (serial 0) to `date`, which must be real.
int DateSerial(const Date& date);
// The date of a serial, clamped to 0001-01-01 and 9999-12-31.
Date DateFromSerial(int serial);
// The weekday of a real date (0001-01-01 was a Monday).
Weekday DateWeekdayOf(const Date& date);
// `date` moved by whole days, clamped to the years 1 to 9999.
Date DateAddDays(const Date& date, int days);
// `date` moved by whole months, the day clamped to the length of the new month, the year to 1
// to 9999.
Date DateAddMonths(const Date& date, int months);
// -1, 0 or 1 by year, then month, then day.
int DateCompare(const Date& a, const Date& b);
// Whether `date` lies in [min, max]; a bound without a date does not limit.
bool DateWithin(const Date& date, const Date& min, const Date& max);
// `date` moved into [min, max].
Date DateClampTo(const Date& date, const Date& min, const Date& max);
// Today in local time.
Date DateToday();
// The column (0 to 6) of the first day of a month in a week that starts on `first`.
int DateMonthStartColumn(int year, int month, Weekday first);
// Strings::monthJanuary to monthDecember; "" outside 1 to 12.
const char* DateMonthName(int month);
// Strings::weekdayMonday to weekdaySunday.
const char* DateWeekdayName(Weekday day);
// The text of a date, ISO 8601 order so it reads the same everywhere: "2026-11-21", "2026-11"
// at Month precision, "2026" at Year precision; "" for no date.
void DateFormat(char* out, std::size_t size, const Date& date, DatePrecision precision);
// The date written at a precision: Month sets the day to 1, Year month and day.
Date DateAtPrecision(const Date& date, DatePrecision precision);

// ---- Calendar panel -----------------------------------------------------------------------------

enum class CalendarView
{
    Day,
    Month,
    Year,
};

struct CalendarPanelDesc
{
    Date min;                                 // no date: unlimited
    Date max;
    Weekday firstWeekday = Weekday::Monday;
    DatePrecision precision = DatePrecision::Day;  // the view a pick happens in
    int months = 1;                           // day view: 1 or 2 months side by side (range); 2 has no month and year views
    Date picked;                              // single: the picked date (accent fill)
    bool range = false;                       // rangeStart and rangeEnd are the ends of a band
    Date rangeStart;
    Date rangeEnd;                            // no date while the end is being picked
    Date bandTo;                              // range with no end: the band runs from rangeStart to here
    float cell = 0.0f;                        // pixels of one day cell; 0 for picker.dayCell
    bool readOnly = false;                    // cells focus and move, nothing is picked
    bool live = true;                         // false: a capture shows the panel; no input is read
    const ForcedItemRequest* forced = nullptr;  // part 0: the cell of the keyboard cursor
};

struct CalendarPanelResult
{
    bool picked = false;     // a date was chosen at the panel's precision (click, Enter or Space)
    Date date;               // the chosen date
    bool byKey = false;      // the pick came from a key
    Date pointer;            // the day under the pointer in the day view; no date elsewhere
    bool pressed = false;    // the mouse went down on a day cell this frame
    Date pressedDate;
    bool focused = false;    // a cell of the panel has the keyboard focus
};

// Starts the panel at `focus` (no date: today), clamped to [min, max], in the view of the
// precision. Call when the popover opens, before CalendarPanel.
void CalendarPanelOpen(ImGuiID panelId, const Date& focus, const CalendarPanelDesc& desc);
// The size of the panel in pixels.
ImVec2 CalendarPanelSize(const CalendarPanelDesc& desc);
// Draws the panel at the cursor of the current window and runs its input. State (view, shown
// month, keyboard cursor) under `panelId`. Advances the cursor like one item.
CalendarPanelResult CalendarPanel(ImGuiID panelId, const CalendarPanelDesc& desc);
// The id and the rectangle (screen pixels, last frame) of the day cell of `date` in the panel, for
// tests and selftests; an empty rect when the day is not shown.
ImGuiID CalendarDayCellId(ImGuiID panelId, const Date& date);
ImRect CalendarDayCellRect(ImGuiID panelId, const Date& date);
// The keyboard cursor and the first shown month of the panel.
Date CalendarPanelCursor(ImGuiID panelId);
Date CalendarPanelShownMonth(ImGuiID panelId);
CalendarView CalendarPanelView(ImGuiID panelId);
// The previous and next buttons and the title of the header, last frame (screen pixels).
ImRect CalendarPanelButtonRect(ImGuiID panelId, int button);  // 0 previous, 1 title, 2 next

// ---- Segment stepping ---------------------------------------------------------------------------

// The stepping of the focused segment of a segmented field (the date and time pickers): Up and
// Down while focused, the wheel while focused and hovered, and a vertical drag on the field run
// through the value drag core, so they make edit sessions, repeat at the key repeat rate and
// cancel with Escape. The caller keeps one PickerSegmentStepper under its id and applies the
// whole steps it returns to the focused segment.
struct PickerSegmentStepper
{
    double counter = 0.0;  // the value the drag core edits; whole numbers are steps
    long long applied = 0; // the steps already returned
    bool session = false;  // a key, wheel or drag session is under way
};

struct PickerSegmentStep
{
    int steps = 0;          // whole steps to apply this frame (negative: down)
    bool canceled = false;  // Escape ended the session: restore the value from its start
    bool started = false;   // a session started this frame: remember the value
    bool dragging = false;
};

// `item` is the field (ItemBehavior); `frame` its frame. A press grabs (no jump), a drag of
// spacing.sm per step moves up or down. Read-only or disabled items return no steps.
PickerSegmentStep PickerSegmentStepRun(PickerSegmentStepper* stepper, const ItemState& item, const ImRect& frame,
                                       bool readOnly);

} // namespace facet::internal
