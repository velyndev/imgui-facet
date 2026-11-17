// IconButton: planned. Draws the placeholder at the button's default size until Phase 2
// implements the widget in this file.

#include "facet/widgets/buttons.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool IconButton(const char* label, Icon, const IconButtonOptions& options)
{
    const float side = ControlHeight(options.size);
    internal::ControlStub("IconButton", label, side, side);
    return false;
}

} // namespace facet
