// BeginDataTable, DataTableRow, EndDataTable and the cell edit pair (facet/widgets/display.h):
// planned. A placeholder until Phase 2 implements them. The placeholder opens no Dear ImGui
// table, so BeginDataTable returns false and the caller submits no rows.

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

bool BeginDataTable(const char* id, std::span<const DataTableColumn>, int, ImGuiSelectionBasicStorage*,
                    const DataTableOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("DataTable", id, stub::ResolveWidth(options.width, 0.0f), stub::ResolveHeight(options.height, 0.0f));
    return false;
}

bool DataTableRow(int)
{
    return false;
}

void EndDataTable()
{
    internal::content::StubEnd("BeginDataTable");
}

bool BeginDataTableCellEdit()
{
    return false;
}

void EndDataTableCellEdit()
{
    internal::content::StubEnd("BeginDataTableCellEdit");
}

} // namespace facet