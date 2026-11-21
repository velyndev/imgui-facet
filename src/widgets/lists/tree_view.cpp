// BeginTreeView, BeginTreeNode, TreeLeaf and their Ends (lists.h): a tree over the caller's nodes.
//
// The tree support of the list core (list_view.h, BeginListTree) does the rows, the selection, the
// keys, typeahead, the open states, the loading row and the indent guides; this file draws the
// content of each row as the other lists of the family do (list_parts.h) and pairs the Begin / End
// calls through the scope stack.

#include "facet/widgets/lists.h"

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "internal/control.h"
#include "internal/focus.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "internal/scope_stack.h"
#include "widgets/lists/list_parts.h"

namespace facet
{
namespace
{

// Whether the last EndTreeView wrote its selection, and in which frame.
struct TreeViewChange
{
    bool changed = false;
    int frame = -1;
};

TreeViewChange& LastChange()
{
    static TreeViewChange change;
    return change;
}

internal::ListTreeNodeState TreeRow(const char* label, const TreeNodeOptions& options, bool leaf)
{
    using namespace internal;
    if (label == nullptr)
        label = "";
    ListTreeNodeOptions node;
    node.key = options.id;
    node.leaf = leaf;
    node.defaultOpen = options.defaultOpen;
    node.loading = options.loading;
    // A node inside ImGui::BeginDisabled is left out of ranges and select-all as well.
    node.disabled = (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    // Outside a tree view the list core reports the call and returns an empty state.
    const ListTreeNodeState state = ListTreeNode(label, node);
    if (state.row.item.id == 0)
        return state;
    ChoiceItem entry;
    entry.label = label;
    entry.icon = options.icon;
    entry.detail = options.detail;
    ListsRowLook look;
    look.selected = state.row.selected;
    DrawListsRow(state.row, entry, look, ControlSize::Inherit);
    return state;
}

} // namespace

bool BeginTreeView(const char* id, ImGuiSelectionBasicStorage* selection, const TreeViewOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    ListTreeDesc desc;
    desc.selection = selection;
    desc.areaSize = ImVec2(ResolveItemWidth(options.width, next, kFill), options.height > 0.0f ? Px(options.height) : 0.0f);
    desc.fitHeight = options.height <= 0.0f;
    desc.indentGuides = options.indentGuides;
    desc.state = options.state;
    desc.emptyText = options.emptyText;
    desc.errorText = options.errorText;
    if (!BeginListTree(window->GetID(id), desc))
        return false;
    PushScope("BeginTreeView");
    return true;
}

void EndTreeView()
{
    using namespace internal;
    if (!PopScope("BeginTreeView"))
        return;
    const ListTreeResult result = EndListTree();
    LastChange() = {result.changed, ImGui::GetFrameCount()};
}

bool TreeViewSelectionChanged()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    const TreeViewChange& change = LastChange();
    return change.changed && change.frame == ImGui::GetFrameCount();
}

bool BeginTreeNode(const char* label, const TreeNodeOptions& options)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    if (!TreeRow(label, options, false).open)
        return false;
    internal::PushScope("BeginTreeNode");
    return true;
}

void EndTreeNode()
{
    if (!internal::PopScope("BeginTreeNode"))
        return;
    internal::ListTreePop();
}

bool TreeLeaf(const char* label, const TreeNodeOptions& options)
{
    if (ImGui::GetCurrentContext() == nullptr)
        return false;
    return TreeRow(label, options, true).row.activated;
}

} // namespace facet
