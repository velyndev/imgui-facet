// StepIndicator (navigation.h). Planned: draws the placeholder of src/widgets/structure_stub.h
// until Phase 2 implements the widget. The interactive overload returns false and never writes
// *current.

#include "facet/widgets/navigation.h"
#include "widgets/structure_stub.h"

namespace facet
{
namespace
{

void DrawStepIndicator(const char* id, const StepIndicatorOptions& options)
{
    internal::StubItem("StepIndicator", id, internal::StubWidth(options.width, true), ControlHeight(options.size));
}

} // namespace

void StepIndicator(const char* id, int, std::span<const ChoiceItem>, const StepIndicatorOptions& options)
{
    DrawStepIndicator(id, options);
}

bool StepIndicator(const char* id, int*, std::span<const ChoiceItem>, const StepIndicatorOptions& options)
{
    DrawStepIndicator(id, options);
    return false;
}

} // namespace facet
