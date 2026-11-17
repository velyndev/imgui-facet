// RadioGroup: planned. Draws the placeholder at the group's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/selection.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, std::size_t itemCount, const RadioGroupOptions& options)
{
    const bool vertical = options.orientation == Orientation::Vertical;
    const int rows = vertical && itemCount > 0 ? static_cast<int>(itemCount) : 1;
    const float height = internal::StubLabelLine(label) + static_cast<float>(rows) * ControlHeight(options.size)
                         + internal::StubMessageHeight(options.error, options.helpText);
    internal::ControlStub("RadioGroup", label, internal::StubWidth(0.0f, 0.0f), height);
}

} // namespace

bool RadioGroup(const char* label, int*, std::span<const ChoiceItem> items, const RadioGroupOptions& options)
{
    Stub(label, items.size(), options);
    return false;
}

bool RadioGroup(const char* label, int*, std::span<const char* const> items, const RadioGroupOptions& options)
{
    Stub(label, items.size(), options);
    return false;
}

} // namespace facet
