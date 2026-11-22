// Segment strip (segment_strip.h): the joined segments of SegmentedControl, KeybindModeSelector
// and ToggleButtonGroup.

#include "widgets/selection/segment_strip.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/anim.h"
#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/icons.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/overflow.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"

namespace facet::internal
{
namespace
{

// Segments a strip shows at most; further items are left out (and reported).
constexpr int kSegmentStripMax = kOverflowInlineItems;
// Animation sub key of a latched segment's highlight (below kItemAnimSubBase).
constexpr int kLatchSub = 1;
// A label measured this much wider than its room counts as fitting (rounding of the edges).
constexpr float kFitSlack = 0.5f;

struct SegmentSlot
{
    float natural = 0.0f;   // pixels: content and paddings, or one control height for an icon
    float labelWidth = 0.0f;
    float x0 = 0.0f;        // screen pixels, whole
    float x1 = 0.0f;
    bool shown = true;      // false: moved into the "more" menu
    bool hasIcon = false;
};

// The content of one segment: icon and label centered, the label cut to the room. Returns whether
// anything of the label was left out (the label is then a tooltip).
bool DrawSegmentContent(ImDrawList* drawList, const ImRect& rect, const ChoiceItem& item, bool showIcon, bool showLabel,
                        float iconSize, const ControlMetrics& m, ImU32 color)
{
    const char* end = ImGui::FindRenderedTextEnd(item.label);
    const bool hasLabel = end != item.label;
    const float inner = std::max(0.0f, rect.GetWidth() - m.paddingX * 2.0f);
    const float iconSlot = showIcon ? iconSize + (showLabel && hasLabel ? m.iconGap : 0.0f) : 0.0f;
    FittedText shown;
    if (showLabel && hasLabel)
        shown = FitText(item.label, end, std::max(0.0f, inner - iconSlot));
    float x = std::round(rect.GetCenter().x - (iconSlot + shown.width) * 0.5f);
    const float centerY = rect.GetCenter().y;
    if (showIcon)
    {
        DrawControlIcon(drawList, item.icon, x, centerY, iconSize, color);
        x += iconSlot;
    }
    if (shown.text[0] != '\0')
        drawList->AddText(ImVec2(x, CenteredTextY(rect.Min.y, rect.GetHeight())), color, shown.text);
    return hasLabel && (!showLabel || shown.truncated);
}

// The width of a segment at its content, in the current (semibold) font: one control height for an
// icon only, else the paddings, the icon and its gap, and the label.
float SegmentNaturalWidth(const ChoiceItem& item, bool iconOnly, const ControlMetrics& m, float iconSize, float* labelWidth)
{
    const bool hasIcon = item.icon.IsValid();
    *labelWidth = ImGui::CalcTextSize(item.label, ImGui::FindRenderedTextEnd(item.label)).x;
    if (iconOnly && hasIcon)
        return m.height;
    return std::ceil(m.paddingX * 2.0f + (hasIcon ? iconSize + (*labelWidth > 0.0f ? m.iconGap : 0.0f) : 0.0f) + *labelWidth);
}

} // namespace

float SegmentStripContentWidth(const char* label, const SelectionItems& items, ControlSize size, bool iconOnly)
{
    const ControlMetrics m = GetControlMetrics(size);
    ControlTextScope text(m);
    const float iconSize = text.IconSize();
    float total = MeasureFrontLabel(label != nullptr ? label : "", m.size, 1.0e9f, FrontLabelFit::Text).width;
    PushPillLabelStyle(m.textStyle, true);
    const int count = std::min(items.Count(), kSegmentStripMax);
    float labelWidth = 0.0f;
    for (int i = 0; i < count; ++i)
        total += SegmentNaturalWidth(items.At(i), iconOnly, m, iconSize, &labelWidth);
    PopTextStyle();
    return total;
}

bool SegmentStrip(const char* label, const SelectionItems& items, int* index, std::span<bool> flags,
                  const SegmentStripDesc& desc)
{
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const ControlMetrics m = GetControlMetrics(desc.size);
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID widgetId = window->GetID(label);
    const bool choice = desc.kind == SegmentStripKind::Choice;
    IM_ASSERT_USER_ERROR(items.Count() <= kSegmentStripMax, "A segment strip shows at most 64 segments.");
    const int count = std::min(items.Count(), kSegmentStripMax);
    const bool readOnly = desc.readOnly || (choice && index == nullptr) || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    // ---- Measure ----
    ControlTextScope text(m);
    const float iconSize = text.IconSize();
    SegmentSlot slots[kSegmentStripMax];
    bool allIcons = count > 0;
    float naturalTotal = 0.0f;
    // Selected labels are semibold, so segments are measured in semibold and fit either weight.
    PushPillLabelStyle(m.textStyle, true);
    for (int i = 0; i < count; ++i)
    {
        const ChoiceItem item = items.At(i);
        SegmentSlot& slot = slots[i];
        slot.hasIcon = item.icon.IsValid();
        slot.natural = SegmentNaturalWidth(item, desc.iconOnly, m, iconSize, &slot.labelWidth);
        allIcons = allIcons && slot.hasIcon;
        naturalTotal += slot.natural;
    }
    PopTextStyle();

    // ---- Width (rule 6): the content, at most the available width ----
    const FrontLabelFit labelFit = CompactFrontLabelFit(desc.width, next);
    const FrontLabel wide = MeasureFrontLabel(label, m.size, 1.0e9f, labelFit);
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float width = ResolveItemWidth(desc.width, next, std::min(wide.width + naturalTotal, available));
    const FrontLabel front = MeasureFrontLabel(label, m.size, width, labelFit);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImRect track(origin.x + front.width, origin.y, origin.x + width, origin.y + m.height);
    const float trackWidth = track.GetWidth();

    // ---- Segment edges ----
    const int selected = choice && index != nullptr ? *index : -1;
    const auto flagOf = [&](int i) { return i < static_cast<int>(flags.size()) && flags[static_cast<std::size_t>(i)]; };
    const auto usable = [&](int i) {
        return !items.At(i).disabled && (choice || i < static_cast<int>(flags.size()));
    };
    float moreWidth = 0.0f;
    int shownCount = count;
    const bool fitsNatural = naturalTotal <= trackWidth + kFitSlack;
    if (desc.overflow && !fitsNatural && count > 0)
    {
        OverflowItem fit[kSegmentStripMax];
        bool visible[kSegmentStripMax];
        for (int i = 0; i < count; ++i)
            fit[i] = {slots[i].natural, (choice && i == selected) || (!choice && flagOf(i)) ? 1 : 0, OverflowKind::Item};
        OverflowState& state = GetState<OverflowState>(widgetId);
        const OverflowResult result = FitOverflow(std::span<const OverflowItem>(fit, static_cast<std::size_t>(count)), trackWidth,
                                                  0.0f, m.height, Px(t.spacing.lg), &state,
                                                  std::span<bool>(visible, static_cast<std::size_t>(count)));
        shownCount = 0;
        float shownNatural = 0.0f;
        for (int i = 0; i < count; ++i)
        {
            slots[i].shown = visible[i];
            if (visible[i])
            {
                ++shownCount;
                shownNatural += slots[i].natural;
            }
        }
        moreWidth = result.showButton ? (shownCount > 0 ? m.height : trackWidth) : 0.0f;
        // The visible segments share what the button leaves, in proportion to their content.
        const float room = trackWidth - moreWidth;
        float x = track.Min.x;
        for (int i = 0; i < count; ++i)
        {
            if (!slots[i].shown)
                continue;
            const float w = shownNatural > 0.0f ? slots[i].natural * room / shownNatural : 0.0f;
            slots[i].x0 = std::round(x);
            x += w;
            slots[i].x1 = std::round(x);
        }
    }
    else
    {
        // The content when it is exactly what was asked for, equal segments otherwise.
        const bool natural = desc.width == 0.0f && !next.hasWidth && window->DC.ItemWidthStack.Size == 0 && fitsNatural;
        float x = track.Min.x;
        for (int i = 0; i < count; ++i)
        {
            const float w = natural ? slots[i].natural : trackWidth / static_cast<float>(count);
            slots[i].x0 = std::round(x);
            x += w;
            slots[i].x1 = i == count - 1 ? track.Max.x : std::round(x);
        }
    }
    // Labels that do not fit collapse to icons, all at once, when every item has one.
    bool labelsFit = true;
    for (int i = 0; i < count; ++i)
        if (slots[i].shown && slots[i].natural > slots[i].x1 - slots[i].x0 + kFitSlack)
            labelsFit = false;
    const bool iconsOnly = desc.iconOnly || (!labelsFit && desc.collapseToIcons && allIcons);

    // ---- Items ----
    ImGui::BeginGroup();
    ImGui::ItemSize(ImVec2(width, m.height), CenteredTextY(origin.y, m.height) - origin.y);
    ImDrawList* drawList = window->DrawList;
    const float inset = std::max(1.0f, std::round(Px(t.metrics.segmentInset)));
    const float innerRadius = std::max(0.0f, m.radius - inset);
    FillRect(drawList, track.Min, track.Max,
             ToU32(groupDisabled ? c.fill[3] : (readOnly ? WithAlpha(c.fill[2], 0.0f) : c.fill[2])), m.radius);
    // A quiet container: the neutral fill and one controlBorder hairline; the segments inside have
    // no outline of their own, the selected one shows by its fill.
    OutlineLook containerEdge;
    containerEdge.disabled = groupDisabled;
    DrawOutline(drawList, track, m.radius, ControlOutlineColor(containerEdge));
    DrawFrontLabel(drawList, front, ImVec2(origin.x, CenteredTextY(origin.y, m.height)), m.size, groupDisabled);

    const bool mixedShown = SelectionMixedShown(widgetId, desc.mixed || (next.itemFlags & ImGuiItemFlags_MixedValue) != 0, false);
    const auto idOf = [&](int i) { return SelectionEntryId(widgetId, items.At(i).id, i); };
    const auto enabled = [&](int i) { return !groupDisabled && usable(i) && slots[i].shown; };
    const int stop = choice && desc.roving ? SelectionRovingStop(count, selected, enabled, idOf) : -1;
    const ImVec4 highlight = groupDisabled ? c.fill[1] : (readOnly ? c.fill[0] : c.accentSubtle);

    SelectionPill pill;
    if (choice)
        pill.Begin(widgetId, drawList, ImVec2(window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y));
    int chosen = selected;
    bool pressedAny = false;
    int focused = -1;
    int dragging = -1;
    int flipped = -1;
    ImGuiID editor = widgetId;  // the part that made the edit, for Dear ImGui's edit queries
    bool focusVisible = false;
    for (int i = 0; i < count; ++i)
    {
        const SegmentSlot& slot = slots[i];
        if (!slot.shown)
            continue;
        const ChoiceItem item = items.At(i);
        const ImGuiID entryId = idOf(i);
        const ImRect rect(slot.x0, track.Min.y, slot.x1, track.Max.y);
        ItemOptions itemOptions;
        itemOptions.disabled = !usable(i);
        itemOptions.readOnly = readOnly;
        itemOptions.tabStop = stop < 0 || i == stop;
        itemOptions.press = desc.dragSelect ? PressMode::Click : PressMode::ClickRelease;
        itemOptions.handCursor = !readOnly;
        itemOptions.part = i;
        itemOptions.forced = &forced;
        const ItemState entry = PartBehavior(entryId, rect, itemOptions);
        const bool live = entry.forced == ForcedItemState::None;
        if (entry.focused && g.NavId == entryId && live)
            focused = i;
        focusVisible = focusVisible || entry.focusVisible;
        if (entry.active && entry.dragging && live)
            dragging = i;
        if (entry.pressed && !entry.readOnly && live)
        {
            pressedAny = true;
            editor = entryId;
            if (choice)
                chosen = !desc.roving && desc.allowEmpty && i == selected && !mixedShown ? -1 : i;
            else
                flipped = i;
        }

        const bool on = !mixedShown && (choice ? i == selected : flagOf(i));
        const ImRect inner(rect.Min.x + inset, rect.Min.y + inset, rect.Max.x - inset, rect.Max.y - inset);
        if (choice && on)
            pill.Target(inner);
        bool truncated = false;
        if (entry.visible)
        {
            if (!choice)
            {
                const float amount = Tween(entryId, kLatchSub, on ? 1.0f : 0.0f, t.motion.durationFast);
                if (amount > 0.0f)
                    FillRect(drawList, inner.Min, inner.Max, ToU32(WithAlpha(highlight, highlight.w * amount)), innerRadius);
            }
            const float feedback = entry.disabled || entry.readOnly ? 0.0f : std::max(entry.hoverAmount, entry.pressAmount);
            if (feedback > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[2], c.fill[1], entry.pressAmount);
                FillRect(drawList, inner.Min, inner.Max, ToU32(WithAlpha(tint, tint.w * feedback)), innerRadius);
            }
            PushPillLabelStyle(m.textStyle, on);
            const bool showIcon = slot.hasIcon && (iconsOnly || !desc.iconOnly);
            const bool showLabel = !(iconsOnly && slot.hasIcon);
            truncated = DrawSegmentContent(drawList, rect, item, showIcon, showLabel, iconSize, m,
                                           ToU32(ControlLabelColor(entry.disabled)));
            PopTextStyle();
            // Inside the segment: a ring outside it would cross the container's edge.
            DrawFocusRingInside(drawList, inner, innerRadius, entry.focusAmount);
        }
        CutTextTooltip(entry, item.label, ImGui::FindRenderedTextEnd(item.label), truncated);
    }

    // ---- The "more" menu for segments that did not fit ----
    if (moreWidth > 0.0f)
    {
        const ImGuiID moreId = ImGui::GetIDWithSeed("more", nullptr, widgetId);
        const ImGuiID menuId = ImGui::GetIDWithSeed("more menu", nullptr, widgetId);
        const ImRect rect(track.Max.x - moreWidth, track.Min.y, track.Max.x, track.Max.y);
        const ImRect inner(rect.Min.x + inset, rect.Min.y + inset, rect.Max.x - inset, rect.Max.y - inset);
        bool hiddenOn = false;
        for (int i = 0; i < count; ++i)
            if (!slots[i].shown && !mixedShown && (choice ? i == selected : flagOf(i)))
                hiddenOn = true;
        ItemOptions itemOptions;
        itemOptions.part = count;
        itemOptions.forced = &forced;
        itemOptions.tabStop = stop < 0;  // a roving strip leaves the menu to the mouse and Tab past it
        const ItemState more = PartBehavior(moreId, rect, itemOptions);
        if (more.pressed && more.forced == ForcedItemState::None)
        {
            PopupOptions popup;
            popup.minWidth = Px(t.overlay.menuMinWidth);
            OpenPopupLevel(menuId, {rect, PopupSide::Below, PopupAlign::End}, popup);
        }
        if (more.visible)
        {
            // A selected or latched entry in the menu marks the button, so the state stays visible.
            if (hiddenOn)
                FillRect(drawList, inner.Min, inner.Max, ToU32(highlight), innerRadius);
            const float feedback = more.disabled ? 0.0f : std::max(more.hoverAmount, more.pressAmount);
            if (feedback > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[2], c.fill[1], more.pressAmount);
                FillRect(drawList, inner.Min, inner.Max, ToU32(WithAlpha(tint, tint.w * feedback)), innerRadius);
            }
            DrawControlIcon(drawList, Icon("more-horizontal"), std::round(rect.GetCenter().x - iconSize * 0.5f),
                            rect.GetCenter().y, iconSize, ToU32(ControlLabelColor(more.disabled)));
            DrawFocusRingInside(drawList, inner, innerRadius, more.focusAmount);
        }
        if (BeginPopupTooltip(moreId, rect, more.hovered, more.focusVisible))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, c.label);
            PushTextStyle(TextStyle::Footnote);
            ImGui::TextUnformatted(GetStrings().more);
            PopTextStyle();
            ImGui::PopStyleColor();
            EndPopupTooltip();
        }
        if (BeginPopupLevel(menuId))
        {
            MenuLevelBegin(menuId);
            for (int i = 0; i < count; ++i)
            {
                if (slots[i].shown)
                    continue;
                const ChoiceItem item = items.At(i);
                ImGui::PushID(i);
                MenuRowDesc row;
                row.icon = item.icon;
                row.checked = !mixedShown && (choice ? i == selected : flagOf(i));
                row.disabled = !usable(i) || groupDisabled;
                if (MenuRow(menuId, item.label, row).pressed && !readOnly)
                {
                    pressedAny = true;
                    if (choice)
                        chosen = desc.allowEmpty && i == selected && !mixedShown ? -1 : i;
                    else
                        flipped = i;
                }
                ImGui::PopID();
            }
            EndPopupLevel();
        }
    }

    const ImRect drawn = choice ? pill.End(ToU32(highlight), innerRadius) : ImRect();

    // Dividers between neighbours that are not selected, and never across the pill.
    for (int i = 0; i + 1 < count; ++i)
    {
        if (!slots[i].shown || !slots[i + 1].shown)
            continue;
        const float x = slots[i].x1;
        const bool nearPill = choice && drawn.GetWidth() > 0.0f && x >= drawn.Min.x - inset && x <= drawn.Max.x + inset;
        const bool latched = !choice && !mixedShown && (flagOf(i) || flagOf(i + 1));
        if (!nearPill && !latched)
            HairlineV(drawList, x, track.Min.y + inset * 2.0f, track.Max.y - inset * 2.0f, ToU32(c.separator));
    }

    // ---- Keys and drags ----
    if (focused >= 0 && choice && desc.roving)
    {
        SelectionRovingKeys keys;
        const int moved = SelectionRovingKey(idOf(focused), focused, count, keys, enabled);
        if (moved >= 0)
        {
            SelectionMoveFocus(idOf(moved));
            if (!readOnly)
            {
                chosen = moved;
                pressedAny = true;
                editor = idOf(moved);
            }
        }
    }
    if (dragging >= 0 && choice && desc.dragSelect && !readOnly)
    {
        const float mouseX = g.IO.MousePos.x;
        for (int i = 0; i < count; ++i)
            if (enabled(i) && mouseX >= slots[i].x0 && mouseX < slots[i].x1)
                chosen = i;
        editor = idOf(dragging);
        // The keyboard focus follows the selection, so the arrow keys continue from it.
        if (chosen >= 0 && chosen != dragging && g.NavId != idOf(chosen))
            ImGui::SetFocusID(idOf(chosen), window);
    }

    // The label in front, cut to its share, shows in full on hover and on keyboard focus (rule 11).
    if (front.shown.truncated)
    {
        const ImRect labelRect(origin.x, origin.y, track.Min.x, track.Max.y);
        CutTextTooltip(ImGui::GetIDWithSeed("label tooltip", nullptr, widgetId), labelRect,
                       ImGui::IsMouseHoveringRect(labelRect.Min, labelRect.Max) && g.HoveredWindow == window, focusVisible,
                       label, front.end, true);
    }

    bool written = false;
    if (choice && index != nullptr && !readOnly && chosen >= -1 && chosen < count &&
        (chosen != selected || (mixedShown && pressedAny)))
    {
        *index = chosen;
        written = true;
    }
    if (!choice && flipped >= 0 && flipped < static_cast<int>(flags.size()) && !readOnly)
    {
        flags[static_cast<std::size_t>(flipped)] = !flags[static_cast<std::size_t>(flipped)];
        written = true;
    }
    SelectionMixedShown(widgetId, desc.mixed || (next.itemFlags & ImGuiItemFlags_MixedValue) != 0, written);

    const ControlMessage message = PickControlMessage(desc.error, desc.helpText);
    const float bottom = DrawControlMessage(message, ImVec2(track.Min.x, track.Max.y), trackWidth, groupDisabled, widgetId,
                                            track);
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(track.Max.x, bottom));
    ImGui::EndGroup();
    FinishWidgetGroup(widgetId);
    if (written)
        MarkWidgetEdited(editor);
    return written;
}

} // namespace facet::internal
