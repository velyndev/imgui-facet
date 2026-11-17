// BeginDataStateView and EndDataStateView (facet/widgets/display.h): planned. A placeholder
// until Phase 2 implements them. In the Ready state with content the placeholder is a header
// line and the caller's content follows it; every other state draws its placeholder alone.

#include <algorithm>

#include "facet/widgets/display.h"
#include "widgets/content_stub.h"

namespace facet
{

bool BeginDataStateView(const char* id, DataState state, int count, const DataStateViewOptions& options)
{
    namespace stub = internal::content;
    if (!stub::CanSubmit())
        return false;
    const float width = stub::ResolveWidth(0.0f, 0.0f);
    if (state == DataState::Ready && count > 0)
    {
        stub::StubBegin("BeginDataStateView", "DataStateView", id, width, stub::ControlRows(1.0f));
        return true;
    }
    const float rows = state == DataState::Loading ? static_cast<float>(std::max(1, options.skeletonRows))
                                                   : stub::kZoneControlRows;
    stub::Stub("DataStateView", id, width, stub::ResolveHeight(options.height, stub::ControlRows(rows)));
    return false;
}

void EndDataStateView()
{
    internal::content::StubEnd("BeginDataStateView");
}

bool DataStateViewRetryPressed()
{
    return false;
}

} // namespace facet