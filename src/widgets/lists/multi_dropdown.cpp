// MultiDropdown (lists.h). Planned: draws the placeholder of src/widgets/structure_stub.h
// until Phase 2 implements the widget. Returns false and never writes `checked`.

#include "facet/widgets/lists.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawMultiDropdown(const char* label, const MultiDropdownOptions& options)
{
    internal::StubItem("MultiDropdown", label, internal::StubWidth(options.width, true), ControlHeight(options.size));
}

} // namespace

bool MultiDropdown(const char* label, std::span<bool>, std::span<const ChoiceItem>, const MultiDropdownOptions& options)
{
    DrawMultiDropdown(label, options);
    return false;
}

bool MultiDropdown(const char* label, std::span<bool>, std::span<const char* const>, const MultiDropdownOptions& options)
{
    DrawMultiDropdown(label, options);
    return false;
}

} // namespace facet
