// Sparkline (facet/widgets/charts.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/charts.h"
#include "widgets/content_stub.h"

namespace facet
{

void Sparkline(const char* label, std::span<const float>, const SparklineOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("Sparkline", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::ControlRows(1.0f)));
}

void Sparkline(const char* label, int, ChartGetter, void*, const SparklineOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("Sparkline", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::ControlRows(1.0f)));
}

} // namespace facet