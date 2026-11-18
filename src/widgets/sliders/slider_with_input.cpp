// SliderWithInput: planned. Draws the placeholder at the widget's default size until Phase 2
// implements it in this file.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const SliderWithInputOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("SliderWithInput", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool SliderWithInput(const char* label, float*, float, float, const SliderWithInputOptions& options)
{
    Stub(label, options);
    return false;
}

bool SliderWithInput(const char* label, int*, int, int, const SliderWithInputOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
