// TimePicker: a segmented time field (hours, minutes, optional seconds and, on a 12-hour clock, AM
// or PM) with scrolling columns in a popover or in place.
//
// The field works like the DatePicker field: one item, Left and Right pick the segment, Up and Down
// (and the wheel while focused, and a vertical drag) step it through the value drag core and write
// at once, wrapping around midnight with the carry into the next larger unit (date_picker_parts.h,
// TimeStep); minutes step by minuteStep. Typed digits fill the segment and are written on Enter, Tab
// or focus loss when they make a valid time; A or P sets the half of the day. A click, Enter, Space
// or Alt+Down opens the columns: one list view of the list core per unit, with the chosen entry on
// the selection pill. A click or an arrow key in a column writes at once; Enter or a double click
// writes and closes.

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
#include "internal/list_view.h"
#include "internal/popup_host.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "internal/value_drag.h"
#include "widgets/lists/list_parts.h"
#include "widgets/pickers/date_picker_parts.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;
using internal::TimeSegment;

constexpr int kTimeSegments = 4;
// Visible rows of a column.
constexpr int kTimeColumnRows = 5;

// "00" to "59", the labels of the column rows (stable for the list view's typeahead).
struct TimeDigits
{
    char text[60][3] = {};
    TimeDigits()
    {
        for (int i = 0; i < 60; ++i)
            std::snprintf(text[i], sizeof(text[i]), "%02d", i);
    }
};

const TimeDigits& Digits()
{
    static const TimeDigits digits;
    return digits;
}

struct TimePickerState
{
    int segment = 0;                // index into the shown segments
    int parts[kTimeSegments] = {};  // hour (as shown), minute, second, 1 for PM, while typing
    bool editing = false;
    int typed = 0;
    bool mixedCleared = false;
    bool wasFocused = false;
    int revealed = -1;              // the time (seconds) the columns last scrolled to; -1 none
    int generation = 0;             // openings of the popover (TimeColumns)
    TimeOfDay sessionStart;
    internal::PickerSegmentStepper stepper;
};

// The segments a field shows, in order.
struct TimeLayout
{
    TimeSegment units[kTimeSegments] = {};
    int count = 0;
};

TimeLayout TimeSegments(const TimePickerOptions& options)
{
    TimeLayout layout;
    layout.units[layout.count++] = TimeSegment::Hour;
    layout.units[layout.count++] = TimeSegment::Minute;
    if (options.showSeconds)
        layout.units[layout.count++] = TimeSegment::Second;
    if (options.twelveHour)
        layout.units[layout.count++] = TimeSegment::HalfDay;
    return layout;
}

void TimeToParts(const TimeOfDay& time, bool twelveHour, int parts[kTimeSegments])
{
    const int hour = std::clamp(time.hour, 0, 23);
    parts[0] = twelveHour ? internal::TimeHour12(hour) : hour;
    parts[1] = std::clamp(time.minute, 0, 59);
    parts[2] = std::clamp(time.second, 0, 59);
    parts[3] = hour >= 12 ? 1 : 0;
}

// The time typed parts make; false when a part is out of its range.
bool TimeFromParts(const int parts[kTimeSegments], bool twelveHour, TimeOfDay* out)
{
    const int hour = parts[0];
    if (twelveHour ? (hour < 1 || hour > 12) : (hour < 0 || hour > 23))
        return false;
    if (parts[1] < 0 || parts[1] > 59 || parts[2] < 0 || parts[2] > 59)
        return false;
    out->hour = twelveHour ? hour % 12 + (parts[3] != 0 ? 12 : 0) : hour;
    out->minute = parts[1];
    out->second = parts[2];
    return true;
}

int TimePartIndex(TimeSegment segment)
{
    return static_cast<int>(segment);
}

// The text of a segment as shown.
const char* TimePartText(TimeSegment segment, const int parts[kTimeSegments])
{
    if (segment == TimeSegment::HalfDay)
        return parts[3] != 0 ? GetStrings().pm : GetStrings().am;
    return Digits().text[std::clamp(parts[TimePartIndex(segment)], 0, 59)];
}

struct TimeGeometry
{
    float x[kTimeSegments] = {};
    float width[kTimeSegments] = {};
};

// Segments are separated by a colon, the half of the day by a space.
TimeGeometry TimeMeasure(const TimeLayout& layout, const int parts[kTimeSegments], float left, float gap)
{
    TimeGeometry geometry;
    float x = left;
    for (int i = 0; i < layout.count; ++i)
    {
        if (i > 0)
            x += layout.units[i] == TimeSegment::HalfDay ? gap * 2.0f : ImGui::CalcTextSize(":").x + gap * 2.0f;
        geometry.x[i] = x;
        geometry.width[i] = ImGui::CalcTextSize(TimePartText(layout.units[i], parts)).x;
        x += geometry.width[i];
    }
    return geometry;
}

// The columns of the popover: entries per unit, the labels and the chosen entry.
struct TimeColumnSource
{
    TimeSegment unit = TimeSegment::Hour;
    bool twelveHour = false;
    int minuteStep = 1;

    int Count() const
    {
        switch (unit)
        {
        case TimeSegment::Hour:
            return twelveHour ? 12 : 24;
        case TimeSegment::Minute:
            return 59 / minuteStep + 1;
        case TimeSegment::Second:
            return 60;
        default:
            return 2;
        }
    }
    const char* Label(int row) const
    {
        switch (unit)
        {
        case TimeSegment::Hour:
            return Digits().text[twelveHour ? (row == 0 ? 12 : row) : row];
        case TimeSegment::Minute:
            return Digits().text[std::clamp(row * minuteStep, 0, 59)];
        case TimeSegment::Second:
            return Digits().text[row];
        default:
            return row == 0 ? GetStrings().am : GetStrings().pm;
        }
    }
    // The row of a time (a minute between slots: the slot before it).
    int RowOf(const TimeOfDay& time) const
    {
        switch (unit)
        {
        case TimeSegment::Hour:
            return twelveHour ? time.hour % 12 : time.hour;
        case TimeSegment::Minute:
            return time.minute / minuteStep;
        case TimeSegment::Second:
            return time.second;
        default:
            return time.hour >= 12 ? 1 : 0;
        }
    }
    // The time with the row chosen.
    TimeOfDay Apply(TimeOfDay time, int row) const
    {
        switch (unit)
        {
        case TimeSegment::Hour:
            time.hour = twelveHour ? row + (time.hour >= 12 ? 12 : 0) : row;
            break;
        case TimeSegment::Minute:
            time.minute = std::clamp(row * minuteStep, 0, 59);
            break;
        case TimeSegment::Second:
            time.second = row;
            break;
        default:
            time.hour = time.hour % 12 + (row == 1 ? 12 : 0);
            break;
        }
        return time;
    }
};

void TimeColumnEntry(void* user, int index, internal::ListEntryInfo* out)
{
    out->label = static_cast<const TimeColumnSource*>(user)->Label(index);
}

struct TimeColumnsResult
{
    bool changed = false;
    TimeOfDay time;
    bool activated = false;  // Enter or a double click: the choice is final
    bool byKey = false;
    bool shown = true;       // every column was in sight and drew its rows
};

// The columns side by side at the cursor. `opened`: scroll each column to its chosen row.
// `generation` counts the openings of a popover: each opening gets columns of its own, so the
// keyboard focus starts on the chosen entries, not where it was the last time.
TimeColumnsResult TimeColumns(ImGuiID id, const TimeLayout& layout, const TimeOfDay& time, const TimePickerOptions& options,
                              int minuteStep, bool opened, bool readOnly, bool live, int generation,
                              const ImVec4& fadeSurface)
{
    using namespace internal;
    TimeColumnsResult result;
    result.time = time;
    const Theme& t = T();
    const float rowHeight = ListRowHeight(options.size);
    const float pad = FocusRingOutset();
    const ImVec2 area(std::round(Px(t.picker.timeColumnWidth)), std::round(rowHeight * kTimeColumnRows + pad * 2.0f));
    ImGui::PushOverrideID(id);
    for (int i = 0; i < layout.count; ++i)
    {
        TimeColumnSource source;
        source.unit = layout.units[i];
        source.twelveHour = options.twelveHour;
        source.minuteStep = minuteStep;
        int choice = source.RowOf(result.time);
        ListViewDesc view;
        view.rowCount = source.Count();
        view.size = options.size;
        view.areaSize = area;
        view.fadeSurface = fadeSurface;
        view.choice = &choice;
        view.pill = true;
        view.readOnly = readOnly || !live;
        view.entry = &TimeColumnEntry;
        view.user = &source;
        view.revealRow = opened ? choice : -1;
        if (i > 0)
            ImGui::SameLine(0.0f, PxSnapped(t.spacing.xs));
        const ImGuiID columnId = ImGui::GetID(static_cast<int>(source.unit) + kTimeSegments * generation);
        if (BeginListView(columnId, view))
        {
            RowRange rows;
            while (NextListRows(&rows))
            {
                for (int row = rows.first; row < rows.last; ++row)
                {
                    const ListRowState state = ListViewRow(row);
                    // A popover opening takes the focus to the chosen hour, not to the first row.
                    if (i == 0 && row == choice)
                        ImGui::SetItemDefaultFocus();
                    if (!state.item.visible)
                        continue;
                    PushPillLabelStyle(TextStyle::Body, state.selected);
                    const char* text = source.Label(row);
                    const float width = ImGui::CalcTextSize(text).x;
                    ImGui::GetWindowDrawList()->AddText(
                        ImVec2(std::round(state.rect.GetCenter().x - width * 0.5f), CenteredTextY(state.rect.Min.y, state.rect.GetHeight())),
                        ToU32(ControlLabelColor(state.disabled)), text);
                    PopTextStyle();
                }
            }
            const ListViewResult picked = EndListView();
            if (picked.changed && live && !readOnly)
            {
                result.time = source.Apply(result.time, choice);
                result.changed = true;
            }
            if (picked.activated >= 0 && live)
            {
                result.activated = true;
                result.byKey = !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            }
        }
        else
        {
            result.shown = false;
        }
    }
    ImGui::PopID();
    return result;
}

} // namespace

bool TimePicker(const char* label, TimeOfDay* time, const TimePickerOptions& options)
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
    const ImGuiID level = ImHashStr("##columns", 0, id);
    const ImGuiID inlineId = ImHashStr("##inline", 0, id);
    TimePickerState& state = GetState<TimePickerState>(id);
    const TimeLayout layout = TimeSegments(options);
    if (state.segment < 0 || state.segment >= layout.count)
        state.segment = 0;
    const int minuteStep = std::clamp(options.minuteStep, 1, 60);
    const bool readOnly = options.readOnly || time == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool mixedFlag = options.mixed || (next.itemFlags & ImGuiItemFlags_MixedValue) != 0;
    if (!mixedFlag)
        state.mixedCleared = false;
    const TimeOfDay current = time != nullptr ? TimeFromSeconds(TimeToSeconds(*time)) : TimeOfDay{};
    if (!state.editing)
        TimeToParts(current, options.twelveHour, state.parts);

    ForcedItemRequest fieldForced;
    ForcedItemRequest columnsForced;
    if (forced.part <= 0)
        fieldForced = forced;
    else if (forced.part == 1)
        columnsForced = {forced.state, -1};

    // 2. Measure: a field widget, the available width by default.
    const float width = ResolveItemWidth(options.width, next, kFill);
    const FrontLabel front = MeasureFrontLabel(label, m.size, width);

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
    const TimeGeometry geometry = TimeMeasure(layout, state.parts, frame.Min.x + m.fieldPadding, separatorGap);

    bool written = false;
    const auto write = [&](const TimeOfDay& value) {
        if (time == nullptr || readOnly)
            return;
        if (!(value == *time))
        {
            *time = value;
            written = true;
        }
        state.mixedCleared = true;
    };
    const auto commit = [&]() {
        state.typed = 0;
        TimeOfDay typed;
        if (!TimeFromParts(state.parts, options.twelveHour, &typed))
            return;  // shown as typed until it is corrected
        write(typed);
        state.editing = false;
    };

    bool openNow = false;
    if (live && !readOnly)
    {
        if (item.activated && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            for (int i = 0; i < layout.count; ++i)
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
                state.segment = std::min(layout.count - 1, state.segment + 1);
                state.typed = 0;
            }
            for (const ImWchar ch : g.IO.InputQueueCharacters)
            {
                const TimeSegment unit = layout.units[state.segment];
                if (unit == TimeSegment::HalfDay)
                {
                    if (ch == 'a' || ch == 'A' || ch == 'p' || ch == 'P')
                    {
                        state.editing = true;
                        state.parts[3] = ch == 'p' || ch == 'P' ? 1 : 0;
                    }
                    continue;
                }
                if (ch >= '0' && ch <= '9')
                {
                    if (!state.editing)
                    {
                        state.editing = true;
                        state.typed = 0;
                    }
                    const int digit = static_cast<int>(ch - '0');
                    int& part = state.parts[TimePartIndex(unit)];
                    part = state.typed == 0 ? digit : part * 10 + digit;
                    ++state.typed;
                    const int largest = unit == TimeSegment::Hour ? (options.twelveHour ? 12 : 23) : 59;
                    if (state.typed >= 2 || part * 10 > largest)
                    {
                        state.typed = 0;
                        if (state.segment < layout.count - 1)
                            ++state.segment;
                    }
                }
                else if ((ch == ':' || ch == '.' || ch == ' ') && state.segment < layout.count - 1)
                {
                    ++state.segment;
                    state.typed = 0;
                }
            }
            if (state.editing)
            {
                ImGui::SetKeyOwner(ImGuiKey_Escape, id);
                if (ImGui::IsKeyPressed(ImGuiKey_Escape, ImGuiInputFlags_None, id))
                {
                    state.editing = false;
                    state.typed = 0;
                    TimeToParts(current, options.twelveHour, state.parts);
                    MarkEditCanceled(id);
                }
            }
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
            TimeOfDay from = current;
            if (state.editing)
                TimeFromParts(state.parts, options.twelveHour, &from);
            write(TimeStep(from, layout.units[state.segment], step.steps, minuteStep));
            state.editing = false;
            state.typed = 0;
        }
    }

    if (live && !readOnly && item.pressed)
    {
        const float threshold = Px(t.drag.startThreshold);
        const bool dragged = g.IO.MouseDragMaxDistanceSqr[ImGuiMouseButton_Left] >= threshold * threshold;
        if (item.keyboardPressed && state.editing)
            commit();
        else if (!options.inlineColumns && (item.keyboardPressed || !dragged))
            openNow = !open;
    }
    if (state.editing && state.wasFocused && !item.focused)
        commit();
    state.wasFocused = item.focused;
    if (openNow && state.editing)
        commit();
    if (written || !state.editing)
        TimeToParts(time != nullptr ? TimeFromSeconds(TimeToSeconds(*time)) : TimeOfDay{}, options.twelveHour, state.parts);

    // 4. Draw the field.
    ImDrawList* drawList = window->DrawList;
    const bool mixed = mixedFlag && !state.mixedCleared;
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
        if (mixed && !state.editing)
        {
            drawList->AddText(ImVec2(textLeft, textY), ToU32(ControlSecondaryColor(item.disabled)), GetStrings().mixed);
        }
        else
        {
            const TimeGeometry at = TimeMeasure(layout, state.parts, textLeft, separatorGap);
            if (!item.disabled && !readOnly && (item.focusVisible || state.editing))
            {
                const ImVec2 hiMin(at.x[state.segment] - separatorGap, frame.Min.y + Px(t.spacing.xs));
                const ImVec2 hiMax(at.x[state.segment] + at.width[state.segment] + separatorGap,
                                   frame.Max.y - Px(t.spacing.xs));
                FillRect(drawList, hiMin, hiMax, ToU32(c.accentSubtle), Px(t.radii.element));
            }
            const ImU32 digits = ToU32(ControlLabelColor(item.disabled));
            const ImU32 marks = ToU32(ControlSecondaryColor(item.disabled));
            for (int i = 0; i < layout.count; ++i)
            {
                if (i > 0 && layout.units[i] != TimeSegment::HalfDay)
                    drawList->AddText(ImVec2(at.x[i] - separatorGap - ImGui::CalcTextSize(":").x, textY), marks, ":");
                drawList->AddText(ImVec2(at.x[i], textY), digits, TimePartText(layout.units[i], state.parts));
            }
        }
        ImGui::PopClipRect();
        if (iconX > textLeft)
            DrawControlIcon(drawList, Icon("clock"), iconX, frame.GetCenter().y, iconSize,
                            ToU32(ControlSecondaryColor(item.disabled)));
        if (front.text != nullptr)
            DrawFrontLabel(drawList, front, ImVec2(item.rect.Min.x, textY), m.size, item.disabled);
        DrawFocusRing(drawList, frame, m.radius, item.focusAmount);
    }
    CutTextTooltip(item, front.text, front.end, front.shown.truncated);
    const ControlMessage message = PickControlMessage(options.error, options.helpText);
    const float messageBottom =
        DrawControlMessage(message, ImVec2(frame.Min.x, frame.Max.y), frame.GetWidth(), item.disabled, id, frame);
    const TimeOfDay shown = time != nullptr ? TimeFromSeconds(TimeToSeconds(*time)) : TimeOfDay{};

    // 5. The columns: in place, or in a popover beside the field (PanelAnchor).
    if (options.inlineColumns)
    {
        ImGui::SetCursorScreenPos(ImVec2(frame.Min.x, messageBottom + PxSnapped(t.spacing.sm)));
        ImGui::BeginDisabled(item.disabled);
        if (columnsForced.state != ForcedItemState::None)
            ForceNextItemState(columnsForced.state, columnsForced.part);
        // The columns scroll to the time whenever it changed elsewhere (the field, the caller).
        const int seconds = TimeToSeconds(shown);
        const TimeColumnsResult result = TimeColumns(inlineId, layout, shown, options, minuteStep, state.revealed != seconds,
                                                     readOnly, !item.disabled && columnsForced.state == ForcedItemState::None, 0, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (result.shown)
            state.revealed = result.changed ? TimeToSeconds(result.time) : seconds;
        ImGui::EndDisabled();
        if (result.changed)
            write(result.time);
    }
    else
    {
        const PopupAnchor anchor = PanelAnchor(frame);
        // A solid surface: the columns' edge fades blend into bgElevated, which a blurred backdrop is not.
        PopupOptions popup;
        if (openNow && !readOnly)
        {
            state.generation = (state.generation + 1) & 0xFFFF;
            OpenPopupLevel(level, anchor, popup);
        }
        if (item.open)
            ForcePopupLevelOpen(level, anchor, popup);
        SetPopupLevelAnchor(level, anchor);
        if (BeginPopupLevel(level))
        {
            const int seconds = TimeToSeconds(shown);
            const TimeColumnsResult result =
                TimeColumns(level, layout, shown, options, minuteStep, state.revealed != seconds || item.open, readOnly,
                            !item.open, state.generation, c.bgElevated);
            if (result.shown)
                state.revealed = result.changed ? TimeToSeconds(result.time) : seconds;
            if (result.changed)
                write(result.time);
            if (result.activated)
            {
                if (result.byKey)
                    ListsCloseByKey(level);
                else
                    ClosePopupLevel(level);
            }
            EndPopupLevel();
        }
        else
        {
            state.revealed = -1;
        }
    }
    ImGui::EndGroup();
    FinishWidgetGroup(id);
    if (written)
    {
        TimeToParts(*time, options.twelveHour, state.parts);
        ListsMarkEdited();
    }
    return written;
}

} // namespace facet
