// CheckboxGroup: planned. Draws the placeholder at the group's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/selection.h"

#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, std::size_t itemCount, const CheckboxGroupOptions& options)
{
    const int rows = static_cast<int>(itemCount) + (options.selectAll ? 1 : 0);
    const float height = internal::StubLabelLine(label) + static_cast<float>(rows > 0 ? rows : 1) * ControlHeight(options.size)
                         + internal::StubMessageHeight(options.error, options.helpText);
    internal::ControlStub("CheckboxGroup", label, internal::StubWidth(0.0f, 0.0f), height);
}

} // namespace

bool CheckboxGroup(const char* label, std::span<bool>, std::span<const ChoiceItem> items, const CheckboxGroupOptions& options)
{
    Stub(label, items.size(), options);
    return false;
}

bool CheckboxGroup(const char* label, std::span<bool>, std::span<const char* const> items,
                   const CheckboxGroupOptions& options)
{
    Stub(label, items.size(), options);
    return false;
}

} // namespace facet
