// RangeSliderHistogram: planned. Draws the placeholder at the widget's default size until
// Phase 2 implements it in this file on the multi-handle block and the chart core.

#include "facet/widgets/sliders.h"

#include "facet/core/scale.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

// Default bar area height, in control heights, until the theme has a token for it.
constexpr float kHistogramInControls = 2.0f;

void Stub(const char* label, const RangeSliderHistogramOptions& options)
{
    namespace stub = internal::controls;
    const float control = ControlHeight(options.size);
    const float bars = options.histogramHeight > 0.0f ? Px(options.histogramHeight) : control * kHistogramInControls;
    stub::ControlStub("RangeSliderHistogram", label, stub::StubWidth(options.width, 0.0f),
                          bars + stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool RangeSliderHistogram(const char* label, float*, float*, float, float, std::span<const float>,
                          const RangeSliderHistogramOptions& options)
{
    Stub(label, options);
    return false;
}

bool RangeSliderHistogram(const char* label, int*, int*, int, int, std::span<const float>,
                          const RangeSliderHistogramOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
