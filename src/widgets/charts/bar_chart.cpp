// BarChart (facet/widgets/charts.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/charts.h"
#include "widgets/content_stub.h"

namespace facet
{

void BarChart(const char* id, std::span<const ChartSeries>, std::span<const char* const>, const BarChartOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("BarChart", id, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::Spans(stub::kChartHeightSpans)));
}

} // namespace facet