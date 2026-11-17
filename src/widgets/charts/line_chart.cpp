// LineChart (facet/widgets/charts.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/charts.h"
#include "widgets/content_stub.h"

namespace facet
{

void LineChart(const char* id, std::span<const ChartSeries>, const LineChartOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("LineChart", id, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::Spans(stub::kChartHeightSpans)));
}

} // namespace facet