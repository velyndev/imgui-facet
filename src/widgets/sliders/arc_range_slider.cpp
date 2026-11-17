// ArcRangeSlider: planned. Draws the placeholder at the slider's default size until Phase 2
// implements the widget in this file on the multi-handle block.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool ArcRangeSlider(const char* label, float*, float*, float, float, const ArcRangeSliderOptions& options)
{
    const float diameter = internal::StubDial(options.diameter, options.size);
    const float height = diameter + internal::StubLabelLine(label) + internal::StubMessageHeight(options.error, options.helpText);
    internal::ControlStub("ArcRangeSlider", label, diameter, height);
    return false;
}

} // namespace facet
