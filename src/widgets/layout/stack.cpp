// BeginStack, StackSpacer and EndStack (layout.h). Planned: shows the items one under the other
// inside the frame placeholder of src/widgets/structure_stub.h until Phase 2 implements the
// stack; spacers take no room.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginStack(const char* id, const StackOptions&)
{
    internal::BeginStubFrame("BeginStack", "Stack", id, false);
}

void StackSpacer()
{
}

void EndStack()
{
    internal::EndStubFrame("BeginStack");
}

} // namespace facet
