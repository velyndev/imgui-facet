// BeginOverlay, EndOverlay and the input region queries (containers.h). Planned: until Phase 2
// implements the controller, the menu content shows inside the frame placeholder of
// src/widgets/structure_stub.h while *visible is true and is skipped at once while it is false;
// nothing fades, the toggle key does nothing and nothing reports input. *visible is never
// written.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginOverlay(const char* id, bool* visible, const OverlayOptions&)
{
    if (visible == nullptr || !*visible || !internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginOverlay", "Overlay", id, false);
    return true;
}

void EndOverlay()
{
    internal::EndStubFrame("BeginOverlay");
}

bool OverlayWantsMouse()
{
    return false;
}

bool OverlayWantsKeyboard()
{
    return false;
}

bool OverlayHitTest(ImVec2)
{
    return false;
}

int GetOverlayInputRegions(std::span<ScreenRegion>)
{
    return 0;
}

} // namespace facet
