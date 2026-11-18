// ChipGroup: planned. Draws the placeholder at the group's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/selection.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, const ChipGroupOptions& options)
{
    namespace stub = internal::controls;
    stub::ControlStub("ChipGroup", label, stub::StubWidth(options.width, 0.0f), ControlHeight(options.size));
}

} // namespace

bool ChipGroup(const char* label, std::span<bool>, std::span<const ChoiceItem>, const ChipGroupOptions& options)
{
    Stub(label, options);
    return false;
}

bool ChipGroup(const char* label, std::span<bool>, std::span<const char* const>, const ChipGroupOptions& options)
{
    Stub(label, options);
    return false;
}

bool ChipGroup(const char* label, int*, std::span<const ChoiceItem>, const ChipGroupOptions& options)
{
    Stub(label, options);
    return false;
}

bool ChipGroup(const char* label, int*, std::span<const char* const>, const ChipGroupOptions& options)
{
    Stub(label, options);
    return false;
}

int ChipGroupRemovedItem()
{
    return -1;
}

} // namespace facet
