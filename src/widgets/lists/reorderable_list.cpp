// ReorderableList (lists.h). Planned: draws the placeholder of src/widgets/structure_stub.h
// until Phase 2 implements the widget. Returns false and never writes *move.

#include "facet/widgets/lists.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool ReorderableList(const char* id, std::span<const ChoiceItem> items, ReorderMove*,
                     const ReorderableListOptions& options)
{
    const int rows = items.empty() ? 1 : static_cast<int>(items.size());
    internal::StubItem("ReorderableList", id, internal::StubWidth(options.width, true),
                       static_cast<float>(rows) * ControlHeight(options.size));
    return false;
}

} // namespace facet
