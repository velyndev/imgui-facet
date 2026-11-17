// Gauge (facet/widgets/charts.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/charts.h"
#include "widgets/content_stub.h"

namespace facet
{

void Gauge(const char* label, double, double, double, const GaugeOptions& options)
{
    namespace stub = internal::content;
    const float diameter = options.diameter > 0.0f ? Px(options.diameter) : stub::Spans(stub::kChartHeightSpans);
    stub::Stub("Gauge", label, diameter, diameter);
}

} // namespace facet