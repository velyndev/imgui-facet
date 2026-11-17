// SliderStepped: planned. Draws the placeholder at the slider's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool SliderStepped(const char* label, float*, std::span<const float>, const SliderSteppedOptions& options)
{
    const ImVec2 size = internal::StubSliderSize(options.orientation, options.width, options.height, options.size,
                                                 options.error, options.helpText);
    internal::ControlStub("SliderStepped", label, size.x, size.y);
    return false;
}

} // namespace facet
