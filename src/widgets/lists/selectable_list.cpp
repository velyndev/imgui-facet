// SelectableList (lists.h). Planned: draws the placeholder of src/widgets/structure_stub.h
// until Phase 2 implements the widget. Returns false and never writes the selection.

#include "facet/widgets/lists.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawSelectableList(const char* label, const SelectableListOptions& options)
{
    const int rows = options.visibleRows > 0 ? options.visibleRows : internal::StubListRows();
    internal::StubItem("SelectableList", label, internal::StubWidth(options.width, true),
                       static_cast<float>(rows) * ControlHeight(options.size));
}

} // namespace

bool SelectableList(const char* label, ImGuiSelectionBasicStorage*, std::span<const ChoiceItem>,
                    const SelectableListOptions& options)
{
    DrawSelectableList(label, options);
    return false;
}

bool SelectableList(const char* label, ImGuiSelectionBasicStorage*, std::span<const char* const>,
                    const SelectableListOptions& options)
{
    DrawSelectableList(label, options);
    return false;
}

bool SelectableList(const char* label, ImGuiSelectionBasicStorage*, int, ChoiceGetter, void*,
                    const SelectableListOptions& options)
{
    DrawSelectableList(label, options);
    return false;
}

int SelectableListActivatedRow()
{
    return -1;
}

} // namespace facet
