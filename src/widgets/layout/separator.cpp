// Separator and SeparatorText (layout.h). Planned: draws the placeholder of
// src/widgets/structure_stub.h at the small control height until Phase 2 implements them.

#include "facet/widgets/layout.h"
#include "widgets/structure_stub.h"

namespace facet
{

void Separator(const SeparatorOptions&)
{
    internal::StubItem("Separator", nullptr, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Small));
}

void SeparatorText(const char* text, const SeparatorTextOptions&)
{
    internal::StubItem("SeparatorText", text, internal::StubWidth(0.0f, true), ControlHeight(ControlSize::Small));
}

} // namespace facet
