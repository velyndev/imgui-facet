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
    namespace stub = internal::controls;
    const float content = stub::StubLabelWidth(label, options.size) + Px(T().metrics.toggleWidth);
    stub::ControlStub("Toggle", label, stub::StubWidth(options.width, content),
                          stub::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
