// BeginScrollArea and EndScrollArea (containers.h). Planned: shows the content inside the frame
// placeholder of src/widgets/structure_stub.h, without scrolling, until Phase 2 implements the
// scroll area. The content gets ids under the area's id, as it will in the child window.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginScrollArea(const char* id, const ScrollAreaOptions&)
{
    if (!internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginScrollArea", "ScrollArea", id, true);
    return true;
}

void EndScrollArea()
{
    internal::EndStubFrame("BeginScrollArea");
}

} // namespace facet
