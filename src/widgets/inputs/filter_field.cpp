// FilterField and PassesFilter: planned. FilterField draws the placeholder at the field's
// default size until Phase 2 implements both in this file.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool FilterField(const char* label, TextFilter*, const FilterFieldOptions& options)
{
    internal::ControlStub("FilterField", label, internal::StubWidth(options.width, 0.0f), ControlHeight(options.size));
    return false;
}

// Planned: until Phase 2 every text passes, as with an empty query.
bool PassesFilter(const TextFilter&, std::string_view)
{
    return true;
}

} // namespace facet
