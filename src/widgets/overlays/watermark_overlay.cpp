// WatermarkOverlay (overlays.h). Planned: until Phase 2 draws the strip in a display corner, the
// placeholder of src/widgets/structure_stub.h shows the call in the page flow.

#include "facet/widgets/overlays.h"
#include "widgets/structure_stub.h"

namespace facet
{

void WatermarkOverlay(const char* id, std::span<const WatermarkField>, const WatermarkOptions&)
{
    // The finished overlay is visible whenever it is called.
    internal::StubLayer("WatermarkOverlay", id, true);
}

} // namespace facet
