// BeginVirtualList, VirtualListRow, EndVirtualList and SetNextVirtualListScrollTo (containers.h):
// the list view of src/internal/list_view.h as a public container. The view puts the rows in a
// scroll area of the scroll core, hands out only the visible rows (plus the focused one and the
// range start of a Shift selection) through the row ranges of the list core, so the cost of a
// frame does not grow with the row count, runs the selection through ImGui::BeginMultiSelect,
// reads the keys, draws the hover and selection fills and the focus ring, and shows the loading,
// error and empty states in the area's place.

#include "facet/widgets/containers.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "internal/control.h"
#include "internal/list_view.h"
#include "internal/scope_stack.h"

namespace facet
{
namespace
{

// SetNextVirtualListScrollTo: the row the next BeginVirtualList scrolls into view, once.
int& PendingVirtualListScroll()
{
    static int row = -1;
    return row;
}

} // namespace

bool BeginVirtualList(const char* id, int rowCount, ImGuiSelectionBasicStorage* selection,
                      const VirtualListOptions& options)
{
    ImGuiWindow* window = internal::ControlWindow();
    int& pending = PendingVirtualListScroll();
    const int reveal = pending;
    pending = -1;
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const internal::NextItemData next = internal::TakeNextItemData();
    internal::ListViewDesc desc;
    desc.rowCount = rowCount < 0 ? 0 : rowCount;
    desc.size = ControlSize::Inherit;
    desc.rowHeight = options.rowHeight > 0.0f ? Px(options.rowHeight) : 0.0f;
    // The width follows the next-item width when one was set, else the available width; 0 for
    // the height fills the available height.
    desc.areaSize.x = internal::ResolveItemWidth(0.0f, next, kFill);
    desc.areaSize.y = options.height > 0.0f ? Px(options.height) : 0.0f;
    desc.selection = selection;
    desc.boxSelect = selection != nullptr;
    desc.state = options.state;
    desc.emptyText = options.emptyText;
    desc.errorText = options.errorText;
    desc.revealRow = reveal < desc.rowCount ? reveal : -1;
    if (!internal::BeginListView(window->GetID(id), desc))
        return false;
    internal::PushScope("BeginVirtualList");
    return true;
}

bool VirtualListRow(int index)
{
    if (internal::ControlWindow() == nullptr)
        return false;
    return internal::ListViewRow(index).activated;
}

void EndVirtualList()
{
    if (!internal::PopScope("BeginVirtualList"))
        return;
    (void)internal::EndListView();
}

void SetNextVirtualListScrollTo(int row)
{
    PendingVirtualListScroll() = row;
}

} // namespace facet
