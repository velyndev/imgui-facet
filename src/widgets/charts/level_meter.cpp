// LevelMeter (facet/widgets/charts.h): planned. A placeholder until Phase 2 implements it.

#include <algorithm>

#include "facet/widgets/charts.h"
#include "widgets/content_stub.h"

namespace facet
{

void LevelMeter(const char* label, std::span<const float> channels, float, float, const LevelMeterOptions& options)
{
    namespace stub = internal::content;
    const float rows = static_cast<float>(std::max<std::size_t>(1, channels.size()));
    if (options.orientation == Orientation::Vertical)
    {
        const float width = options.width > 0.0f ? Px(options.width) : stub::ControlRows(rows);
        stub::Stub("LevelMeter", label, width, stub::ResolveHeight(options.height, stub::Spans(stub::kChartHeightSpans)));
        return;
    }
    stub::Stub("LevelMeter", label, stub::ResolveWidth(options.width, 0.0f),
               stub::ResolveHeight(options.height, stub::ControlRows(1.0f)));
}

} // namespace facet