// XYPad: planned. Draws the placeholder at the pad's default size until Phase 2 implements the
// widget in this file.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool XYPad(const char* label, ImVec2*, ImVec2, ImVec2, const XYPadOptions& options)
{
    const ImVec2 size = internal::StubPadSize(options.width, options.height);
    internal::ControlStub("XYPad", label, size.x, size.y + internal::StubMessageHeight(options.error, options.helpText));
    return false;
}

} // namespace facet
