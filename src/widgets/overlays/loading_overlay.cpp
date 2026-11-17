// BeginLoadingOverlay and EndLoadingOverlay (overlays.h). Planned: shows the content inside the
// frame placeholder of src/widgets/structure_stub.h until Phase 2 implements the overlay; the
// content is neither dimmed nor blocked yet.

#include "facet/widgets/overlays.h"
#include "widgets/structure_stub.h"

namespace facet
{

void BeginLoadingOverlay(const char* id, const LoadingOverlayOptions&)
{
    internal::BeginStubFrame("BeginLoadingOverlay", "LoadingOverlay", id, false);
}

void EndLoadingOverlay()
{
    internal::EndStubFrame("BeginLoadingOverlay");
}

} // namespace facet
