// BeginPanels, BeginPanel and their Ends (layout.h). Planned: shows the panels one under the
// other inside frame placeholders of src/widgets/structure_stub.h until Phase 2 implements
// them. Every panel is expanded; `sizes` is never written.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginPanels(const char* id, std::span<float>, const PanelsOptions&)
{
    internal::BeginStubFrame("BeginPanels", "Panels", id, false);
}

void EndPanels()
{
    internal::EndStubFrame("BeginPanels");
}

bool BeginPanel(const char* label, const PanelOptions&)
{
    if (!internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginPanel", "Panel", label, false);
    return true;
}

void EndPanel()
{
    internal::EndStubFrame("BeginPanel");
}

} // namespace facet
