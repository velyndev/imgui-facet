// Pagination (navigation.h). Planned: draws the placeholder of src/widgets/structure_stub.h
// until Phase 2 implements the widget. Returns false and never writes *page.

#include "facet/widgets/navigation.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool Pagination(const char* id, int*, int, const PaginationOptions& options)
{
    internal::StubItem("Pagination", id, internal::StubWidth(0.0f, false, internal::StubContentWidth("Pagination", id)),
                       ControlHeight(options.size));
    return false;
}

} // namespace facet
