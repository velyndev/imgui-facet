// DatePicker: a segmented date field (year, month and day in ISO order) with a calendar in a
// popover or in place.
//
// The field is one item (item behaviour). Left and Right pick the segment; Up and Down, the wheel
// while focused and a vertical drag step it through the value drag core (date_picker_parts.h,
// PickerSegmentStepRun), so a step is an edit session that writes at once and Escape restores the
// date from its start. A step wraps inside its segment (day 31 goes to 1 of the same month) and
// keeps the date real: the day follows the length of the month. Typed digits fill the focused
// segment and move on to the next one; they are shown as typed and written on Enter, Tab or focus
// loss when they make a real date inside [min, max], and Escape drops them. Backspace or Delete
// clears the segment; a field whose segments are all cleared writes no date.
//
// A click, Enter, Space or Alt+Down opens the calendar (date_picker_parts.h, CalendarPanel) in a
// popover of the popup host beside the field; the keyboard focus moves to the date, a pick writes
// and closes it, and the focus returns to the field.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "internal/value_drag.h"
#include "widgets/lists/list_parts.h"
#include "widgets/pickers/date_picker_parts.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;

constexpr int kDateSegments = 3;
// Digits each segment takes: year, month, day.
constexpr int kDateSegmentDigits[kDateSegments] = {4, 2, 2};

struct DatePickerState
{
    int segment = -1;               // the segment the keys edit; -1 before the first frame
    int parts[kDateSegments] = {};  // year, month and day while typing
    bool set[kDateSegments] = {};   // which parts hold a typed or kept value
    bool editing = false;           // typed parts wait for Enter, Tab or focus loss
    int typed = 0;                  // digits typed into the focused segment
    bool mixedCleared = false;      // written since `mixed` was switched on
    bool wasFocused = false;
    Date sessionStart;              // the date when a step session began
    internal::PickerSegmentStepper stepper;
};

int DateSegmentCount(DatePrecision precision)
{
    return precision == DatePrecision::Year ? 1 : precision == DatePrecision::Month ? 2 : 3;
}

int DateSegmentMax(int segment)
{
    return segment == 0 ? internal::kDateMaxYear : segment == 1 ? 12 : 31;
}

// The text of each segment as shown, and whether it holds a value.
struct DateShown
{
    char text[kDateSegments][8] = {};
    bool set[kDateSegments] = {};
};

DateShown DateShownParts(const int parts[kDateSegments], const bool set[kDateSegments])
{
    DateShown shown;
    for (int i = 0; i < kDateSegments; ++i)
    {
        shown.set[i] = set[i];
        if (set[i])
            std::snprintf(shown.text[i], sizeof(shown.text[i]), i == 0 ? "%04d" : "%02d", std::clamp(parts[i], 0, 9999));
        else
            std::snprintf(shown.text[i], sizeof(shown.text[i]), "%s", i == 0 ? "----" : "--");
    }
    return shown;
}

// The parts a date shows; no date: nothing set.
void DateToParts(const Date& date, int parts[kDateSegments], bool set[kDateSegments])
{
    const bool valid = date.IsValid();
    parts[0] = date.year;
    parts[1] = date.month;
    parts[2] = date.day;
    for (int i = 0; i < kDateSegments; ++i)
        set[i] = valid;
}

// A step of `steps` on `segment` of `from` (a real date), wrapping inside the segment.
Date DateStepSegment(Date from, int segment, int steps)
{
    if (segment == 0)
    {
        from.year = std::clamp(from.year + steps, internal::kDateMinYear, internal::kDateMaxYear);
    }
    else if (segment == 1)
    {
        from.month = ((from.month - 1 + steps) % 12 + 12) % 12 + 1;
    }
    else
    {
        const int length = internal::DateMonthLength(from.year, from.month);
        from.day = ((from.day - 1 + steps) % length + length) % length + 1;
    }
    from.day = std::min(from.day, internal::DateMonthLength(from.year, from.month));
    return from;
}

// The x of every segment and its width, from `left`.
struct DateGeometry
{
    float x[kDateSegments] = {};
    float width[kDateSegments] = {};
};

DateGeometry DateMeasure(const DateShown& shown, int count, float left, float gap)
{
    DateGeometry geometry;
    float x = left;
    for (int i = 0; i < count; ++i)
    {
        if (i > 0)
            x += ImGui::CalcTextSize("-").x + gap * 2.0f;
        geometry.x[i] = x;
        geometry.width[i] = ImGui::CalcTextSize(shown.text[i]).x;
        x += geometry.width[i];
    }
    return geometry;
}

} // namespace

bool DatePicker(const char* label, Date* date, const DatePickerOptions& options)
{
    using namespace internal;
    // 1. Entry.
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const ControlMetrics m = GetControlMetrics(options.size);
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID id = window->GetID(label);
    const ImGuiID level = ImHashStr("##calendar", 0, id);
    const ImGuiID inlinePanel = ImHashStr("##inline", 0, id);
    DatePickerState& state = GetState<DatePickerState>(id);
    const int count = DateSegmentCount(options.precision);
    if (state.segment < 0 || state.segment >= count)
        state.segment = count - 1;
    const bool readOnly = options.readOnly || date == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool mixedFlag = options.mixed || (next.itemFlags & ImGuiItemFlags_MixedValue) != 0;
    if (!mixedFlag)
        state.mixedCleared = false;
    Date min = options.min;
    Date max = options.max;
    if (min.IsValid() && max.IsValid() && DateCompare(min, max) > 0)
        std::swap(min, max);
    const Date current = date != nullptr ? *date : Date{};
    if (!state.editing)
        DateToParts(current, state.parts, state.set);

    // The forced look of a capture: part -1 or 0 is the field, part 1 the inline calendar's cursor.
    ForcedItemRequest fieldForced;
    ForcedItemRequest panelForced;
    if (forced.part <= 0)
        fieldForced = forced;
    else if (forced.part == 1)
        panelForced = {forced.state, 0};

    // 2. Measure: a field widget fills by default; an inline calendar fits the calendar.
    CalendarPanelDesc panel;
    panel.min = min;
    panel.max = max;
    panel.firstWeekday = options.firstWeekday;
    panel.precision = options.precision;
    panel.picked = current.IsValid() ? DateAtPrecision(current, options.precision) : Date{};
    panel.readOnly = readOnly;
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    float defaultWidth = kFill;
    // An inline calendar fits its content like a compact control; the field fills like any field.
    const FrontLabelFit fit = options.inlineCalendar ? CompactFrontLabelFit(options.width, next) : FrontLabelFit::Column;
    if (options.inlineCalendar)
    {
        const FrontLabel natural = MeasureFrontLabel(label, m.size, available * 2.0f, fit);
        defaultWidth = std::min(available, natural.width + CalendarPanelSize(panel).x);
    }
    const float width = ResolveItemWidth(options.width, next, defaultWidth);
    const FrontLabel front = MeasureFrontLabel(label, m.size, width, fit);

    // 3. The field item.
    ImGui::BeginGroup();
    const float top = window->DC.CursorPos.y;
    ItemOptions itemOptions;
    itemOptions.press = PressMode::ClickRelease;
    itemOptions.readOnly = readOnly;
    itemOptions.mixed = mixedFlag;
    itemOptions.baseline = CenteredTextY(top, m.height) - top;
    itemOptions.forced = &fieldForced;
    ApplyNextItemData(next);
    const ItemState item = ItemBehavior(id, ImVec2(width, m.height), itemOptions);
    const ImRect frame(item.rect.Min.x + front.width, item.rect.Min.y, item.rect.Max.x, item.rect.Max.y);
    const bool live = !item.disabled && item.forced == ForcedItemState::None;
    const bool open = IsPopupLevelOpen(level);
    ControlTextScope text(m);
    const float separatorGap = std::round(Px(t.spacing.xs) * 0.5f);
    const DateShown measured = DateShownParts(state.parts, state.set);
    const DateGeometry geometry = DateMeasure(measured, count, frame.Min.x + m.fieldPadding, separatorGap);

    bool written = false;
    const auto write = [&](const Date& value) {
        if (date == nullptr || readOnly)
            return;
        if (!(value == *date))
        {
            *date = value;
            written = true;
        }
        state.mixedCleared = true;
    };
    // Typed parts become the date when they make a real one inside the range.
    const auto commit = [&]() {
        bool any = false;
        for (int i = 0; i < count; ++i)
            any = any || state.set[i];
        state.typed = 0;
        if (!any)
        {
            write(Date{});
            state.editing = false;
            return;
        }
        const Date typed{state.set[0] ? state.parts[0] : 0, count > 1 ? (state.set[1] ? state.parts[1] : 0) : 1,
                         count > 2 ? (state.set[2] ? state.parts[2] : 0) : 1};
        if (!DateIsReal(typed))
            return;  // shown as typed until it is corrected
        write(DateClampTo(typed, min, max));
        state.editing = false;
    };

    bool openNow = false;
    if (live && !readOnly)
    {
        // A press picks the segment under the pointer.
        if (item.activated && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            for (int i = 0; i < count; ++i)
                if (g.IO.MousePos.x >= geometry.x[i] - separatorGap)
                    state.segment = i;
            state.typed = 0;
        }
        if (item.focused && !open)
        {
            ImGui::SetKeyOwner(ImGuiKey_LeftArrow, id);
            ImGui::SetKeyOwner(ImGuiKey_RightArrow, id);
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, ImGuiInputFlags_Repeat, id))
            {
                state.segment = std::max(0, state.segment - 1);
                state.typed = 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, ImGuiInputFlags_Repeat, id))
            {
                state.segment = std::min(count - 1, state.segment + 1);
                state.typed = 0;
            }
            // Typed digits fill the segment; a separator moves to the next one.
            for (const ImWchar ch : g.IO.InputQueueCharacters)
            {
                if (ch >= '0' && ch <= '9')
                {
                    if (!state.editing)
                    {
                        state.editing = true;
                        state.typed = 0;
                    }
                    const int digit = static_cast<int>(ch - '0');
                    int& part = state.parts[state.segment];
                    part = state.typed == 0 || !state.set[state.segment] ? digit : part * 10 + digit;
                    state.set[state.segment] = true;
                    ++state.typed;
                    const bool full = state.typed >= kDateSegmentDigits[state.segment] ||
                                      part * 10 > DateSegmentMax(state.segment);
                    if (full)
                    {
                        state.typed = 0;
                        if (state.segment < count - 1)
                            ++state.segment;
                    }
                }
                else if ((ch == '-' || ch == '/' || ch == '.') && state.segment < count - 1)
                {
                    ++state.segment;
                    state.typed = 0;
                }
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace, ImGuiInputFlags_None) ||
                ImGui::IsKeyPressed(ImGuiKey_Delete, ImGuiInputFlags_None))
            {
                state.editing = true;
                state.set[state.segment] = false;
                state.typed = 0;
            }
            // Escape drops typed parts before any layer sees it.
            if (state.editing)
            {
                ImGui::SetKeyOwner(ImGuiKey_Escape, id);
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
                {
                    state.editing = false;
                    state.typed = 0;
                    DateToParts(current, state.parts, state.set);
                    MarkEditCanceled(id);
                }
            }
            // Alt+Down opens; plain Up and Down step.
            if (g.IO.KeyAlt)
            {
                ImGui::SetKeyOwner(ImGuiKey_DownArrow, id);
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_None, id))
                    openNow = true;
            }
        }
    }

    // Steps: Up and Down, the wheel and a vertical drag on the focused segment.
    if (!g.IO.KeyAlt || !item.focused)
    {
        const PickerSegmentStep step = PickerSegmentStepRun(&state.stepper, item, frame, readOnly || !live || open);
        if (step.started)
            state.sessionStart = current;
        if (step.canceled)
        {
            write(state.sessionStart);
            state.editing = false;
        }
        else if (step.steps != 0 && !readOnly)
        {
            Date from = state.editing ? Date{state.set[0] ? state.parts[0] : 0, state.set[1] ? state.parts[1] : 0,
                                             state.set[2] ? state.parts[2] : 0}
                                      : current;
            // Parts without a value start from today's.
            const Date today = DateToday();
            if (from.year < kDateMinYear || from.year > kDateMaxYear)
                from.year = today.year;
            if (from.month < 1 || from.month > 12)
                from.month = today.month;
            if (from.day < 1)
                from.day = today.day;
            from.day = std::min(from.day, DateMonthLength(from.year, from.month));
            const Date stepped = DateAtPrecision(DateStepSegment(from, state.segment, step.steps), options.precision);
            write(DateClampTo(stepped, min, max));
            state.editing = false;
            state.typed = 0;
        }
    }

    // Enter or Space while typing commits; otherwise they open, like a click without a drag.
    if (live && !readOnly && item.pressed)
    {
        const float threshold = Px(t.drag.startThreshold);
        const bool dragged = g.IO.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= threshold * threshold;
        if (item.keyboardPressed && state.editing)
            commit();
        else if (item.keyboardPressed || !dragged)
            openNow = !open;
    }
    // Tab or a click elsewhere commits typed parts.
    if (state.editing && state.wasFocused && !item.focused)
        commit();
    state.wasFocused = item.focused;
    if (openNow && state.editing)
        commit();
    if (written || !state.editing)
        DateToParts(date != nullptr ? *date : Date{}, state.parts, state.set);

    // 4. Draw the field.
    ImDrawList* drawList = window->DrawList;
    const bool mixed = mixedFlag && !state.mixedCleared;
    const Date shownDate = date != nullptr ? *date : Date{};
    const float iconSize = text.IconSize();
    const float iconX = frame.Max.x - m.fieldPadding - iconSize;
    if (item.visible)
    {
        DrawFieldFrame(drawList, frame,
                       {.hoverAmount = item.hoverAmount,
                        .active = open || item.open,
                        .disabled = item.disabled,
                        .readOnly = readOnly,
                        .error = options.error != nullptr});
        const float textY = CenteredTextY(frame.Min.y, frame.GetHeight());
        const float textLeft = frame.Min.x + m.fieldPadding;
        const float textRight = std::max(textLeft, iconX - m.iconGap);
        ImGui::PushClipRect(ImVec2(frame.Min.x + m.fieldPadding * 0.5f, frame.Min.y), ImVec2(textRight, frame.Max.y), true);
        const bool showSegments = state.editing || shownDate.IsValid() || (item.focusVisible && !readOnly);
        if (mixed && !state.editing)
        {
            drawList->AddText(ImVec2(textLeft, textY), ToU32(ControlSecondaryColor(item.disabled)), GetStrings().mixed);
        }
        else if (!showSegments)
        {
            const char* placeholder = options.placeholder != nullptr ? options.placeholder : GetStrings().select;
            const FittedText shown = FitText(placeholder, nullptr, std::max(1.0f, textRight - textLeft));
            drawList->AddText(ImVec2(textLeft, textY), ToU32(ControlSecondaryColor(item.disabled)), shown.text);
        }
        else
        {
            const DateShown shown = DateShownParts(state.parts, state.set);
            const DateGeometry at = DateMeasure(shown, count, textLeft, separatorGap);
            // The segment the keys edit: accentSubtle behind it while the keyboard is on the field.
            if (!item.disabled && !readOnly && (item.focusVisible || state.editing))
            {
                const ImVec2 hiMin(at.x[state.segment] - separatorGap, frame.Min.y + Px(t.spacing.xs));
                const ImVec2 hiMax(at.x[state.segment] + at.width[state.segment] + separatorGap,
                                   frame.Max.y - Px(t.spacing.xs));
                FillRect(drawList, hiMin, hiMax, ToU32(c.accentSubtle), Px(t.radii.element));
            }
            const ImU32 digits = ToU32(ControlLabelColor(item.disabled));
            const ImU32 marks = ToU32(ControlSecondaryColor(item.disabled));
            for (int i = 0; i < count; ++i)
            {
                if (i > 0)
                    drawList->AddText(ImVec2(at.x[i] - separatorGap - ImGui::CalcTextSize("-").x, textY), marks, "-");
                drawList->AddText(ImVec2(at.x[i], textY), shown.set[i] ? digits : marks, shown.text[i]);
            }
        }
        ImGui::PopClipRect();
        if (iconX > textLeft)
            DrawControlIcon(drawList, Icon("calendar"), iconX, frame.GetCenter().y, iconSize,
                            ToU32(ControlSecondaryColor(item.disabled)));
        if (front.text != nullptr)
            DrawFrontLabel(drawList, front, ImVec2(item.rect.Min.x, textY), m.size, item.disabled);
        DrawFocusRing(drawList, frame, m.radius, item.focusAmount);
    }
    CutTextTooltip(item, front.text, front.end, front.shown.truncated);
    const ControlMessage message = PickControlMessage(options.error, options.helpText);
    const float messageBottom =
        DrawControlMessage(message, ImVec2(frame.Min.x, frame.Max.y), frame.GetWidth(), item.disabled, id, frame);

    // 5. The calendar: in place, or in a popover beside the field (PanelAnchor).
    if (options.inlineCalendar)
    {
        const float gap = PxSnapped(t.spacing.sm);
        const float cellFit = std::floor(frame.GetWidth() / 7.0f);
        panel.cell = std::min(std::floor(Px(t.picker.dayCell)), std::max(1.0f, cellFit));
        panel.live = !item.disabled && panelForced.state == ForcedItemState::None;
        panel.forced = &panelForced;
        ImGui::SetCursorScreenPos(ImVec2(frame.Min.x, messageBottom + gap));
        ImGui::BeginDisabled(item.disabled);
        const CalendarPanelResult picked = CalendarPanel(inlinePanel, panel);
        ImGui::EndDisabled();
        if (picked.picked && !readOnly)
        {
            write(DateAtPrecision(picked.date, options.precision));
            state.editing = false;
        }
    }
    else
    {
        const PopupAnchor anchor = PanelAnchor(frame);
        PopupOptions popup;
        popup.glass = true;
        if (openNow && !readOnly)
        {
            CalendarPanelOpen(level, current, panel);
            OpenPopupLevel(level, anchor, popup);
        }
        if (item.open)
            ForcePopupLevelOpen(level, anchor, popup);
        SetPopupLevelAnchor(level, anchor);
        if (BeginPopupLevel(level))
        {
            panel.live = !item.open;
            const CalendarPanelResult picked = CalendarPanel(level, panel);
            if (picked.picked && !readOnly)
            {
                write(DateAtPrecision(picked.date, options.precision));
                state.editing = false;
                if (picked.byKey)
                    ListsCloseByKey(level);
                else
                    ClosePopupLevel(level);
            }
            EndPopupLevel();
        }
    }
    ImGui::EndGroup();
    FinishWidgetGroup(id);
    if (written)
    {
        DateToParts(*date, state.parts, state.set);
        ListsMarkEdited();
    }
    return written;
}

} // namespace facet
