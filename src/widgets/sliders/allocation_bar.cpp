// AllocationBar: planned. Draws the placeholder at the bar's default size until Phase 2
// implements the widget in this file on the multi-handle block.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const AllocationBarOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("AllocationBar", label, stub::StubWidth(options.width, 0.0f),
                          stub::StubHeight(options.size, options.error, options.helpText));
}

} // namespace

bool AllocationBar(const char* label, std::span<float>, std::span<const ChoiceItem>, const AllocationBarOptions& options)
{
    Stub(label, options);
    return false;
}

bool AllocationBar(const char* label, std::span<float>, std::span<const char* const>, const AllocationBarOptions& options)
{
    Stub(label, options);
    return false;
}

} // namespace facet
