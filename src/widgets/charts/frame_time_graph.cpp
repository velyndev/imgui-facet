// FrameTimeGraph (facet/widgets/charts.h): planned. A placeholder until Phase 2 implements it.
// The frame time samples will be taken in the content subsystem (src/widgets/content_frame.cpp).

#include "facet/widgets/charts.h"
#include "widgets/content_stub.h"

namespace facet
{
namespace
{

void FrameTimeGraphStub(const char* id, const FrameTimeGraphOptions& options)
{
    namespace stub = internal::content;
    const float natural = options.compact ? stub::ControlRows(stub::kTallControlRows) : stub::Spans(stub::kChartHeightSpans);
    stub::Stub("FrameTimeGraph", id, stub::ResolveWidth(options.width, 0.0f), stub::ResolveHeight(options.height, natural));
}

} // namespace

void FrameTimeGraph(const char* id, const FrameTimeGraphOptions& options)
{
    FrameTimeGraphStub(id, options);
}

void FrameTimeGraph(const char* id, std::span<const float>, const FrameTimeGraphOptions& options)
{
    FrameTimeGraphStub(id, options);
}

} // namespace facet