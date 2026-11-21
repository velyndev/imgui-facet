// SearchableChecklist (lists.h): an inline checklist for long lists, with a filter field, a
// select-all row for the shown items and the number of checked items.
//
// The label line carries the count (list_parts.h, BeginListsBox with a trailing text); the filter is
// a text field of the text field core above the frame; the rows are a list view of the list core with
// a checkbox per row, toggled by a click, Space or Enter.

#include "facet/widgets/lists.h"

#include <cstring>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/state_storage.h"
#include "internal/text_field.h"
#include "internal/text_util.h"
#include "widgets/lists/list_parts.h"

namespace facet
{
namespace
{

struct ChecklistState
{
    internal::ListsRows rows;
    char query[internal::kListsQueryCapacity] = {};
    int builtCount = -1;
    bool built = false;
    bool listFocused = false;  // the rows had the keyboard focus in the last frame
};

bool SearchableChecklistImpl(const char* label, std::span<bool> checked, const internal::ListsSource& source,
                             const SearchableChecklistOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const NextItemData next = TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    ChecklistState& state = GetState<ChecklistState>(id);
    const bool readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    const Theme& theme = T();

    // The count of checked items at the end of the label line.
    char count[64];
    const TextToken tokens[] = {{"count", ListsCheckedCount(checked, source.count)}};
    FormatTokens(count, sizeof(count), GetStrings().selectedCount, tokens);

    ListsBoxDesc box;
    box.width = options.width;
    box.next = &next;
    box.visibleRows = options.visibleRows;
    box.readOnly = readOnly;
    box.error = options.error;
    box.helpText = options.helpText;
    box.trailing = count;
    box.headerHeight = ControlHeight(ControlSize::Inherit) + Px(theme.spacing.sm);
    const ListsBoxLayout layout = BeginListsBox(label, box);

    // The filter field, in the room above the frame.
    bool filterChanged = false;
    {
        const ImVec2 frameStart = window->DC.CursorPos;
        window->DC.CursorPos = layout.header.Min;
        TextFieldParams params;
        params.placeholder = options.placeholder != nullptr ? options.placeholder : GetStrings().search;
        params.icon = Icon("search");
        params.clearButton = true;
        params.width = layout.width / Px(1.0f);
        ImGui::PushOverrideID(id);  // the field's id belongs to this checklist
        const TextFieldResult field = TextFieldBehavior("##facet.filter", state.query, sizeof(state.query), params);
        ImGui::PopID();
        filterChanged = field.edited;
        window->DC.CursorPos = frameStart;
    }

    // The rows are built when the filter or the item count changes. An unfiltered span list also
    // follows the caller's items every frame (cheap without a filter); with checkedFirst only while
    // the pointer and the keyboard are elsewhere, so a row does not jump away under a click.
    const ImGuiID listId = ImHashStr("##facet.rows", 0, id);
    const bool busy = ImGui::IsMouseHoveringRect(layout.frame.Min, layout.frame.Max) || state.listFocused;
    const bool spanFollows =
        source.getter == nullptr && state.query[0] == '\0' && (!options.checkedFirst || !busy);
    if (!state.built || filterChanged || state.builtCount != source.count || spanFollows)
    {
        ListsRowsDesc rowsDesc;
        rowsDesc.query = state.query;
        rowsDesc.groups = false;
        rowsDesc.selectAll = options.selectAllRow;
        if (options.checkedFirst)
            rowsDesc.first = checked;
        BuildListsRows(&state.rows, source, rowsDesc);
        state.built = true;
        state.builtCount = source.count;
    }

    const ListsEntryContext entries{&source, &state.rows};
    ListViewDesc view;
    view.rowCount = state.rows.Count();
    view.areaSize = layout.areaSize;
    view.readOnly = readOnly;
    view.entry = &ListsEntry;
    view.user = const_cast<ListsEntryContext*>(&entries);
    view.state = options.state;
    view.emptyText = options.emptyText != nullptr ? options.emptyText
                                                  : (state.query[0] != '\0' ? GetStrings().noResults : GetStrings().empty);
    view.errorText = options.errorText;
    bool changed = false;
    state.listFocused = false;
    if (BeginListView(listId, view))
    {
        state.listFocused = GImGui->NavWindow == GImGui->CurrentWindow;
        int toggled = -1;
        RowRange range;
        while (NextListRows(&range))
        {
            for (int i = range.first; i < range.last; ++i)
            {
                const ListRowState row = ListViewRow(i);
                const int value = state.rows.rows[i];
                if (value == kListsSelectAllRow)
                    DrawListsSelectAllRow(row, ListsSelectAllState(source, state.rows, checked), readOnly,
                                          ControlSize::Inherit);
                else
                {
                    ChoiceItem item;
                    source.Get(value, &item);
                    ListsRowLook look;
                    look.checkbox = true;
                    look.check = ListsFlag(checked, value) ? CheckState::On : CheckState::Off;
                    look.readOnly = readOnly;
                    look.query = state.query;
                    DrawListsRow(row, item, look, ControlSize::Inherit);
                }
                if (row.item.pressed && !readOnly && !row.disabled)
                    toggled = i;
            }
        }
        EndListView();
        if (toggled >= 0)
        {
            const int value = state.rows.rows[toggled];
            if (value == kListsSelectAllRow)
            {
                const bool on = ListsSelectAllState(source, state.rows, checked) != CheckState::On;
                changed = ListsApplySelectAll(source, state.rows, checked, on);
            }
            else if (value >= 0 && value < static_cast<int>(checked.size()))
            {
                checked[static_cast<std::size_t>(value)] = !checked[static_cast<std::size_t>(value)];
                changed = true;
            }
        }
    }
    EndListsBox(id, layout, box);
    if (changed)
        ListsMarkEdited();
    return changed;
}

} // namespace

bool SearchableChecklist(const char* label, std::span<bool> checked, std::span<const ChoiceItem> items,
                         const SearchableChecklistOptions& options)
{
    return SearchableChecklistImpl(label, checked, internal::ListsSourceOf(items), options);
}

bool SearchableChecklist(const char* label, std::span<bool> checked, std::span<const char* const> items,
                         const SearchableChecklistOptions& options)
{
    return SearchableChecklistImpl(label, checked, internal::ListsSourceOf(items), options);
}

bool SearchableChecklist(const char* label, std::span<bool> checked, int itemCount, ChoiceGetter getter, void* user,
                         const SearchableChecklistOptions& options)
{
    return SearchableChecklistImpl(label, checked, internal::ListsSourceOf(itemCount, getter, user), options);
}

} // namespace facet
