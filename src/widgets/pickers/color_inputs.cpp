// ColorInputs: planned. Draws the placeholder at the widget's default size until Phase 2
// implements it in this file on the value drag core.

#include "facet/widgets/pickers.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool ColorInputs(const char* label, ImVec4*, const ColorInputsOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("ColorInputs", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
