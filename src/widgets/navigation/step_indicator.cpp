// StepIndicator (navigation.h): numbered step markers joined by connectors. Steps before `current`
// are done (an accent disc with a check mark), `current` is marked (an accent ring around an
// accentSubtle disc, its number and label semibold), the rest are open (a boundary ring); a step
// whose ChoiceItem is disabled draws the disabled look whatever its place. Connectors between done
// steps take the focus ring color, the others the 3:1 control boundary, so the progress reads
// without color alone: done steps also carry the check mark.
//
// Horizontal: the width is divided into one column per step, the marker centered at the top and
// the label (and the item detail as a second line) centered under it, cut with an ellipsis and a
// tooltip. Vertical, and horizontal in a Compact width class: one row per step, the marker on the
// left, the label beside it, connectors running down between the markers.
//
// The interactive overload makes every done step that is not disabled a part of its own (the
// whole column or row): one Tab stop for the indicator, Left and Right (Up and Down when vertical),
// Home and End move the focus between them, a click, Enter or Space writes *current. The current
// and open steps cannot be activated, so a flow cannot skip ahead past a step that is not done.

#include "facet/widgets/navigation.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/layout.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "widgets/navigation/navigation_parts.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

using namespace internal;

constexpr int kStepMax = 32;

struct StepState
{
    NavigationRoving roving;
};

enum class StepLook
{
    Done,
    Current,
    Open,
    Disabled,
};

void DrawStepMarker(ImDrawList* drawList, const ImRect& marker, StepLook look, int number, float hover, TextStyle style)
{
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImVec2 center = marker.GetCenter();
    const float radius = marker.GetWidth() * 0.5f;
    const float ring = std::max(1.0f, std::round(Px(t.metrics.stepConnectorWidth)));
    char text[16];
    std::snprintf(text, sizeof(text), "%d", number);
    ImU32 ink = 0;
    switch (look)
    {
    case StepLook::Done:
        drawList->AddCircleFilled(center, radius, ToU32(Mix(c.accent, c.accentHover, hover)));
        break;
    case StepLook::Current:
        drawList->AddCircleFilled(center, radius, ToU32(c.accentSubtle));
        drawList->AddCircle(center, radius - ring * 0.5f, ToU32(c.focusRing), 0, ring);
        ink = ToU32(c.accentText);
        break;
    case StepLook::Open:
        drawList->AddCircle(center, radius - HairlineThickness() * 0.5f, ToU32(ControlBoundaryColor(c)), 0, HairlineThickness());
        ink = ToU32(c.labelSecondary);
        break;
    case StepLook::Disabled:
        drawList->AddCircleFilled(center, radius, ToU32(c.fill[3]));
        drawList->AddCircle(center, radius - HairlineThickness() * 0.5f, ToU32(c.separator), 0, HairlineThickness());
        ink = ToU32(c.labelTertiary);
        break;
    }
    if (look == StepLook::Done)
    {
        const float iconSize = std::round(radius);
        DrawControlIcon(drawList, Icon("check"), std::round(center.x - iconSize * 0.5f), center.y, iconSize, ToU32(c.onAccent));
        return;
    }
    PushPillLabelStyle(style, look == StepLook::Current);
    const ImVec2 size = ImGui::CalcTextSize(text);
    drawList->AddText(ImVec2(std::round(center.x - size.x * 0.5f), std::round(center.y - ImGui::GetFontSize() * 0.5f)), ink, text);
    PopTextStyle();
}

bool StepIndicatorImpl(const char* id, int current, int* write, const SelectionItems& items,
                       const StepIndicatorOptions& options)
{
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const ControlMetrics m = GetControlMetrics(options.size);
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID indicatorId = window->GetID(id);
    StepState& state = GetState<StepState>(indicatorId);
    IM_ASSERT_USER_ERROR(items.Count() <= kStepMax, "A step indicator shows at most 32 steps.");
    const int count = std::min(items.Count(), kStepMax);
    const bool interactive = write != nullptr && (next.itemFlags & ImGuiItemFlags_ReadOnly) == 0;
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const float width = ResolveItemWidth(options.width, next, kFill);
    const bool vertical = options.orientation == Orientation::Vertical || WidthClassOf(width / Px(1.0f)) == WidthClass::Compact;

    const float marker = std::max(1.0f, std::round(Px(t.metrics.stepMarkerSize) * ControlHeight(options.size) /
                                                   ControlHeight(ControlSize::Regular)));
    const float connector = std::max(1.0f, std::round(Px(t.metrics.stepConnectorWidth)));
    const TextStyle numberStyle = m.size == ControlSize::Small ? TextStyle::Caption : TextStyle::Footnote;
    bool anyDetail = false;
    for (int i = 0; i < count; ++i)
        anyDetail = anyDetail || (items.At(i).detail != nullptr && items.At(i).detail[0] != '\0');
    float labelLine = 0.0f;
    float detailLine = 0.0f;
    {
        ControlTextScope text(m);
        labelLine = ImGui::GetFontSize();
    }
    {
        ControlTextScope text(TextStyle::Footnote);
        detailLine = anyDetail ? ImGui::GetFontSize() : 0.0f;
    }
    const float gapY = Px(t.spacing.xs);
    const float rowGap = Px(t.spacing.lg);
    const float textBlock = labelLine + (anyDetail ? gapY + detailLine : 0.0f);
    const float rowHeight = std::round(std::max(marker, textBlock));
    const float height = vertical ? (count > 0 ? rowHeight * static_cast<float>(count) + rowGap * static_cast<float>(count - 1) : 0.0f)
                                  : std::round(marker + gapY + textBlock);

    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    ImGui::BeginGroup();
    ImGui::ItemSize(ImVec2(width, std::max(height, 1.0f)));
    ImDrawList* drawList = window->DrawList;
    if (interactive)
        state.roving.BeginFrame();
    const int shownCurrent = current;
    ImRect markers[kStepMax];
    ImRect parts[kStepMax];
    const float column = count > 0 ? width / static_cast<float>(count) : width;
    for (int i = 0; i < count; ++i)
    {
        if (vertical)
        {
            const float top = origin.y + (rowHeight + rowGap) * static_cast<float>(i);
            markers[i] = ImRect(origin.x, std::round(top + (rowHeight - marker) * 0.5f), origin.x + marker,
                                std::round(top + (rowHeight - marker) * 0.5f) + marker);
            parts[i] = ImRect(origin.x, top, origin.x + width, top + rowHeight);
        }
        else
        {
            const float left = std::round(origin.x + column * static_cast<float>(i));
            const float right = std::round(origin.x + column * static_cast<float>(i + 1));
            const float cx = std::round((left + right - marker) * 0.5f);
            markers[i] = ImRect(cx, origin.y, cx + marker, origin.y + marker);
            parts[i] = ImRect(left, origin.y, right, origin.y + height);
        }
    }

    // Connectors first, under the markers.
    for (int i = 0; i + 1 < count; ++i)
    {
        const bool done = i + 1 <= shownCurrent && !items.At(i).disabled && !items.At(i + 1).disabled && !groupDisabled;
        const ImU32 color = ToU32(groupDisabled ? c.separator : (done ? c.focusRing : ControlBoundaryColor(c)));
        const float spacing = Px(t.spacing.xs);
        if (vertical)
        {
            const float cx = std::round(markers[i].GetCenter().x - connector * 0.5f);
            const float y0 = markers[i].Max.y + spacing;
            const float y1 = markers[i + 1].Min.y - spacing;
            if (y1 > y0)
                FillRect(drawList, ImVec2(cx, y0), ImVec2(cx + connector, y1), color, 0.0f);
        }
        else
        {
            const float cy = std::round(markers[i].GetCenter().y - connector * 0.5f);
            const float x0 = markers[i].Max.x + spacing;
            const float x1 = markers[i + 1].Min.x - spacing;
            if (x1 > x0)
                FillRect(drawList, ImVec2(x0, cy), ImVec2(x1, cy + connector), color, 0.0f);
        }
    }

    int chosen = -1;
    ImGuiID editor = indicatorId;
    for (int i = 0; i < count; ++i)
    {
        const ChoiceItem item = items.At(i);
        const bool disabled = item.disabled || groupDisabled;
        const StepLook look = disabled ? StepLook::Disabled
                                       : (i < shownCurrent ? StepLook::Done : (i == shownCurrent ? StepLook::Current : StepLook::Open));
        ItemState part;
        part.rect = parts[i];
        part.visible = ImGui::IsRectVisible(parts[i].Min, parts[i].Max);
        part.hovered = ImGui::IsMouseHoveringRect(parts[i].Min, parts[i].Max) && GImGui->HoveredWindow == window;
        const ImGuiID stepId = SelectionEntryId(indicatorId, item.id, i);
        if (interactive && look == StepLook::Done)
        {
            ItemOptions itemOptions;
            itemOptions.part = i;
            itemOptions.forced = &forced;
            itemOptions.tabStop = state.roving.IsTabStop(stepId);
            part = PartBehavior(stepId, parts[i], itemOptions);
            state.roving.Add(stepId, !part.disabled, i == shownCurrent - 1);
            if (part.focused && part.forced == ForcedItemState::None)
                (void)state.roving.Step(stepId, vertical);
            if (part.pressed && part.forced == ForcedItemState::None)
            {
                chosen = i;
                editor = stepId;
            }
        }
        NavigationRecordPart(indicatorId, i, parts[i]);
        if (!part.visible)
            continue;
        const float hover = std::max(part.hoverAmount, part.pressAmount);
        if (hover > 0.0f && look == StepLook::Done && interactive)
        {
            const ImVec4 tint = Mix(c.fill[2], c.fill[1], part.pressAmount);
            FillRect(drawList, parts[i].Min, parts[i].Max, ToU32(WithAlpha(tint, tint.w * hover)), m.radius);
        }
        DrawStepMarker(drawList, markers[i], look, i + 1, hover, numberStyle);
        if (interactive && look == StepLook::Done)
            DrawFocusRing(drawList, markers[i], marker * 0.5f, part.focusAmount, 0, RingShape::Round);

        // The label and the detail line.
        const char* labelEnd = ImGui::FindRenderedTextEnd(item.label);
        const ImU32 ink = ToU32(look == StepLook::Disabled ? c.labelTertiary : (look == StepLook::Open ? c.labelSecondary : c.label));
        float textLeft = 0.0f;
        float textTop = 0.0f;
        float room = 0.0f;
        if (vertical)
        {
            textLeft = markers[i].Max.x + Px(t.spacing.sm);
            room = std::max(0.0f, parts[i].Max.x - textLeft);
            textTop = std::round(parts[i].GetCenter().y - textBlock * 0.5f);
        }
        else
        {
            room = std::max(0.0f, parts[i].GetWidth() - Px(t.spacing.xs) * 2.0f);
            textTop = markers[i].Max.y + gapY;
        }
        bool cut = false;
        {
            ControlTextScope text(m);
            PushPillLabelStyle(m.textStyle, look == StepLook::Current);
            const FittedText shown = FitText(item.label, labelEnd, room);
            cut = shown.truncated;
            const float x = vertical ? textLeft : std::round(parts[i].GetCenter().x - shown.width * 0.5f);
            drawList->AddText(ImVec2(x, textTop), ink, shown.text);
            PopTextStyle();
        }
        if (item.detail != nullptr && item.detail[0] != '\0')
        {
            ControlTextScope text(TextStyle::Footnote);
            const FittedText shown = FitText(item.detail, nullptr, room);
            cut = cut || shown.truncated;
            const float x = vertical ? textLeft : std::round(parts[i].GetCenter().x - shown.width * 0.5f);
            drawList->AddText(ImVec2(x, textTop + labelLine + gapY), ToU32(ControlSecondaryColor(disabled)), shown.text);
        }
        if (cut)
        {
            if (interactive && look == StepLook::Done)
                CutTextTooltip(part, item.label, labelEnd, true);
            else
                CutTextTooltip(ImGui::GetIDWithSeed("tip", nullptr, stepId), parts[i], part.hovered, false, item.label, labelEnd,
                               true);
        }
    }
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(origin.x + width, origin.y + height));
    ImGui::EndGroup();
    FinishWidgetGroup(indicatorId);
    if (!interactive || groupDisabled || chosen < 0 || chosen == current)
        return false;
    *write = chosen;
    ImGui::MarkItemEdited(editor);
    return true;
}

} // namespace

void StepIndicator(const char* id, int current, std::span<const ChoiceItem> steps, const StepIndicatorOptions& options)
{
    (void)StepIndicatorImpl(id, current, nullptr, {steps, {}}, options);
}

void StepIndicator(const char* id, int current, std::span<const char* const> steps, const StepIndicatorOptions& options)
{
    (void)StepIndicatorImpl(id, current, nullptr, {{}, steps}, options);
}

bool StepIndicator(const char* id, int* current, std::span<const ChoiceItem> steps, const StepIndicatorOptions& options)
{
    return StepIndicatorImpl(id, current != nullptr ? *current : 0, current, {steps, {}}, options);
}

bool StepIndicator(const char* id, int* current, std::span<const char* const> steps, const StepIndicatorOptions& options)
{
    return StepIndicatorImpl(id, current != nullptr ? *current : 0, current, {{}, steps}, options);
}

} // namespace facet
