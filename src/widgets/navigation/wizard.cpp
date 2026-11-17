// BeginWizard, WizardFooter and EndWizard (navigation.h). Planned: shows the step content
// inside the frame placeholder of src/widgets/structure_stub.h until Phase 2 implements the
// wizard. The footer is a placeholder that returns None; *step is never written.

#include "facet/widgets/navigation.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginWizard(const char* id, int*, std::span<const ChoiceItem>, const WizardOptions&)
{
    internal::BeginStubFrame("BeginWizard", "Wizard", id, false);
}

void EndWizard()
{
    internal::EndStubFrame("BeginWizard");
}

WizardAction WizardFooter(const WizardFooterOptions&)
{
    internal::StubItem("WizardFooter", nullptr, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Inherit));
    return WizardAction::None;
}

} // namespace facet
