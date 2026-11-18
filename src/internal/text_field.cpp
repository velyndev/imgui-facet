// Text field core: wraps Dear ImGui's text editing (IME, undo, clipboard stay Dear ImGui's)
// and draws the Facet frame around it.
// Planned for Phase 1c part B: text_field.h and the behaviour. Hooks are optional; a text field
// core without per-frame work leaves them null.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem TextFieldSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "text_field";
    return subsystem;
}

} // namespace facet::internal