// FieldValidation and ValidationSummary: planned. Both draw the placeholder until Phase 2
// implements them in this file, with the per-frame message list in the controls subsystem.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{

void FieldValidation(const char* message, const FieldValidationOptions&)
{
    namespace stub = internal::controls;
    if (message == nullptr)
        return;
    // Layout only, under the validated item, which stays the last item.
    stub::MessageStub("FieldValidation", message);
}

bool ValidationSummary(const char* label, const ValidationSummaryOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("ValidationSummary", label, stub::StubWidth(options.width, 0.0f),
                          ControlHeight(ControlSize::Inherit));
    return false;
}

} // namespace facet
