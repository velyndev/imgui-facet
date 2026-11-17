// ColorInputs: planned. Draws the placeholder at the widget's default size until Phase 2
// implements it in this file on the value drag core.

#include "facet/widgets/pickers.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool ColorInputs(const char* label, ImVec4*, const ColorInputsOptions& options)
{
    internal::ControlStub("ColorInputs", label, internal::StubWidth(options.width, 0.0f),
                          internal::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
