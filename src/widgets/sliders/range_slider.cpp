// RangeSlider: planned. Draws the placeholder at the slider's default size until Phase 2
// implements the widget in this file on the multi-handle block.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const RangeSliderOptions& options)
{
    const ImVec2 size = internal::StubSliderSize(options.orientation, options.width, options.height, options.size,
                                                 options.error, options.helpText);
    internal::ControlStub("RangeSlider", label, size.x, size.y);
}

} // namespace

bool RangeSlider(const char* label, float*, float*, float, float, const RangeSliderOptions& options)
{
    Stub(label, options);
    return false;
}

bool RangeSlider(const char* label, int*, int*, int, int, const RangeSliderOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
