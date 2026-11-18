// BoxRangePad: planned. Draws the placeholder at the pad's default size until Phase 2
// implements the widget in this file on the multi-handle block.

#include "facet/widgets/sliders.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool BoxRangePad(const char* label, ImVec2*, ImVec2*, ImVec2, ImVec2, const BoxRangePadOptions& options)
{
    namespace stub = internal::controls;
    const ImVec2 size = stub::StubPadSize(options.width, options.height);
    stub::ControlStub("BoxRangePad", label, size.x,
                          size.y + stub::StubMessageHeight(options.error, options.helpText));
    return false;
}

} // namespace facet
