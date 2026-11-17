// BeginAppFrame, EndAppFrame, the hit test, size limits and placement storage (containers.h).
// Planned: until Phase 2 implements the app frame, BeginAppFrame shows its content inside the
// frame placeholder of src/widgets/structure_stub.h at the cursor instead of filling the
// viewport, the hit test reports Client everywhere, and nothing is stored.

#include "facet/widgets/containers.h"
#include "widgets/structure_stub.h"

namespace facet
{

bool BeginAppFrame(const char* id, const AppFrameOptions&)
{
    if (!internal::StubCanDraw())
        return false;
    internal::BeginStubFrame("BeginAppFrame", "AppFrame", id, false);
    return true;
}

void EndAppFrame()
{
    internal::EndStubFrame("BeginAppFrame");
}

FrameHit AppFrameHitTest(ImVec2)
{
    return FrameHit::Client;
}

ImVec2 AppFrameMinSize()
{
    return ImVec2(0.0f, 0.0f);
}

ImVec2 AppFrameMaxSize()
{
    return ImVec2(0.0f, 0.0f);
}

void SaveAppFramePlacement(const char*, const FramePlacement&)
{
}

bool LoadAppFramePlacement(const char*, FramePlacement*)
{
    return false;
}

} // namespace facet
