// BeginCard and EndCard (containers.h). Planned: shows the content inside the frame placeholder
// of src/widgets/structure_stub.h until Phase 2 implements the card.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginCard(const char* label, const CardOptions&)
{
    internal::BeginStubFrame("BeginCard", "Card", label, false);
}

void EndCard()
{
    internal::EndStubFrame("BeginCard");
}

} // namespace facet
