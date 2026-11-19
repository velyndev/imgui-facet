// BeginItemGrid, ItemGridTile and EndItemGrid (layout.h). Planned: draws the placeholders of
// src/widgets/structure_stub.h until Phase 2 implements the grid. NextVisibleRows (common.h)
// hands out tiles only inside the list core's row ranges, which this placeholder does not open,
// so a tile loop inside the frame submits nothing. While the grid is
// loading, failed or empty, BeginItemGrid draws one placeholder line where the state will be and
// returns false, as declared.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginItemGrid(const char* id, int itemCount, ImGuiSelectionBasicStorage*, const ItemGridOptions& options)
{
    if (!internal::StubCanDraw())
        return false;
    if (options.state != DataState::Ready || itemCount <= 0)
    {
        internal::StubItem("ItemGrid", id, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Regular));
        return false;
    }
    internal::BeginStubFrame("BeginItemGrid", "ItemGrid", id, false);
    return true;
}

bool ItemGridTile(int)
{
    internal::StubItem("ItemGridTile", nullptr,
                       internal::StubWidth(0.0f, false, internal::StubContentWidth("ItemGridTile", nullptr)),
                       ControlHeight(ControlSize::Large));
    return false;
}

void EndItemGrid()
{
    internal::EndStubFrame("BeginItemGrid");
}

} // namespace facet
