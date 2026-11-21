// SelectableList (lists.h): a framed, scrolling list with multiple selection.
//
// A list view of the list core with one row per item (no group headers, so the storage keys rows by
// item index), the caller's ImGuiSelectionBasicStorage driven by ImGui::BeginMultiSelect, and the
// label above the frame (list_parts.h, BeginListsBox).

#include "facet/widgets/lists.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "internal/control.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "widgets/lists/list_parts.h"

namespace facet
{
namespace
{

// The row the last SelectableList call activated, and the frame of that call.
struct ActivatedRow
{
    int row = -1;
    int frame = -1;
};

ActivatedRow& LastActivated()
{
    static ActivatedRow last;
    return last;
}

bool SelectableListImpl(const char* label, ImGuiSelectionBasicStorage* selection,
                        const internal::ListsSource& source, const SelectableListOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    LastActivated() = {-1, ImGui::GetFrameCount()};
    if (label == nullptr)
        label = "";
    const NextItemData next = TakeNextItemData();
    const ImGuiID id = window->GetID(label);

    ListsBoxDesc box;
    box.size = options.size;
    box.width = options.width;
    box.next = &next;
    box.visibleRows = options.visibleRows;
    box.readOnly = options.readOnly;
    box.error = options.error;
    box.helpText = options.helpText;
    const ListsBoxLayout layout = BeginListsBox(label, box);

    ListViewDesc view;
    view.rowCount = source.count;
    view.size = options.size;
    view.areaSize = layout.areaSize;
    view.selection = selection;
    view.boxSelect = options.boxSelect;
    view.readOnly = options.readOnly;
    view.entry = &ListsItemEntry;
    view.user = const_cast<ListsSource*>(&source);
    view.state = options.state;
    view.emptyText = options.emptyText;
    view.errorText = options.errorText;
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
                source.Get(i, &item);
                ListsRowLook look;
                look.selected = row.selected;
                DrawListsRow(row, item, look, options.size);
            }
        }
        const ListViewResult result = EndListView();
        changed = result.changed && selection != nullptr && !options.readOnly;
        if (result.activated >= 0)
            LastActivated().row = result.activated;
    }
    EndListsBox(id, layout, box);
    if (changed)
        ListsMarkEdited();
    return changed;
}

} // namespace

bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, std::span<const ChoiceItem> items,
                    const SelectableListOptions& options)
{
    return SelectableListImpl(label, selection, internal::ListsSourceOf(items), options);
}

bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, std::span<const char* const> items,
                    const SelectableListOptions& options)
{
    return SelectableListImpl(label, selection, internal::ListsSourceOf(items), options);
}

bool SelectableList(const char* label, ImGuiSelectionBasicStorage* selection, int itemCount, ChoiceGetter getter,
                    void* user, const SelectableListOptions& options)
{
    return SelectableListImpl(label, selection, internal::ListsSourceOf(itemCount, getter, user), options);
}

int SelectableListActivatedRow()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return -1;
    const ActivatedRow& last = LastActivated();
    return last.frame == ImGui::GetFrameCount() ? last.row : -1;
}

} // namespace facet
