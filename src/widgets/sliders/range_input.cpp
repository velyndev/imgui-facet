// RangeInput: planned. Draws the placeholder at the widget's default size until Phase 2
// implements it in this file on the multi-handle block.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const RangeInputOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("RangeInput", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool RangeInput(const char* label, float*, float*, float, float, const RangeInputOptions& options)
{
    Stub(label, options);
    return false;
}

bool RangeInput(const char* label, int*, int*, int, int, const RangeInputOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
