// Chip: a pill-shaped tag that can be selected and removed, and the chip item that ChipGroup builds
// on (selection_parts.h).

#include "facet/widgets/selection.h"

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
#include "internal/popup_host.h"
#include "internal/selection_pill.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace internal
{
namespace
{

// Animation sub key of the selected tint (below kItemAnimSubBase).
constexpr int kSelectedSub = 0;

// The measures of a chip, in pixels.
struct ChipMetrics
{
    ControlMetrics control;
    float iconSize = 0.0f;
    float dot = 0.0f;          // the leading dot's diameter
    float removeSide = 0.0f;   // the remove button, square, inside the pill's end
    float inset = 0.0f;        // between the pill's edge and the remove button
    float detailGap = 0.0f;    // between the label and the count
};

ChipMetrics MeasureChip(const SelectionChipDesc& desc)
{
    const Theme& t = T();
    ChipMetrics cm;
    cm.control = GetControlMetrics(desc.size);
    {
        ControlTextScope text(cm.control);
        cm.iconSize = text.IconSize();
    }
    cm.dot = std::max(1.0f, PxSnapped(t.metrics.statusDotSize));
    cm.inset = std::max(1.0f, std::round(Px(t.metrics.segmentInset)));
    cm.removeSide = std::round(cm.control.height - cm.inset * 2.0f);
    cm.detailGap = std::round(Px(t.spacing.xs));
    return cm;
}

struct ChipQuery
{
    bool removePressed = false;  // the last Chip call this frame
    int frame = -1;
};

ChipQuery& LastChip()
{
    static ChipQuery query;
    return query;
}

} // namespace

float SelectionChipWidth(const char* label, const SelectionChipDesc& desc)
{
    const ChipMetrics cm = MeasureChip(desc);
    const ControlMetrics& m = cm.control;
    // The selected label is semibold: measure that weight so selecting a chip never cuts it.
    PushPillLabelStyle(m.textStyle, true);
    const char* end = ImGui::FindRenderedTextEnd(label);
    float content = ImGui::CalcTextSize(label, end).x;
    PopTextStyle();
    if (desc.detail != nullptr && desc.detail[0] != '\0')
    {
        ControlTextScope text(m);
        content += cm.detailGap + ImGui::CalcTextSize(desc.detail).x;
    }
    if (desc.color != 0)
        content += cm.dot + m.iconGap;
    if (desc.icon.IsValid())
        content += cm.iconSize + m.iconGap;
    // With a remove button: the inset before it, the button and the inset after it (see SelectionChip).
    const float trailing = desc.removable ? cm.removeSide + cm.inset * 2.0f : m.paddingX;
    return std::ceil(m.paddingX + content + trailing);
}

SelectionChipResult SelectionChip(const char* label, ImGuiID id, float width, const SelectionChipDesc& desc)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    const Theme& t = T();
    const Colors& c = t.colors;
    const ChipMetrics cm = MeasureChip(desc);
    const ControlMetrics& m = cm.control;
    const ForcedItemRequest forced = desc.forced != nullptr ? *desc.forced : TakeForcedItemState();

    SelectionChipResult result;
    const float top = window->DC.CursorPos.y;
    ItemOptions bodyOptions;
    bodyOptions.baseline = CenteredTextY(top, m.height) - top;
    bodyOptions.disabled = desc.disabled;
    bodyOptions.readOnly = desc.readOnly;
    bodyOptions.tabStop = desc.tabStop;
    bodyOptions.allowOverlap = desc.removable;  // the remove button over it takes the pointer
    bodyOptions.handCursor = !desc.readOnly;
    bodyOptions.part = desc.part;
    bodyOptions.forced = &forced;
    ImGui::BeginGroup();
    const ItemState body = ItemBehavior(id, ImVec2(width, m.height), bodyOptions);
    result.body = body;
    result.rect = body.rect;
    const bool live = body.forced == ForcedItemState::None;
    result.pressed = body.pressed && !body.readOnly && live;

    // The remove button: a mouse target inside the pill's end; the keyboard uses Delete and
    // Backspace on the chip, so it is not a Tab stop of its own.
    ItemState remove;
    ImRect removeRect;
    if (desc.removable)
    {
        removeRect = ImRect(body.rect.Max.x - cm.inset - cm.removeSide, body.rect.Min.y + cm.inset,
                            body.rect.Max.x - cm.inset, body.rect.Min.y + cm.inset + cm.removeSide);
        ItemOptions removeOptions;
        removeOptions.disabled = desc.disabled || body.disabled;
        removeOptions.readOnly = desc.readOnly;
        removeOptions.navigable = false;
        removeOptions.handCursor = !desc.readOnly;
        removeOptions.hitTarget = false;  // never reaching past the pill into a neighbour
        removeOptions.part = desc.part;
        ForcedItemRequest none;
        removeOptions.forced = &none;
        remove = PartBehavior(ImGui::GetIDWithSeed("remove", nullptr, id), removeRect, removeOptions);
        if (remove.pressed && !remove.readOnly)
            result.removed = true;
        if (body.focused && !body.disabled && !body.readOnly && live)
        {
            ImGui::SetKeyOwner(ImGuiKey_Delete, id);
            ImGui::SetKeyOwner(ImGuiKey_Backspace, id);
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, ImGuiInputFlags_None, id) ||
                ImGui::IsKeyPressed(ImGuiKey_Backspace, ImGuiInputFlags_None, id))
                result.removed = true;
        }
    }

    const float selectedAmount = Tween(id, kSelectedSub, desc.selected ? 1.0f : 0.0f, t.motion.durationFast);
    bool truncated = false;
    if (body.visible)
    {
        ImDrawList* drawList = window->DrawList;
        const ImRect& r = body.rect;
        const float radius = m.height * 0.5f;
        // Unselected: the neutral fill with the 3:1 boundary hairline, like a secondary button.
        // Selected: the selection tint (accentSubtle) with a focusRing hairline and a semibold label.
        ImVec4 fill;
        ImVec4 outline;
        if (body.disabled)
        {
            fill = desc.selected ? c.fill[1] : c.fill[3];
            outline = c.separator;
        }
        else
        {
            ItemState look = body;
            if (desc.removable && remove.hovered)
                look.hoverAmount = 0.0f;
            const ImVec4 clear = WithAlpha(c.fill[2], 0.0f);
            if (body.readOnly)
                fill = clear;
            else if (desc.selected && !desc.selectedFill)
                fill = StateFill(clear, c.fill[2], c.fill[1], look);  // the group's pill shows through
            else
                fill = NeutralFill(look);
            outline = Mix(ControlBoundaryColor(c), body.readOnly ? c.labelSecondary : c.focusRing, selectedAmount);
        }
        FillRect(drawList, r.Min, r.Max, ToU32(fill), radius);
        if (!body.disabled && desc.selectedFill && selectedAmount > 0.0f)
        {
            const ImVec4 tint = body.readOnly ? c.fill[0] : c.accentSubtle;
            FillRect(drawList, r.Min, r.Max, ToU32(WithAlpha(tint, tint.w * selectedAmount)), radius);
        }
        StrokeRect(drawList, r.Min, r.Max, ToU32(outline), radius, HairlineThickness());

        float x = r.Min.x + m.paddingX;
        const float centerY = r.GetCenter().y;
        const ImU32 labelColor = ToU32(ControlLabelColor(body.disabled));
        if (desc.color != 0)
        {
            drawList->AddCircleFilled(ImVec2(std::round(x + cm.dot * 0.5f), std::round(centerY)), cm.dot * 0.5f,
                                      body.disabled ? ToU32(c.labelTertiary) : desc.color);
            x += cm.dot + m.iconGap;
        }
        if (desc.icon.IsValid())
        {
            DrawControlIcon(drawList, desc.icon, x, centerY, cm.iconSize, labelColor);
            x += cm.iconSize + m.iconGap;
        }
        const float contentEnd = desc.removable ? removeRect.Min.x - cm.inset : r.Max.x - m.paddingX;
        float detailWidth = 0.0f;
        const bool hasDetail = desc.detail != nullptr && desc.detail[0] != '\0';
        if (hasDetail)
        {
            ControlTextScope text(m);
            detailWidth = ImGui::CalcTextSize(desc.detail).x + cm.detailGap;
        }
        const char* end = ImGui::FindRenderedTextEnd(label);
        PushPillLabelStyle(m.textStyle, desc.selected);
        const FittedText shown = FitText(label, end, std::max(0.0f, contentEnd - x - detailWidth));
        truncated = shown.truncated;
        drawList->AddText(ImVec2(x, CenteredTextY(r.Min.y, m.height)), labelColor, shown.text);
        PopTextStyle();
        if (hasDetail)
        {
            ControlTextScope text(m);
            drawList->AddText(ImVec2(x + shown.width + cm.detailGap, CenteredTextY(r.Min.y, m.height)),
                              ToU32(ControlSecondaryColor(body.disabled)), desc.detail);
        }
        if (desc.removable)
        {
            const float feedback = remove.disabled || remove.readOnly ? 0.0f : std::max(remove.hoverAmount, remove.pressAmount);
            if (feedback > 0.0f)
            {
                const ImVec4 tint = Mix(c.fill[1], c.fill[0], remove.pressAmount);
                drawList->AddCircleFilled(removeRect.GetCenter(), removeRect.GetWidth() * 0.5f,
                                          ToU32(WithAlpha(tint, tint.w * feedback)));
            }
            const float glyph = std::round(cm.iconSize);
            DrawControlIcon(drawList, Icon("close"), std::round(removeRect.GetCenter().x - glyph * 0.5f),
                            removeRect.GetCenter().y, glyph, ToU32(ControlSecondaryColor(body.disabled)));
        }
        DrawFocusRing(drawList, r, radius, body.focusAmount, 0, RingShape::Round);
    }

    // The remove button's name, and the whole label when it was cut (rule 11).
    if (desc.removable && BeginPopupTooltip(remove.id, removeRect, remove.hovered, false))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, c.label);
        PushTextStyle(TextStyle::Footnote);
        ImGui::TextUnformatted(GetStrings().remove);
        PopTextStyle();
        ImGui::PopStyleColor();
        EndPopupTooltip();
    }
    ItemState tipItem = body;
    tipItem.hovered = body.hovered && !(desc.removable && remove.hovered);
    CutTextTooltip(tipItem, label, ImGui::FindRenderedTextEnd(label), truncated);

    // The item queries after the call describe the whole chip (rule 11).
    ImGui::EndGroup();
    FinishWidgetGroup(id);
    return result;
}

} // namespace internal

bool Chip(const char* label, const ChipOptions& options)
{
    internal::ChipQuery& query = internal::LastChip();
    query.removePressed = false;
    query.frame = ImGui::GetCurrentContext() != nullptr ? ImGui::GetFrameCount() : -1;
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    internal::SelectionChipDesc desc;
    desc.selected = options.selected;
    desc.removable = options.removable;
    desc.icon = options.icon;
    desc.color = options.color;
    desc.size = options.size;
    const float width = internal::ResolveItemWidth(0.0f, next, internal::SelectionChipWidth(label, desc));
    internal::ApplyNextItemData(next);
    const internal::SelectionChipResult result = internal::SelectionChip(label, window->GetID(label), width, desc);
    query.removePressed = result.removed;
    return result.pressed;
}

bool ChipRemovePressed()
{
    const internal::ChipQuery& query = internal::LastChip();
    return ImGui::GetCurrentContext() != nullptr && query.frame == ImGui::GetFrameCount() && query.removePressed;
}

} // namespace facet
