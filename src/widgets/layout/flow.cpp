// BeginFlow, FlowNext and EndFlow (layout.h). Planned: shows every item one under the other
// inside the frame placeholder of src/widgets/structure_stub.h until Phase 2 implements the flow
// layout; FlowNext lets every item through.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginFlow(const char* id, const FlowOptions&)
{
    internal::BeginStubFrame("BeginFlow", "Flow", id, false);
}

bool FlowNext()
{
    return internal::StubCanDraw();
}

void EndFlow()
{
    internal::EndStubFrame("BeginFlow");
}

} // namespace facet
