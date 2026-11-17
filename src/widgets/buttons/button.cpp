// Button: planned. Draws the placeholder at the button's default size until Phase 2 implements
// the widget in this file.

#include "facet/widgets/buttons.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{

bool Button(const char* label, const ButtonOptions& options)
{
    const Theme& t = T();
    const float height = ControlHeight(options.size);
    float content = internal::StubLabelWidth(label, options.size) + 2.0f * Px(t.spacing.md);
    if (options.icon.IsValid())
        content += Px(t.spacing.lg) + Px(t.spacing.xs);
    internal::ControlStub("Button", label, internal::StubWidth(options.width, content), height);
    return false;
}

} // namespace facet
