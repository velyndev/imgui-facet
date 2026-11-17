// CopyButton: planned. Draws the placeholder at the button's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/buttons.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool CopyButton(const char* label, const char*, const CopyButtonOptions& options)
{
    const float height = ControlHeight(options.size);
    const float width = options.showLabel ? height + internal::StubLabelWidth(label, options.size) : height;
    internal::ControlStub("CopyButton", label, width, height);
    return false;
}

} // namespace facet
