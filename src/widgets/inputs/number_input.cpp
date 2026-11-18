// NumberInput: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the text field core and the value drag core.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const NumberInputOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("NumberInput", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool NumberInput(const char* label, int*, const NumberInputOptions& options)
{
    Stub(label, options);
    return false;
}

bool NumberInput(const char* label, float*, const NumberInputOptions& options)
{
    Stub(label, options);
    return false;
}

bool NumberInput(const char* label, double*, const NumberInputOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
