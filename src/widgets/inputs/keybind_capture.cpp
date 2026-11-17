// KeybindCapture: planned. Draws the placeholder at the field's default size until Phase 2
// implements the widget in this file on the key chord utilities.

#include "facet/widgets/inputs.h"

#include "widgets/controls_stub.h"

namespace facet
{

bool KeybindCapture(const char* label, Keybind*, const KeybindCaptureOptions& options)
{
    internal::ControlStub("KeybindCapture", label, internal::StubWidth(options.width, 0.0f),
                          internal::StubHeight(options.size, options.error, options.helpText));
    return false;
}

} // namespace facet
