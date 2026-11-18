// Slider: planned. Draws the placeholder at the slider's default size until Phase 2 implements
// the widget in this file.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const SliderOptions& options)
{
    namespace stub = internal::controls;
    const ImVec2 size = stub::StubSliderSize(options.orientation, options.width, options.height, options.size,
                                                 options.error, options.helpText);
    stub::ControlStub("Slider", label, size.x, size.y);
}

} // namespace

bool Slider(const char* label, float*, float, float, const SliderOptions& options)
{
    Stub(label, options);
    return false;
}

bool Slider(const char* label, int*, int, int, const SliderOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
