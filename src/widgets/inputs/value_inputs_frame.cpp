// Per-frame and per-device work of the number-like inputs of inputs.h (NumberInput, DragNumber,
// UnitInput, VectorInput, SizeInput, HexInput, DurationInput, KeybindCapture, Rating). The entry
// exists in the subsystem table (src/core/subsystems.cpp) from the start, so these widgets fill
// their hooks here without touching a shared file. No hooks yet.

#include "internal/subsystem.h"

namespace facet::internal
{

Subsystem WidgetsValueInputsSubsystem()
{
    Subsystem subsystem;
    subsystem.name = "widgets_value_inputs";
    return subsystem;
}

} // namespace facet::internal
