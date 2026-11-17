// BeginDataTable, DataTableRow, EndDataTable and the cell edit pair (facet/widgets/display.h):
// planned. A placeholder until Phase 2 implements them. While the table is Ready with rows the
// placeholder is a header line followed by a plain Dear ImGui table with the given columns, so
// the caller's rows and cells (DataTableRow, ImGui::TableNextColumn) land in a real table;
// every other state draws its placeholder alone and BeginDataTable returns false. The cell edit
// pair never opens: cells always draw their display content.

#include <algorithm>
#include <cstring>

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{
namespace
{

constexpr const char* kScope = "BeginDataTable";

// True while the innermost Facet scope is a table this stub opened.
bool InsideTable()
{
    const char* scope = internal::InnermostScope();
    return scope != nullptr && std::strcmp(scope, kScope) == 0 && ImGui::GetCurrentTable() != nullptr;
}

} // namespace

bool BeginDataTable(const char* id, std::span<const DataTableColumn> columns, int rowCount, ImGuiSelectionBasicStorage*,
                    const DataTableOptions& options)
{
    namespace stub = internal::content;
    if (!stub::CanSubmit())
        return false;
    const float width = stub::ResolveWidth(options.width, 0.0f);
    const int columnCount = static_cast<int>(std::min<std::size_t>(columns.size(), IMGUI_TABLE_MAX_COLUMNS));
    if (options.state != DataState::Ready || rowCount <= 0 || columnCount <= 0)
    {
        stub::Stub("DataTable", id, width, stub::ResolveHeight(options.height, 0.0f));
        return false;
    }

    stub::StubBegin(kScope, "DataTable", id, width, stub::ControlRows(1.0f));
    // The table id is derived from the id, so it never equals the placeholder item's id.
    const ImGuiID tableId = ImHashStr("##rows", 0, ImGui::GetID(id != nullptr ? id : ""));
    if (!ImGui::BeginTableEx("##rows", tableId, columnCount, ImGuiTableFlags_None, ImVec2(width, 0.0f)))
    {
        stub::StubEnd(kScope);
        return false;
    }
    for (int i = 0; i < columnCount; ++i)
    {
        const DataTableColumn& column = columns[static_cast<std::size_t>(i)];
        const ImGuiTableColumnFlags sizing =
            column.width > 0.0f ? ImGuiTableColumnFlags_WidthFixed : ImGuiTableColumnFlags_WidthStretch;
        ImGui::TableSetupColumn(column.label, column.flags | sizing, column.width > 0.0f ? Px(column.width) : 0.0f);
    }
    return true;
}

bool DataTableRow(int)
{
    if (InsideTable())
        ImGui::TableNextRow();
    return false;
}

void EndDataTable()
{
    // EndTable only when the innermost scope is this table: an End without its Begin is
    // reported by PopScope and touches no Dear ImGui table.
    if (InsideTable())
        ImGui::EndTable();
    internal::content::StubEnd(kScope);
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
