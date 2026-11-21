// Dropdown (lists.h): a field that opens a list of items below it (above when there is no room),
// with icons, group headers, typeahead and an optional fuzzy filter.
//
// The field is one item (list_parts.h, ListsField). The open list is a level of the popup host
// holding a list view of the list core, driven from the field: the field keeps the keyboard focus
// and gives the list its keys (ListsClaimListKeys), so the arrows move the active row and Enter,
// Space or Tab choose it. A searchable list has its own filter field at the top, which takes the
// focus and drives the rows the same way through the text field core.

#include "facet/widgets/lists.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/control.h"
#include "internal/item.h"
#include "internal/popup_host.h"
#include "internal/state_storage.h"
#include "widgets/lists/list_parts.h"

namespace facet
{
namespace
{

struct DropdownState
{
    internal::ListTypeahead typed;  // typeahead on the closed field
    bool mixedCleared = false;      // a choice was made since `mixed` was switched on
};

bool DropdownImpl(const char* label, int* index, const internal::ListsSource& source, const DropdownOptions& options)
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
    DropdownState& state = GetState<DropdownState>(id);
    const Strings& strings = GetStrings();

    const bool readOnly = options.readOnly || index == nullptr || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const bool mixedFlag = options.mixed || (next.itemFlags & ImGuiItemFlags_MixedValue) != 0;
    if (!mixedFlag)
        state.mixedCleared = false;
    const bool mixed = mixedFlag && !state.mixedCleared;
    const bool open = IsPopupLevelOpen(level);

    // The field shows the chosen item, else the placeholder; Strings::mixed until a choice is made.
    ChoiceItem chosen;
    const bool hasChoice = index != nullptr && source.Get(*index, &chosen);
    ListsFieldDesc field;
    field.size = options.size;
    field.width = options.width;
    field.next = &next;
    field.forced = &forced;
    if (mixed)
    {
        field.text = strings.mixed;
        field.secondaryText = true;
    }
    else if (hasChoice)
    {
        field.icon = chosen.icon;
        field.text = chosen.label;
        field.textEnd = ImGui::FindRenderedTextEnd(chosen.label);
    }
    else
    {
        field.text = options.placeholder != nullptr ? options.placeholder : strings.select;
        field.secondaryText = true;
    }
    field.open = open;
    field.itemsError = options.state == DataState::Error;
    field.readOnly = readOnly;
    field.error = options.error;
    field.helpText = options.helpText;
    const ListsFieldResult result = ListsField(id, label, field);
    const ItemState& item = result.item;

    bool changed = false;
    const bool canOpen = !readOnly && !item.disabled && item.forced == ForcedItemState::None;
    bool openNow = canOpen && !open && result.openRequest;
    if (canOpen && !open && item.focused)
    {
        // Down opens the list; it is owned while the field has the focus, so navigation leaves it.
        ImGui::SetKeyOwner(ImGuiKey_DownArrow, id);
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, ImGuiInputFlags_None, id))
            openNow = true;
        // Typing on the closed field jumps to the next item whose label starts with the text.
        if (state.typed.Update())
        {
            const int target = state.typed.Find(source.count, *index, &ListsItemEntry,
                                                const_cast<ListsSource*>(&source));
            if (target >= 0)
            {
                *index = target;
                state.mixedCleared = true;
                changed = true;
            }
        }
    }

    const PopupAnchor anchor = ListsFieldAnchor(result.frame);
    PopupOptions popup;
    popup.takeFocus = options.searchable;
    if (openNow)
        OpenPopupLevel(level, anchor, popup);
    if (item.open)
        ForcePopupLevelOpen(level, anchor, popup);
    SetPopupLevelAnchor(level, anchor);
    const bool listOpen = IsPopupLevelOpen(level);
    if (listOpen && !options.searchable && !item.open)
        ListsClaimListKeys(level, true);

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
        list.chosen = hasChoice ? *index : -1;
        list.readOnly = readOnly;
        list.opened = openNow;
        list.forced = item.open;
        const ListsPopupResult picked = ListsPopupList(list);
        if (picked.activated >= 0 && index != nullptr && !readOnly)
        {
            *index = picked.activated;
            state.mixedCleared = true;
            changed = true;
            if (picked.byKey)
                ListsCloseByKey(level);
            else
                ClosePopupLevel(level);
        }
        if (picked.close)
            ListsCloseByKey(level);
        EndPopupLevel();
    }
    if (changed)
        ListsMarkEdited();
    return changed;
}

} // namespace

bool Dropdown(const char* label, int* index, std::span<const ChoiceItem> items, const DropdownOptions& options)
{
    return DropdownImpl(label, index, internal::ListsSourceOf(items), options);
}

bool Dropdown(const char* label, int* index, std::span<const char* const> items, const DropdownOptions& options)
{
    return DropdownImpl(label, index, internal::ListsSourceOf(items), options);
}

bool Dropdown(const char* label, int* index, int itemCount, ChoiceGetter getter, void* user,
              const DropdownOptions& options)
{
    return DropdownImpl(label, index, internal::ListsSourceOf(itemCount, getter, user), options);
}

} // namespace facet
