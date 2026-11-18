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

float ButtonWidth(const char* text, bool iconOnly, const ToggleButtonGroupOptions& options)
{
    namespace stub = internal::controls;
    if (iconOnly)
        return ControlHeight(options.size);
    return stub::StubLabelWidth(text, options.size) + 2.0f * Px(T().spacing.sm);
}

void Stub(const char* label, float content, const ToggleButtonGroupOptions& options)
{
    namespace stub = internal::controls;
    // Buttons that do not fit move into a "more" menu, so the group never outgrows the row.
    stub::ControlStub("ToggleButtonGroup", label, stub::StubFittedWidth(options.width, content),
                          ControlHeight(options.size));
}

float ContentWidth(std::span<const ChoiceItem> items, const ToggleButtonGroupOptions& options)
{
    float content = 0.0f;
    for (const ChoiceItem& item : items)
        content += ButtonWidth(item.label, options.iconOnly, options);
    return content;
}

float ContentWidth(std::span<const char* const> items, const ToggleButtonGroupOptions& options)
{
    float content = 0.0f;
    for (const char* item : items)
        content += ButtonWidth(item, false, options);
    return content;
}

} // namespace

bool ToggleButtonGroup(const char* label, std::span<bool>, std::span<const ChoiceItem> items,
                       const ToggleButtonGroupOptions& options)
{
    Stub(label, ContentWidth(items, options), options);
    return false;
}

bool ToggleButtonGroup(const char* label, int*, std::span<const ChoiceItem> items, const ToggleButtonGroupOptions& options)
{
    Stub(label, ContentWidth(items, options), options);
    return false;
}

bool ToggleButtonGroup(const char* label, std::span<bool>, std::span<const char* const> items,
                       const ToggleButtonGroupOptions& options)
{
    Stub(label, ContentWidth(items, options), options);
    return false;
}

bool ToggleButtonGroup(const char* label, int*, std::span<const char* const> items,
                       const ToggleButtonGroupOptions& options)
{
    Stub(label, ContentWidth(items, options), options);
    return false;
}

} // namespace facet
