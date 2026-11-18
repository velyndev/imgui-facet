// ReorderableList (lists.h). Planned: draws the placeholder of src/widgets/structure_stub.h
// until Phase 2 implements the widget, one row per item, or the one row of the loading, error or
// empty state. Returns false and never writes *move.

#include "facet/widgets/lists.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawReorderableList(const char* id, std::size_t count, const ReorderableListOptions& options)
{
    const bool rowsShown = options.state == DataState::Ready && count > 0;
    const int rows = rowsShown ? static_cast<int>(count) : 1;
    internal::StubItem("ReorderableList", id, internal::StubWidth(options.width, true),
                       static_cast<float>(rows) * ControlHeight(options.size));
}

} // namespace

bool ReorderableList(const char* id, ReorderMove*, std::span<const ChoiceItem> items,
                     const ReorderableListOptions& options)
{
    DrawReorderableList(id, items.size(), options);
    return false;
}

bool ReorderableList(const char* id, ReorderMove*, std::span<const char* const> items,
                     const ReorderableListOptions& options)
{
    DrawReorderableList(id, items.size(), options);
    return false;
}

} // namespace facet
