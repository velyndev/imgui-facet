// DonutChart (facet/widgets/charts.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/charts.h"
#include "widgets/content_stub.h"

namespace facet
{

void DonutChart(const char* id, std::span<const DonutSlice>, const DonutChartOptions& options)
{
    namespace stub = internal::content;
    const float diameter = options.diameter > 0.0f ? Px(options.diameter) : stub::Spans(stub::kChartHeightSpans);
    stub::Stub("DonutChart", id, diameter, diameter);
}

} // namespace facet