// BeginGroupBox and EndGroupBox (containers.h). Planned: shows the content inside the frame
// placeholder of src/widgets/structure_stub.h until Phase 2 implements the group box. It keeps
// no open state yet: a collapsible group box shows its defaultOpen state, so one that starts
// closed draws its title line and returns false, as declared.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginGroupBox(const char* label, const GroupBoxOptions& options)
{
    if (!internal::StubCanDraw())
        return false;
    if (options.collapsible && !options.defaultOpen)
    {
        internal::StubItem("GroupBox", label, internal::StubWidth(options.width, true), ControlHeight(ControlSize::Regular));
        return false;
    }
    internal::BeginStubFrame("BeginGroupBox", "GroupBox", label, false);
    return true;
}

void EndGroupBox()
{
    internal::EndStubFrame("BeginGroupBox");
}

} // namespace facet
