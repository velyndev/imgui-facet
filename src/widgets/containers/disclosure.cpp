// BeginDisclosure and EndDisclosure (containers.h). Planned: draws the placeholder of
// src/widgets/structure_stub.h until Phase 2 implements the disclosure. It stays closed: the
// stub returns false and never writes *open.

#include "facet/widgets/containers.h"
#include "internal/scope_stack.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginDisclosure(const char* label, bool*, const DisclosureOptions&)
{
    internal::StubItem("Disclosure", label, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Inherit));
    return false;
}

void EndDisclosure()
{
    // BeginDisclosure never returns true yet, so any call here is unbalanced; PopScope reports it.
    internal::PopScope("BeginDisclosure");
}

} // namespace facet
