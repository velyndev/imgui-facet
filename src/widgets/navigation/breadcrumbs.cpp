// Breadcrumbs (navigation.h). Planned: draws the placeholder of src/widgets/structure_stub.h
// until Phase 2 implements the widget. No segment is ever activated: returns -1.

#include "facet/widgets/navigation.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawBreadcrumbs(const char* id, const BreadcrumbsOptions& options)
{
    internal::StubItem("Breadcrumbs", id, internal::StubWidth(options.width, true), ControlHeight(options.size));
}

} // namespace

int Breadcrumbs(const char* id, std::span<const ChoiceItem>, const BreadcrumbsOptions& options)
{
    DrawBreadcrumbs(id, options);
    return -1;
}

int Breadcrumbs(const char* id, std::span<const char* const>, const BreadcrumbsOptions& options)
{
    DrawBreadcrumbs(id, options);
    return -1;
}

} // namespace facet
