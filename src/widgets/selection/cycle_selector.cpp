// CycleSelector: planned. Draws the placeholder at the selector's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/selection.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const CycleSelectorOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("CycleSelector", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool CycleSelector(const char* label, int*, std::span<const ChoiceItem>, const CycleSelectorOptions& options)
{
    Stub(label, options);
    return false;
}

bool CycleSelector(const char* label, int*, std::span<const char* const>, const CycleSelectorOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
