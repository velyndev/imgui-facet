// KeybindModeSelector: planned. Draws the placeholder at the selector's default size until
// Phase 2 implements the widget in this file.

#include "facet/widgets/selection.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{

bool KeybindModeSelector(const char* label, BindMode*, const KeybindModeSelectorOptions& options)
{
    const Strings& strings = GetStrings();
    const char* const modes[] = {strings.bindHold, strings.bindToggle, options.allowAlways ? strings.bindAlways : nullptr,
                                 options.allowOff ? strings.bindOff : nullptr};
    float content = 0.0f;
    for (const char* mode : modes)
    {
        if (mode != nullptr)
            content += internal::StubLabelWidth(mode, options.size) + 2.0f * Px(T().spacing.sm);
    }
    // Too narrow for the segments it becomes a dropdown, so it never outgrows the row.
    internal::ControlStub("KeybindModeSelector", label, internal::StubFittedWidth(options.width, content),
                          internal::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
