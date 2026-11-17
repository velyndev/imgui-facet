// BeginGrid, GridNext and EndGrid (layout.h). Planned: shows the cells one under the other
// inside the frame placeholder of src/widgets/structure_stub.h until Phase 2 implements the grid.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginGrid(const char* id, const GridOptions&)
{
    internal::BeginStubFrame("BeginGrid", "Grid", id, false);
}

void GridNext(const GridCellOptions&)
{
}

void EndGrid()
{
    internal::EndStubFrame("BeginGrid");
}

} // namespace facet
