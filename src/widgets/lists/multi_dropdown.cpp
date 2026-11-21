// MultiDropdown (lists.h): a field whose list has a checkbox per item and stays open while items
// are toggled.
//
// The field and the open list are the ones of Dropdown (list_parts.h); the rows show checkboxes,
// and activating a row (Space, Enter, a click) toggles its entry of `checked` instead of closing.

#include "facet/widgets/lists.h"

#include <algorithm>
#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/control.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/text_util.h"
#include "widgets/lists/list_parts.h"

namespace facet
{
namespace
{

// Longest summary the field builds; FitText cuts what it shows to the field width anyway.
constexpr int kSummarySize = 256;

// The checked labels, comma separated, into `out`; returns how many items are checked.
int BuildSummary(const internal::ListsSource& source, std::span<const bool> checked, MultiDropdownSummary summary,
                 char* out, int outSize)
{
    int count = 0;
    int length = 0;
    out[0] = '\0';
    const int limit = std::min(static_cast<int>(checked.size()), source.count);
    for (int i = 0; i < limit; ++i)
    {
        if (!checked[static_cast<std::size_t>(i)])
            continue;
        ++count;
        if (summary != MultiDropdownSummary::List || length >= outSize - 1)
            continue;
        ChoiceItem item;
        if (!source.Get(i, &item))
            continue;
        const char* end = ImGui::FindRenderedTextEnd(item.label);
        if (length > 0 && length + 2 < outSize - 1)
        {
            out[length++] = ',';
            out[length++] = ' ';
        }
        const int bytes = std::min(static_cast<int>(end - item.label), outSize - 1 - length);
        std::memcpy(out + length, item.label, static_cast<std::size_t>(std::max(bytes, 0)));
        length += std::max(bytes, 0);
        out[length] = '\0';
    }
    if (summary == MultiDropdownSummary::Count && count > 0)
    {
        const internal::TextToken tokens[] = {{"count", count}};
        internal::FormatTokens(out, static_cast<std::size_t>(outSize), GetStrings().selectedCount, tokens);
    }
    return count;
}

bool MultiDropdownImpl(const char* label, std::span<bool> checked, const internal::ListsSource& source,
                       const MultiDropdownOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const NextItemData next = TakeNextItemData();
    const ForcedItemRequest forced = TakeForcedItemState();
    const ImGuiID id = window->GetID(label);
    const ImGuiID level = ListsLevelId(id);
    const bool readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool open = IsPopupLevelOpen(level);

    char summary[kSummarySize];
    const int count = BuildSummary(source, checked, options.summary, summary, kSummarySize);
    ListsFieldDesc field;
    field.size = options.size;
    field.width = options.width;
    field.next = &next;
    field.forced = &forced;
    if (count > 0)
    {
        field.text = summary;
    }
    else
    {
        field.text = options.placeholder != nullptr ? options.placeholder : GetStrings().select;
        field.secondaryText = true;
    }
    field.open = open;
    field.itemsError = options.state == DataState::Error;
    field.readOnly = readOnly;
    field.error = options.error;
    field.helpText = options.helpText;
    const ListsFieldResult result = ListsField(id, label, field);
    const ItemState& item = result.item;

    const bool canOpen = !readOnly && !item.disabled && item.forced == ForcedItemState::None;
    bool openNow = canOpen && !open && result.openRequest;
    if (canOpen && !open && item.focused)
    {
        ImGui::SetKeyOwner(ImGuiKey_DownArrow, id);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_None, id))
            openNow = true;
    }
    const PopupAnchor anchor = ListsFieldAnchor(result.frame);
    PopupOptions popup;
    popup.takeFocus = options.searchable;
    if (openNow)
        OpenPopupLevel(level, anchor, popup);
    if (item.open)
        ForcePopupLevelOpen(level, anchor, popup);
    SetPopupLevelAnchor(level, anchor);
    if (IsPopupLevelOpen(level) && !options.searchable && !item.open)
        ListsClaimListKeys(level, true);

    bool changed = false;
    if (BeginPopupLevel(level))
    {
        ListsPopupDesc list;
        list.level = level;
        list.source = source;
        list.size = options.size;
        list.maxVisibleRows = options.maxVisibleRows;
        list.width = result.frame.GetWidth();
        list.state = options.state;
        list.emptyText = options.emptyText;
        list.errorText = options.errorText;
        list.searchable = options.searchable;
        list.multi = true;
        list.checked = checked;
        list.selectAllRow = options.selectAllRow;
        list.readOnly = readOnly;
        list.opened = openNow;
        list.forced = item.open;
        const ListsPopupResult picked = ListsPopupList(list);
        changed = picked.changed;
        if (picked.close)
            ListsCloseByKey(level);
        EndPopupLevel();
    }
    if (changed)
        ListsMarkEdited();
    return changed;
}

} // namespace

bool MultiDropdown(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                   const MultiDropdownOptions& options)
{
    return MultiDropdownImpl(label, checked, internal::ListsSourceOf(items), options);
}

bool MultiDropdown(const char* label, std::span<bool> checked, std::span<const char* const> items,
                   const MultiDropdownOptions& options)
{
    return MultiDropdownImpl(label, checked, internal::ListsSourceOf(items), options);
}

} // namespace facet
