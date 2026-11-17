// Legend (facet/widgets/charts.h): planned. A placeholder until Phase 2 implements it.

#include "facet/widgets/charts.h"
#include "widgets/content_stub.h"

namespace facet
{

bool Legend(const char* id, std::span<LegendItem>, const LegendOptions& options)
{
    namespace stub = internal::content;
    stub::Stub("Legend", id, stub::ResolveWidth(options.width, 0.0f), stub::ControlRows(1.0f));
    return false;
}

} // namespace facet