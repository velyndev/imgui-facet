// BeginDependentGroup and EndDependentGroup (layout.h). Planned: shows the content inside the
// frame placeholder of src/widgets/structure_stub.h until Phase 2 implements the group; it
// neither disables nor hides the content yet.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginDependentGroup(const char* id, bool, const DependentGroupOptions&)
{
    if (!internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginDependentGroup", "DependentGroup", id, false);
    return true;
}

void EndDependentGroup()
{
    internal::EndStubFrame("BeginDependentGroup");
}

} // namespace facet
