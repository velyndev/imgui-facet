// BeginInspectorPane, InspectorPaneNext and EndInspectorPane (containers.h). Planned: shows the
// main content and the pane content one after the other inside the frame placeholder of
// src/widgets/structure_stub.h until Phase 2 implements the pane. *visible is never written.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginInspectorPane(const char* label, bool*, const InspectorPaneOptions&)
{
    internal::BeginStubFrame("BeginInspectorPane", "InspectorPane", label, false);
}

bool InspectorPaneNext()
{
    return internal::StubCanDraw();
}

void EndInspectorPane()
{
    internal::EndStubFrame("BeginInspectorPane");
}

} // namespace facet
