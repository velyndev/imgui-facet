// ToggleButtonGroup: planned. Draws the placeholder at the group's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/buttons.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{
namespace
{

void Stub(const char* label, std::span<const ChoiceItem> items, const ToggleButtonGroupOptions& options)
{
    const float height = ControlHeight(options.size);
    float content = 0.0f;
    for (const ChoiceItem& item : items)
    {
        content += options.iconOnly ? height
                                    : internal::StubLabelWidth(item.label, options.size) + 2.0f * Px(T().spacing.sm);
    }
    // Buttons that do not fit move into a "more" menu, so the group never outgrows the row.
    internal::ControlStub("ToggleButtonGroup", label, internal::StubFittedWidth(options.width, content), height);
}

} // namespace

bool ToggleButtonGroup(const char* label, std::span<bool>, std::span<const ChoiceItem> items,
                       const ToggleButtonGroupOptions& options)
{
    Stub(label, items, options);
    return false;
}

bool ToggleButtonGroup(const char* label, int*, std::span<const ChoiceItem> items, const ToggleButtonGroupOptions& options)
{
    Stub(label, items, options);
    return false;
}

} // namespace facet
