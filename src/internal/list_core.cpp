// List and selection core (list_core.h): row ranges, the selection storage of lists, grids and
// tables, typeahead and list rows.
// The functions below are linking stubs of the frozen interface until the list core is
// implemented in this file: no range hands out rows (as facet::NextVisibleRows did before), and
// no selection changes.

#include "internal/list_core.h"

namespace facet::internal
{

void BeginListRows(const ListRowsDesc&)
{
}

bool NextListRows(RowRange* rows)
{
    if (rows != nullptr)
        *rows = {};
    return false;
}

void EndListRows()
{
}

void BeginListSelection(ListSelection*, int, const ListSelectionOptions&)
{
}

bool EndListSelection()
{
    return false;
}

bool IsListEntrySelected(ListSelection*, int)
{
    return false;
}

} // namespace facet::internal
