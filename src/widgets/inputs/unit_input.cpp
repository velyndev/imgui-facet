// UnitInput: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool UnitInput(const char* label, double*, int*, std::span<const UnitItem>, const UnitInputOptions& options)
{
    internal::ControlStub("UnitInput", label, internal::StubWidth(options.width, 0.0f),
                          internal::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
