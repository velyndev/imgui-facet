// CheckboxGroup: a labelled list of checkboxes in one or more columns, with an optional select-all
// box that shows Mixed while the list is partly checked.

#include "facet/widgets/selection.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

bool CheckboxGroupImpl(const char* label, std::span<bool> checked, const internal::SelectionItems& items,
                       const CheckboxGroupOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const Theme& t = T();
    const ImGuiID widgetId = window->GetID(label);
    const int count = items.Count();
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const auto hasFlag = [&](int i) { return i < static_cast<int>(checked.size()); };
    const auto usable = [&](int i) { return hasFlag(i) && !items.At(i).disabled; };

    // ---- Layout: the label above, the select-all box, then the items in columns ----
    const float width = internal::ResolveItemWidth(0.0f, next, kFill);
    const internal::AboveLabel above = internal::MeasureAboveLabel(label, m.size);
    const float box = std::max(1.0f, PxSnapped(t.metrics.checkboxSize));
    const float markGap = std::round(Px(t.spacing.sm));
    const float columnGap = std::round(Px(t.spacing.xl));
    // Under a select-all box the items are indented so their boxes line up with its label.
    const float indent = options.selectAll ? box + markGap : 0.0f;
    const float itemsWidth = std::max(1.0f, width - indent);
    float widest = 0.0f;
    {
        internal::ControlTextScope text(m);
        for (int i = 0; i < count; ++i)
        {
            const ChoiceItem item = items.At(i);
            widest = std::max(widest, ImGui::CalcTextSize(item.label, ImGui::FindRenderedTextEnd(item.label)).x);
        }
    }
    const float natural = std::ceil(box + markGap + widest);
    int columns = options.columns > 0 ? options.columns
                                      : static_cast<int>((itemsWidth + columnGap) / std::max(1.0f, natural + columnGap));
    columns = std::clamp(columns, 1, std::max(1, count));
    // Columns as wide as the widest item, so the boxes stay close to their labels in a wide row;
    // narrower when the width shares out less than that.
    const float share =
        std::max(1.0f, std::floor((itemsWidth - columnGap * static_cast<float>(columns - 1)) / static_cast<float>(columns)));
    const float columnWidth = std::min(natural, share);
    const int rows = (count + columns - 1) / columns;
    const float listTop = above.height + (options.selectAll ? m.height : 0.0f);
    const float height = listTop + static_cast<float>(rows) * m.height;

    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    ImGui::BeginGroup();
    if (above.text != nullptr)
        internal::DrawAboveLabel(window->DrawList, above, origin, width, m.size, groupDisabled);

    bool written = false;
    ImGuiID editor = widgetId;
    const auto boxNext = [&](float boxWidth) {
        internal::NextItemData data;
        data.itemFlags = next.itemFlags;
        data.hasWidth = true;
        data.width = boxWidth;
        return data;
    };

    // The select-all box: Mixed while some enabled items are checked and others are not.
    if (options.selectAll)
    {
        int on = 0;
        int enabled = 0;
        for (int i = 0; i < count; ++i)
            if (usable(i))
            {
                ++enabled;
                on += checked[static_cast<std::size_t>(i)] ? 1 : 0;
            }
        internal::SelectionMark all = on == 0 ? internal::SelectionMark::Off
                                      : on == enabled ? internal::SelectionMark::On
                                                      : internal::SelectionMark::Mixed;
        window->DC.CursorPos = ImVec2(origin.x, origin.y + above.height);
        internal::SelectionCheckboxDesc desc;
        desc.size = options.size;
        desc.readOnly = options.readOnly;
        desc.disabled = enabled == 0;
        const ImGuiID allId = ImGui::GetIDWithSeed("select all", nullptr, widgetId);
        if (internal::SelectionCheckbox(GetStrings().selectAll, allId, &all, boxNext(width), desc))
        {
            const bool value = all == internal::SelectionMark::On;
            for (int i = 0; i < count; ++i)
                if (usable(i) && checked[static_cast<std::size_t>(i)] != value)
                {
                    checked[static_cast<std::size_t>(i)] = value;
                    written = true;
                }
            editor = allId;
        }
    }

    // The items, row by row so the Tab order follows the reading order.
    for (int i = 0; i < count; ++i)
    {
        const ChoiceItem item = items.At(i);
        const int row = i / columns;
        const int column = i % columns;
        window->DC.CursorPos = ImVec2(origin.x + indent + static_cast<float>(column) * (columnWidth + columnGap),
                                      origin.y + listTop + static_cast<float>(row) * m.height);
        internal::SelectionMark mark = hasFlag(i) && checked[static_cast<std::size_t>(i)] ? internal::SelectionMark::On
                                                                                         : internal::SelectionMark::Off;
        internal::SelectionCheckboxDesc desc;
        desc.size = options.size;
        desc.readOnly = options.readOnly;
        desc.disabled = !usable(i);
        const ImGuiID itemId = internal::SelectionEntryId(widgetId, item.id, i);
        if (internal::SelectionCheckbox(item.label, itemId, hasFlag(i) ? &mark : nullptr, boxNext(columnWidth), desc))
        {
            checked[static_cast<std::size_t>(i)] = mark == internal::SelectionMark::On;
            written = true;
            editor = itemId;
        }
    }

    // The message line under the list.
    window->DC.CursorPos = ImVec2(origin.x, origin.y + height);
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(origin.x + width, origin.y + height));
    const internal::ControlMessage message = internal::PickControlMessage(options.error, options.helpText);
    const float bottom = internal::DrawControlMessage(message, ImVec2(origin.x, origin.y + height), width, groupDisabled,
                                                      widgetId, ImRect(origin, ImVec2(origin.x + width, origin.y + height)));
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(origin.x + width, bottom));
    ImGui::EndGroup();
    internal::FinishWidgetGroup(widgetId);
    if (written)
        internal::MarkWidgetEdited(editor);
    return written;
}

} // namespace

bool CheckboxGroup(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                   const CheckboxGroupOptions& options)
{
    return CheckboxGroupImpl(label, checked, {items, {}}, options);
}

bool CheckboxGroup(const char* label, std::span<bool> checked, std::span<const char* const> items,
                   const CheckboxGroupOptions& options)
{
    return CheckboxGroupImpl(label, checked, {{}, items}, options);
}

} // namespace facet
