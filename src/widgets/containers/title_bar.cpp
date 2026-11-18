// BeginTitleBar, EndTitleBar and TitleBarButtonPressed (containers.h). Planned: shows the
// caller's items inside the frame placeholder of src/widgets/structure_stub.h until Phase 2
// implements the title bar; no window button is ever reported.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginTitleBar(const char* label, const TitleBarOptions&)
{
    internal::BeginStubFrame("BeginTitleBar", "TitleBar", label, false);
}

void EndTitleBar()
{
    internal::EndStubFrame("BeginTitleBar");
}

WindowButton TitleBarButtonPressed()
{
    return WindowButton::None;
}

} // namespace facet
