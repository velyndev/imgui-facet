// BeginMenuBar and EndMenuBar (navigation.h). Planned: shows the menu titles one under the
// other inside the frame placeholder of src/widgets/structure_stub.h until Phase 2 implements
// the menu bar.

#include "facet/widgets/navigation.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginMenuBar(const char* id, const MenuBarOptions&)
{
    if (!internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginMenuBar", "MenuBar", id, false);
    return true;
}

void EndMenuBar()
{
    internal::EndStubFrame("BeginMenuBar");
}

} // namespace facet
