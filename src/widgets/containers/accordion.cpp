// BeginAccordion, BeginAccordionSection and their Ends (containers.h). Planned: draws the
// placeholders of src/widgets/structure_stub.h until Phase 2 implements the accordion. Every
// section is closed, so BeginAccordionSection returns false.

#include "facet/widgets/containers.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginAccordion(const char* id, const AccordionOptions&)
{
    internal::BeginStubFrame("BeginAccordion", "Accordion", id, false);
}

void EndAccordion()
{
    internal::EndStubFrame("BeginAccordion");
}

bool BeginAccordionSection(const char* label, const AccordionSectionOptions&)
{
    internal::StubItem("AccordionSection", label, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Inherit));
    return false;
}

void EndAccordionSection()
{
    // BeginAccordionSection never returns true yet, so any call here is unbalanced; PopScope
    // reports it.
    internal::PopScope("BeginAccordionSection");
}

} // namespace facet
