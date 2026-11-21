// TabBar, TabBarClosedTab and TabBarMovedTab (navigation.h): a strip of tabs with the sliding
// selection pill (or an accent underline that slides the same way), one Tab stop with the arrow
// keys inside, close buttons, reordering by drag and by keyboard, and a "more" menu for the tabs
// that do not fit.
//
// Layout, in pixels: tabs start at the left edge, each its content (icon, label, detail badge,
// close button) plus paddingX on both sides, between tabs.minWidth and tabs.maxWidth, spacing.xs
// apart. Tabs that do not fit move into a "more" menu after the last visible tab (FitOverflow,
// overflow.h); the selected tab has the highest priority, and when even it does not fit beside
// the button it shrinks to the room left and cuts its label, so it always shows.
//
// The selection mark is a SelectionPill (selection_pill.h): the same spring as SegmentedControl,
// relative to the window so scrolling does not animate it, following a change of width or scale.
// Edges are on whole pixels, so it rests on whole pixels; with reduced motion it jumps.
//
// Closing: the close button, a middle click or Delete and Backspace on the focused tab report the
// tab through TabBarClosedTab. *index is written for the list without that tab: the selected tab
// keeps its place, and when it is the one closed, its next enabled neighbour (the following one,
// else the previous one) becomes selected and takes the keyboard focus if the closed tab had it.
//
// Reordering: every visible tab is a drag source of a payload type that names the bar, and the
// strip is one ordered drop zone (drag_drop.h). Ctrl+Space picks the focused tab up, Left and
// Right move it, Space or Enter drops it and Escape puts it back. The tabs are laid out in data
// order but drawn where they move to, springing toward the layout plus the gap shift, keyed by
// the item id (the index without one).

#include "facet/widgets/navigation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/drag_drop.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/overflow.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "widgets/navigation/navigation_parts.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

using namespace internal;

// Tabs one bar lays out; later tabs are left out with a usage error.
constexpr int kTabBarMax = kOverflowInlineItems;
// Captures: how far right of its place a forced drag shows the dragged tab, in tab heights.
constexpr float kTabForcedDragHeights = 1.5f;
// A row measured this much wider than its room counts as fitting (rounding of the edges).
constexpr float kTabFitSlack = 0.5f;
// The payload type names the bar: "facet.tab." and eight hex digits.
constexpr int kTabTypeSize = 24;

struct TabBarState
{
    OverflowState overflow;
    ImGuiID focusTarget = 0;  // the tab that takes the keyboard focus when it is submitted
    int focusFrame = -1;
};

struct TabBarReport
{
    int closed = -1;
    int closedFrame = -1;
    ReorderMove moved;
    int movedFrame = -1;
    // Ctrl+Page Down and Up: the bar that held the keyboard focus last, and its root window.
    ImGuiID focusedBar = 0;
    ImGuiID focusedBarWindow = 0;
    // The first bar of a root window in a frame takes the keys while the focus is in no bar there.
    ImGuiID claimWindow = 0;
    int claimFrame = -1;
};

TabBarReport& Report()
{
    static TabBarReport report;
    return report;
}

struct TabPayload
{
    ImGuiID bar = 0;
    int index = -1;
};

struct TabSlot
{
    float natural = 0.0f;   // the content and paddings, clamped to the tab tokens
    float width = 0.0f;     // as laid out this frame
    float labelWidth = 0.0f;
    float badgeWidth = 0.0f;
    bool shown = true;
    ImRect rect;            // laid out, screen pixels
    ImGuiID id = 0;
    ImGuiID key = 0;        // the stable key for the position spring
    char badge[32] = {};    // the detail text, zero terminated
    const char* badgeEnd = nullptr;
};

struct TabScratch
{
    TabSlot slots[kTabBarMax];
    ItemState states[kTabBarMax];
    ImRect zoneSlots[kTabBarMax];
    int slotTab[kTabBarMax] = {};  // visible slot -> tab index
    OverflowItem fit[kTabBarMax];
    bool visible[kTabBarMax] = {};
};

TabScratch& Scratch()
{
    static TabScratch scratch;  // one bar is laid out at a time; no allocation per frame
    return scratch;
}

// The index of `j` after `move` was applied to the list.
int TabIndexAfterMove(int j, const ReorderMove& move)
{
    if (!move.IsValid() || j < 0)
        return j;
    if (j == move.from)
        return move.to;
    if (move.from < move.to && j > move.from && j <= move.to)
        return j - 1;
    if (move.from > move.to && j >= move.to && j < move.from)
        return j + 1;
    return j;
}

void FocusTabLater(TabBarState& state, ImGuiID id)
{
    state.focusTarget = id;
    state.focusFrame = ImGui::GetFrameCount();
}

bool TabBarImpl(const char* id, int* index, const SelectionItems& items, const TabBarOptions& options)
{
    TabBarReport& report = Report();
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    const int frame = ImGui::GetFrameCount();
    report.closed = -1;
    report.closedFrame = frame;
    report.moved = {};
    report.movedFrame = frame;
    if (id == nullptr)
        id = "";
    ImGuiContext& g = *GImGui;
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const ControlMetrics m = GetControlMetrics(options.size);
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID barId = window->GetID(id);
    TabBarState& state = GetState<TabBarState>(barId);
    // A focus request that no tab took within a frame is dropped (the tab is gone).
    if (state.focusTarget != 0 && state.focusFrame < frame - 1)
        state.focusTarget = 0;
    IM_ASSERT_USER_ERROR(items.Count() <= kTabBarMax, "A tab bar shows at most 64 tabs.");
    const int count = std::min(items.Count(), kTabBarMax);
    const bool readOnly = index == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const int selected = index != nullptr ? *index : -1;
    const bool underline = options.style == TabStyle::Underline;

    // ---- Measure ----
    ControlTextScope text(m);
    const float iconSize = text.IconSize();
    const float gap = Px(t.spacing.xs);
    const float inset = std::max(1.0f, std::round(Px(t.metrics.segmentInset)));
    const float closeSide = std::round(std::min(m.height - inset * 2.0f, iconSize + Px(t.spacing.xs) * 2.0f));
    const float closeInset = std::round(m.paddingX * 0.5f);
    const float minWidth = std::round(Px(t.tabs.minWidth));
    const float maxWidth = std::max(minWidth, std::round(Px(t.tabs.maxWidth)));
    TabScratch& s = Scratch();
    // Selected labels are semibold, so tabs are measured in semibold and fit either weight.
    PushPillLabelStyle(m.textStyle, true);
    for (int i = 0; i < count; ++i)
    {
        const ChoiceItem item = items.At(i);
        TabSlot& slot = s.slots[i];
        slot.id = SelectionEntryId(barId, item.id, i);
        slot.key = item.id != 0 ? item.id : static_cast<ImGuiID>(i);
        slot.labelWidth = ImGui::CalcTextSize(item.label, ImGui::FindRenderedTextEnd(item.label)).x;
        slot.badge[0] = '\0';
        slot.badgeEnd = nullptr;
        slot.badgeWidth = 0.0f;
        if (item.detail != nullptr && item.detail[0] != '\0')
        {
            std::snprintf(slot.badge, sizeof(slot.badge), "%s", item.detail);
            slot.badgeEnd = slot.badge + std::strlen(slot.badge);
            slot.badgeWidth = MeasureNavigationBadge(slot.badge, slot.badgeEnd).width;
        }
        float content = m.paddingX * 2.0f + slot.labelWidth;
        if (item.icon.IsValid())
            content += iconSize + (slot.labelWidth > 0.0f ? m.iconGap : 0.0f);
        if (slot.badgeWidth > 0.0f)
            content += m.iconGap + slot.badgeWidth;
        if (options.closable)
            content += m.iconGap + closeSide + closeInset - m.paddingX;
        slot.natural = std::clamp(std::ceil(content), minWidth, maxWidth);
        slot.shown = true;
    }
    PopTextStyle();

    // ---- Width and overflow ----
    const float width = ResolveItemWidth(options.width, next, kFill);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const float moreWidth = m.height;
    float total = 0.0f;
    for (int i = 0; i < count; ++i)
        total += s.slots[i].natural + (i > 0 ? gap : 0.0f);
    bool showMore = false;
    if (total > width + kTabFitSlack && count > 0)
    {
        for (int i = 0; i < count; ++i)
        {
            float w = s.slots[i].natural;
            // The selected tab stays: it gives up width rather than its place.
            if (i == selected)
                w = std::min(w, std::max(1.0f, std::floor(width - moreWidth - gap)));
            s.fit[i] = {w, i == selected ? 1 : 0, OverflowKind::Item};
            s.slots[i].width = w;
        }
        const OverflowResult fit =
            FitOverflow(std::span<const OverflowItem>(s.fit, static_cast<std::size_t>(count)), width, gap, moreWidth,
                        Px(t.spacing.lg), &state.overflow, std::span<bool>(s.visible, static_cast<std::size_t>(count)));
        showMore = fit.showButton;
        for (int i = 0; i < count; ++i)
            s.slots[i].shown = s.visible[i];
    }
    else
    {
        state.overflow.hidden = 0;
        for (int i = 0; i < count; ++i)
            s.slots[i].width = s.slots[i].natural;
    }
    int shownCount = 0;
    float x = origin.x;
    for (int i = 0; i < count; ++i)
    {
        TabSlot& slot = s.slots[i];
        if (!slot.shown)
            continue;
        slot.rect = ImRect(x, origin.y, std::round(x + slot.width), origin.y + m.height);
        x = slot.rect.Max.x + gap;
        s.zoneSlots[shownCount] = slot.rect;
        s.slotTab[shownCount] = i;
        ++shownCount;
    }
    const float moreX = std::min(std::round(x), std::round(origin.x + width - moreWidth));
    const ImRect moreRect(moreX, origin.y, moreX + moreWidth, origin.y + m.height);
    const ImRect bar(origin, ImVec2(origin.x + width, origin.y + m.height));

    // ---- Items ----
    ImGui::BeginGroup();
    ImGui::ItemSize(ImVec2(width, m.height), CenteredTextY(origin.y, m.height) - origin.y);
    ImDrawList* drawList = window->DrawList;
    if (underline)
        HairlineH(drawList, bar.Min.x, bar.Max.x, bar.Max.y - HairlineThickness(), ToU32(c.separator));

    char type[kTabTypeSize];
    std::snprintf(type, sizeof(type), "facet.tab.%08X", barId);
    DragDropInfo drag;
    const bool dragging = options.reorderable && DragDropActive(type, &drag);
    const bool keyboardDrag = dragging && drag.keyboard;
    const ImGuiID zoneId = ImGui::GetIDWithSeed("##zone", nullptr, barId);
    const ImVec2 springOrigin(window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y);
    const auto enabledTab = [&](int i) { return !groupDisabled && !items.At(i).disabled; };

    // The one Tab stop: the focused tab, else the selected one, else the first enabled visible one.
    int stop = -1;
    for (int i = 0; i < count && stop < 0; ++i)
        if (s.slots[i].shown && enabledTab(i) && g.NavId == s.slots[i].id)
            stop = i;
    if (stop < 0 && selected >= 0 && selected < count && s.slots[selected].shown && enabledTab(selected))
        stop = selected;
    for (int i = 0; i < count && stop < 0; ++i)
        if (s.slots[i].shown && enabledTab(i))
            stop = i;

    int chosen = selected;
    int focused = -1;
    int closeRequest = -1;
    bool closeHadFocus = false;
    ImGuiID editor = barId;
    int lifted = -1;
    // Pass 1: the tabs as items and drag sources, at their laid-out places.
    for (int i = 0; i < count; ++i)
    {
        TabSlot& slot = s.slots[i];
        if (!slot.shown)
            continue;
        ItemOptions itemOptions;
        itemOptions.disabled = !enabledTab(i);
        itemOptions.readOnly = readOnly;
        itemOptions.tabStop = i == stop;
        itemOptions.handCursor = !readOnly;
        itemOptions.allowOverlap = options.closable;
        itemOptions.part = i;
        itemOptions.forced = &forced;
        const ItemState tab = PartBehavior(slot.id, slot.rect, itemOptions);
        s.states[i] = tab;
        NavigationRecordPart(barId, i, slot.rect);
        const bool live = tab.forced == ForcedItemState::None;
        if (state.focusTarget == slot.id)
        {
            if (state.focusFrame >= frame - 1 && !tab.focused && !tab.disabled)
                SelectionMoveFocus(slot.id);
            state.focusTarget = 0;
        }
        if (tab.focused && g.NavId == slot.id && live)
            focused = i;
        if (tab.pressed && live && !tab.readOnly)
        {
            chosen = i;
            editor = slot.id;
        }
        if (options.closable && live && !tab.disabled && !readOnly)
        {
            // A middle click (press and release on the tab) closes it. The pointer test is the tab's own:
            // the item behaviour does not report a focused tab as hovered.
            const bool over = g.HoveredWindow == window && slot.rect.Contains(g.IO.MousePos) &&
                              (g.HoveredId == 0 || g.HoveredId == slot.id);
            if (over && ImGui::IsMouseReleased(ImGuiMouseButton_Middle) &&
                slot.rect.Contains(g.IO.MouseClickedPos[ImGuiMouseButton_Middle]))
                closeRequest = i;
            if (tab.focused && !keyboardDrag)
            {
                ImGui::SetKeyOwner(ImGuiKey_Delete, slot.id);
                ImGui::SetKeyOwner(ImGuiKey_Backspace, slot.id);
                if (ImGui::IsKeyPressed(ImGuiKey_Delete, 0, slot.id) || ImGui::IsKeyPressed(ImGuiKey_Backspace, 0, slot.id))
                {
                    closeRequest = i;
                    closeHadFocus = true;
                }
            }
        }
        if (options.reorderable && !readOnly && enabledTab(i))
        {
            if (Has(tab.forced, ForcedItemState::Dragging))
                DragDropForceNextSource(ImVec2(m.height * kTabForcedDragHeights, 0.0f));
            const TabPayload payload{barId, i};
            DragDropPreview preview;
            const ChoiceItem item = items.At(i);
            preview.previewText = item.label;
            preview.icon = item.icon;
            preview.keyboard = true;
            preview.pickUpChord = ImGuiMod_Ctrl | ImGuiKey_Space;
            if (DragDropBeginSource(type, &payload, sizeof(payload), preview))
            {
                lifted = i;
                DragDropEndSource();
            }
        }
    }

    // The drop zone over the visible tabs.
    int sourceTab = -1;
    int sourceSlot = -1;
    int insert = -1;
    DragDropZoneResult drop;
    if (options.reorderable && shownCount > 0)
    {
        DragDropZoneOptions zoneOptions;
        zoneOptions.slots = std::span<const ImRect>(s.zoneSlots, static_cast<std::size_t>(shownCount));
        zoneOptions.axis = Orientation::Horizontal;
        zoneOptions.accept = !readOnly && !groupDisabled;
        drop = DragDropZone(zoneId, bar, type, zoneOptions);
        const ImGuiPayload* payload = drop.payload != nullptr ? drop.payload : (dragging ? drag.payload : nullptr);
        if (payload != nullptr && payload->DataSize == static_cast<int>(sizeof(TabPayload)))
        {
            const TabPayload* from = static_cast<const TabPayload*>(payload->Data);
            if (from->bar == barId && from->index >= 0 && from->index < count)
                sourceTab = from->index;
        }
        for (int k = 0; k < shownCount; ++k)
            if (s.slotTab[k] == sourceTab)
                sourceSlot = k;
        insert = drop.hovered && sourceSlot >= 0 ? drop.insertIndex : -1;
    }
    const float dragWidth = sourceTab >= 0 ? s.slots[sourceTab].rect.GetWidth() : 0.0f;
    const ImRect gapRect = insert >= 0 ? DragDropGapRect(std::span<const ImRect>(s.zoneSlots, static_cast<std::size_t>(shownCount)),
                                                         sourceSlot, insert, Orientation::Horizontal, dragWidth)
                                       : ImRect();

    // Pass 2: the pill, then each tab where it moves to.
    SelectionPill pill;
    pill.Begin(barId, drawList, springOrigin);
    // The underline is a non-text mark that must reach 3:1 on every surface: the focus ring color,
    // which the theme derives from the accent for exactly that.
    const ImVec4 mark = groupDisabled ? c.fill[1] : (underline ? c.focusRing : c.accentSubtle);
    const float tabRadius = m.radius;
    const float underlineHeight = std::max(1.0f, std::round(Px(t.tabs.underlineHeight)));
    int slotIndex = 0;
    for (int i = 0; i < count; ++i)
    {
        TabSlot& slot = s.slots[i];
        if (!slot.shown)
            continue;
        const int k = slotIndex++;
        const ItemState& tab = s.states[i];
        ImRect rect = slot.rect;
        if (options.reorderable)
        {
            float target = slot.rect.Min.x - springOrigin.x;
            if (i == lifted && insert >= 0)
                target = gapRect.Min.x - springOrigin.x;
            else if (i != lifted)
                target += DragDropGapShift(k, sourceSlot, insert, dragWidth + gap);
            const float drawnX = std::round(springOrigin.x + DragDropAnimatedPosition(zoneId, slot.key, target));
            rect.Min.x = drawnX;
            rect.Max.x = drawnX + slot.rect.GetWidth();
        }
        const bool on = i == selected;
        if (on)
        {
            const ImRect markRect = underline ? ImRect(rect.Min.x + closeInset, rect.Max.y - underlineHeight,
                                                       rect.Max.x - closeInset, rect.Max.y)
                                              : rect;
            pill.Target(markRect);
        }
        if (i == lifted || !tab.visible)
            continue;
        const ChoiceItem item = items.At(i);
        const float feedback = tab.disabled || tab.readOnly || dragging ? 0.0f : std::max(tab.hoverAmount, tab.pressAmount);
        if (feedback > 0.0f)
        {
            const ImVec4 tint = Mix(c.fill[2], c.fill[1], tab.pressAmount);
            FillRect(drawList, rect.Min, rect.Max, ToU32(WithAlpha(tint, tint.w * feedback)), tabRadius);
        }
        // Content: icon, label, badge, then the close button at the end.
        const ImU32 ink = ToU32(ControlLabelColor(tab.disabled));
        float left = rect.Min.x + m.paddingX;
        float right = rect.Max.x - m.paddingX;
        ImRect closeRect;
        if (options.closable)
        {
            const float closeRight = rect.Max.x - closeInset;
            const float closeTop = std::round(rect.GetCenter().y - closeSide * 0.5f);
            closeRect = ImRect(closeRight - closeSide, closeTop, closeRight, closeTop + closeSide);
            right = closeRect.Min.x - m.iconGap;
        }
        if (item.icon.IsValid())
        {
            DrawControlIcon(drawList, item.icon, left, rect.GetCenter().y, iconSize, ink);
            left += iconSize + m.iconGap;
        }
        if (slot.badgeWidth > 0.0f && right - slot.badgeWidth >= left)
        {
            DrawNavigationBadge(drawList, right - slot.badgeWidth, rect.GetCenter().y, slot.badge, slot.badgeEnd,
                                Tone::Neutral, tab.disabled);
            right -= slot.badgeWidth + m.iconGap;
        }
        PushPillLabelStyle(m.textStyle, on);
        const char* labelEnd = ImGui::FindRenderedTextEnd(item.label);
        const FittedText shown = FitText(item.label, labelEnd, std::max(0.0f, right - left));
        drawList->AddText(ImVec2(left, CenteredTextY(rect.Min.y, m.height)), ink, shown.text);
        PopTextStyle();
        DrawFocusRing(drawList, rect, tabRadius, tab.focusAmount);
        CutTextTooltip(tab, item.label, labelEnd, shown.truncated);

        if (options.closable)
        {
            const ImGuiID closeId = ImGui::GetIDWithSeed("close", nullptr, slot.id);
            ItemOptions closeOptions;
            closeOptions.disabled = tab.disabled || readOnly;
            closeOptions.navigable = false;
            closeOptions.tabStop = false;
            const ItemState close = PartBehavior(closeId, closeRect, closeOptions);
            NavigationRecordPart(barId, NavigationClosePart(i), closeRect);
            const float closeFeedback = close.disabled ? 0.0f : std::max(close.hoverAmount, close.pressAmount);
            if (closeFeedback > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[1], c.fill[0], close.pressAmount);
                FillRect(drawList, closeRect.Min, closeRect.Max, ToU32(WithAlpha(tint, tint.w * closeFeedback)),
                         Px(t.radii.element));
            }
            DrawControlIcon(drawList, Icon("close"), std::round(closeRect.GetCenter().x - iconSize * 0.5f),
                            closeRect.GetCenter().y, iconSize, ToU32(ControlSecondaryColor(close.disabled)));
            if (close.pressed && !close.disabled && close.forced == ForcedItemState::None)
                closeRequest = i;
            if (BeginPopupTooltip(closeId, closeRect, close.hovered, false))
            {
                ImGui::TextUnformatted(GetStrings().close);
                EndPopupTooltip();
            }
        }
    }

    // The "more" menu for the tabs that did not fit.
    if (showMore)
    {
        const ImGuiID moreId = ImGui::GetIDWithSeed("more", nullptr, barId);
        const ImGuiID menuId = ImGui::GetIDWithSeed("more menu", nullptr, barId);
        const bool hiddenSelected = selected >= 0 && selected < count && !s.slots[selected].shown;
        ItemOptions moreOptions;
        moreOptions.part = count;
        moreOptions.forced = &forced;
        moreOptions.tabStop = false;  // the tabs in the menu are reached with the arrow keys
        moreOptions.disabled = groupDisabled;
        const ItemState more = PartBehavior(moreId, moreRect, moreOptions);
        NavigationRecordPart(barId, kNavigationPartMore, moreRect);
        const PopupAnchor anchor{moreRect, PopupSide::Below, PopupAlign::Start};
        PopupOptions popup;
        popup.minWidth = Px(t.overlay.menuMinWidth);
        const bool open = IsPopupLevelOpen(menuId) || more.open;
        if (more.open)
            ForcePopupLevelOpen(menuId, anchor, popup);
        else if (more.pressed && more.forced == ForcedItemState::None && !open)
            OpenPopupLevel(menuId, anchor, popup);
        SetPopupLevelAnchor(menuId, anchor);
        if (more.visible)
        {
            if (hiddenSelected)
                FillRect(drawList, moreRect.Min, moreRect.Max, ToU32(underline ? c.accentSubtle : mark), tabRadius);
            const float feedback = more.disabled ? 0.0f : std::max({more.hoverAmount, more.pressAmount, open ? 1.0f : 0.0f});
            if (feedback > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[2], c.fill[1], more.pressAmount);
                FillRect(drawList, moreRect.Min, moreRect.Max, ToU32(WithAlpha(tint, tint.w * feedback)), tabRadius);
            }
            DrawControlIcon(drawList, Icon("more-horizontal"), std::round(moreRect.GetCenter().x - iconSize * 0.5f),
                            moreRect.GetCenter().y, iconSize, ToU32(ControlLabelColor(more.disabled)));
            DrawFocusRing(drawList, moreRect, tabRadius, more.focusAmount);
        }
        if (!open && BeginPopupTooltip(moreId, moreRect, more.hovered, more.focusVisible))
        {
            ImGui::TextUnformatted(GetStrings().more);
            EndPopupTooltip();
        }
        if (BeginPopupLevel(menuId))
        {
            MenuLevelBegin(menuId);
            for (int i = 0; i < count; ++i)
            {
                if (s.slots[i].shown)
                    continue;
                const ChoiceItem item = items.At(i);
                ImGui::PushID(i);
                MenuRowDesc row;
                row.icon = item.icon;
                row.checked = i == selected;
                row.slot = true;
                row.disabled = !enabledTab(i) || readOnly;
                const MenuRowResult result = MenuRow(menuId, item.label, row);
                NavigationRecordPart(barId, NavigationMenuRowPart(i), result.item.rect);
                if (result.pressed)
                {
                    chosen = i;
                    editor = s.slots[i].id;
                    if (result.item.keyboardPressed)
                        FocusTabLater(state, s.slots[i].id);
                }
                ImGui::PopID();
            }
            EndPopupLevel();
        }
    }
    pill.End(ToU32(mark), underline ? underlineHeight * 0.5f : tabRadius);

    // ---- Keys ----
    if (focused >= 0 && !keyboardDrag && closeRequest < 0)
    {
        SelectionRovingKeys keys;
        const int moved = SelectionRovingKey(s.slots[focused].id, focused, count, keys, enabledTab);
        if (moved >= 0)
        {
            if (s.slots[moved].shown)
                SelectionMoveFocus(s.slots[moved].id);
            else
                FocusTabLater(state, s.slots[moved].id);  // a tab in the menu shows from the next frame on, as the selected one
            if (!readOnly)
            {
                chosen = moved;
                editor = s.slots[moved].id;
            }
        }
    }
    // Ctrl+Page Down and Up switch tabs while the focus is anywhere in the window: the bar that held
    // the focus last takes the keys; while no bar of the window did, the first bar of the window.
    if (focused >= 0)
    {
        report.focusedBar = barId;
        report.focusedBarWindow = window->RootWindow->ID;
    }
    const bool firstInWindow = report.claimWindow != window->RootWindow->ID || report.claimFrame != frame;
    if (firstInWindow)
    {
        report.claimWindow = window->RootWindow->ID;
        report.claimFrame = frame;
    }
    const bool takesSwitch = report.focusedBar == barId || (report.focusedBarWindow != window->RootWindow->ID && firstInWindow);
    if (takesSwitch && !readOnly && !groupDisabled && count > 0 && !dragging && closeRequest < 0)
    {
        const bool nextTab = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_PageDown, ImGuiInputFlags_Repeat);
        const bool prevTab = ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_PageUp, ImGuiInputFlags_Repeat);
        if (nextTab != prevTab)
        {
            const int from = selected >= 0 && selected < count ? selected : (nextTab ? -1 : count);
            const int to = SelectionStepEnabled(from, nextTab ? 1 : -1, count, true, enabledTab);
            if (to >= 0 && to != selected)
            {
                chosen = to;
                editor = s.slots[to].id;
                if (focused >= 0)
                    FocusTabLater(state, s.slots[to].id);
            }
        }
    }

    // ---- Results ----
    bool changed = false;
    int after = selected;
    if (!readOnly && closeRequest >= 0)
    {
        report.closed = closeRequest;
        if (closeRequest == selected)
        {
            // The neighbour that takes over: the next enabled tab, else the previous one.
            int neighbour = SelectionStepEnabled(closeRequest, 1, count, false, enabledTab);
            if (neighbour < 0)
                neighbour = SelectionStepEnabled(closeRequest, -1, count, false, enabledTab);
            after = neighbour < 0 ? -1 : (neighbour > closeRequest ? neighbour - 1 : neighbour);
            changed = true;
            if (neighbour >= 0 && (closeHadFocus || focused == closeRequest))
                FocusTabLater(state, SelectionEntryId(barId, items.At(neighbour).id, after));
        }
        else if (selected > closeRequest)
        {
            after = selected - 1;
        }
        editor = s.slots[closeRequest].id;
    }
    else if (!readOnly && drop.delivered && sourceTab >= 0)
    {
        const int insertTab = drop.insertIndex < shownCount ? s.slotTab[drop.insertIndex] : s.slotTab[shownCount - 1] + 1;
        const ReorderMove move = DragDropReorderMove(sourceTab, insertTab);
        if (move.IsValid())
        {
            report.moved = move;
            after = TabIndexAfterMove(selected, move);
            editor = s.slots[sourceTab].id;
            if (drop.keyboard)
                FocusTabLater(state, SelectionEntryId(barId, items.At(sourceTab).id, move.to));
        }
    }
    else if (!readOnly && chosen >= 0 && chosen < count && chosen != selected)
    {
        after = chosen;
        changed = true;
    }
    if (after != selected && index != nullptr)
    {
        *index = after;
        changed = true;
    }

    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, bar.Max);
    ImGui::EndGroup();
    FinishWidgetGroup(barId);
    if (changed)
        ImGui::MarkItemEdited(editor);
    return changed;
}

} // namespace

bool TabBar(const char* id, int* index, std::span<const ChoiceItem> tabs, const TabBarOptions& options)
{
    return TabBarImpl(id, index, {tabs, {}}, options);
}

bool TabBar(const char* id, int* index, std::span<const char* const> tabs, const TabBarOptions& options)
{
    return TabBarImpl(id, index, {{}, tabs}, options);
}

int TabBarClosedTab()
{
    const TabBarReport& report = Report();
    if (ImGui::GetCurrentContext() == nullptr || report.closedFrame != ImGui::GetFrameCount())
        return -1;
    return report.closed;
}

ReorderMove TabBarMovedTab()
{
    const TabBarReport& report = Report();
    if (ImGui::GetCurrentContext() == nullptr || report.movedFrame != ImGui::GetFrameCount())
        return {};
    return report.moved;
}

} // namespace facet
