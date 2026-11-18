// FilterField and PassesFilter: planned. FilterField draws the placeholder at the field's
// default size until Phase 2 implements both in this file.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool FilterField(const char* label, std::string*, FilterSettings*, const FilterFieldOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("FilterField", label, stub::StubWidth(options.width, 0.0f), ControlHeight(options.size));
    return false;
}

bool FilterField(const char* label, char*, std::size_t, FilterSettings*, const FilterFieldOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("FilterField", label, stub::StubWidth(options.width, 0.0f), ControlHeight(options.size));
    return false;
}

// Planned: until Phase 2 every text passes, as with an empty query.
bool PassesFilter(std::string_view, const FilterSettings&, std::string_view)
{
    return true;
}

} // namespace facet
