// BeginGroupBox and EndGroupBox (containers.h). Planned: shows the content inside the frame
// placeholder of src/widgets/structure_stub.h until Phase 2 implements the group box; it never
// collapses.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginGroupBox(const char* label, const GroupBoxOptions&)
{
    if (!internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginGroupBox", "GroupBox", label, false);
    return true;
}

void EndGroupBox()
{
    internal::EndStubFrame("BeginGroupBox");
}

} // namespace facet
