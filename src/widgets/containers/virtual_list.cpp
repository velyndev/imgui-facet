// BeginVirtualList, VirtualListRow, EndVirtualList and SetNextVirtualListScrollTo
// (containers.h). Planned: draws the placeholders of src/widgets/structure_stub.h until Phase 2
// implements the list. NextVisibleRows (common.h) hands out no rows yet, so a row loop inside
// the frame submits nothing.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginVirtualList(const char* id, int, ImGuiSelectionBasicStorage*, const VirtualListOptions&)
{
    if (!internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginVirtualList", "VirtualList", id, false);
    return true;
}

bool VirtualListRow(int)
{
    internal::StubItem("VirtualListRow", nullptr, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Regular));
    return false;
}

void EndVirtualList()
{
    internal::EndStubFrame("BeginVirtualList");
}

void SetNextVirtualListScrollTo(int)
{
}

} // namespace facet
