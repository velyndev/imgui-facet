// ListBox (lists.h): a framed, scrolling list with one chosen row.
//
// A list view of the list core over the display rows (items and group headers) with a single
// choice on the selection pill; the label sits above the frame (list_parts.h, BeginListsBox).

#include "facet/widgets/lists.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/control.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/state_storage.h"
#include "widgets/lists/list_parts.h"

namespace facet
{
namespace
{

struct ListBoxState
{
    internal::ListsRows rows;
    int builtCount = -1;
    bool shown = false;  // the list was drawn before: the chosen row was scrolled into view then
};

bool ListBoxImpl(const char* label, int* index, const internal::ListsSource& source, const ListBoxOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (label == nullptr)
        label = "";
    const NextItemData next = TakeNextItemData();
    const ImGuiID id = window->GetID(label);
    ListBoxState& state = GetState<ListBoxState>(id);
    // Spans are cheap to read every frame, so their rows follow the caller's items at once; a getter
    // is read in full only when its count changes (the group headers need every item).
    if (source.getter == nullptr || state.builtCount != source.count)
    {
        ListsRowsDesc rowsDesc;
        rowsDesc.groups = true;
        BuildListsRows(&state.rows, source, rowsDesc);
        state.builtCount = source.count;
    }
    const bool readOnly = options.readOnly || index == nullptr;
    int choice = index != nullptr ? state.rows.RowOf(*index) : -1;

    ListsBoxDesc box;
    box.size = options.size;
    box.width = options.width;
    box.next = &next;
    box.visibleRows = options.visibleRows;
    box.readOnly = readOnly;
    box.error = options.error;
    box.helpText = options.helpText;
    const ListsBoxLayout layout = BeginListsBox(label, box);

    const ListsEntryContext entries{&source, &state.rows};
    ListViewDesc view;
    view.rowCount = state.rows.Count();
    view.size = options.size;
    view.areaSize = layout.areaSize;
    view.choice = &choice;
    view.pill = true;
    view.readOnly = readOnly;
    view.entry = &ListsEntry;
    view.user = const_cast<ListsEntryContext*>(&entries);
    view.state = options.state;
    view.emptyText = options.emptyText;
    view.errorText = options.errorText;
    // The first time the list is shown, the chosen row is scrolled into view.
    view.revealRow = state.shown ? -1 : choice;
    state.shown = true;
    bool changed = false;
    if (BeginListView(id, view))
    {
        RowRange range;
        while (NextListRows(&range))
        {
            for (int i = range.first; i < range.last; ++i)
            {
                const ListRowState row = ListViewRow(i);
                ChoiceItem item;
                source.Get(state.rows.ItemAt(i), &item);
                if (state.rows.IsHeader(i))
                {
                    if (row.item.visible)
                        DrawListsHeader(row.content, item.group);
                    if (ImGui::IsMouseHoveringRect(row.rect.Min, row.rect.Max) && ImGui::IsWindowHovered())
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
                    continue;
                }
                ListsRowLook look;
                look.selected = row.selected;
                DrawListsRow(row, item, look, options.size);
            }
        }
        const ListViewResult result = EndListView();
        if (result.changed && index != nullptr && !readOnly)
        {
            const int item = state.rows.ItemAt(choice);
            if (item >= 0 && item != *index)
            {
                *index = item;
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

bool ListBox(const char* label, int* index, std::span<const ChoiceItem> items, const ListBoxOptions& options)
{
    return ListBoxImpl(label, index, internal::ListsSourceOf(items), options);
}

bool ListBox(const char* label, int* index, std::span<const char* const> items, const ListBoxOptions& options)
{
    return ListBoxImpl(label, index, internal::ListsSourceOf(items), options);
}

bool ListBox(const char* label, int* index, int itemCount, ChoiceGetter getter, void* user,
             const ListBoxOptions& options)
{
    return ListBoxImpl(label, index, internal::ListsSourceOf(itemCount, getter, user), options);
}

} // namespace facet
