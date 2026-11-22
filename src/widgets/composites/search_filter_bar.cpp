// SearchFilterBar: a TextField with the search icon and a clear button, an optional result count,
// and the filter chips as a single-line ChipGroup. The field and the chip group are the finished
// widgets; this file adds the arrangement, the count text and the copy of the chips' active flags.
//
// The field is a TextField rather than a SearchField because the declaration promises the text
// field's Escape (restore the query from activation, rule 3), while SearchField's Escape clears the
// query first; the look is the same field frame with the search icon.
//
// Layout: the field with the label in front takes the width the chips and the count leave, but
// never less than composites.searchFieldMinWidth; when that does not fit, the chips go on a line of
// their own under the field and take the whole width. Chips that do not fit their line move into
// the chip group's "more" menu. The field keeps the widget's id, so the item queries after the
// call describe the query edit; the chips are named under that id.

#include "facet/widgets/composites.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <memory>
#include <new>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/color.h"
#include "facet/core/scale.h"
#include "facet/core/text.h"
#include "facet/core/theme.h"
#include "facet/widgets/inputs.h"
#include "facet/widgets/selection.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/state_storage.h"
#include "internal/text_util.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

// The chips as the chip group takes them, rebuilt every frame in storage that only grows.
struct SearchFilterBarState
{
    std::unique_ptr<ChoiceItem[]> items;
    std::unique_ptr<bool[]> flags;
    int capacity = 0;

    bool Reserve(int count)
    {
        if (count <= capacity)
            return true;
        try
        {
            std::unique_ptr<ChoiceItem[]> newItems(new ChoiceItem[static_cast<std::size_t>(count)]);
            std::unique_ptr<bool[]> newFlags(new bool[static_cast<std::size_t>(count)]);
            items = std::move(newItems);
            flags = std::move(newFlags);
            capacity = count;
            return true;
        }
        catch (const std::bad_alloc&)
        {
            return false;
        }
    }
};

template <class FieldCall>
bool SearchFilterBarImpl(const char* label, std::span<FilterChip> filters, const SearchFilterBarOptions& options,
                         FieldCall&& field)
{
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const Theme& t = T();
    const Strings& s = GetStrings();
    internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(ControlSize::Inherit);
    const ImGuiID id = window->GetID(label);
    const bool readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool disabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;

    // ---- The chips ----
    SearchFilterBarState& state = internal::GetState<SearchFilterBarState>(id);
    int chipCount = static_cast<int>(filters.size());
    if (!state.Reserve(chipCount))
        chipCount = state.capacity;
    float chipsWidth = 0.0f;
    const float gap = std::round(Px(t.spacing.sm));
    for (int i = 0; i < chipCount; ++i)
    {
        const FilterChip& chip = filters[static_cast<std::size_t>(i)];
        ChoiceItem& item = state.items[static_cast<std::size_t>(i)];
        item = ChoiceItem{};
        item.label = chip.label != nullptr ? chip.label : "";
        item.icon = chip.icon;
        item.id = chip.id;
        state.flags[static_cast<std::size_t>(i)] = chip.active;
        internal::SelectionChipDesc desc;
        desc.selected = chip.active;
        desc.icon = chip.icon;
        chipsWidth += internal::SelectionChipWidth(item.label, desc) + (i > 0 ? gap : 0.0f);
    }

    // ---- The result count ----
    char countText[64] = "";
    float countWidth = 0.0f;
    if (options.resultCount >= 0)
    {
        const internal::TextToken token{"count", options.resultCount};
        internal::FormatTokens(countText, sizeof(countText), options.resultCount == 1 ? s.resultCountOne : s.resultCount,
                               std::span<const internal::TextToken>(&token, 1));
        countWidth = std::ceil(MeasureText(TextStyle::Footnote, countText).x) + gap;
    }

    // ---- Layout ----
    const float width = std::round(internal::ResolveItemWidth(options.width, next, kFill));
    const internal::FrontLabel front = internal::MeasureFrontLabel(label, m.size, FLT_MAX);
    const float minField = std::round(front.width + Px(t.composites.searchFieldMinWidth));
    const bool beside = chipCount > 0 && width - chipsWidth - gap - countWidth >= minField;
    const float fieldWidth = std::max(1.0f, beside ? width - chipsWidth - gap - countWidth : width - countWidth);
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));

    // The next-item data goes to the field, which stands for the widget; the width was used above.
    next.data.HasFlags &= ~ImGuiNextItemDataFlags_HasWidth;
    next.hasWidth = false;

    ImGui::BeginGroup();
    // Part 0 (or -1) is the field, part i + 1 the chip i.
    if (forced.state != internal::ForcedItemState::None && forced.part <= 0)
        internal::ForceNextItemState(forced.state);
    internal::ApplyNextItemData(next);
    ImGui::SetNextItemWidth(fieldWidth);
    const bool queryWritten = field(readOnly);

    if (countWidth > 0.0f)
    {
        ImGui::SameLine(0.0f, 0.0f);
        const ImVec2 at = window->DC.CursorPos;
        ImGui::Dummy(ImVec2(countWidth, m.height));
        if (ImGui::IsRectVisible(at, ImVec2(at.x + countWidth, at.y + m.height)))
        {
            internal::ControlTextScope text(TextStyle::Footnote);
            window->DrawList->AddText(ImVec2(at.x + gap, CenteredTextY(at.y, m.height)),
                                      ToU32(internal::ControlSecondaryColor(disabled)), countText);
        }
    }

    bool chipsWritten = false;
    if (chipCount > 0)
    {
        if (beside)
            ImGui::SameLine(0.0f, gap);
        else
            ImGui::SetCursorScreenPos(ImVec2(origin.x, window->DC.CursorPos.y));
        ImGui::PushOverrideID(id);
        if (forced.state != internal::ForcedItemState::None && forced.part >= 1)
            internal::ForceNextItemState(forced.state, forced.part - 1);
        ImGui::SetNextItemWidth(beside ? chipsWidth : width);
        ChipGroupOptions chips;
        chips.singleLine = true;
        chips.readOnly = readOnly;
        if (ChipGroup("##chips", std::span<bool>(state.flags.get(), static_cast<std::size_t>(chipCount)),
                      std::span<const ChoiceItem>(state.items.get(), static_cast<std::size_t>(chipCount)), chips) &&
            !readOnly)
        {
            for (int i = 0; i < chipCount; ++i)
            {
                FilterChip& chip = filters[static_cast<std::size_t>(i)];
                if (chip.active != state.flags[static_cast<std::size_t>(i)])
                {
                    chip.active = state.flags[static_cast<std::size_t>(i)];
                    chipsWritten = true;
                }
            }
        }
        ImGui::PopID();
    }
    ImGui::EndGroup();
    internal::FinishWidgetGroup(id);
    if (chipsWritten)
        internal::MarkWidgetEdited(id);
    return queryWritten || chipsWritten;
}

TextFieldOptions SearchFilterBarField(const SearchFilterBarOptions& options, bool readOnly)
{
    TextFieldOptions field;
    field.placeholder = options.placeholder != nullptr ? options.placeholder : GetStrings().search;
    field.icon = Icon("search");
    field.clearButton = true;
    field.readOnly = readOnly;
    return field;
}

} // namespace

bool SearchFilterBar(const char* label, std::string* query, std::span<FilterChip> filters,
                     const SearchFilterBarOptions& options)
{
    return SearchFilterBarImpl(label, filters, options, [&](bool readOnly) {
        return TextField(label != nullptr ? label : "", query, SearchFilterBarField(options, readOnly));
    });
}

bool SearchFilterBar(const char* label, char* buffer, std::size_t bufferSize, std::span<FilterChip> filters,
                     const SearchFilterBarOptions& options)
{
    return SearchFilterBarImpl(label, filters, options, [&](bool readOnly) {
        return TextField(label != nullptr ? label : "", buffer, bufferSize, SearchFilterBarField(options, readOnly));
    });
}

} // namespace facet
