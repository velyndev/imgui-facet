// RadioGroup: mutually exclusive options with a radio mark each, a detail line per option, the
// label above, vertical or wrapping horizontal rows, and one Tab stop with arrow keys inside.

#include "facet/widgets/selection.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/draw.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

// The measures every option of one group shares, in pixels.
struct RadioLayout
{
    float width = 0.0f;       // the group
    float mark = 0.0f;        // the radio mark's side
    float gap = 0.0f;         // between the mark and the label
    float rowHeight = 0.0f;   // one option without a detail line: the control height
    float detailHeight = 0.0f;  // the detail line under a label, Footnote
    float detailOffset = 0.0f;  // from the option top to the detail text top
    float spacingX = 0.0f;    // between options in a horizontal row
    float spacingY = 0.0f;    // between rows of a horizontal group
    bool horizontal = false;
};

// The size of option `item`: the mark, the gap and the wider of label and detail, at most the
// group width; the control height plus the detail line when there is one. Measured in the label
// text style, which the caller has pushed.
ImVec2 OptionSize(const ChoiceItem& item, const RadioLayout& layout)
{
    const char* end = ImGui::FindRenderedTextEnd(item.label);
    float content = ImGui::CalcTextSize(item.label, end).x;
    const bool detail = item.detail != nullptr && item.detail[0] != '\0';
    if (detail)
    {
        internal::ControlTextScope footnote(TextStyle::Footnote);
        content = std::max(content, ImGui::CalcTextSize(item.detail).x);
    }
    const float width = std::min(layout.width, std::ceil(layout.mark + layout.gap + content));
    return ImVec2(width, layout.rowHeight + (detail ? layout.detailHeight : 0.0f));
}

// Walks the options in layout order and calls `visit(index, rect)` for each; returns the height
// of every row together. Horizontal rows wrap when the next option does not fit.
template <class Visit>
float PlaceOptions(const internal::SelectionItems& items, const RadioLayout& layout, ImVec2 origin, Visit visit)
{
    float x = 0.0f;
    float y = 0.0f;
    float rowHeight = 0.0f;
    const int count = items.Count();
    for (int i = 0; i < count; ++i)
    {
        const ImVec2 size = OptionSize(items.At(i), layout);
        if (layout.horizontal)
        {
            if (x > 0.0f && x + size.x > layout.width)
            {
                y += rowHeight + layout.spacingY;
                x = 0.0f;
                rowHeight = 0.0f;
            }
            visit(i, ImRect(origin.x + x, origin.y + y, origin.x + x + size.x, origin.y + y + size.y));
            x += size.x + layout.spacingX;
            rowHeight = std::max(rowHeight, size.y);
        }
        else
        {
            visit(i, ImRect(origin.x, origin.y + y, origin.x + size.x, origin.y + y + size.y));
            y += size.y;
        }
    }
    return layout.horizontal ? (count > 0 ? y + rowHeight : 0.0f) : y;
}

bool RadioGroupImpl(const char* label, int* index, const internal::SelectionItems& items,
                    const RadioGroupOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    ImGuiContext& g = *GImGui;
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const Theme& t = T();
    const ImGuiID widgetId = window->GetID(label);
    const int count = items.Count();
    const bool readOnly = options.readOnly || index == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    internal::ControlTextScope labelText(m);
    RadioLayout layout;
    layout.width = internal::ResolveItemWidth(0.0f, next, kFill);
    layout.mark = std::max(1.0f, PxSnapped(t.metrics.checkboxSize));
    layout.gap = std::round(Px(t.spacing.sm));
    layout.rowHeight = m.height;
    {
        internal::ControlTextScope footnote(TextStyle::Footnote);
        layout.detailHeight = std::round(footnote.FontSize());
    }
    layout.detailOffset = std::round(CenteredTextY(0.0f, m.height) + labelText.FontSize());
    layout.spacingX = std::round(Px(t.spacing.xl));
    layout.spacingY = std::round(Px(t.spacing.xs));
    layout.horizontal = options.orientation == Orientation::Horizontal;

    const internal::AboveLabel above = internal::MeasureAboveLabel(label, m.size);
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    const ImVec2 origin = window->DC.CursorPos;
    const ImVec2 optionsOrigin(origin.x, origin.y + above.height);
    const float optionsHeight = PlaceOptions(items, layout, optionsOrigin, [](int, const ImRect&) {});

    ImGui::BeginGroup();
    ImGui::ItemSize(ImVec2(layout.width, above.height + optionsHeight));
    ImDrawList* drawList = window->DrawList;
    if (above.text != nullptr)
        internal::DrawAboveLabel(drawList, above, origin, layout.width, m.size, groupDisabled);

    const int selected = index != nullptr ? *index : -1;
    const auto enabled = [&](int i) { return !groupDisabled && !items.At(i).disabled; };
    const auto idOf = [&](int i) { return internal::SelectionEntryId(widgetId, items.At(i).id, i); };
    const int stop = internal::SelectionRovingStop(count, selected, enabled, idOf);
    int chosen = selected;
    int focused = -1;
    PlaceOptions(items, layout, optionsOrigin, [&](int i, const ImRect& rect) {
        const ChoiceItem item = items.At(i);
        const ImGuiID entryId = idOf(i);
        internal::ItemOptions itemOptions;
        itemOptions.disabled = item.disabled;
        itemOptions.readOnly = readOnly;
        itemOptions.tabStop = i == stop;
        itemOptions.handCursor = !readOnly;
        itemOptions.part = i;
        itemOptions.forced = &forced;
        const internal::ItemState entry = internal::PartBehavior(entryId, rect, itemOptions);
        if (entry.focused && g.NavId == entryId && entry.forced == internal::ForcedItemState::None)
            focused = i;
        if (entry.pressed && !entry.readOnly && entry.forced == internal::ForcedItemState::None)
            chosen = i;

        const bool on = i == selected;
        const float onAmount = internal::SelectionMarkAmount(entryId, on);
        bool truncated = false;
        if (entry.visible)
        {
            const ImRect markRect = internal::SelectionMarkRect(rect.Min.x, rect.Min.y + layout.rowHeight * 0.5f);
            internal::DrawSelectionRadio(drawList, markRect, onAmount, entry);
            const float x = markRect.Max.x + layout.gap;
            const float room = std::max(0.0f, rect.Max.x - x);
            const char* end = ImGui::FindRenderedTextEnd(item.label);
            const internal::FittedText shown = internal::FitText(item.label, end, room);
            truncated = shown.truncated;
            drawList->AddText(ImVec2(x, CenteredTextY(rect.Min.y, layout.rowHeight)),
                              ToU32(internal::ControlLabelColor(entry.disabled)), shown.text);
            if (item.detail != nullptr && item.detail[0] != '\0')
            {
                internal::ControlTextScope footnote(TextStyle::Footnote);
                const internal::FittedText detail = internal::FitText(item.detail, nullptr, room);
                truncated = truncated || detail.truncated;
                drawList->AddText(ImVec2(x, rect.Min.y + layout.detailOffset),
                                  ToU32(internal::ControlSecondaryColor(entry.disabled)), detail.text);
            }
            internal::DrawFocusRing(drawList, markRect, markRect.GetWidth() * 0.5f, entry.focusAmount, 0,
                                    internal::RingShape::Round);
        }
        internal::CutTextTooltip(entry, item.label, ImGui::FindRenderedTextEnd(item.label), truncated);
    });

    // Arrow keys, Home and End move the focus and the selection, wrapping at the ends.
    if (focused >= 0)
    {
        internal::SelectionRovingKeys keys;
        keys.upDown = true;
        keys.wrap = true;
        const int moved = internal::SelectionRovingKey(idOf(focused), focused, count, keys, enabled);
        if (moved >= 0)
        {
            internal::SelectionMoveFocus(idOf(moved));
            if (!readOnly)
                chosen = moved;
        }
    }

    bool written = false;
    if (!readOnly && chosen != selected && chosen >= 0 && chosen < count)
    {
        *index = chosen;
        written = true;
        ImGui::MarkItemEdited(widgetId);
    }

    const float bottom = internal::DrawControlMessage(message, ImVec2(origin.x, optionsOrigin.y + optionsHeight),
                                                      layout.width, groupDisabled, widgetId,
                                                      ImRect(origin, ImVec2(origin.x + layout.width, optionsOrigin.y + optionsHeight)));
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(origin.x + layout.width, bottom));
    ImGui::EndGroup();
    internal::FinishWidgetGroup(widgetId);
    if (written)
        g.LastItemData.StatusFlags |= ImGuiItemStatusFlags_Edited;
    return written;
}

} // namespace

bool RadioGroup(const char* label, int* index, std::span<const ChoiceItem> items, const RadioGroupOptions& options)
{
    return RadioGroupImpl(label, index, {items, {}}, options);
}

bool RadioGroup(const char* label, int* index, std::span<const char* const> items, const RadioGroupOptions& options)
{
    return RadioGroupImpl(label, index, {{}, items}, options);
}

} // namespace facet
