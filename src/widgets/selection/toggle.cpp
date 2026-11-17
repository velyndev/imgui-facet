// Toggle: planned. Draws the placeholder at the switch's default size until Phase 2 implements
// the widget in this file.

#include "facet/widgets/selection.h"

#include "facet/core/scale.h"
#include "facet/core/theme.h"
#include "widgets/controls_stub.h"

namespace facet
{

bool Toggle(const char* label, bool*, const ToggleOptions& options)
{
    const float content = internal::StubLabelWidth(label, options.size) + Px(T().metrics.toggleWidth);
    internal::ControlStub("Toggle", label, internal::StubWidth(options.width, content),
                          internal::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
