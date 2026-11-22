// ChipGroup: chips that wrap into rows, as filters (one flag per item) or as one choice (an index
// shown by the sliding selection pill), with remove buttons, a clear-all chip and a single-line form
// that moves the chips that do not fit into a "more" menu.

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
#include "internal/item.h"
#include "internal/overflow.h"
#include "internal/popup_host.h"
#include "internal/popup_menu.h"
#include "internal/selection_pill.h"
#include "internal/state_storage.h"
#include "widgets/selection/selection_parts.h"

namespace facet
{
namespace
{

// Chips a single-line group can rank for the "more" menu; a longer group wraps instead.
constexpr int kSingleLineMax = internal::kOverflowInlineItems;

struct ChipGroupQuery
{
    int removed = -1;  // the last ChipGroup call this frame
    int frame = -1;
};

ChipGroupQuery& LastGroup()
{
    static ChipGroupQuery query;
    return query;
}

enum class ChipGroupKind
{
    Filters,  // one flag per item
    Choice,   // one index
};

// Where the chips of a group go, row by row.
struct ChipFlow
{
    float width = 0.0f;   // the group, pixels
    float gap = 0.0f;     // between chips and between rows
    float height = 0.0f;  // one chip
    float x = 0.0f;
    float y = 0.0f;
    // The position of the next chip `chipWidth` pixels wide, starting a new row when it does not fit.
    ImVec2 Next(float chipWidth)
    {
        if (x > 0.0f && x + chipWidth > width)
        {
            x = 0.0f;
            y += height + gap;
        }
        const ImVec2 at(x, y);
        x += chipWidth + gap;
        return at;
    }
};

bool ChipGroupImpl(const char* label, ChipGroupKind kind, std::span<bool> flags, int* index,
                   const internal::SelectionItems& items, const ChipGroupOptions& options)
{
    ChipGroupQuery& query = LastGroup();
    query.removed = -1;
    query.frame = ImGui::GetCurrentContext() != nullptr ? ImGui::GetFrameCount() : -1;
    ImGuiWindow* window = internal::ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    const internal::ForcedItemRequest forced = internal::TakeForcedItemState();
    const internal::ControlMetrics m = internal::GetControlMetrics(options.size);
    const Theme& t = T();
    const Colors& c = t.colors;
    const ImGuiID widgetId = window->GetID(label);
    const bool choice = kind == ChipGroupKind::Choice;
    const int count = items.Count();
    const bool readOnly = options.readOnly || (choice && index == nullptr) || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool groupDisabled = (next.itemFlags & ImGuiItemFlags_Disabled) != 0;
    const int selected = choice && index != nullptr ? *index : -1;
    const auto flagOf = [&](int i) { return i < static_cast<int>(flags.size()) && flags[static_cast<std::size_t>(i)]; };
    const auto on = [&](int i) { return choice ? i == selected : flagOf(i); };
    const auto usable = [&](int i) { return !items.At(i).disabled && (choice || i < static_cast<int>(flags.size())); };
    const bool clearChip = options.clearAll && !choice;

    const auto chipDesc = [&](int i) {
        const ChoiceItem item = items.At(i);
        internal::SelectionChipDesc desc;
        desc.selected = on(i);
        desc.removable = options.removable;
        desc.icon = item.icon;
        desc.detail = item.detail;
        desc.size = options.size;
        desc.disabled = !usable(i);
        desc.readOnly = readOnly;
        desc.selectedFill = !choice;  // the choice form's pill draws the tint
        desc.forced = &forced;
        desc.part = i;
        return desc;
    };
    internal::SelectionChipDesc clearDesc;
    clearDesc.icon = Icon("clear");
    clearDesc.size = options.size;
    clearDesc.readOnly = readOnly;
    clearDesc.forced = &forced;
    clearDesc.part = count;
    const char* clearLabel = GetStrings().clear;

    // ---- Layout ----
    const float width = internal::ResolveItemWidth(options.width, next, kFill);
    const internal::AboveLabel above = internal::MeasureAboveLabel(label, m.size);
    const float gap = std::round(Px(t.spacing.sm));
    const ImVec2 origin(std::round(window->DC.CursorPos.x), std::round(window->DC.CursorPos.y));
    const ImVec2 chipsOrigin(origin.x, origin.y + above.height);

    // Single line: which chips stay visible; the rest go into the "more" menu.
    bool shown[kSingleLineMax];
    bool showMore = false;
    float moreWidth = 0.0f;
    const bool singleLine = options.singleLine && count <= kSingleLineMax;
    internal::SelectionChipDesc moreDesc;
    moreDesc.size = options.size;
    moreDesc.forced = &forced;
    moreDesc.part = count + 1;
    const char* moreLabel = GetStrings().more;
    if (singleLine && count > 0)
    {
        internal::OverflowItem fit[kSingleLineMax];
        for (int i = 0; i < count; ++i)
            fit[i] = {std::min(width, internal::SelectionChipWidth(items.At(i).label, chipDesc(i))), on(i) ? 1 : 0,
                      internal::OverflowKind::Item};
        float reserved = 0.0f;
        if (clearChip)
            reserved = internal::SelectionChipWidth(clearLabel, clearDesc) + gap;
        moreWidth = internal::SelectionChipWidth(moreLabel, moreDesc);
        internal::OverflowState& state = internal::GetState<internal::OverflowState>(widgetId);
        const internal::OverflowResult result =
            internal::FitOverflow(std::span<const internal::OverflowItem>(fit, static_cast<std::size_t>(count)),
                                  std::max(0.0f, width - reserved), gap, moreWidth, Px(t.spacing.lg), &state,
                                  std::span<bool>(shown, static_cast<std::size_t>(count)));
        showMore = result.showButton;
    }
    else
    {
        std::fill(shown, shown + kSingleLineMax, true);
    }
    const auto isShown = [&](int i) { return !singleLine || shown[i]; };

    // Measure: every chip in flow order, then the clear and "more" chips.
    ChipFlow flow{width, gap, m.height};
    int rows = count > 0 || clearChip ? 1 : 0;
    const auto place = [&](float chipWidth) {
        const float y = flow.y;
        const ImVec2 at = flow.Next(chipWidth);
        if (at.y > y)
            ++rows;
        return at;
    };
    for (int i = 0; i < count; ++i)
        if (isShown(i))
            place(std::min(width, internal::SelectionChipWidth(items.At(i).label, chipDesc(i))));
    if (showMore)
        place(std::min(width, moreWidth));
    if (clearChip)
        place(std::min(width, internal::SelectionChipWidth(clearLabel, clearDesc)));
    const float chipsHeight = rows > 0 ? flow.y + m.height : 0.0f;

    // ---- Items ----
    ImGui::BeginGroup();
    ImGui::ItemSize(ImVec2(width, above.height + chipsHeight));
    ImDrawList* drawList = window->DrawList;
    if (above.text != nullptr)
        internal::DrawAboveLabel(drawList, above, origin, width, m.size, groupDisabled);

    internal::SelectionPill pill;
    if (choice)
        pill.Begin(widgetId, drawList, ImVec2(window->Pos.x - window->Scroll.x, window->Pos.y - window->Scroll.y));
    const ImVec2 backup = window->DC.CursorPos;
    ChipFlow layout{width, gap, m.height};
    int chosen = selected;
    int flipped = -1;
    bool clear = false;
    ImGuiID editor = widgetId;
    for (int i = 0; i < count; ++i)
    {
        if (!isShown(i))
            continue;
        const internal::SelectionChipDesc desc = chipDesc(i);
        const ChoiceItem item = items.At(i);
        const float chipWidth = std::min(width, internal::SelectionChipWidth(item.label, desc));
        const ImVec2 at = layout.Next(chipWidth);
        window->DC.CursorPos = ImVec2(chipsOrigin.x + at.x, chipsOrigin.y + at.y);
        const ImGuiID chipId = internal::SelectionEntryId(widgetId, item.id, i);
        const internal::SelectionChipResult chip = internal::SelectionChip(item.label, chipId, chipWidth, desc);
        if (choice && desc.selected && !groupDisabled)
            pill.Target(chip.rect);
        if (chip.pressed)
        {
            editor = chipId;
            if (choice)
                chosen = i;
            else
                flipped = i;
        }
        if (chip.removed)
            query.removed = i;
    }

    // The "more" chip lists the chips that did not fit.
    if (showMore)
    {
        const ImVec2 at = layout.Next(std::min(width, moreWidth));
        window->DC.CursorPos = ImVec2(chipsOrigin.x + at.x, chipsOrigin.y + at.y);
        const ImGuiID moreId = ImGui::GetIDWithSeed("more", nullptr, widgetId);
        const ImGuiID menuId = ImGui::GetIDWithSeed("more menu", nullptr, widgetId);
        bool hiddenOn = false;
        for (int i = 0; i < count; ++i)
            hiddenOn = hiddenOn || (!isShown(i) && on(i));
        moreDesc.selected = hiddenOn;
        moreDesc.selectedFill = !choice;
        const internal::SelectionChipResult more = internal::SelectionChip(moreLabel, moreId, std::min(width, moreWidth), moreDesc);
        if (choice && hiddenOn && !groupDisabled)
            pill.Target(more.rect);
        if (more.pressed)
        {
            internal::PopupOptions popup;
            popup.minWidth = Px(t.overlay.menuMinWidth);
            internal::OpenPopupLevel(menuId, {more.rect, internal::PopupSide::Below, internal::PopupAlign::Start}, popup);
        }
        if (internal::BeginPopupLevel(menuId))
        {
            internal::MenuLevelBegin(menuId);
            for (int i = 0; i < count; ++i)
            {
                if (isShown(i))
                    continue;
                const ChoiceItem item = items.At(i);
                ImGui::PushID(i);
                internal::MenuRowDesc row;
                row.icon = item.icon;
                row.checked = on(i);
                row.disabled = !usable(i) || groupDisabled;
                if (internal::MenuRow(menuId, item.label, row).pressed && !readOnly)
                {
                    if (choice)
                        chosen = i;
                    else
                        flipped = i;
                }
                ImGui::PopID();
            }
            internal::EndPopupLevel();
        }
    }
    if (clearChip)
    {
        const float chipWidth = std::min(width, internal::SelectionChipWidth(clearLabel, clearDesc));
        const ImVec2 at = layout.Next(chipWidth);
        window->DC.CursorPos = ImVec2(chipsOrigin.x + at.x, chipsOrigin.y + at.y);
        // Always enabled: disabling it under the focus after it cleared the flags would strand the
        // keyboard focus; with nothing set it simply writes nothing.
        const ImGuiID clearId = ImGui::GetIDWithSeed("clear", nullptr, widgetId);
        if (internal::SelectionChip(clearLabel, clearId, chipWidth, clearDesc).pressed)
        {
            clear = true;
            editor = clearId;
        }
    }
    if (choice)
        pill.End(ToU32(readOnly ? c.fill[0] : c.accentSubtle), m.height * 0.5f);
    window->DC.CursorPos = backup;

    // ---- Writes ----
    bool written = false;
    if (!readOnly)
    {
        if (choice && index != nullptr && chosen != selected && chosen >= 0 && chosen < count)
        {
            *index = chosen;
            written = true;
        }
        if (!choice && flipped >= 0 && flipped < static_cast<int>(flags.size()))
        {
            flags[static_cast<std::size_t>(flipped)] = !flags[static_cast<std::size_t>(flipped)];
            written = true;
        }
        if (clear)
            for (int i = 0; i < count && i < static_cast<int>(flags.size()); ++i)
                if (flags[static_cast<std::size_t>(i)] && usable(i))
                {
                    flags[static_cast<std::size_t>(i)] = false;
                    written = true;
                }
    }

    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, ImVec2(origin.x + width, chipsOrigin.y + chipsHeight));
    ImGui::EndGroup();
    internal::FinishWidgetGroup(widgetId);
    if (written)
        internal::MarkWidgetEdited(editor);
    return written;
}

} // namespace

bool ChipGroup(const char* label, std::span<bool> selected, std::span<const ChoiceItem> items,
               const ChipGroupOptions& options)
{
    return ChipGroupImpl(label, ChipGroupKind::Filters, selected, nullptr, {items, {}}, options);
}

bool ChipGroup(const char* label, std::span<bool> selected, std::span<const char* const> items,
               const ChipGroupOptions& options)
{
    return ChipGroupImpl(label, ChipGroupKind::Filters, selected, nullptr, {{}, items}, options);
}

bool ChipGroup(const char* label, int* index, std::span<const ChoiceItem> items, const ChipGroupOptions& options)
{
    return ChipGroupImpl(label, ChipGroupKind::Choice, {}, index, {items, {}}, options);
}

bool ChipGroup(const char* label, int* index, std::span<const char* const> items, const ChipGroupOptions& options)
{
    return ChipGroupImpl(label, ChipGroupKind::Choice, {}, index, {{}, items}, options);
}

int ChipGroupRemovedItem()
{
    const ChipGroupQuery& query = LastGroup();
    if (ImGui::GetCurrentContext() == nullptr || query.frame != ImGui::GetFrameCount())
        return -1;
    return query.removed;
}

} // namespace facet
