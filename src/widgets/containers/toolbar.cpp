// BeginToolbar, ToolbarButton, ToolbarSeparator, ToolbarSpacer and EndToolbar (containers.h).
// Planned: draws the placeholders of src/widgets/structure_stub.h until Phase 2 implements the
// toolbar. Buttons are stacked inside the frame and never activate; separators and spacers
// draw nothing.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginToolbar(const char* id, const ToolbarOptions&)
{
    internal::BeginStubFrame("BeginToolbar", "Toolbar", id, false);
}

void EndToolbar()
{
    internal::EndStubFrame("BeginToolbar");
}

bool ToolbarButton(const char* label, Icon, const ToolbarButtonOptions&)
{
    internal::StubItem("ToolbarButton", label,
                       internal::StubWidth(0.0f, false, internal::StubContentWidth("ToolbarButton", label)),
                       ControlHeight(ControlSize::Inherit));
    return false;
}

void ToolbarSeparator()
{
}

void ToolbarSpacer()
{
}

} // namespace facet
