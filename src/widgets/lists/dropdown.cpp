// Dropdown (lists.h). Planned: draws the placeholder of src/widgets/structure_stub.h until
// Phase 2 implements the widget. Returns false and never writes *index.

#include "facet/widgets/lists.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawDropdown(const char* label, const DropdownOptions& options)
{
    internal::StubItem("Dropdown", label, internal::StubWidth(options.width, true), ControlHeight(options.size));
}

} // namespace

bool Dropdown(const char* label, int*, std::span<const ChoiceItem>, const DropdownOptions& options)
{
    DrawDropdown(label, options);
    return false;
}

bool Dropdown(const char* label, int*, std::span<const char* const>, const DropdownOptions& options)
{
    DrawDropdown(label, options);
    return false;
}

bool Dropdown(const char* label, int*, int, ChoiceGetter, void*, const DropdownOptions& options)
{
    DrawDropdown(label, options);
    return false;
}

} // namespace facet
