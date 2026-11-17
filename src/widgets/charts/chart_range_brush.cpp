// ChartRangeBrush (facet/widgets/charts.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/charts.h"
#include "widgets/content_stub.h"

namespace facet
{

bool ChartRangeBrush(const char* label, double*, double*, double, double, std::span<const ChartSeries>,
                     const ChartRangeBrushOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("ChartRangeBrush", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::ControlRows(stub::kTallControlRows)));
    return false;
}

} // namespace facet