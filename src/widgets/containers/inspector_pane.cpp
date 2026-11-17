// BeginInspectorPane, InspectorPaneNext and EndInspectorPane (containers.h). Planned: shows the
// main content and, while the caller shows the pane, the pane content one after the other inside
// the frame placeholder of src/widgets/structure_stub.h until Phase 2 implements the pane.
// *visible is never written. The stub keeps the visible flag of the latest Begin only, so a
// pane nested in another pane's main content answers for itself until its End.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

bool g_paneShown = false;

} // namespace

void BeginInspectorPane(const char* label, bool* visible, const InspectorPaneOptions&)
{
    g_paneShown = visible == nullptr || *visible;
    internal::BeginStubFrame("BeginInspectorPane", "InspectorPane", label, false);
}

bool InspectorPaneNext()
{
    return g_paneShown && internal::StubCanDraw();
}

void EndInspectorPane()
{
    internal::EndStubFrame("BeginInspectorPane");
    g_paneShown = false;
}

} // namespace facet
