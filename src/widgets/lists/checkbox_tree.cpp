// CheckboxTree (lists.h): a tree with a tri-state checkbox per node.
//
// The tree support of the list core (list_view.h, BeginListTree, without a selection) does the rows,
// the open states, the keys and typeahead; this file walks the caller's depth-first items, draws a
// checkbox in each row (list_parts.h) and applies the cascade: a toggled node sets its descendants,
// and each ancestor becomes On, Off or Mixed from its children.

#include "facet/widgets/lists.h"

#include <algorithm>
#include <cstdio>

#include <imgui.h>
#include <imgui_internal.h>

#include "facet/core/scale.h"
#include "internal/control.h"
#include "internal/item.h"
#include "internal/list_view.h"
#include "widgets/lists/list_parts.h"

namespace facet
{
namespace
{

// The deepest nesting walked; deeper items are shown under their last shown ancestor's level.
constexpr int kCheckTreeDepth = internal::kListTreeDepth;
// Room for a node's label with its id suffix ("###facet.<key>").
constexpr int kCheckTreeLabelSize = 256;

CheckState StateAt(std::span<const CheckState> states, int index)
{
    return index >= 0 && index < static_cast<int>(states.size()) ? states[static_cast<std::size_t>(index)]
                                                                  : CheckState::Off;
}

void SetState(std::span<CheckState> states, int index, CheckState value)
{
    if (index >= 0 && index < static_cast<int>(states.size()))
        states[static_cast<std::size_t>(index)] = value;
}

int DepthOf(std::span<const CheckTreeItem> items, int index)
{
    return std::max(0, items[static_cast<std::size_t>(index)].depth);
}

// The state of node `parent` from its direct children: On or Off when they all are, else Mixed.
CheckState FromChildren(std::span<const CheckTreeItem> items, std::span<const CheckState> states, int parent)
{
    const int depth = DepthOf(items, parent);
    int on = 0;
    int off = 0;
    for (int j = parent + 1; j < static_cast<int>(items.size()) && DepthOf(items, j) > depth; ++j)
    {
        if (DepthOf(items, j) != depth + 1)
            continue;
        const CheckState child = StateAt(states, j);
        on += child == CheckState::On ? 1 : 0;
        off += child == CheckState::Off ? 1 : 0;
        if (child == CheckState::Mixed)
            return CheckState::Mixed;
    }
    if (on > 0 && off == 0)
        return CheckState::On;
    if (off > 0 && on == 0)
        return CheckState::Off;
    return on == 0 && off == 0 ? StateAt(states, parent) : CheckState::Mixed;
}

// Toggles node `index` (Off and Mixed become On, On becomes Off) and, with cascade, sets its
// descendants and updates its ancestors.
void ToggleNode(std::span<const CheckTreeItem> items, std::span<CheckState> states, int index, bool cascade)
{
    const CheckState value = StateAt(states, index) == CheckState::On ? CheckState::Off : CheckState::On;
    SetState(states, index, value);
    if (!cascade)
        return;
    const int depth = DepthOf(items, index);
    for (int j = index + 1; j < static_cast<int>(items.size()) && DepthOf(items, j) > depth; ++j)
        SetState(states, j, value);
    // Ancestors, nearest first: each is the closest earlier item with a smaller depth.
    int childDepth = depth;
    for (int j = index - 1; j >= 0 && childDepth > 0; --j)
    {
        if (DepthOf(items, j) >= childDepth)
            continue;
        SetState(states, j, FromChildren(items, states, j));
        childDepth = DepthOf(items, j);
    }
}

} // namespace

bool CheckboxTree(const char* id, std::span<CheckState> states, std::span<const CheckTreeItem> items,
                  const CheckboxTreeOptions& options)
{
    using namespace internal;
    ImGuiWindow* window = ControlWindow();
    if (window == nullptr)
        return false;
    if (id == nullptr)
        id = "";
    const NextItemData next = TakeNextItemData();
    const bool readOnly = options.readOnly || (next.itemFlags & ImGuiItemFlags_ReadOnly) != 0;
    ListTreeDesc desc;
    desc.size = options.size;
    desc.areaSize = ImVec2(ResolveItemWidth(options.width, next, kFill), options.height > 0.0f ? Px(options.height) : 0.0f);
    desc.fitHeight = options.height <= 0.0f;
    desc.readOnly = readOnly;
    desc.state = options.state;
    desc.emptyText = options.emptyText;
    desc.errorText = options.errorText;
    if (!BeginListTree(window->GetID(id), desc))
        return false;

    const ImGuiContext& g = *GImGui;
    const int count = static_cast<int>(items.size());
    int openDepths[kCheckTreeDepth];
    int openCount = 0;
    int skipDeeperThan = -1;  // the children of a closed node are not submitted
    int toggled = -1;
    char label[kCheckTreeLabelSize];
    for (int i = 0; i < count; ++i)
    {
        const CheckTreeItem& item = items[static_cast<std::size_t>(i)];
        const int depth = DepthOf(items, i);
        if (skipDeeperThan >= 0)
        {
            if (depth > skipDeeperThan)
                continue;
            skipDeeperThan = -1;
        }
        while (openCount > 0 && openDepths[openCount - 1] >= depth)
        {
            ListTreePop();
            --openCount;
        }
        const bool parent = i + 1 < count && DepthOf(items, i + 1) > depth;
        // The open state and the row id follow the item id (else the index), not the label, so equal
        // labels and labels that change keep their state.
        const ImGuiID key = item.id != 0 ? item.id : static_cast<ImGuiID>(i);
        std::snprintf(label, sizeof(label), "%s###facet.%u", item.label != nullptr ? item.label : "", key);
        ListTreeNodeOptions node;
        node.leaf = !parent;
        node.defaultOpen = options.defaultOpen;
        node.disabled = (g.CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
        const ListTreeNodeState state = ListTreeNode(label, node);

        ChoiceItem entry;
        entry.label = item.label;
        entry.icon = item.icon;
        ListsRowLook look;
        look.checkbox = true;
        look.check = StateAt(states, i);
        look.readOnly = readOnly;
        DrawListsRow(state.row, entry, look, options.size);

        // A click or Space toggles the check; Enter is the tree's (it opens and closes a node).
        const ItemState& row = state.row.item;
        const bool enter = row.keyboardPressed && (g.NavActivateFlags & ImGuiActivateFlags_PreferInput) != 0;
        if (row.pressed && !enter && !readOnly && !row.disabled && i < static_cast<int>(states.size()))
            toggled = i;

        if (state.open && openCount < kCheckTreeDepth)
            openDepths[openCount++] = depth;
        else if (parent && !state.open)
            skipDeeperThan = depth;
    }
    while (openCount > 0)
    {
        ListTreePop();
        --openCount;
    }
    EndListTree();
    if (toggled < 0)
        return false;
    ToggleNode(items, states, toggled, options.cascade);
    ListsMarkEdited();
    return true;
}

} // namespace facet
