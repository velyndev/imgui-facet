// DateRangePicker: a field showing a start and an end date that opens two months side by side in a
// popover, with optional presets beside them.
//
// The field is one item (item behaviour) drawn like the other fields: the label in front, the
// start date, an arrow, the end date and the calendar icon. A click, Enter, Space or Alt+Down opens
// the popover (popup host). The calendars are the calendar panel of date_picker_parts.h with two
// months (one when the display is too narrow for two): the first pick writes *start and clears
// *end, the band then follows the pointer or the keyboard cursor, and the second pick writes both
// ends in order and closes. A completed range can be changed by dragging one of its ends; an end
// dragged past the other one takes its place. A preset writes both ends and closes.

#include "facet/widgets/pickers.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/buttons.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/lists/list_parts.h"
#include "widgets/pickers/date_picker_parts.h"

namespace facet
{
namespace
{

using internal::ForcedItemState;

struct DateRangePickerState
{
    bool picking = false;      // the first pick is made; the next one ends the range
    Date anchor;               // the first pick
    Date hover;                // the day under the pointer in the last frame
    int dragEnd = -1;          // the end being dragged: 0 start, 1 end, -1 none
    bool mixedCleared = false;
};

} // namespace

bool DateRangePicker(const char* label, Date* start, Date* end, const DateRangePickerOptions& options)
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
    const ImGuiID id = window->GetID(label);
    const ImGuiID level = ImHashStr("##calendar", 0, id);
    DateRangePickerState& state = GetState<DateRangePickerState>(id);
    const bool readOnly = options.readOnly || start == nullptr || end == nullptr ||
                          (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool mixedFlag = options.mixed || (next.itemFlags & ImGuiItemFlags_MixedValue) != 0;
    if (!mixedFlag)
        state.mixedCleared = false;
    Date min = options.min;
    Date max = options.max;
    if (min.IsValid() && max.IsValid() && DateCompare(min, max) > 0)
        std::swap(min, max);
    const Date first = start != nullptr ? *start : Date{};
    const Date last = end != nullptr ? *end : Date{};

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
    itemOptions.forced = &forced;
    ApplyNextItemData(next);
    const ItemState item = ItemBehavior(id, ImVec2(width, m.height), itemOptions);
    const ImRect frame(item.rect.Min.x + front.width, item.rect.Min.y, item.rect.Max.x, item.rect.Max.y);
    const bool live = !item.disabled && item.forced == ForcedItemState::None;
    const bool open = IsPopupLevelOpen(level);
    bool openNow = false;
    if (live && !readOnly)
    {
        if (item.pressed)
            openNow = !open;
        if (item.focused && !open && g.IO.KeyAlt)
        {
            ImGui::SetKeyOwner(ImGuiKey_DownArrow, id);
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_None, id))
                openNow = true;
        }
    }

    bool written = false;
    const auto writeBoth = [&](const Date& a, const Date& b) {
        if (start == nullptr || end == nullptr || readOnly)
            return;
        if (!(a == *start) || !(b == *end))
        {
            *start = a;
            *end = b;
            written = true;
        }
        state.mixedCleared = true;
    };

    // 4. Draw the field: start, an arrow, end, the calendar icon.
    ImDrawList* drawList = window->DrawList;
    const bool mixed = mixedFlag && !state.mixedCleared;
    {
        ControlTextScope text(m);
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
            const ImU32 strong = ToU32(ControlLabelColor(item.disabled));
            const ImU32 weak = ToU32(ControlSecondaryColor(item.disabled));
            ImGui::PushClipRect(ImVec2(frame.Min.x + m.fieldPadding * 0.5f, frame.Min.y), ImVec2(textRight, frame.Max.y), true);
            if (mixed)
            {
                drawList->AddText(ImVec2(textLeft, textY), weak, GetStrings().mixed);
            }
            else if (!first.IsValid() && !last.IsValid())
            {
                const char* placeholder = options.placeholder != nullptr ? options.placeholder : GetStrings().select;
                const FittedText shown = FitText(placeholder, nullptr, std::max(1.0f, textRight - textLeft));
                drawList->AddText(ImVec2(textLeft, textY), weak, shown.text);
            }
            else
            {
                char a[16];
                char b[16];
                DateFormat(a, sizeof(a), first, DatePrecision::Day);
                DateFormat(b, sizeof(b), last, DatePrecision::Day);
                const char* startText = first.IsValid() ? a : "----------";
                const char* endText = last.IsValid() ? b : "----------";
                float x = textLeft;
                drawList->AddText(ImVec2(x, textY), first.IsValid() ? strong : weak, startText);
                x += ImGui::CalcTextSize(startText).x + m.iconGap;
                DrawControlIcon(drawList, Icon("arrow-right"), x, frame.GetCenter().y, iconSize, weak);
                x += iconSize + m.iconGap;
                drawList->AddText(ImVec2(x, textY), last.IsValid() ? strong : weak, endText);
            }
            ImGui::PopClipRect();
            if (iconX > textLeft)
                DrawControlIcon(drawList, Icon("calendar"), iconX, frame.GetCenter().y, iconSize, weak);
            if (front.text != nullptr)
                DrawFrontLabel(drawList, front, ImVec2(item.rect.Min.x, textY), m.size, item.disabled);
            DrawFocusRing(drawList, frame, m.radius, item.focusAmount);
        }
    }
    CutTextTooltip(item, front.text, front.end, front.shown.truncated);
    const ControlMessage message = PickControlMessage(options.error, options.helpText);
    DrawControlMessage(message, ImVec2(frame.Min.x, frame.Max.y), frame.GetWidth(), item.disabled, id, frame);

    // 5. The popover: presets beside two months (one when the display is narrow).
    CalendarPanelDesc panel;
    panel.min = min;
    panel.max = max;
    panel.firstWeekday = options.firstWeekday;
    panel.range = true;
    panel.readOnly = readOnly;
    panel.months = 2;
    const Strings& strings = GetStrings();
    float presetsWidth = 0.0f;
    if (!options.presets.empty())
    {
        ControlTextScope text(m);
        presetsWidth = ImGui::CalcTextSize(strings.presets).x;
        for (const DateRangePreset& preset : options.presets)
            if (preset.label != nullptr)
                presetsWidth = std::max(presetsWidth, ImGui::CalcTextSize(preset.label, ImGui::FindRenderedTextEnd(preset.label)).x);
        presetsWidth = std::ceil(presetsWidth + m.paddingX * 2.0f);
    }
    const float columnGap = PxSnapped(t.spacing.lg);
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float room = viewport->WorkSize.x - Px(t.overlay.viewportMargin) * 2.0f - Px(t.spacing.xs) * 2.0f -
                       (presetsWidth > 0.0f ? presetsWidth + columnGap : 0.0f);
    if (CalendarPanelSize(panel).x > room)
        panel.months = 1;
    const bool presetsAbove = presetsWidth > 0.0f && CalendarPanelSize(panel).x > room;

    const PopupAnchor anchor = PanelAnchor(frame);
    PopupOptions popup;
    popup.glass = true;
    if (openNow)
    {
        state.picking = false;
        state.dragEnd = -1;
        state.hover = Date{};
        CalendarPanelOpen(level, first.IsValid() ? first : last, panel);
        OpenPopupLevel(level, anchor, popup);
    }
    if (item.open)
        ForcePopupLevelOpen(level, anchor, popup);
    SetPopupLevelAnchor(level, anchor);
    if (BeginPopupLevel(level))
    {
        const bool panelLive = !item.open;
        bool close = false;
        bool closeByKey = false;
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        float calendarX = origin.x;
        float calendarY = origin.y;
        if (presetsWidth > 0.0f)
        {
            // The presets: a caption, then one plain button per preset.
            {
                ControlTextScope footnote(TextStyle::Footnote);
                drawList = ImGui::GetWindowDrawList();
                drawList->AddText(ImVec2(origin.x + m.paddingX, CenteredTextY(origin.y, m.height)),
                                  ToU32(T().colors.labelSecondary), strings.presets);
            }
            float y = origin.y + m.height;
            const float unitsWide = presetsWidth / Scale();
            for (int i = 0; i < static_cast<int>(options.presets.size()); ++i)
            {
                const DateRangePreset& preset = options.presets[static_cast<std::size_t>(i)];
                ImGui::PushID(i);
                ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
                const bool usable = preset.label != nullptr && preset.start.IsValid() && preset.end.IsValid();
                ImGui::BeginDisabled(!usable);
                if (Button(preset.label != nullptr ? preset.label : "", {.variant = ButtonVariant::Plain, .size = options.size, .width = unitsWide}) &&
                    panelLive && usable && !readOnly)
                {
                    Date a = DateClampTo(preset.start, min, max);
                    Date b = DateClampTo(preset.end, min, max);
                    if (DateCompare(b, a) < 0)
                        std::swap(a, b);
                    writeBoth(a, b);
                    close = true;
                    closeByKey = g.NavActivateId != 0 && g.NavActivateId == ImGui::GetItemID();
                }
                ImGui::EndDisabled();
                ImGui::PopID();
                y += m.height;
            }
            if (presetsAbove)
                calendarY = y + PxSnapped(t.spacing.sm);
            else
                calendarX = origin.x + presetsWidth + columnGap;
        }

        // The calendars.
        panel.live = panelLive;
        panel.rangeStart = state.picking ? state.anchor : first;
        panel.rangeEnd = state.picking ? Date{} : last;
        panel.bandTo = state.picking ? (state.hover.IsValid() ? state.hover : CalendarPanelCursor(level)) : Date{};
        ImGui::SetCursorScreenPos(ImVec2(calendarX, calendarY));
        const CalendarPanelResult result = CalendarPanel(level, panel);
        state.hover = result.pointer.IsValid() && DateWithin(result.pointer, min, max) ? result.pointer : Date{};

        if (panelLive && !readOnly)
        {
            // A press on an end of a completed range starts dragging that end.
            if (result.pressed && !state.picking && first.IsValid() && last.IsValid())
            {
                if (result.pressedDate == first)
                    state.dragEnd = 0;
                else if (result.pressedDate == last)
                    state.dragEnd = 1;
            }
            if (state.dragEnd >= 0)
            {
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                {
                    state.dragEnd = -1;
                }
                else if (state.hover.IsValid() && !(state.hover == (state.dragEnd == 0 ? first : last)))
                {
                    Date a = state.dragEnd == 0 ? state.hover : first;
                    Date b = state.dragEnd == 0 ? last : state.hover;
                    if (DateCompare(b, a) < 0)
                    {
                        // The dragged end passed the other one: they swap roles.
                        std::swap(a, b);
                        state.dragEnd = 1 - state.dragEnd;
                    }
                    writeBoth(a, b);
                }
            }
            if (result.picked && state.dragEnd < 0)
            {
                if (!state.picking)
                {
                    state.picking = true;
                    state.anchor = result.date;
                    writeBoth(result.date, Date{});
                }
                else
                {
                    Date a = state.anchor;
                    Date b = result.date;
                    if (DateCompare(b, a) < 0)
                        std::swap(a, b);
                    writeBoth(a, b);
                    state.picking = false;
                    close = true;
                    closeByKey = result.byKey;
                }
            }
            else if (result.picked)
            {
                // A release on the end that was pressed without moving: a plain pick of it.
                state.dragEnd = -1;
                state.picking = true;
                state.anchor = result.date;
                writeBoth(result.date, Date{});
            }
        }
        if (close)
        {
            if (closeByKey)
                ListsCloseByKey(level);
            else
                ClosePopupLevel(level);
        }
        EndPopupLevel();
    }
    else if (!open)
    {
        state.picking = false;
        state.dragEnd = -1;
    }
    ImGui::EndGroup();
    FinishWidgetGroup(id);
    if (written)
        ListsMarkEdited();
    return written;
}

} // namespace facet
