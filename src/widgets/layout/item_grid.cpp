// BeginItemGrid, ItemGridTile and EndItemGrid (layout.h). Planned: draws the placeholders of
// src/widgets/structure_stub.h until Phase 2 implements the grid. NextVisibleRows (common.h)
// hands out no tiles yet, so a tile loop inside the frame submits nothing.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginItemGrid(const char* id, int, ImGuiSelectionBasicStorage*, const ItemGridOptions&)
{
    if (!internal::StubCanDraw())
        return false;
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
