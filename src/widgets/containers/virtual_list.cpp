// BeginVirtualList, VirtualListRow, EndVirtualList and SetNextVirtualListScrollTo
// (containers.h). Planned: draws the placeholders of src/widgets/structure_stub.h until Phase 2
// implements the list. NextVisibleRows (common.h) hands out rows only inside the list core's row
// ranges, which this placeholder does not open, so a row loop inside the frame submits nothing. While the list is loading, failed or empty, BeginVirtualList draws
// one placeholder line where the state will be and returns false, as declared.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginVirtualList(const char* id, int rowCount, ImGuiSelectionBasicStorage*, const VirtualListOptions& options)
{
    if (!internal::StubCanDraw())
        return false;
    if (options.state != DataState::Ready || rowCount <= 0)
    {
        internal::StubItem("VirtualList", id, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Regular));
        return false;
    }
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
