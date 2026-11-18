// CopyButton: planned. Draws the placeholder at the button's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/buttons.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool CopyButton(const char* label, const char*, const CopyButtonOptions& options)
{
    namespace stub = internal::controls;
    const float height = ControlHeight(options.size);
    const float width = !options.iconOnly ? height + stub::StubLabelWidth(label, options.size) : height;
    stub::ControlStub("CopyButton", label, width, height);
    return false;
}

} // namespace facet
