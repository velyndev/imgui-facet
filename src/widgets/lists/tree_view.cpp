// BeginTreeView, BeginTreeNode, TreeLeaf and their Ends (lists.h). Planned: draws the
// placeholders of src/widgets/structure_stub.h until Phase 2 implements the widget. The view
// shows its nodes inside a frame; every node is closed, so BeginTreeNode returns false.

#include "facet/widgets/lists.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginTreeView(const char* id, ImGuiSelectionBasicStorage*, const TreeViewOptions&)
{
    if (!internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginTreeView", "TreeView", id, false);
    return true;
}

void EndTreeView()
{
    internal::EndStubFrame("BeginTreeView");
}

bool TreeViewSelectionChanged()
{
    return false;
}

bool BeginTreeNode(const char* label, const TreeNodeOptions&)
{
    internal::StubItem("TreeNode", label, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Inherit));
    return false;
}

void EndTreeNode()
{
    // BeginTreeNode never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginTreeNode");
}

bool TreeLeaf(const char* label, const TreeNodeOptions&)
{
    internal::StubItem("TreeLeaf", label, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Inherit));
    return false;
}

} // namespace facet
