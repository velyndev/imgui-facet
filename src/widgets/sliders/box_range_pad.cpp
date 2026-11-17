// BoxRangePad: planned. Draws the placeholder at the pad's default size until Phase 2
// implements the widget in this file on the multi-handle block.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool BoxRangePad(const char* label, ImVec2*, ImVec2*, ImVec2, ImVec2, const BoxRangePadOptions& options)
{
    const ImVec2 size = internal::StubPadSize(options.width, options.height);
    internal::ControlStub("BoxRangePad", label, size.x,
                          size.y + internal::StubMessageHeight(options.error, options.helpText));
    return false;
}

} // namespace facet
