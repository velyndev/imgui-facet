// BeginDependentGroup and EndDependentGroup (layout.h). Planned: shows the content inside the
// frame placeholder of src/widgets/structure_stub.h until Phase 2 implements the group. In Hide
// mode with the parent off it returns false at once, without the collapse animation; in Disable
// mode the content is shown but not disabled yet.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginDependentGroup(const char* id, bool enabled, const DependentGroupOptions& options)
{
    if ((options.mode == DependentMode::Hide && !enabled) || !internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginDependentGroup", "DependentGroup", id, false);
    return true;
}

void EndDependentGroup()
{
    internal::EndStubFrame("BeginDependentGroup");
}

} // namespace facet
