// BeginStatusBar, StatusItem and EndStatusBar (containers.h). Planned: draws the placeholders
// of src/widgets/structure_stub.h until Phase 2 implements the status bar.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginStatusBar(const char* id, const StatusBarOptions&)
{
    internal::BeginStubFrame("BeginStatusBar", "StatusBar", id, false);
}

void EndStatusBar()
{
    internal::EndStubFrame("BeginStatusBar");
}

void StatusItem(const char* text, const StatusItemOptions&)
{
    internal::StubItem("StatusItem", text,
                       internal::StubWidth(0.0f, false, internal::StubContentWidth("StatusItem", text)),
                       ControlHeight(ControlSize::Small));
}

} // namespace facet
