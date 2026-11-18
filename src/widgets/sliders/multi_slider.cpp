// MultiSlider: planned. Draws the placeholder at the slider's default size until Phase 2
// implements the widget in this file on the multi-handle block.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const MultiSliderOptions& options)
{
    namespace stub = internal::controls;
    const ImVec2 size = stub::StubSliderSize(options.orientation, options.width, options.height, options.size,
                                                 options.error, options.helpText);
    stub::ControlStub("MultiSlider", label, size.x, size.y);
}

} // namespace

bool MultiSlider(const char* label, std::span<float>, float, float, const MultiSliderOptions& options)
{
    Stub(label, options);
    return false;
}

bool MultiSlider(const char* label, std::span<int>, int, int, const MultiSliderOptions& options)
{
    Stub(label, options);
    return false;
}

bool MultiSlider(const char* label, std::vector<float>*, float, float, const MultiSliderOptions& options)
{
    Stub(label, options);
    return false;
}

int MultiSliderAddedHandle()
{
    return -1;
}

int MultiSliderRemovedHandle()
{
    return -1;
}

} // namespace facet
